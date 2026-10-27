#include "dashboard.h"
#include "database.h"

StatusCode dashboard_get_stats(DashboardStats *out_stats) {
    if (!out_stats) return STATUS_INVALID_INPUT;
    memset(out_stats, 0, sizeof(DashboardStats));

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    /* Vehicle counts by status */
    const char *veh_sql = 
        "SELECT "
        "  COUNT(*) AS total_active, "
        "  COALESCE(SUM(CASE WHEN status = 'Available' THEN 1 ELSE 0 END), 0) AS avail, "
        "  COALESCE(SUM(CASE WHEN status = 'On Trip' THEN 1 ELSE 0 END), 0) AS on_trip, "
        "  COALESCE(SUM(CASE WHEN status = 'Under Maintenance' THEN 1 ELSE 0 END), 0) AS under_maint, "
        "  COALESCE(SUM(CASE WHEN status = 'Out of Service' THEN 1 ELSE 0 END), 0) AS out_of_svc, "
        "  COALESCE(SUM(CASE WHEN fuel_percentage < 20 THEN 1 ELSE 0 END), 0) AS low_fuel "
        "FROM vehicles WHERE archived_at IS NULL;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, veh_sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out_stats->total_active_vehicles = sqlite3_column_int(stmt, 0);
            out_stats->available_vehicles = sqlite3_column_int(stmt, 1);
            out_stats->on_trip_vehicles = sqlite3_column_int(stmt, 2);
            out_stats->under_maintenance_vehicles = sqlite3_column_int(stmt, 3);
            out_stats->out_of_service_vehicles = sqlite3_column_int(stmt, 4);
            out_stats->low_fuel_vehicles_count = sqlite3_column_int(stmt, 5);
        }
        sqlite3_finalize(stmt);
    }

    /* Schedules: upcoming and overdue */
    const char *sched_sql = 
        "SELECT "
        "  COALESCE(SUM(CASE WHEN status = 'Due' THEN 1 ELSE 0 END), 0) AS upcoming_cnt, "
        "  COALESCE(SUM(CASE WHEN status = 'Overdue' THEN 1 ELSE 0 END), 0) AS overdue_cnt "
        "FROM maintenance_schedules;";

    stmt = NULL;
    if (sqlite3_prepare_v2(db, sched_sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out_stats->upcoming_services_count = sqlite3_column_int(stmt, 0);
            out_stats->overdue_services_count = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    /* Spending */
    char ym[16];
    char today[16];
    get_current_iso_date(today, sizeof(today));
    safe_strncpy(ym, today, 8); /* "YYYY-MM" */

    const char *spend_sql = 
        "SELECT "
        "  COALESCE(SUM(CASE WHEN category = 'Maintenance' THEN amount ELSE 0 END), 0) AS total_maint, "
        "  COALESCE(SUM(CASE WHEN substr(expense_date, 1, 7) = ? THEN amount ELSE 0 END), 0) AS cur_month "
        "FROM expenses;";

    stmt = NULL;
    if (sqlite3_prepare_v2(db, spend_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ym, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out_stats->total_maintenance_spending = sqlite3_column_int64(stmt, 0);
            out_stats->current_month_spending = sqlite3_column_int64(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    /* Notifications */
    stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM notifications WHERE status = 'Active';", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out_stats->active_notifications_count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return STATUS_OK;
}

StatusCode dashboard_get_low_fuel_vehicles(Vehicle **out_vehicles, int *out_count) {
    if (!out_vehicles || !out_count) return STATUS_INVALID_INPUT;
    VehicleFilterOptions f = {0};
    f.include_archived = false;
    f.sort_field = VEHICLE_SORT_FUEL;
    f.sort_descending = false;

    Vehicle *all = NULL;
    int total = 0;
    StatusCode sc = vehicle_list(&f, &all, &total);
    if (sc != STATUS_OK) return sc;

    int low_cnt = 0;
    for (int i = 0; i < total; i++) {
        if (all[i].fuel_percentage < 20) low_cnt++;
    }

    if (low_cnt == 0) {
        vehicle_free_list(all);
        *out_vehicles = NULL;
        *out_count = 0;
        return STATUS_OK;
    }

    Vehicle *filtered = (Vehicle *)malloc(low_cnt * sizeof(Vehicle));
    int idx = 0;
    for (int i = 0; i < total; i++) {
        if (all[i].fuel_percentage < 20) {
            filtered[idx++] = all[i];
        }
    }
    vehicle_free_list(all);

    *out_vehicles = filtered;
    *out_count = low_cnt;
    return STATUS_OK;
}

StatusCode dashboard_get_recent_maintenance(int limit, MaintenanceRecord **out_records, int *out_count) {
    if (limit <= 0) limit = 5;
    MaintenanceFilterOptions f = {0};
    MaintenanceRecord *records = NULL;
    int count = 0;
    StatusCode sc = maintenance_list(&f, &records, &count);
    if (sc != STATUS_OK) return sc;

    if (count > limit) {
        count = limit;
    }
    *out_records = records;
    *out_count = count;
    return STATUS_OK;
}

StatusCode dashboard_get_urgent_schedules(int limit, MaintenanceSchedule **out_schedules, int *out_count) {
    if (limit <= 0) limit = 5;
    ScheduleFilterOptions f = {0};
    f.active_only = true;

    MaintenanceSchedule *list = NULL;
    int count = 0;
    StatusCode sc = schedule_list(&f, &list, &count);
    if (sc != STATUS_OK) return sc;

    if (count > limit) {
        count = limit;
    }
    *out_schedules = list;
    *out_count = count;
    return STATUS_OK;
}
