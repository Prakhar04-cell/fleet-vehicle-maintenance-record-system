#include "work_order.h"
#include "database.h"
#include "vehicle.h"
#include "maintenance.h"
#include "schedule.h"
#include "audit.h"

static void fill_work_order_from_stmt(sqlite3_stmt *stmt, WorkOrder *wo) {
    memset(wo, 0, sizeof(WorkOrder));
    wo->id = sqlite3_column_int(stmt, 0);
    safe_strncpy(wo->wo_number, (const char *)sqlite3_column_text(stmt, 1), sizeof(wo->wo_number));
    wo->vehicle_id = sqlite3_column_int(stmt, 2);
    safe_strncpy(wo->vehicle_reg, (const char *)sqlite3_column_text(stmt, 3), sizeof(wo->vehicle_reg));
    wo->schedule_id = sqlite3_column_type(stmt, 4) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 4);
    wo->inspection_id = sqlite3_column_type(stmt, 5) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 5);
    safe_strncpy(wo->title, (const char *)sqlite3_column_text(stmt, 6), sizeof(wo->title));
    const char *desc = (const char *)sqlite3_column_text(stmt, 7);
    safe_strncpy(wo->description, desc ? desc : "", sizeof(wo->description));
    safe_strncpy(wo->priority, (const char *)sqlite3_column_text(stmt, 8), sizeof(wo->priority));
    safe_strncpy(wo->status, (const char *)sqlite3_column_text(stmt, 9), sizeof(wo->status));
    wo->assigned_to_user_id = sqlite3_column_type(stmt, 10) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 10);
    const char *aname = (const char *)sqlite3_column_text(stmt, 11);
    safe_strncpy(wo->assigned_to_name, aname ? aname : "", sizeof(wo->assigned_to_name));
    wo->odometer_km = sqlite3_column_type(stmt, 12) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 12);
    const char *st = (const char *)sqlite3_column_text(stmt, 13);
    safe_strncpy(wo->start_time, st ? st : "", sizeof(wo->start_time));
    const char *ct = (const char *)sqlite3_column_text(stmt, 14);
    safe_strncpy(wo->completion_time, ct ? ct : "", sizeof(wo->completion_time));
    wo->labour_cost = sqlite3_column_int64(stmt, 15);
    wo->parts_cost = sqlite3_column_int64(stmt, 16);
    wo->total_cost = sqlite3_column_int64(stmt, 17);
    wo->maintenance_record_id = sqlite3_column_type(stmt, 18) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 18);
    const char *notes = (const char *)sqlite3_column_text(stmt, 19);
    safe_strncpy(wo->notes, notes ? notes : "", sizeof(wo->notes));
    wo->created_by = sqlite3_column_type(stmt, 20) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 20);
    safe_strncpy(wo->created_at, (const char *)sqlite3_column_text(stmt, 21), sizeof(wo->created_at));
    safe_strncpy(wo->updated_at, (const char *)sqlite3_column_text(stmt, 22), sizeof(wo->updated_at));
}

StatusCode work_order_create(WorkOrder *wo, int user_id, const char *user_name) {
    if (!wo || wo->vehicle_id <= 0 || str_is_blank(wo->title)) {
        return STATUS_INVALID_INPUT;
    }

    Vehicle v;
    StatusCode sc = vehicle_get_by_id(wo->vehicle_id, &v);
    if (sc != STATUS_OK) return sc;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    if (str_is_blank(wo->priority)) {
        safe_strncpy(wo->priority, "Medium", sizeof(wo->priority));
    }
    if (str_is_blank(wo->status)) {
        safe_strncpy(wo->status, "Open", sizeof(wo->status));
    }
    if (str_is_blank(wo->wo_number)) {
        snprintf(wo->wo_number, sizeof(wo->wo_number), "WO-%ld-%d", (long)time(NULL), rand() % 900 + 100);
    }

    const char *sql = 
        "INSERT INTO work_orders (wo_number, vehicle_id, schedule_id, inspection_id, title, "
        "description, priority, status, assigned_to_user_id, assigned_to_name, odometer_km, "
        "labour_cost, parts_cost, total_cost, notes, created_by, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'), datetime('now'));";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_text(stmt, 1, wo->wo_number, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, wo->vehicle_id);

    if (wo->schedule_id > 0) sqlite3_bind_int(stmt, 3, wo->schedule_id);
    else sqlite3_bind_null(stmt, 3);

    if (wo->inspection_id > 0) sqlite3_bind_int(stmt, 4, wo->inspection_id);
    else sqlite3_bind_null(stmt, 4);

    sqlite3_bind_text(stmt, 5, wo->title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, wo->description, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, wo->priority, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, wo->status, -1, SQLITE_STATIC);

    if (wo->assigned_to_user_id > 0) {
        sqlite3_bind_int(stmt, 9, wo->assigned_to_user_id);
        sqlite3_bind_text(stmt, 10, wo->assigned_to_name, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 9);
        sqlite3_bind_null(stmt, 10);
    }

    if (wo->odometer_km > 0) sqlite3_bind_int(stmt, 11, wo->odometer_km);
    else sqlite3_bind_int(stmt, 11, v.odometer_km);

    sqlite3_bind_int64(stmt, 12, wo->labour_cost);
    sqlite3_bind_int64(stmt, 13, wo->parts_cost);
    sqlite3_bind_int64(stmt, 14, wo->labour_cost + wo->parts_cost);
    sqlite3_bind_text(stmt, 15, wo->notes, -1, SQLITE_STATIC);

    if (user_id > 0) sqlite3_bind_int(stmt, 16, user_id);
    else sqlite3_bind_null(stmt, 16);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    wo->id = (int)db_last_insert_id();

    /* Record initial history entry */
    const char *h_sql = 
        "INSERT INTO work_order_history (work_order_id, from_status, to_status, changed_by, changed_by_name, notes, timestamp) "
        "VALUES (?, 'None', ?, ?, ?, 'Work order created', datetime('now'));";
    sqlite3_stmt *h_stmt = NULL;
    if (sqlite3_prepare_v2(db, h_sql, -1, &h_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(h_stmt, 1, wo->id);
        sqlite3_bind_text(h_stmt, 2, wo->status, -1, SQLITE_STATIC);
        if (user_id > 0) sqlite3_bind_int(h_stmt, 3, user_id);
        else sqlite3_bind_null(h_stmt, 3);
        sqlite3_bind_text(h_stmt, 4, user_name ? user_name : "System", -1, SQLITE_STATIC);
        sqlite3_step(h_stmt);
        sqlite3_finalize(h_stmt);
    }

    char details[MAX_BUFFER_XL];
    snprintf(details, sizeof(details), "Created Work Order %s for vehicle %s", wo->wo_number, v.registration_number);
    audit_log_action(user_id, "WORK_ORDER_CREATE", "work_orders", wo->id, details);

    return STATUS_OK;
}

StatusCode work_order_get_by_id(int id, WorkOrder *out_wo) {
    if (id <= 0 || !out_wo) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "SELECT w.id, w.wo_number, w.vehicle_id, v.registration_number, w.schedule_id, w.inspection_id, "
        "       w.title, w.description, w.priority, w.status, w.assigned_to_user_id, w.assigned_to_name, "
        "       w.odometer_km, w.start_time, w.completion_time, w.labour_cost, w.parts_cost, w.total_cost, "
        "       w.maintenance_record_id, w.notes, w.created_by, w.created_at, w.updated_at "
        "FROM work_orders w "
        "JOIN vehicles v ON w.vehicle_id = v.id "
        "WHERE w.id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, id);

    StatusCode sc = STATUS_NOT_FOUND;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        fill_work_order_from_stmt(stmt, out_wo);
        sc = STATUS_OK;
    }
    sqlite3_finalize(stmt);
    return sc;
}

static bool is_valid_status_transition(const char *current, const char *target) {
    if (!current || !target) return false;
    if (strcmp(current, target) == 0) return true;

    if (strcmp(current, "Open") == 0) {
        return strcmp(target, "Assigned") == 0 ||
               strcmp(target, "In Progress") == 0 ||
               strcmp(target, "Cancelled") == 0;
    }
    if (strcmp(current, "Assigned") == 0) {
        return strcmp(target, "In Progress") == 0 ||
               strcmp(target, "Open") == 0 ||
               strcmp(target, "Cancelled") == 0;
    }
    if (strcmp(current, "In Progress") == 0) {
        return strcmp(target, "Waiting for Parts") == 0 ||
               strcmp(target, "Assigned") == 0 ||
               strcmp(target, "Completed") == 0 ||
               strcmp(target, "Cancelled") == 0;
    }
    if (strcmp(current, "Waiting for Parts") == 0) {
        return strcmp(target, "In Progress") == 0 ||
               strcmp(target, "Cancelled") == 0;
    }
    /* Completed and Cancelled are terminal states */
    return false;
}

StatusCode work_order_update_status(int id, const char *new_status, const char *notes, int user_id, const char *user_name) {
    if (id <= 0 || str_is_blank(new_status)) return STATUS_INVALID_INPUT;

    WorkOrder wo;
    StatusCode sc = work_order_get_by_id(id, &wo);
    if (sc != STATUS_OK) return sc;

    if (!is_valid_status_transition(wo.status, new_status)) {
        fprintf(stderr, "Error: Invalid work order status transition from '%s' to '%s'.\n", wo.status, new_status);
        return STATUS_INVALID_INPUT;
    }

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "UPDATE work_orders SET status = ?, updated_at = datetime('now'), "
        "start_time = CASE WHEN ? = 'In Progress' AND start_time IS NULL THEN datetime('now') ELSE start_time END, "
        "completion_time = CASE WHEN ? = 'Completed' AND completion_time IS NULL THEN datetime('now') ELSE completion_time END "
        "WHERE id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_text(stmt, 1, new_status, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, new_status, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, new_status, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    /* Record transition history */
    const char *h_sql = 
        "INSERT INTO work_order_history (work_order_id, from_status, to_status, changed_by, changed_by_name, notes, timestamp) "
        "VALUES (?, ?, ?, ?, ?, ?, datetime('now'));";
    sqlite3_stmt *h_stmt = NULL;
    if (sqlite3_prepare_v2(db, h_sql, -1, &h_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(h_stmt, 1, id);
        sqlite3_bind_text(h_stmt, 2, wo.status, -1, SQLITE_STATIC);
        sqlite3_bind_text(h_stmt, 3, new_status, -1, SQLITE_STATIC);
        if (user_id > 0) sqlite3_bind_int(h_stmt, 4, user_id);
        else sqlite3_bind_null(h_stmt, 4);
        sqlite3_bind_text(h_stmt, 5, user_name ? user_name : "User", -1, SQLITE_STATIC);
        sqlite3_bind_text(h_stmt, 6, notes ? notes : "", -1, SQLITE_STATIC);
        sqlite3_step(h_stmt);
        sqlite3_finalize(h_stmt);
    }

    char details[MAX_BUFFER_XL];
    snprintf(details, sizeof(details), "Work Order %s status changed %s -> %s", wo.wo_number, wo.status, new_status);
    audit_log_action(user_id, "WORK_ORDER_STATUS", "work_orders", id, details);

    return STATUS_OK;
}

StatusCode work_order_assign(int id, int technician_id, const char *technician_name, int user_id, const char *user_name) {
    if (id <= 0 || technician_id <= 0) return STATUS_INVALID_INPUT;

    WorkOrder wo;
    StatusCode sc = work_order_get_by_id(id, &wo);
    if (sc != STATUS_OK) return sc;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *target_status = (strcmp(wo.status, "Open") == 0) ? "Assigned" : wo.status;

    const char *sql = 
        "UPDATE work_orders SET assigned_to_user_id = ?, assigned_to_name = ?, status = ?, updated_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_int(stmt, 1, technician_id);
    sqlite3_bind_text(stmt, 2, technician_name ? technician_name : "Technician", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, target_status, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    /* Record transition history */
    char hnotes[MAX_BUFFER_MD];
    snprintf(hnotes, sizeof(hnotes), "Assigned to technician %s (ID %d)", technician_name ? technician_name : "Technician", technician_id);

    const char *h_sql = 
        "INSERT INTO work_order_history (work_order_id, from_status, to_status, changed_by, changed_by_name, notes, timestamp) "
        "VALUES (?, ?, ?, ?, ?, ?, datetime('now'));";
    sqlite3_stmt *h_stmt = NULL;
    if (sqlite3_prepare_v2(db, h_sql, -1, &h_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(h_stmt, 1, id);
        sqlite3_bind_text(h_stmt, 2, wo.status, -1, SQLITE_STATIC);
        sqlite3_bind_text(h_stmt, 3, target_status, -1, SQLITE_STATIC);
        if (user_id > 0) sqlite3_bind_int(h_stmt, 4, user_id);
        else sqlite3_bind_null(h_stmt, 4);
        sqlite3_bind_text(h_stmt, 5, user_name ? user_name : "Manager", -1, SQLITE_STATIC);
        sqlite3_bind_text(h_stmt, 6, hnotes, -1, SQLITE_STATIC);
        sqlite3_step(h_stmt);
        sqlite3_finalize(h_stmt);
    }

    audit_log_action(user_id, "WORK_ORDER_ASSIGN", "work_orders", id, hnotes);
    return STATUS_OK;
}

StatusCode work_order_complete(int id, int odometer, int64_t labour_cost, int64_t parts_cost, const char *notes, int user_id, const char *user_name, int *out_maint_id) {
    if (id <= 0) return STATUS_INVALID_INPUT;

    WorkOrder wo;
    StatusCode sc = work_order_get_by_id(id, &wo);
    if (sc != STATUS_OK) return sc;

    /* Prevent duplicate completion */
    if (strcmp(wo.status, "Completed") == 0) {
        fprintf(stderr, "Error: Work order %s is already completed. Duplicate completion blocked.\n", wo.wo_number);
        return STATUS_INVALID_INPUT;
    }

    Vehicle v;
    sc = vehicle_get_by_id(wo.vehicle_id, &v);
    if (sc != STATUS_OK) return sc;

    int current_odom = (odometer > 0) ? odometer : ((wo.odometer_km > 0) ? wo.odometer_km : v.odometer_km);
    int64_t l_cost = (labour_cost >= 0) ? labour_cost : wo.labour_cost;
    int64_t p_cost = (parts_cost >= 0) ? parts_cost : wo.parts_cost;
    int64_t tot_cost = l_cost + p_cost;

    /* 1. Create Maintenance Record if not already linked (handles its own transaction) */
    int maint_id = wo.maintenance_record_id;
    if (maint_id <= 0) {
        MaintenanceRecord mr = {0};
        mr.vehicle_id = wo.vehicle_id;
        safe_strncpy(mr.service_type, wo.title, sizeof(mr.service_type));
        get_current_iso_date(mr.service_date, sizeof(mr.service_date));
        mr.odometer_km = current_odom;
        safe_strncpy(mr.description, !str_is_blank(wo.description) ? wo.description : wo.title, sizeof(mr.description));
        safe_strncpy(mr.workshop_name, "Internal Fleet Workshop", sizeof(mr.workshop_name));
        safe_strncpy(mr.mechanic_name, !str_is_blank(wo.assigned_to_name) ? wo.assigned_to_name : user_name, sizeof(mr.mechanic_name));
        mr.labour_cost = l_cost;
        mr.parts_cost = p_cost;
        mr.tax_amount = 0;
        mr.total_cost = tot_cost;

        /* Create maintenance record with linked expense */
        sc = maintenance_create(&mr, user_id, true);
        if (sc != STATUS_OK) {
            return sc;
        }
        maint_id = mr.id;
    }

    if (db_begin_transaction() != STATUS_OK) return STATUS_DB_ERROR;

    /* 2. Update Work Order status to Completed and attach maintenance record ID */
    sqlite3 *db = db_get_handle();
    const char *upd_sql = 
        "UPDATE work_orders SET status = 'Completed', completion_time = datetime('now'), "
        "labour_cost = ?, parts_cost = ?, total_cost = ?, odometer_km = ?, "
        "maintenance_record_id = ?, notes = ?, updated_at = datetime('now') WHERE id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, upd_sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_rollback_transaction();
        return STATUS_DB_ERROR;
    }

    sqlite3_bind_int64(stmt, 1, l_cost);
    sqlite3_bind_int64(stmt, 2, p_cost);
    sqlite3_bind_int64(stmt, 3, tot_cost);
    sqlite3_bind_int(stmt, 4, current_odom);
    sqlite3_bind_int(stmt, 5, maint_id);
    sqlite3_bind_text(stmt, 6, notes ? notes : wo.notes, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        db_rollback_transaction();
        return STATUS_DB_ERROR;
    }

    /* 3. Insert history record */
    const char *h_sql = 
        "INSERT INTO work_order_history (work_order_id, from_status, to_status, changed_by, changed_by_name, notes, timestamp) "
        "VALUES (?, ?, 'Completed', ?, ?, ?, datetime('now'));";
    sqlite3_stmt *h_stmt = NULL;
    if (sqlite3_prepare_v2(db, h_sql, -1, &h_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(h_stmt, 1, id);
        sqlite3_bind_text(h_stmt, 2, wo.status, -1, SQLITE_STATIC);
        if (user_id > 0) sqlite3_bind_int(h_stmt, 3, user_id);
        else sqlite3_bind_null(h_stmt, 3);
        sqlite3_bind_text(h_stmt, 4, user_name ? user_name : "Staff", -1, SQLITE_STATIC);
        sqlite3_bind_text(h_stmt, 5, notes ? notes : "Work completed and closed", -1, SQLITE_STATIC);
        sqlite3_step(h_stmt);
        sqlite3_finalize(h_stmt);
    }

    /* 4. Complete linked schedule if any */
    if (wo.schedule_id > 0) {
        int next_sched = 0;
        schedule_complete(wo.schedule_id, maint_id, user_id, &next_sched);
    }

    if (db_commit_transaction() != STATUS_OK) {
        return STATUS_DB_ERROR;
    }

    /* Re-evaluate preventive schedules and notifications */
    schedule_evaluate_all(NULL, NULL);

    char details[MAX_BUFFER_XL];
    snprintf(details, sizeof(details), "Completed Work Order %s (Linked Maintenance ID: %d)", wo.wo_number, maint_id);
    audit_log_action(user_id, "WORK_ORDER_COMPLETE", "work_orders", id, details);

    if (out_maint_id) *out_maint_id = maint_id;
    return STATUS_OK;
}

StatusCode work_order_list(const WorkOrderFilter *filter, WorkOrder **out_list, int *out_count) {
    if (!out_list || !out_count) return STATUS_INVALID_INPUT;
    *out_list = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    char query[MAX_BUFFER_XL] = 
        "SELECT w.id, w.wo_number, w.vehicle_id, v.registration_number, w.schedule_id, w.inspection_id, "
        "       w.title, w.description, w.priority, w.status, w.assigned_to_user_id, w.assigned_to_name, "
        "       w.odometer_km, w.start_time, w.completion_time, w.labour_cost, w.parts_cost, w.total_cost, "
        "       w.maintenance_record_id, w.notes, w.created_by, w.created_at, w.updated_at "
        "FROM work_orders w "
        "JOIN vehicles v ON w.vehicle_id = v.id "
        "WHERE 1=1 ";

    if (filter && filter->vehicle_id > 0) {
        strcat(query, "AND w.vehicle_id = ? ");
    }
    if (filter && !str_is_blank(filter->status)) {
        strcat(query, "AND w.status = ? ");
    }
    if (filter && !str_is_blank(filter->priority)) {
        strcat(query, "AND w.priority = ? ");
    }
    if (filter && filter->assigned_to_user_id > 0) {
        strcat(query, "AND w.assigned_to_user_id = ? ");
    }

    strcat(query, "ORDER BY w.id DESC;");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    int bind_idx = 1;
    if (filter && filter->vehicle_id > 0) {
        sqlite3_bind_int(stmt, bind_idx++, filter->vehicle_id);
    }
    if (filter && !str_is_blank(filter->status)) {
        sqlite3_bind_text(stmt, bind_idx++, filter->status, -1, SQLITE_STATIC);
    }
    if (filter && !str_is_blank(filter->priority)) {
        sqlite3_bind_text(stmt, bind_idx++, filter->priority, -1, SQLITE_STATIC);
    }
    if (filter && filter->assigned_to_user_id > 0) {
        sqlite3_bind_int(stmt, bind_idx++, filter->assigned_to_user_id);
    }

    int capacity = 16;
    int count = 0;
    WorkOrder *list = (WorkOrder *)malloc(capacity * sizeof(WorkOrder));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            WorkOrder *tmp = (WorkOrder *)realloc(list, capacity * sizeof(WorkOrder));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }
        fill_work_order_from_stmt(stmt, &list[count]);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_list = list;
    *out_count = count;
    return STATUS_OK;
}

StatusCode work_order_get_history(int work_order_id, WorkOrderHistory **out_history, int *out_count) {
    if (work_order_id <= 0 || !out_history || !out_count) return STATUS_INVALID_INPUT;
    *out_history = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "SELECT id, work_order_id, from_status, to_status, changed_by, changed_by_name, notes, timestamp "
        "FROM work_order_history "
        "WHERE work_order_id = ? "
        "ORDER BY id ASC;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, work_order_id);

    int capacity = 8;
    int count = 0;
    WorkOrderHistory *history = (WorkOrderHistory *)malloc(capacity * sizeof(WorkOrderHistory));
    if (!history) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            WorkOrderHistory *tmp = (WorkOrderHistory *)realloc(history, capacity * sizeof(WorkOrderHistory));
            if (!tmp) {
                free(history);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            history = tmp;
        }
        memset(&history[count], 0, sizeof(WorkOrderHistory));
        history[count].id = sqlite3_column_int(stmt, 0);
        history[count].work_order_id = sqlite3_column_int(stmt, 1);
        safe_strncpy(history[count].from_status, (const char *)sqlite3_column_text(stmt, 2), sizeof(history[count].from_status));
        safe_strncpy(history[count].to_status, (const char *)sqlite3_column_text(stmt, 3), sizeof(history[count].to_status));
        history[count].changed_by = sqlite3_column_type(stmt, 4) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 4);
        const char *cname = (const char *)sqlite3_column_text(stmt, 5);
        safe_strncpy(history[count].changed_by_name, cname ? cname : "", sizeof(history[count].changed_by_name));
        const char *notes = (const char *)sqlite3_column_text(stmt, 6);
        safe_strncpy(history[count].notes, notes ? notes : "", sizeof(history[count].notes));
        safe_strncpy(history[count].timestamp, (const char *)sqlite3_column_text(stmt, 7), sizeof(history[count].timestamp));
        count++;
    }

    sqlite3_finalize(stmt);
    *out_history = history;
    *out_count = count;
    return STATUS_OK;
}

void work_order_free_list(WorkOrder *list) {
    if (list) free(list);
}

void work_order_free_history(WorkOrderHistory *history) {
    if (history) free(history);
}
