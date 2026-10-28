#include "auth.h"
#include "database.h"
#include "audit.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>

static User g_session_user = {0};
static bool g_is_logged_in = false;

const char *auth_role_to_string(UserRole role) {
    switch (role) {
        case ROLE_ADMIN:   return "admin";
        case ROLE_MANAGER: return "manager";
        case ROLE_STAFF:   return "staff";
        case ROLE_VIEWER:  return "viewer";
        default:           return "unknown";
    }
}

UserRole auth_string_to_role(const char *role_str) {
    if (!role_str) return ROLE_UNKNOWN;
    if (strcmp(role_str, "admin") == 0) return ROLE_ADMIN;
    if (strcmp(role_str, "manager") == 0) return ROLE_MANAGER;
    if (strcmp(role_str, "staff") == 0) return ROLE_STAFF;
    if (strcmp(role_str, "viewer") == 0) return ROLE_VIEWER;
    return ROLE_UNKNOWN;
}

bool auth_has_permission(UserRole role, int permission_flag) {
    switch (role) {
        case ROLE_ADMIN:
            return true; /* Admin has all permissions */
        case ROLE_MANAGER: {
            int manager_perms = PERM_READ | PERM_MAINTENANCE_WRITE | PERM_SCHEDULE_WRITE | 
                               PERM_VEHICLE_WRITE | PERM_DRIVER_WRITE | PERM_EXPENSE_WRITE | PERM_REPORTS;
            return (permission_flag & manager_perms) == permission_flag;
        }
        case ROLE_STAFF: {
            int staff_perms = PERM_READ | PERM_MAINTENANCE_WRITE | PERM_SCHEDULE_WRITE;
            return (permission_flag & staff_perms) == permission_flag;
        }
        case ROLE_VIEWER: {
            int viewer_perms = PERM_READ;
            return (permission_flag & viewer_perms) == permission_flag;
        }
        default:
            return false;
    }
}

static void bytes_to_hex(const unsigned char *bytes, size_t len, char *out_hex) {
    for (size_t i = 0; i < len; i++) {
        sprintf(out_hex + (i * 2), "%02x", bytes[i]);
    }
    out_hex[len * 2] = '\0';
}

static bool hex_to_bytes(const char *hex, unsigned char *out_bytes, size_t out_len) {
    if (strlen(hex) != out_len * 2) return false;
    for (size_t i = 0; i < out_len; i++) {
        unsigned int byte_val;
        if (sscanf(hex + (i * 2), "%02x", &byte_val) != 1) return false;
        out_bytes[i] = (unsigned char)byte_val;
    }
    return true;
}

StatusCode auth_generate_salt(char *out_hex_salt, size_t out_len) {
    if (!out_hex_salt || out_len < (SALT_BYTE_LENGTH * 2 + 1)) return STATUS_INVALID_INPUT;
    unsigned char salt_bytes[SALT_BYTE_LENGTH];
    if (RAND_bytes(salt_bytes, sizeof(salt_bytes)) != 1) {
        fprintf(stderr, "[SECURITY ERROR] OpenSSL RAND_bytes failed\n");
        return STATUS_ERROR;
    }
    bytes_to_hex(salt_bytes, sizeof(salt_bytes), out_hex_salt);
    return STATUS_OK;
}

StatusCode auth_hash_password(const char *password, const char *salt, char *out_hex_hash, size_t out_len) {
    if (!password || !salt || !out_hex_hash || out_len < HASH_HEX_LENGTH) {
        return STATUS_INVALID_INPUT;
    }

    unsigned char salt_bytes[SALT_BYTE_LENGTH];
    if (!hex_to_bytes(salt, salt_bytes, sizeof(salt_bytes))) {
        return STATUS_INVALID_INPUT;
    }

    unsigned char hash_bytes[32];
    int rc = PKCS5_PBKDF2_HMAC(
        password,
        (int)strlen(password),
        salt_bytes,
        sizeof(salt_bytes),
        PBKDF2_ITERATIONS,
        EVP_sha256(),
        sizeof(hash_bytes),
        hash_bytes
    );

    if (rc != 1) {
        fprintf(stderr, "[SECURITY ERROR] PKCS5_PBKDF2_HMAC failed\n");
        return STATUS_ERROR;
    }

    bytes_to_hex(hash_bytes, sizeof(hash_bytes), out_hex_hash);
    return STATUS_OK;
}

bool auth_verify_password(const char *password, const char *salt, const char *expected_hash) {
    if (!password || !salt || !expected_hash) return false;

    char computed_hash[HASH_HEX_LENGTH];
    if (auth_hash_password(password, salt, computed_hash, sizeof(computed_hash)) != STATUS_OK) {
        return false;
    }

    unsigned char comp_bytes[32];
    unsigned char exp_bytes[32];
    if (!hex_to_bytes(computed_hash, comp_bytes, 32)) return false;
    if (!hex_to_bytes(expected_hash, exp_bytes, 32)) return false;

    /* Constant-time comparison to prevent timing attacks */
    return (CRYPTO_memcmp(comp_bytes, exp_bytes, 32) == 0);
}

StatusCode auth_is_first_run(bool *out_is_first_run) {
    if (!out_is_first_run) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "SELECT COUNT(*) FROM users WHERE role = 'admin' AND active = 1;";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return STATUS_DB_ERROR;

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    *out_is_first_run = (count == 0);
    return STATUS_OK;
}

StatusCode auth_create_user(const char *username, const char *password, UserRole role) {
    if (!username || !password || role == ROLE_UNKNOWN) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    if (strlen(password) < 6) {
        fprintf(stderr, "Password must be at least 6 characters long.\n");
        return STATUS_INVALID_INPUT;
    }

    char salt[SALT_BYTE_LENGTH * 2 + 1];
    if (auth_generate_salt(salt, sizeof(salt)) != STATUS_OK) return STATUS_ERROR;

    char hash[HASH_HEX_LENGTH];
    if (auth_hash_password(password, salt, hash, sizeof(hash)) != STATUS_OK) return STATUS_ERROR;

    const char *sql = 
        "INSERT INTO users (username, password_hash, password_salt, role, active, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, 1, datetime('now'), datetime('now'));";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, salt, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, auth_role_to_string(role), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_CONSTRAINT) {
        return STATUS_EXISTS;
    } else if (rc != SQLITE_DONE) {
        return STATUS_DB_ERROR;
    }

    int user_id = (int)db_last_insert_id();
    char details[MAX_BUFFER_MD];
    snprintf(details, sizeof(details), "Created user '%s' with role '%s'", username, auth_role_to_string(role));
    audit_log_action(g_is_logged_in ? g_session_user.id : user_id, "USER_CREATE", "users", user_id, details);

    return STATUS_OK;
}

StatusCode auth_login(const char *username, const char *password, User *out_user) {
    if (!username || !password) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "SELECT id, username, password_hash, password_salt, role, active, failed_attempts, created_at, updated_at "
        "FROM users WHERE username = ?;";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        audit_log_action(0, "LOGIN_FAILED", "users", 0, "Unknown username");
        return STATUS_AUTH_FAILED;
    }

    int id = sqlite3_column_int(stmt, 0);
    char db_username[MAX_BUFFER_SM] = {0};
    safe_strncpy(db_username, (const char *)sqlite3_column_text(stmt, 1), sizeof(db_username));
    char hash[HASH_HEX_LENGTH] = {0};
    safe_strncpy(hash, (const char *)sqlite3_column_text(stmt, 2), sizeof(hash));
    char salt[SALT_BYTE_LENGTH * 2 + 1] = {0};
    safe_strncpy(salt, (const char *)sqlite3_column_text(stmt, 3), sizeof(salt));
    char role_str[MAX_BUFFER_SM] = {0};
    safe_strncpy(role_str, (const char *)sqlite3_column_text(stmt, 4), sizeof(role_str));
    int active = sqlite3_column_int(stmt, 5);
    char created_at[MAX_BUFFER_SM] = {0};
    safe_strncpy(created_at, (const char *)sqlite3_column_text(stmt, 7), sizeof(created_at));
    char updated_at[MAX_BUFFER_SM] = {0};
    safe_strncpy(updated_at, (const char *)sqlite3_column_text(stmt, 8), sizeof(updated_at));

    if (!active) {
        sqlite3_finalize(stmt);
        audit_log_action(id, "LOGIN_BLOCKED", "users", id, "Account is disabled");
        return STATUS_FORBIDDEN;
    }

    bool matched = auth_verify_password(password, salt, hash);
    sqlite3_finalize(stmt);

    if (!matched) {
        /* Increment failed attempts */
        sqlite3_stmt *upd = NULL;
        const char *upd_sql = "UPDATE users SET failed_attempts = failed_attempts + 1 WHERE id = ?;";
        if (sqlite3_prepare_v2(db, upd_sql, -1, &upd, NULL) == SQLITE_OK) {
            sqlite3_bind_int(upd, 1, id);
            sqlite3_step(upd);
            sqlite3_finalize(upd);
        }
        audit_log_action(id, "LOGIN_FAILED", "users", id, "Incorrect password");
        return STATUS_AUTH_FAILED;
    }

    /* Reset failed attempts on successful login */
    sqlite3_stmt *reset_stmt = NULL;
    const char *reset_sql = "UPDATE users SET failed_attempts = 0, updated_at = datetime('now') WHERE id = ?;";
    if (sqlite3_prepare_v2(db, reset_sql, -1, &reset_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(reset_stmt, 1, id);
        sqlite3_step(reset_stmt);
        sqlite3_finalize(reset_stmt);
    }

    if (out_user) {
        out_user->id = id;
        safe_strncpy(out_user->username, db_username, sizeof(out_user->username));
        out_user->role = auth_string_to_role(role_str);
        out_user->active = (active != 0);
        out_user->failed_attempts = 0;
        safe_strncpy(out_user->created_at, created_at, sizeof(out_user->created_at));
        safe_strncpy(out_user->updated_at, updated_at, sizeof(out_user->updated_at));
    }

    audit_log_action(id, "LOGIN_SUCCESS", "users", id, "Successful authentication");
    return STATUS_OK;
}

StatusCode auth_change_password(int user_id, const char *old_password, const char *new_password) {
    if (user_id <= 0 || !old_password || !new_password) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    if (strlen(new_password) < 6) {
        fprintf(stderr, "New password must be at least 6 characters long.\n");
        return STATUS_INVALID_INPUT;
    }

    const char *sql = "SELECT password_hash, password_salt FROM users WHERE id = ? AND active = 1;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, user_id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return STATUS_NOT_FOUND;
    }

    const char *hash = (const char *)sqlite3_column_text(stmt, 0);
    const char *salt = (const char *)sqlite3_column_text(stmt, 1);
    if (!auth_verify_password(old_password, salt, hash)) {
        sqlite3_finalize(stmt);
        return STATUS_AUTH_FAILED;
    }
    sqlite3_finalize(stmt);

    char new_salt[SALT_BYTE_LENGTH * 2 + 1];
    if (auth_generate_salt(new_salt, sizeof(new_salt)) != STATUS_OK) return STATUS_ERROR;
    char new_hash[HASH_HEX_LENGTH];
    if (auth_hash_password(new_password, new_salt, new_hash, sizeof(new_hash)) != STATUS_OK) return STATUS_ERROR;

    const char *upd_sql = "UPDATE users SET password_hash = ?, password_salt = ?, updated_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *upd_stmt = NULL;
    if (sqlite3_prepare_v2(db, upd_sql, -1, &upd_stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_text(upd_stmt, 1, new_hash, -1, SQLITE_STATIC);
    sqlite3_bind_text(upd_stmt, 2, new_salt, -1, SQLITE_STATIC);
    sqlite3_bind_int(upd_stmt, 3, user_id);

    int rc = sqlite3_step(upd_stmt);
    sqlite3_finalize(upd_stmt);

    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    audit_log_action(user_id, "PASSWORD_CHANGE", "users", user_id, "User changed own password");
    return STATUS_OK;
}

StatusCode auth_admin_reset_password(int admin_user_id, int target_user_id, const char *new_password) {
    if (admin_user_id <= 0 || target_user_id <= 0 || !new_password) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    if (strlen(new_password) < 6) {
        fprintf(stderr, "Password must be at least 6 characters long.\n");
        return STATUS_INVALID_INPUT;
    }

    char new_salt[SALT_BYTE_LENGTH * 2 + 1];
    if (auth_generate_salt(new_salt, sizeof(new_salt)) != STATUS_OK) return STATUS_ERROR;
    char new_hash[HASH_HEX_LENGTH];
    if (auth_hash_password(new_password, new_salt, new_hash, sizeof(new_hash)) != STATUS_OK) return STATUS_ERROR;

    const char *sql = "UPDATE users SET password_hash = ?, password_salt = ?, failed_attempts = 0, updated_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_text(stmt, 1, new_hash, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, new_salt, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, target_user_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    char details[MAX_BUFFER_MD];
    snprintf(details, sizeof(details), "Administrator reset password for user_id %d", target_user_id);
    audit_log_action(admin_user_id, "ADMIN_PASSWORD_RESET", "users", target_user_id, details);
    return STATUS_OK;
}

StatusCode auth_set_user_status(int admin_user_id, int target_user_id, bool active) {
    if (admin_user_id <= 0 || target_user_id <= 0) return STATUS_INVALID_INPUT;
    if (admin_user_id == target_user_id) {
        fprintf(stderr, "Error: You cannot deactivate your own administrator account.\n");
        return STATUS_FORBIDDEN;
    }

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "UPDATE users SET active = ?, updated_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, active ? 1 : 0);
    sqlite3_bind_int(stmt, 2, target_user_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    char details[MAX_BUFFER_MD];
    snprintf(details, sizeof(details), "Status set to %s for user_id %d", active ? "ACTIVE" : "INACTIVE", target_user_id);
    audit_log_action(admin_user_id, "USER_STATUS_CHANGE", "users", target_user_id, details);
    return STATUS_OK;
}

StatusCode auth_list_users(void) {
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "SELECT id, username, role, active, created_at, updated_at FROM users ORDER BY id ASC;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    printf("\n+-----+----------------------+---------------+----------+---------------------+\n");
    printf("| ID  | Username             | Role          | Status   | Created At          |\n");
    printf("+-----+----------------------+---------------+----------+---------------------+\n");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char *username = (const char *)sqlite3_column_text(stmt, 1);
        const char *role = (const char *)sqlite3_column_text(stmt, 2);
        int active = sqlite3_column_int(stmt, 3);
        const char *created = (const char *)sqlite3_column_text(stmt, 4);

        printf("| %-3d | %-20s | %-13s | %-8s | %-19s |\n",
               id,
               username ? username : "",
               role ? role : "",
               active ? "ACTIVE" : "DISABLED",
               created ? created : "");
        count++;
    }
    printf("+-----+----------------------+---------------+----------+---------------------+\n");
    printf("Total users: %d\n", count);

    sqlite3_finalize(stmt);
    return STATUS_OK;
}

StatusCode auth_get_users(User **out_users, int *out_count) {
    if (!out_users || !out_count) return STATUS_INVALID_INPUT;
    *out_users = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "SELECT id, username, role, active, created_at, updated_at FROM users ORDER BY id ASC;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    int capacity = 8;
    int count = 0;
    User *list = (User *)malloc(capacity * sizeof(User));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            User *tmp = (User *)realloc(list, capacity * sizeof(User));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }

        memset(&list[count], 0, sizeof(User));
        list[count].id = sqlite3_column_int(stmt, 0);
        safe_strncpy(list[count].username, (const char *)sqlite3_column_text(stmt, 1), sizeof(list[count].username));
        const char *role_str = (const char *)sqlite3_column_text(stmt, 2);
        list[count].role = auth_string_to_role(role_str);
        list[count].active = (sqlite3_column_int(stmt, 3) != 0);
        safe_strncpy(list[count].created_at, (const char *)sqlite3_column_text(stmt, 4), sizeof(list[count].created_at));
        safe_strncpy(list[count].updated_at, (const char *)sqlite3_column_text(stmt, 5), sizeof(list[count].updated_at));
        count++;
    }

    sqlite3_finalize(stmt);
    *out_users = list;
    *out_count = count;
    return STATUS_OK;
}

void auth_free_users(User *users) {
    if (users) free(users);
}

void auth_set_session_user(const User *user) {
    if (user) {
        g_session_user = *user;
        g_is_logged_in = true;
    } else {
        auth_clear_session();
    }
}

const User *auth_get_session_user(void) {
    return g_is_logged_in ? &g_session_user : NULL;
}

void auth_clear_session(void) {
    memset(&g_session_user, 0, sizeof(g_session_user));
    g_is_logged_in = false;
}

void auth_logout(void) {
    auth_clear_session();
}

bool auth_is_logged_in(void) {
    return g_is_logged_in;
}

/* ==================== HTTP CRYPTOGRAPHIC SESSION MANAGEMENT ==================== */
#define MAX_ACTIVE_SESSIONS 64
#define SESSION_TTL_SECONDS (24 * 3600) /* 24 hours */

typedef struct {
    char token[AUTH_SESSION_TOKEN_LEN];
    User user;
    time_t created_at;
    time_t last_active_at;
    bool valid;
} SessionEntry;

static SessionEntry g_sessions[MAX_ACTIVE_SESSIONS] = {0};

void auth_sessions_cleanup(void) {
    time_t now = time(NULL);
    for (int i = 0; i < MAX_ACTIVE_SESSIONS; i++) {
        if (g_sessions[i].valid && (now - g_sessions[i].last_active_at > SESSION_TTL_SECONDS)) {
            memset(&g_sessions[i], 0, sizeof(SessionEntry));
        }
    }
}

StatusCode auth_session_create(const User *user, char *out_token, size_t out_len) {
    if (!user || !out_token || out_len < AUTH_SESSION_TOKEN_LEN) {
        return STATUS_INVALID_INPUT;
    }

    auth_sessions_cleanup();

    unsigned char raw_token[32];
    if (RAND_bytes(raw_token, sizeof(raw_token)) != 1) {
        fprintf(stderr, "[SECURITY ERROR] RAND_bytes failed for session token\n");
        return STATUS_ERROR;
    }

    char hex_token[AUTH_SESSION_TOKEN_LEN];
    bytes_to_hex(raw_token, sizeof(raw_token), hex_token);

    int slot_idx = -1;
    time_t oldest_time = time(NULL);
    int oldest_idx = 0;

    for (int i = 0; i < MAX_ACTIVE_SESSIONS; i++) {
        if (!g_sessions[i].valid) {
            slot_idx = i;
            break;
        }
        if (g_sessions[i].last_active_at < oldest_time) {
            oldest_time = g_sessions[i].last_active_at;
            oldest_idx = i;
        }
    }

    if (slot_idx < 0) {
        slot_idx = oldest_idx;
    }

    safe_strncpy(g_sessions[slot_idx].token, hex_token, sizeof(g_sessions[slot_idx].token));
    g_sessions[slot_idx].user = *user;
    g_sessions[slot_idx].created_at = time(NULL);
    g_sessions[slot_idx].last_active_at = time(NULL);
    g_sessions[slot_idx].valid = true;

    safe_strncpy(out_token, hex_token, out_len);
    return STATUS_OK;
}

StatusCode auth_session_validate(const char *token, User *out_user) {
    if (!token || strlen(token) != 64) {
        return STATUS_AUTH_FAILED;
    }

    time_t now = time(NULL);
    for (int i = 0; i < MAX_ACTIVE_SESSIONS; i++) {
        if (g_sessions[i].valid && strcmp(g_sessions[i].token, token) == 0) {
            if (now - g_sessions[i].last_active_at > SESSION_TTL_SECONDS) {
                g_sessions[i].valid = false;
                return STATUS_AUTH_FAILED;
            }

            /* Live database check: re-verify user status and latest role */
            sqlite3 *db = db_get_handle();
            if (db) {
                sqlite3_stmt *stmt = NULL;
                const char *sql = "SELECT role, active FROM users WHERE id = ?;";
                if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(stmt, 1, g_sessions[i].user.id);
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *role_str = (const char *)sqlite3_column_text(stmt, 0);
                        int active = sqlite3_column_int(stmt, 1);
                        if (!active) {
                            sqlite3_finalize(stmt);
                            g_sessions[i].valid = false;
                            return STATUS_AUTH_FAILED;
                        }
                        g_sessions[i].user.role = auth_string_to_role(role_str);
                        g_sessions[i].user.active = true;
                    } else {
                        sqlite3_finalize(stmt);
                        g_sessions[i].valid = false;
                        return STATUS_AUTH_FAILED;
                    }
                    sqlite3_finalize(stmt);
                }
            }

            g_sessions[i].last_active_at = now;
            if (out_user) {
                *out_user = g_sessions[i].user;
            }
            return STATUS_OK;
        }
    }

    return STATUS_AUTH_FAILED;
}

StatusCode auth_session_destroy(const char *token) {
    if (!token) return STATUS_INVALID_INPUT;
    for (int i = 0; i < MAX_ACTIVE_SESSIONS; i++) {
        if (g_sessions[i].valid && strcmp(g_sessions[i].token, token) == 0) {
            memset(&g_sessions[i], 0, sizeof(SessionEntry));
            return STATUS_OK;
        }
    }
    return STATUS_OK;
}
