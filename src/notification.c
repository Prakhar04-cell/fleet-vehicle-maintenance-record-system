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

StatusCode notification_event_create(const char *event_key, const char *category, const char *severity,
                                     const char *title, const char *message, const char *entity_type,
                                     int entity_id, const char *role_visibility) {
    if (str_is_blank(event_key) || str_is_blank(title)) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *cat = str_is_blank(category) ? "System" : category;
    const char *sev = str_is_blank(severity) ? "Info" : severity;
    const char *role = str_is_blank(role_visibility) ? "All" : role_visibility;

    const char *sql = 
        "INSERT OR IGNORE INTO system_notifications "
        "(event_key, category, severity, title, message, entity_type, entity_id, role_visibility, is_read, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0, datetime('now'));";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_text(stmt, 1, event_key, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, cat, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, sev, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, message ? message : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, entity_type ? entity_type : "system", -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, entity_id);
    sqlite3_bind_text(stmt, 8, role, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;
    return (sqlite3_changes(db) > 0) ? STATUS_OK : STATUS_EXISTS;
}

StatusCode notification_system_list(const char *category, int unread_only, const char *user_role,
                                    SystemNotification **out_list, int *out_count) {
    if (!out_list || !out_count) return STATUS_INVALID_INPUT;
    *out_list = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    char query[MAX_BUFFER_XL] = 
        "SELECT id, event_key, category, severity, title, message, entity_type, entity_id, "
        "       role_visibility, is_read, acknowledged_by, COALESCE(acknowledged_at, ''), created_at "
        "FROM system_notifications WHERE 1=1 ";

    if (!str_is_blank(category) && strcmp(category, "All") != 0) {
        strcat(query, "AND category = ? ");
    }
    if (unread_only) {
        strcat(query, "AND is_read = 0 ");
    }
    if (!str_is_blank(user_role) && strcasecmp(user_role, "admin") != 0) {
        strcat(query, "AND (role_visibility = 'All' OR LOWER(role_visibility) = LOWER(?)) ");
    }
    strcat(query, "ORDER BY id DESC LIMIT 100;");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    int bind_idx = 1;
    if (!str_is_blank(category) && strcmp(category, "All") != 0) {
        sqlite3_bind_text(stmt, bind_idx++, category, -1, SQLITE_STATIC);
    }
    if (!str_is_blank(user_role) && strcasecmp(user_role, "admin") != 0) {
        sqlite3_bind_text(stmt, bind_idx++, user_role, -1, SQLITE_STATIC);
    }

    int capacity = 16;
    int count = 0;
    SystemNotification *list = (SystemNotification *)malloc(capacity * sizeof(SystemNotification));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            SystemNotification *tmp = (SystemNotification *)realloc(list, capacity * sizeof(SystemNotification));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }

        memset(&list[count], 0, sizeof(SystemNotification));
        list[count].id = sqlite3_column_int(stmt, 0);
        safe_strncpy(list[count].event_key, (const char *)sqlite3_column_text(stmt, 1), sizeof(list[count].event_key));
        safe_strncpy(list[count].category, (const char *)sqlite3_column_text(stmt, 2), sizeof(list[count].category));
        safe_strncpy(list[count].severity, (const char *)sqlite3_column_text(stmt, 3), sizeof(list[count].severity));
        safe_strncpy(list[count].title, (const char *)sqlite3_column_text(stmt, 4), sizeof(list[count].title));
        const char *msg = (const char *)sqlite3_column_text(stmt, 5);
        safe_strncpy(list[count].message, msg ? msg : "", sizeof(list[count].message));
        safe_strncpy(list[count].entity_type, (const char *)sqlite3_column_text(stmt, 6), sizeof(list[count].entity_type));
        list[count].entity_id = sqlite3_column_int(stmt, 7);
        safe_strncpy(list[count].role_visibility, (const char *)sqlite3_column_text(stmt, 8), sizeof(list[count].role_visibility));
        list[count].is_read = sqlite3_column_int(stmt, 9);
        list[count].acknowledged_by = sqlite3_column_type(stmt, 10) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 10);
        const char *ack_at = (const char *)sqlite3_column_text(stmt, 11);
        safe_strncpy(list[count].acknowledged_at, ack_at ? ack_at : "", sizeof(list[count].acknowledged_at));
        safe_strncpy(list[count].created_at, (const char *)sqlite3_column_text(stmt, 12), sizeof(list[count].created_at));
        count++;
    }

    sqlite3_finalize(stmt);
    *out_list = list;
    *out_count = count;
    return STATUS_OK;
}

StatusCode notification_acknowledge(int id, int user_id) {
    if (id <= 0) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "UPDATE system_notifications "
        "SET is_read = 1, acknowledged_by = ?, acknowledged_at = datetime('now') "
        "WHERE id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    if (user_id > 0) sqlite3_bind_int(stmt, 1, user_id);
    else sqlite3_bind_null(stmt, 1);
    sqlite3_bind_int(stmt, 2, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;
    audit_log_action(user_id, "NOTIFICATION_ACK", "system_notifications", id, "Acknowledged notification");
    return STATUS_OK;
}

StatusCode notification_acknowledge_all(int user_id, const char *user_role) {
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    char sql[MAX_BUFFER_MD] = 
        "UPDATE system_notifications "
        "SET is_read = 1, acknowledged_by = ?, acknowledged_at = datetime('now') "
        "WHERE is_read = 0 ";

    if (!str_is_blank(user_role) && strcasecmp(user_role, "admin") != 0) {
        strcat(sql, "AND (role_visibility = 'All' OR LOWER(role_visibility) = LOWER(?)) ");
    }
    strcat(sql, ";");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    if (user_id > 0) sqlite3_bind_int(stmt, 1, user_id);
    else sqlite3_bind_null(stmt, 1);
    if (!str_is_blank(user_role) && strcasecmp(user_role, "admin") != 0) {
        sqlite3_bind_text(stmt, 2, user_role, -1, SQLITE_STATIC);
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;
    audit_log_action(user_id, "NOTIFICATION_ACK_ALL", "system_notifications", 0, "Acknowledged all visible notifications");
    return STATUS_OK;
}

StatusCode notification_sync_all_events(int *out_synced_count) {
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;
    int synced = 0;

    /* 1. Sync Overdue and Due Maintenance Schedules */
    const char *sched_sql = 
        "SELECT s.id, v.registration_number, s.service_name, s.due_date, s.status "
        "FROM maintenance_schedules s "
        "JOIN vehicles v ON s.vehicle_id = v.id "
        "WHERE s.status IN ('Due', 'Overdue');";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sched_sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int sid = sqlite3_column_int(stmt, 0);
            const char *reg = (const char *)sqlite3_column_text(stmt, 1);
            const char *svc = (const char *)sqlite3_column_text(stmt, 2);
            const char *due = (const char *)sqlite3_column_text(stmt, 3);
            const char *st = (const char *)sqlite3_column_text(stmt, 4);

            char key[128], title[128], msg[256];
            snprintf(key, sizeof(key), "SCHED-%d-%s-%s", sid, due ? due : "nodate", st ? st : "due");
            snprintf(title, sizeof(title), "Maintenance %s: %s", st, reg);
            snprintf(msg, sizeof(msg), "Service '%s' is %s (due date: %s)", svc, st, due ? due : "N/A");

            const char *sev = (st && strcmp(st, "Overdue") == 0) ? "Critical" : "Warning";
            if (notification_event_create(key, "Maintenance", sev, title, msg, "schedule", sid, "All") == STATUS_OK) {
                synced++;
            }
        }
        sqlite3_finalize(stmt);
    }

    /* 2. Sync Unresolved Inspection Defects */
    const char *insp_sql = 
        "SELECT it.id, v.registration_number, it.item_name, it.severity, it.notes "
        "FROM inspection_items it "
        "JOIN inspections i ON it.inspection_id = i.id "
        "JOIN vehicles v ON i.vehicle_id = v.id "
        "WHERE it.status = 'Fail' AND it.work_order_id IS NULL;";

    if (sqlite3_prepare_v2(db, insp_sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int item_id = sqlite3_column_int(stmt, 0);
            const char *reg = (const char *)sqlite3_column_text(stmt, 1);
            const char *item_name = (const char *)sqlite3_column_text(stmt, 2);
            const char *severity = (const char *)sqlite3_column_text(stmt, 3);
            const char *notes = (const char *)sqlite3_column_text(stmt, 4);

            char key[128], title[128], msg[256];
            snprintf(key, sizeof(key), "INSP-DEFECT-%d", item_id);
            snprintf(title, sizeof(title), "Inspection Defect: %s on %s", item_name, reg);
            snprintf(msg, sizeof(msg), "Severity %s: %s", severity, (notes && notes[0]) ? notes : "Repair work order needed");

            const char *sev = (severity && strstr(severity, "Critical")) ? "Critical" : "Warning";
            if (notification_event_create(key, "Defect", sev, title, msg, "inspection", item_id, "All") == STATUS_OK) {
                synced++;
            }
        }
        sqlite3_finalize(stmt);
    }

    /* 3. Sync Open or Waiting Work Orders */
    const char *wo_sql = 
        "SELECT wo.id, wo.wo_number, v.registration_number, wo.title, wo.priority, wo.status "
        "FROM work_orders wo "
        "JOIN vehicles v ON wo.vehicle_id = v.id "
        "WHERE wo.status IN ('Open', 'Waiting for Parts');";

    if (sqlite3_prepare_v2(db, wo_sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int woid = sqlite3_column_int(stmt, 0);
            const char *wo_num = (const char *)sqlite3_column_text(stmt, 1);
            const char *reg = (const char *)sqlite3_column_text(stmt, 2);
            const char *title = (const char *)sqlite3_column_text(stmt, 3);
            const char *prio = (const char *)sqlite3_column_text(stmt, 4);
            const char *st = (const char *)sqlite3_column_text(stmt, 5);

            char key[128], notif_title[128], msg[256];
            snprintf(key, sizeof(key), "WO-STATUS-%d-%s", woid, st ? st : "open");
            snprintf(notif_title, sizeof(notif_title), "Work Order %s (%s)", wo_num, st);
            snprintf(msg, sizeof(msg), "%s on %s [Priority: %s]", title, reg, prio ? prio : "Medium");

            const char *sev = (prio && strcmp(prio, "Critical") == 0) ? "Critical" : "Info";
            if (notification_event_create(key, "WorkOrder", sev, notif_title, msg, "work_order", woid, "All") == STATUS_OK) {
                synced++;
            }
        }
        sqlite3_finalize(stmt);
    }

    if (out_synced_count) *out_synced_count = synced;
    return STATUS_OK;
}

void notification_free_system_list(SystemNotification *list) {
    if (list) free(list);
}

