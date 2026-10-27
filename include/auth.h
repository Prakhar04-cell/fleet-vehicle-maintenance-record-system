#ifndef AUTH_H
#define AUTH_H

#include "common.h"

typedef enum {
    ROLE_UNKNOWN = 0,
    ROLE_ADMIN = 1,
    ROLE_MANAGER = 2,
    ROLE_STAFF = 3,
    ROLE_VIEWER = 4
} UserRole;

/* Permission bitflags */
#define PERM_READ                (1 << 0)
#define PERM_MAINTENANCE_WRITE   (1 << 1)
#define PERM_SCHEDULE_WRITE      (1 << 2)
#define PERM_VEHICLE_WRITE       (1 << 3)
#define PERM_EXPENSE_WRITE       (1 << 4)
#define PERM_REPORTS             (1 << 5)
#define PERM_USER_MANAGE         (1 << 6)
#define PERM_BACKUP_RESTORE      (1 << 7)

#define SALT_BYTE_LENGTH 16
#define HASH_HEX_LENGTH 65
#define PBKDF2_ITERATIONS 100000

typedef struct {
    int id;
    char username[MAX_BUFFER_SM];
    UserRole role;
    bool active;
    int failed_attempts;
    char created_at[MAX_BUFFER_SM];
    char updated_at[MAX_BUFFER_SM];
} User;

/* Role helpers */
const char *auth_role_to_string(UserRole role);
UserRole auth_string_to_role(const char *role_str);
bool auth_has_permission(UserRole role, int permission_flag);

/* Cryptographic password hashing & verification */
StatusCode auth_generate_salt(char *out_hex_salt, size_t out_len);
StatusCode auth_hash_password(const char *password, const char *salt, char *out_hex_hash, size_t out_len);
bool auth_verify_password(const char *password, const char *salt, const char *expected_hash);

/* User lifecycle & session operations */
StatusCode auth_is_first_run(bool *out_is_first_run);
StatusCode auth_create_user(const char *username, const char *password, UserRole role);
StatusCode auth_login(const char *username, const char *password, User *out_user);
StatusCode auth_change_password(int user_id, const char *old_password, const char *new_password);
StatusCode auth_admin_reset_password(int admin_user_id, int target_user_id, const char *new_password);
StatusCode auth_set_user_status(int admin_user_id, int target_user_id, bool active);
StatusCode auth_list_users(void);

/* Current session management */
void auth_set_session_user(const User *user);
const User *auth_get_session_user(void);
void auth_clear_session(void);
void auth_logout(void);
bool auth_is_logged_in(void);

#endif /* AUTH_H */
