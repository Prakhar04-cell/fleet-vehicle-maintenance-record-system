#include "schedule.h"
#include "database.h"
#include "vehicle.h"
#include "notification.h"
#include "audit.h"

static void fill_schedule_from_stmt(sqlite3_stmt *stmt, MaintenanceSchedule *s) {
    memset(s, 0, sizeof(MaintenanceSchedule));
    s->id = sqlite3_column_int(stmt, 0);
    s->vehicle_id = sqlite3_column_int(stmt, 1);
    safe_strncpy(s->vehicle_reg, (const char *)sqlite3_column_text(stmt, 2), sizeof(s->vehicle_reg));
    safe_strncpy(s->service_name, (const char *)sqlite3_column_text(stmt, 3), sizeof(s->service_name));
    const char *desc = (const char *)sqlite3_column_text(stmt, 4);
    safe_strncpy(s->description, desc ? desc : "", sizeof(s->description));
    const char *due_d = (const char *)sqlite3_column_text(stmt, 5);
    safe_strncpy(s->due_date, due_d ? due_d : "", sizeof(s->due_date));
    s->due_odometer_km = sqlite3_column_type(stmt, 6) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 6);
    s->reminder_days_before = sqlite3_column_int(stmt, 7);
    safe_strncpy(s->status, (const char *)sqlite3_column_text(stmt, 8), sizeof(s->status));
    s->recurring_interval_days = sqlite3_column_int(stmt, 9);
    s->recurring_interval_km = sqlite3_column_int(stmt, 10);
    const char *notif = (const char *)sqlite3_column_text(stmt, 11);
    safe_strncpy(s->last_notified_at, notif ? notif : "", sizeof(s->last_notified_at));
    safe_strncpy(s->created_at, (const char *)sqlite3_column_text(stmt, 12), sizeof(s->created_at));
    safe_strncpy(s->updated_at, (const char *)sqlite3_column_text(stmt, 13), sizeof(s->updated_at));
    s->current_vehicle_odometer = sqlite3_column_int(stmt, 14);

    /* Compute remaining days & km */
    char today[16];
    get_current_iso_date(today, sizeof(today));
    if (!str_is_blank(s->due_date)) {
        s->days_remaining = days_between_dates(today, s->due_date);
    } else {
        s->days_remaining = 99999;
    }

    if (s->due_odometer_km > 0) {
        s->km_remaining = s->due_odometer_km - s->current_vehicle_odometer;
    } else {
        s->km_remaining = 999999;
    }
}

StatusCode schedule_create(MaintenanceSchedule *schedule, int user_id) {
    if (!schedule || schedule->vehicle_id <= 0 || str_is_blank(schedule->service_name)) {
        return STATUS_INVALID_INPUT;
    }
    if (str_is_blank(schedule->due_date) && schedule->due_odometer_km <= 0) {
        fprintf(stderr, "Error: Maintenance schedule requires at least a due date or a due odometer reading.\n");
        return STATUS_INVALID_INPUT;
    }
    if (!str_is_blank(schedule->due_date) && !validate_iso_date(schedule->due_date)) {
        fprintf(stderr, "Error: Invalid due date format. Must be YYYY-MM-DD.\n");
        return STATUS_INVALID_INPUT;
    }

    Vehicle v;
    StatusCode sc = vehicle_get_by_id(schedule->vehicle_id, &v);
    if (sc != STATUS_OK) return sc;

    if (schedule->reminder_days_before <= 0) {
        schedule->reminder_days_before = 7;
    }

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "INSERT INTO maintenance_schedules (vehicle_id, service_name, description, due_date, "
        "due_odometer_km, reminder_days_before, status, recurring_interval_days, recurring_interval_km, "
        "last_notified_at, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, 'Scheduled', ?, ?, NULL, datetime('now'), datetime('now'));";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_int(stmt, 1, schedule->vehicle_id);
    sqlite3_bind_text(stmt, 2, schedule->service_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, schedule->description, -1, SQLITE_STATIC);

    if (!str_is_blank(schedule->due_date)) {
        sqlite3_bind_text(stmt, 4, schedule->due_date, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 4);
    }

    if (schedule->due_odometer_km > 0) {
        sqlite3_bind_int(stmt, 5, schedule->due_odometer_km);
    } else {
        sqlite3_bind_null(stmt, 5);
    }

    sqlite3_bind_int(stmt, 6, schedule->reminder_days_before);
    sqlite3_bind_int(stmt, 7, schedule->recurring_interval_days);
    sqlite3_bind_int(stmt, 8, schedule->recurring_interval_km);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    schedule->id = (int)db_last_insert_id();

    char details[MAX_BUFFER_XL];
    snprintf(details, sizeof(details), "Scheduled '%s' for vehicle %s", schedule->service_name, v.registration_number);
    audit_log_action(user_id, "SCHEDULE_CREATE", "maintenance_schedules", schedule->id, details);

    return STATUS_OK;
}

StatusCode schedule_get_by_id(int id, MaintenanceSchedule *out_schedule) {
    if (id <= 0 || !out_schedule) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "SELECT s.id, s.vehicle_id, v.registration_number, s.service_name, s.description, "
        "       s.due_date, s.due_odometer_km, s.reminder_days_before, s.status, "
        "       s.recurring_interval_days, s.recurring_interval_km, s.last_notified_at, "
        "       s.created_at, s.updated_at, v.odometer_km "
        "FROM maintenance_schedules s "
        "JOIN vehicles v ON s.vehicle_id = v.id "
        "WHERE s.id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return STATUS_NOT_FOUND;
    }

    fill_schedule_from_stmt(stmt, out_schedule);
    sqlite3_finalize(stmt);
    return STATUS_OK;
}

StatusCode schedule_update(const MaintenanceSchedule *schedule, int user_id) {
    if (!schedule || schedule->id <= 0) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "UPDATE maintenance_schedules SET service_name = ?, description = ?, due_date = ?, "
        "due_odometer_km = ?, reminder_days_before = ?, recurring_interval_days = ?, "
        "recurring_interval_km = ?, updated_at = datetime('now') WHERE id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_text(stmt, 1, schedule->service_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, schedule->description, -1, SQLITE_STATIC);
    if (!str_is_blank(schedule->due_date)) {
        sqlite3_bind_text(stmt, 3, schedule->due_date, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    if (schedule->due_odometer_km > 0) {
        sqlite3_bind_int(stmt, 4, schedule->due_odometer_km);
    } else {
        sqlite3_bind_null(stmt, 4);
    }
    sqlite3_bind_int(stmt, 5, schedule->reminder_days_before);
    sqlite3_bind_int(stmt, 6, schedule->recurring_interval_days);
    sqlite3_bind_int(stmt, 7, schedule->recurring_interval_km);
    sqlite3_bind_int(stmt, 8, schedule->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    audit_log_action(user_id, "SCHEDULE_UPDATE", "maintenance_schedules", schedule->id, "Updated schedule details");
    return STATUS_OK;
}

StatusCode schedule_reschedule(int id, const char *new_due_date, int new_due_odometer, int user_id) {
    if (id <= 0) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "UPDATE maintenance_schedules SET due_date = ?, due_odometer_km = ?, status = 'Scheduled', "
        "updated_at = datetime('now') WHERE id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    if (!str_is_blank(new_due_date)) {
        sqlite3_bind_text(stmt, 1, new_due_date, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 1);
    }
    if (new_due_odometer > 0) {
        sqlite3_bind_int(stmt, 2, new_due_odometer);
    } else {
        sqlite3_bind_null(stmt, 2);
    }
    sqlite3_bind_int(stmt, 3, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    char details[MAX_BUFFER_MD];
    snprintf(details, sizeof(details), "Rescheduled to %s / %d km", new_due_date ? new_due_date : "none", new_due_odometer);
    audit_log_action(user_id, "SCHEDULE_RESCHEDULE", "maintenance_schedules", id, details);
    return STATUS_OK;
}

StatusCode schedule_complete(int id, int service_record_id, int user_id, int *out_next_schedule_id) {
    if (id <= 0) return STATUS_INVALID_INPUT;
    MaintenanceSchedule current;
    StatusCode sc = schedule_get_by_id(id, &current);
    if (sc != STATUS_OK) return sc;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "UPDATE maintenance_schedules SET status = 'Completed', updated_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    char details[MAX_BUFFER_MD];
    snprintf(details, sizeof(details), "Marked schedule completed (Service Record ID: %d)", service_record_id);
    audit_log_action(user_id, "SCHEDULE_COMPLETE", "maintenance_schedules", id, details);

    /* If recurring intervals exist, spawn the next recurring schedule */
    if (out_next_schedule_id) *out_next_schedule_id = 0;
    if (current.recurring_interval_days > 0 || current.recurring_interval_km > 0) {
        MaintenanceSchedule next_sched = current;
        next_sched.id = 0;

        char today[16];
        get_current_iso_date(today, sizeof(today));

        if (current.recurring_interval_days > 0) {
            add_days_to_date(today, current.recurring_interval_days, next_sched.due_date, sizeof(next_sched.due_date));
        }

        if (current.recurring_interval_km > 0) {
            next_sched.due_odometer_km = current.current_vehicle_odometer + current.recurring_interval_km;
        }

        if (schedule_create(&next_sched, user_id) == STATUS_OK) {
            if (out_next_schedule_id) *out_next_schedule_id = next_sched.id;
        }
    }

    return STATUS_OK;
}

StatusCode schedule_cancel(int id, const char *reason, int user_id) {
    if (id <= 0) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "UPDATE maintenance_schedules SET status = 'Cancelled', updated_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    char details[MAX_BUFFER_MD];
    snprintf(details, sizeof(details), "Cancelled schedule. Reason: %s", reason ? reason : "none");
    audit_log_action(user_id, "SCHEDULE_CANCEL", "maintenance_schedules", id, details);

    return STATUS_OK;
}

StatusCode schedule_list(const ScheduleFilterOptions *filter, MaintenanceSchedule **out_schedules, int *out_count) {
    if (!out_schedules || !out_count) return STATUS_INVALID_INPUT;
    *out_schedules = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    char query[MAX_BUFFER_XL] = 
        "SELECT s.id, s.vehicle_id, v.registration_number, s.service_name, s.description, "
        "       s.due_date, s.due_odometer_km, s.reminder_days_before, s.status, "
        "       s.recurring_interval_days, s.recurring_interval_km, s.last_notified_at, "
        "       s.created_at, s.updated_at, v.odometer_km "
        "FROM maintenance_schedules s "
        "JOIN vehicles v ON s.vehicle_id = v.id "
        "WHERE 1=1 ";

    if (filter && filter->vehicle_id > 0) {
        strcat(query, "AND s.vehicle_id = ? ");
    }
    if (filter && !str_is_blank(filter->status)) {
        strcat(query, "AND s.status = ? ");
    } else if (filter && filter->active_only) {
        strcat(query, "AND s.status IN ('Scheduled', 'Due', 'Overdue') ");
    }

    strcat(query, "ORDER BY s.due_date ASC, s.due_odometer_km ASC;");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    int bind_idx = 1;
    if (filter && filter->vehicle_id > 0) {
        sqlite3_bind_int(stmt, bind_idx++, filter->vehicle_id);
    }
    if (filter && !str_is_blank(filter->status)) {
        sqlite3_bind_text(stmt, bind_idx++, filter->status, -1, SQLITE_STATIC);
    }

    int capacity = 16;
    int count = 0;
    MaintenanceSchedule *list = (MaintenanceSchedule *)malloc(capacity * sizeof(MaintenanceSchedule));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            MaintenanceSchedule *tmp = (MaintenanceSchedule *)realloc(list, capacity * sizeof(MaintenanceSchedule));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }
        fill_schedule_from_stmt(stmt, &list[count]);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_schedules = list;
    *out_count = count;
    return STATUS_OK;
}

StatusCode schedule_evaluate_all(int *out_updated_count, int *out_notification_count) {
    ScheduleFilterOptions f = {0};
    f.active_only = true;

    MaintenanceSchedule *list = NULL;
    int count = 0;
    StatusCode sc = schedule_list(&f, &list, &count);
    if (sc != STATUS_OK) return sc;

    char today[16];
    get_current_iso_date(today, sizeof(today));

    int updated = 0;
    int notif_count = 0;

    sqlite3 *db = db_get_handle();

    for (int i = 0; i < count; i++) {
        MaintenanceSchedule *s = &list[i];
        bool is_overdue = false;
        bool is_due = false;
        const char *notif_type = NULL;
        const char *scheduled_at = s->due_date;

        /* Check Date threshold */
        if (!str_is_blank(s->due_date)) {
            int days = days_between_dates(today, s->due_date);
            if (days < 0) {
                is_overdue = true;
                notif_type = "Overdue Date";
            } else if (days <= s->reminder_days_before) {
                is_due = true;
                if (!notif_type) notif_type = "Upcoming Date";
            }
        }

        /* Check Odometer threshold */
        if (s->due_odometer_km > 0) {
            if (s->current_vehicle_odometer >= s->due_odometer_km) {
                is_overdue = true;
                notif_type = "Overdue Odometer";
            } else if ((s->due_odometer_km - s->current_vehicle_odometer) <= 500) {
                is_due = true;
                if (!is_overdue && !notif_type) notif_type = "Upcoming Odometer";
            }
        }

        const char *new_status = s->status;
        if (is_overdue) {
            new_status = "Overdue";
        } else if (is_due) {
            new_status = "Due";
        } else {
            new_status = "Scheduled";
        }

        if (strcmp(new_status, s->status) != 0) {
            const char *upd_sql = "UPDATE maintenance_schedules SET status = ?, updated_at = datetime('now') WHERE id = ?;";
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(db, upd_sql, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, new_status, -1, SQLITE_STATIC);
                sqlite3_bind_int(stmt, 2, s->id);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                updated++;
            }
        }

        if (notif_type) {
            if (str_is_blank(scheduled_at)) scheduled_at = today;
            if (notification_create(s->id, notif_type, scheduled_at, NULL) == STATUS_OK) {
                notif_count++;
            }
        }
    }

    schedule_free_list(list);

    if (out_updated_count) *out_updated_count = updated;
    if (out_notification_count) *out_notification_count = notif_count;

    return STATUS_OK;
}

void schedule_free_list(MaintenanceSchedule *schedules) {
    if (schedules) free(schedules);
}
