#include "database.h"
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#else
#define MKDIR(d) mkdir(d, 0755)
#endif

static sqlite3 *g_db = NULL;
static char g_active_db_path[MAX_BUFFER_MD] = "";

const char *db_get_path(void) {
    return g_active_db_path;
}

static void ensure_parent_directory(const char *filepath) {
    char dir[MAX_BUFFER_MD];
    safe_strncpy(dir, filepath, sizeof(dir));
    char *p = strrchr(dir, '/');
    if (!p) p = strrchr(dir, '\\');
    if (p) {
        *p = '\0';
        MKDIR(dir);
    }
}

StatusCode db_init(const char *db_path) {
    if (g_db) {
        return STATUS_OK;
    }
    const char *path = db_path ? db_path : DEFAULT_DB_PATH;
    safe_strncpy(g_active_db_path, path, sizeof(g_active_db_path));
    ensure_parent_directory(path);

    int rc = sqlite3_open_v2(path, &g_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB ERROR] Failed to open database '%s': %s\n", path, sqlite3_errmsg(g_db));
        if (g_db) {
            sqlite3_close(g_db);
            g_db = NULL;
        }
        return STATUS_DB_ERROR;
    }

    /* Enforce foreign keys */
    rc = sqlite3_exec(g_db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB ERROR] Failed to enable foreign keys: %s\n", sqlite3_errmsg(g_db));
        return STATUS_DB_ERROR;
    }

    /* Set busy timeout to 5000ms */
    sqlite3_busy_timeout(g_db, 5000);

    return STATUS_OK;
}

void db_close(void) {
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}

sqlite3 *db_get_handle(void) {
    return g_db;
}

StatusCode db_begin_transaction(void) {
    if (!g_db) return STATUS_DB_ERROR;
    int rc = sqlite3_exec(g_db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    return (rc == SQLITE_OK) ? STATUS_OK : STATUS_DB_ERROR;
}

StatusCode db_commit_transaction(void) {
    if (!g_db) return STATUS_DB_ERROR;
    int rc = sqlite3_exec(g_db, "COMMIT;", NULL, NULL, NULL);
    return (rc == SQLITE_OK) ? STATUS_OK : STATUS_DB_ERROR;
}

StatusCode db_rollback_transaction(void) {
    if (!g_db) return STATUS_DB_ERROR;
    int rc = sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
    return (rc == SQLITE_OK) ? STATUS_OK : STATUS_DB_ERROR;
}

StatusCode db_exec_simple(const char *sql) {
    if (!g_db || !sql) return STATUS_DB_ERROR;
    char *err_msg = NULL;
    int rc = sqlite3_exec(g_db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            fprintf(stderr, "[DB EXEC ERROR] %s\n", err_msg);
            sqlite3_free(err_msg);
        }
        return STATUS_DB_ERROR;
    }
    return STATUS_OK;
}

int64_t db_last_insert_id(void) {
    if (!g_db) return 0;
    return sqlite3_last_insert_rowid(g_db);
}

StatusCode db_check_integrity(char *out_result, size_t max_len) {
    if (!g_db) return STATUS_DB_ERROR;
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, "PRAGMA integrity_check;", -1, &stmt, NULL);
    if (rc != SQLITE_OK) return STATUS_DB_ERROR;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *res = (const char *)sqlite3_column_text(stmt, 0);
        if (out_result && max_len > 0) {
            safe_strncpy(out_result, res ? res : "unknown", max_len);
        }
    }
    sqlite3_finalize(stmt);
    return STATUS_OK;
}

static StatusCode apply_migration_file(const char *filepath, int version, const char *name) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        fprintf(stderr, "[MIGRATION] Cannot open migration file: %s\n", filepath);
        return STATUS_ERROR;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 0) {
        fclose(fp);
        return STATUS_OK;
    }

    char *sql_buffer = (char *)malloc(size + 1);
    if (!sql_buffer) {
        fclose(fp);
        return STATUS_ERROR;
    }

    size_t read_bytes = fread(sql_buffer, 1, size, fp);
    sql_buffer[read_bytes] = '\0';
    fclose(fp);

    char *err_msg = NULL;
    int rc = sqlite3_exec(g_db, sql_buffer, NULL, NULL, &err_msg);
    free(sql_buffer);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "[MIGRATION ERROR in %s] %s\n", filepath, err_msg ? err_msg : "Unknown error");
        if (err_msg) sqlite3_free(err_msg);
        return STATUS_DB_ERROR;
    }

    /* Record migration in tracking table */
    const char *track_sql = "INSERT OR IGNORE INTO schema_migrations (version, name, applied_at) VALUES (?, ?, datetime('now'));";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, track_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, version);
        sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    return STATUS_OK;
}

StatusCode db_run_migrations(const char *migrations_dir) {
    if (!g_db) return STATUS_DB_ERROR;

    /* Ensure schema_migrations table exists */
    const char *init_track =
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "  version INTEGER PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  applied_at TEXT NOT NULL"
        ");";
    if (db_exec_simple(init_track) != STATUS_OK) {
        return STATUS_DB_ERROR;
    }

    const char *dir = migrations_dir ? migrations_dir : DEFAULT_MIGRATIONS_DIR;
    char path001[MAX_BUFFER_MD];
    snprintf(path001, sizeof(path001), "%s/001_initial_schema.sql", dir);

    /* Check if migration 1 is already applied */
    sqlite3_stmt *check_stmt = NULL;
    int applied = 0;
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM schema_migrations WHERE version = 1;", -1, &check_stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            applied = sqlite3_column_int(check_stmt, 0);
        }
        sqlite3_finalize(check_stmt);
    }

    if (!applied) {
        printf("[DB] Applying migration 001_initial_schema.sql...\n");
        StatusCode status = apply_migration_file(path001, 1, "001_initial_schema");
        if (status != STATUS_OK) {
            return status;
        }
        printf("[DB] Migration 001 applied successfully.\n");
    }

    /* Check if migration 2 is already applied */
    char path002[MAX_BUFFER_MD];
    snprintf(path002, sizeof(path002), "%s/002_smart_fleet_management.sql", dir);
    int applied2 = 0;
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM schema_migrations WHERE version = 2;", -1, &check_stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            applied2 = sqlite3_column_int(check_stmt, 0);
        }
        sqlite3_finalize(check_stmt);
    }

    if (!applied2) {
        printf("[DB] Applying migration 002_smart_fleet_management.sql...\n");
        StatusCode status = apply_migration_file(path002, 2, "002_smart_fleet_management");
        if (status != STATUS_OK) {
            return status;
        }
        printf("[DB] Migration 002 applied successfully.\n");
    }

    /* Check if migration 3 is already applied */
    char path003[MAX_BUFFER_MD];
    snprintf(path003, sizeof(path003), "%s/003_live_monitoring_and_dsa.sql", dir);
    int applied3 = 0;
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM schema_migrations WHERE version = 3;", -1, &check_stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            applied3 = sqlite3_column_int(check_stmt, 0);
        }
        sqlite3_finalize(check_stmt);
    }

    if (!applied3) {
        printf("[DB] Applying migration 003_live_monitoring_and_dsa.sql...\n");
        StatusCode status = apply_migration_file(path003, 3, "003_live_monitoring_and_dsa");
        if (status != STATUS_OK) {
            return status;
        }
        printf("[DB] Migration 003 applied successfully.\n");
    }

    return STATUS_OK;
}
