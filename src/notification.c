#include "notification.h"
#include "database.h"
#include "audit.h"

StatusCode notification_create(int schedule_id, const char *type, const char *scheduled_at, const char *failure_reason) {
    if (schedule_id <= 0 || str_is_blank(type) || str_is_blank(scheduled_at)) {
        return STATUS_INVALID_INPUT;
    }
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "INSERT OR IGNORE INTO notifications (schedule_id, notification_type, status, scheduled_at, sent_at, failure_reason, created_at) "
        "VALUES (?, ?, 'Active', ?, datetime('now'), ?, datetime('now'));";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_int(stmt, 1, schedule_id);
    sqlite3_bind_text(stmt, 2, type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, scheduled_at, -1, SQLITE_STATIC);
    if (!str_is_blank(failure_reason)) {
        sqlite3_bind_text(stmt, 4, failure_reason, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_text(stmt, 4, "Delivered In-App (External SMS/Email not configured)", -1, SQLITE_STATIC);
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;
    int changes = sqlite3_changes(db);
    return (changes > 0) ? STATUS_OK : STATUS_EXISTS;
}

StatusCode notification_list_active(Notification **out_notifications, int *out_count) {
    if (!out_notifications || !out_count) return STATUS_INVALID_INPUT;
    *out_notifications = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "SELECT n.id, n.schedule_id, s.vehicle_id, v.registration_number, s.service_name, "
        "       n.notification_type, n.status, n.scheduled_at, n.sent_at, n.failure_reason, n.created_at "
        "FROM notifications n "
        "JOIN maintenance_schedules s ON n.schedule_id = s.id "
        "JOIN vehicles v ON s.vehicle_id = v.id "
        "WHERE n.status = 'Active' "
        "ORDER BY n.created_at DESC;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    int capacity = 16;
    int count = 0;
    Notification *list = (Notification *)malloc(capacity * sizeof(Notification));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    char today[16];
    get_current_iso_date(today, sizeof(today));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            Notification *tmp = (Notification *)realloc(list, capacity * sizeof(Notification));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }

        memset(&list[count], 0, sizeof(Notification));
        list[count].id = sqlite3_column_int(stmt, 0);
        list[count].schedule_id = sqlite3_column_int(stmt, 1);
        list[count].vehicle_id = sqlite3_column_int(stmt, 2);
        safe_strncpy(list[count].vehicle_reg, (const char *)sqlite3_column_text(stmt, 3), sizeof(list[count].vehicle_reg));
        safe_strncpy(list[count].service_name, (const char *)sqlite3_column_text(stmt, 4), sizeof(list[count].service_name));
        safe_strncpy(list[count].notification_type, (const char *)sqlite3_column_text(stmt, 5), sizeof(list[count].notification_type));
        safe_strncpy(list[count].status, (const char *)sqlite3_column_text(stmt, 6), sizeof(list[count].status));
        safe_strncpy(list[count].scheduled_at, (const char *)sqlite3_column_text(stmt, 7), sizeof(list[count].scheduled_at));
        safe_strncpy(list[count].sent_at, (const char *)sqlite3_column_text(stmt, 8), sizeof(list[count].sent_at));
        const char *fail = (const char *)sqlite3_column_text(stmt, 9);
        safe_strncpy(list[count].failure_reason, fail ? fail : "", sizeof(list[count].failure_reason));
        safe_strncpy(list[count].created_at, (const char *)sqlite3_column_text(stmt, 10), sizeof(list[count].created_at));

        list[count].days_overdue_or_remaining = days_between_dates(today, list[count].scheduled_at);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_notifications = list;
    *out_count = count;
    return STATUS_OK;
}

StatusCode notification_dismiss(int id, int user_id) {
    if (id <= 0) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "UPDATE notifications SET status = 'Dismissed' WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    audit_log_action(user_id, "NOTIFICATION_DISMISS", "notifications", id, "Dismissed in-app notification");
    return STATUS_OK;
}

StatusCode notification_dismiss_all(int user_id) {
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "UPDATE notifications SET status = 'Dismissed' WHERE status = 'Active';";
    int rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) return STATUS_DB_ERROR;

    audit_log_action(user_id, "NOTIFICATIONS_CLEAR", "notifications", 0, "Dismissed all active notifications");
    return STATUS_OK;
}

void notification_free_list(Notification *notifications) {
    if (notifications) free(notifications);
}
