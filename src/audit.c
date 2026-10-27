#include "audit.h"
#include "database.h"

StatusCode audit_log_action(int user_id, const char *action, const char *entity_type, int entity_id, const char *details) {
    if (str_is_blank(action) || str_is_blank(entity_type)) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "INSERT INTO audit_logs (user_id, action, entity_type, entity_id, timestamp, details) "
        "VALUES (?, ?, ?, ?, datetime('now'), ?);";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    if (user_id > 0) {
        sqlite3_bind_int(stmt, 1, user_id);
    } else {
        sqlite3_bind_null(stmt, 1);
    }
    sqlite3_bind_text(stmt, 2, action, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, entity_type, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, entity_id);
    sqlite3_bind_text(stmt, 5, details ? details : "", -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? STATUS_OK : STATUS_DB_ERROR;
}

static void fill_audit_from_stmt(sqlite3_stmt *stmt, AuditLog *log) {
    memset(log, 0, sizeof(AuditLog));
    log->id = sqlite3_column_int(stmt, 0);
    log->user_id = sqlite3_column_type(stmt, 1) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 1);
    const char *u = (const char *)sqlite3_column_text(stmt, 2);
    safe_strncpy(log->username, u ? u : "system", sizeof(log->username));
    safe_strncpy(log->action, (const char *)sqlite3_column_text(stmt, 3), sizeof(log->action));
    safe_strncpy(log->entity_type, (const char *)sqlite3_column_text(stmt, 4), sizeof(log->entity_type));
    log->entity_id = sqlite3_column_int(stmt, 5);
    safe_strncpy(log->timestamp, (const char *)sqlite3_column_text(stmt, 6), sizeof(log->timestamp));
    const char *d = (const char *)sqlite3_column_text(stmt, 7);
    safe_strncpy(log->details, d ? d : "", sizeof(log->details));
}

StatusCode audit_list(int limit, AuditLog **out_logs, int *out_count) {
    if (!out_logs || !out_count) return STATUS_INVALID_INPUT;
    *out_logs = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    if (limit <= 0) limit = 50;

    const char *sql = 
        "SELECT a.id, a.user_id, u.username, a.action, a.entity_type, a.entity_id, a.timestamp, a.details "
        "FROM audit_logs a "
        "LEFT JOIN users u ON a.user_id = u.id "
        "ORDER BY a.timestamp DESC, a.id DESC LIMIT ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, limit);

    int capacity = 16;
    int count = 0;
    AuditLog *list = (AuditLog *)malloc(capacity * sizeof(AuditLog));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            AuditLog *tmp = (AuditLog *)realloc(list, capacity * sizeof(AuditLog));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }
        fill_audit_from_stmt(stmt, &list[count]);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_logs = list;
    *out_count = count;
    return STATUS_OK;
}

StatusCode audit_filter(const char *entity_type, int entity_id, AuditLog **out_logs, int *out_count) {
    if (!out_logs || !out_count || str_is_blank(entity_type)) return STATUS_INVALID_INPUT;
    *out_logs = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "SELECT a.id, a.user_id, u.username, a.action, a.entity_type, a.entity_id, a.timestamp, a.details "
        "FROM audit_logs a "
        "LEFT JOIN users u ON a.user_id = u.id "
        "WHERE a.entity_type = ? AND a.entity_id = ? "
        "ORDER BY a.timestamp DESC, a.id DESC;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_text(stmt, 1, entity_type, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, entity_id);

    int capacity = 16;
    int count = 0;
    AuditLog *list = (AuditLog *)malloc(capacity * sizeof(AuditLog));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            AuditLog *tmp = (AuditLog *)realloc(list, capacity * sizeof(AuditLog));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }
        fill_audit_from_stmt(stmt, &list[count]);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_logs = list;
    *out_count = count;
    return STATUS_OK;
}

void audit_free_list(AuditLog *logs) {
    if (logs) free(logs);
}
