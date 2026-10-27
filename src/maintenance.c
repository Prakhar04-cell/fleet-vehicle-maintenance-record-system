#include "maintenance.h"
#include "database.h"
#include "vehicle.h"
#include "audit.h"
#include "expense.h"

static void fill_maintenance_from_stmt(sqlite3_stmt *stmt, MaintenanceRecord *r) {
    memset(r, 0, sizeof(MaintenanceRecord));
    r->id = sqlite3_column_int(stmt, 0);
    r->vehicle_id = sqlite3_column_int(stmt, 1);
    safe_strncpy(r->vehicle_reg, (const char *)sqlite3_column_text(stmt, 2), sizeof(r->vehicle_reg));
    safe_strncpy(r->service_type, (const char *)sqlite3_column_text(stmt, 3), sizeof(r->service_type));
    safe_strncpy(r->service_date, (const char *)sqlite3_column_text(stmt, 4), sizeof(r->service_date));
    r->odometer_km = sqlite3_column_int(stmt, 5);
    safe_strncpy(r->description, (const char *)sqlite3_column_text(stmt, 6), sizeof(r->description));
    safe_strncpy(r->workshop_name, (const char *)sqlite3_column_text(stmt, 7), sizeof(r->workshop_name));
    const char *mech = (const char *)sqlite3_column_text(stmt, 8);
    safe_strncpy(r->mechanic_name, mech ? mech : "", sizeof(r->mechanic_name));
    r->labour_cost = sqlite3_column_int64(stmt, 9);
    r->parts_cost = sqlite3_column_int64(stmt, 10);
    r->tax_amount = sqlite3_column_int64(stmt, 11);
    r->total_cost = sqlite3_column_int64(stmt, 12);
    const char *next_date = (const char *)sqlite3_column_text(stmt, 13);
    safe_strncpy(r->next_service_date, next_date ? next_date : "", sizeof(r->next_service_date));
    r->next_service_odometer_km = sqlite3_column_type(stmt, 14) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 14);
    const char *inv = (const char *)sqlite3_column_text(stmt, 15);
    safe_strncpy(r->invoice_doc_path, inv ? inv : "", sizeof(r->invoice_doc_path));
    r->created_by = sqlite3_column_int(stmt, 16);
    const char *creator = (const char *)sqlite3_column_text(stmt, 17);
    safe_strncpy(r->created_by_username, creator ? creator : "system", sizeof(r->created_by_username));
    safe_strncpy(r->created_at, (const char *)sqlite3_column_text(stmt, 18), sizeof(r->created_at));
    safe_strncpy(r->updated_at, (const char *)sqlite3_column_text(stmt, 19), sizeof(r->updated_at));
    const char *arch = (const char *)sqlite3_column_text(stmt, 20);
    if (arch && strlen(arch) > 0) {
        safe_strncpy(r->archived_at, arch, sizeof(r->archived_at));
        r->is_archived = true;
    }
}

StatusCode maintenance_create(MaintenanceRecord *record, bool create_linked_expense, int user_id) {
    if (!record || record->vehicle_id <= 0 || str_is_blank(record->service_type) ||
        str_is_blank(record->description) || str_is_blank(record->workshop_name)) {
        return STATUS_INVALID_INPUT;
    }
    if (!validate_iso_date(record->service_date)) {
        fprintf(stderr, "Error: Invalid service date. Must be YYYY-MM-DD.\n");
        return STATUS_INVALID_INPUT;
    }
    if (record->odometer_km < 0 || record->labour_cost < 0 || record->parts_cost < 0 || record->tax_amount < 0) {
        return STATUS_INVALID_INPUT;
    }

    /* Verify vehicle exists and is not archived */
    Vehicle v;
    StatusCode sc = vehicle_get_by_id(record->vehicle_id, &v);
    if (sc != STATUS_OK) {
        fprintf(stderr, "Error: Vehicle ID %d does not exist.\n", record->vehicle_id);
        return sc;
    }

    /* Calculate total consistently */
    record->total_cost = record->labour_cost + record->parts_cost + record->tax_amount;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    /* Use atomic transaction for multi-step write */
    if (db_begin_transaction() != STATUS_OK) return STATUS_DB_ERROR;

    const char *sql = 
        "INSERT INTO maintenance_records (vehicle_id, service_type, service_date, odometer_km, "
        "description, workshop_name, mechanic_name, labour_cost, parts_cost, tax_amount, total_cost, "
        "next_service_date, next_service_odometer_km, invoice_doc_path, created_by, created_at, updated_at, archived_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'), datetime('now'), NULL);";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_rollback_transaction();
        return STATUS_DB_ERROR;
    }

    sqlite3_bind_int(stmt, 1, record->vehicle_id);
    sqlite3_bind_text(stmt, 2, record->service_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, record->service_date, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, record->odometer_km);
    sqlite3_bind_text(stmt, 5, record->description, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, record->workshop_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, record->mechanic_name, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 8, record->labour_cost);
    sqlite3_bind_int64(stmt, 9, record->parts_cost);
    sqlite3_bind_int64(stmt, 10, record->tax_amount);
    sqlite3_bind_int64(stmt, 11, record->total_cost);

    if (!str_is_blank(record->next_service_date)) {
        sqlite3_bind_text(stmt, 12, record->next_service_date, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 12);
    }

    if (record->next_service_odometer_km > 0) {
        sqlite3_bind_int(stmt, 13, record->next_service_odometer_km);
    } else {
        sqlite3_bind_null(stmt, 13);
    }

    if (!str_is_blank(record->invoice_doc_path)) {
        sqlite3_bind_text(stmt, 14, record->invoice_doc_path, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 14);
    }

    if (user_id > 0) {
        sqlite3_bind_int(stmt, 15, user_id);
    } else {
        sqlite3_bind_null(stmt, 15);
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        db_rollback_transaction();
        return STATUS_DB_ERROR;
    }

    record->id = (int)db_last_insert_id();

    /* Auto-update vehicle odometer if the service odometer is higher */
    if (record->odometer_km > v.odometer_km) {
        sqlite3_stmt *upd_odom = NULL;
        const char *upd_sql = "UPDATE vehicles SET odometer_km = ?, updated_at = datetime('now') WHERE id = ?;";
        if (sqlite3_prepare_v2(db, upd_sql, -1, &upd_odom, NULL) == SQLITE_OK) {
            sqlite3_bind_int(upd_odom, 1, record->odometer_km);
            sqlite3_bind_int(upd_odom, 2, record->vehicle_id);
            sqlite3_step(upd_odom);
            sqlite3_finalize(upd_odom);
        }
    }

    /* Create corresponding expense record in the same transaction if requested */
    if (create_linked_expense && record->total_cost > 0) {
        const char *exp_sql = 
            "INSERT INTO expenses (vehicle_id, maintenance_record_id, category, description, amount, currency, expense_date, vendor, created_by, created_at) "
            "VALUES (?, ?, 'Maintenance', ?, ?, 'INR', ?, ?, ?, datetime('now'));";

        sqlite3_stmt *exp_stmt = NULL;
        if (sqlite3_prepare_v2(db, exp_sql, -1, &exp_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(exp_stmt, 1, record->vehicle_id);
            sqlite3_bind_int(exp_stmt, 2, record->id);
            sqlite3_bind_text(exp_stmt, 3, record->description, -1, SQLITE_STATIC);
            sqlite3_bind_int64(exp_stmt, 4, record->total_cost);
            sqlite3_bind_text(exp_stmt, 5, record->service_date, -1, SQLITE_STATIC);
            sqlite3_bind_text(exp_stmt, 6, record->workshop_name, -1, SQLITE_STATIC);
            if (user_id > 0) {
                sqlite3_bind_int(exp_stmt, 7, user_id);
            } else {
                sqlite3_bind_null(exp_stmt, 7);
            }
            rc = sqlite3_step(exp_stmt);
            sqlite3_finalize(exp_stmt);
            if (rc != SQLITE_DONE) {
                db_rollback_transaction();
                return STATUS_DB_ERROR;
            }
        }
    }

    if (db_commit_transaction() != STATUS_OK) {
        return STATUS_DB_ERROR;
    }

    char details[MAX_BUFFER_XL];
    snprintf(details, sizeof(details), "Recorded %s service for vehicle %s (ID %d) - Total: Rs. %lld.%02d",
             record->service_type, v.registration_number, record->vehicle_id,
             (long long)(record->total_cost / 100), (int)(record->total_cost % 100));
    audit_log_action(user_id, "MAINTENANCE_CREATE", "maintenance_records", record->id, details);

    return STATUS_OK;
}

StatusCode maintenance_get_by_id(int id, MaintenanceRecord *out_record) {
    if (id <= 0 || !out_record) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "SELECT m.id, m.vehicle_id, v.registration_number, m.service_type, m.service_date, "
        "       m.odometer_km, m.description, m.workshop_name, m.mechanic_name, m.labour_cost, "
        "       m.parts_cost, m.tax_amount, m.total_cost, m.next_service_date, m.next_service_odometer_km, "
        "       m.invoice_doc_path, m.created_by, u.username, m.created_at, m.updated_at, m.archived_at "
        "FROM maintenance_records m "
        "JOIN vehicles v ON m.vehicle_id = v.id "
        "LEFT JOIN users u ON m.created_by = u.id "
        "WHERE m.id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return STATUS_NOT_FOUND;
    }

    fill_maintenance_from_stmt(stmt, out_record);
    sqlite3_finalize(stmt);
    return STATUS_OK;
}

StatusCode maintenance_update(const MaintenanceRecord *record, int user_id) {
    if (!record || record->id <= 0) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    int64_t total = record->labour_cost + record->parts_cost + record->tax_amount;

    const char *sql = 
        "UPDATE maintenance_records SET service_type = ?, service_date = ?, odometer_km = ?, "
        "description = ?, workshop_name = ?, mechanic_name = ?, labour_cost = ?, parts_cost = ?, "
        "tax_amount = ?, total_cost = ?, next_service_date = ?, next_service_odometer_km = ?, "
        "invoice_doc_path = ?, updated_at = datetime('now') WHERE id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_text(stmt, 1, record->service_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, record->service_date, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, record->odometer_km);
    sqlite3_bind_text(stmt, 4, record->description, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, record->workshop_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, record->mechanic_name, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 7, record->labour_cost);
    sqlite3_bind_int64(stmt, 8, record->parts_cost);
    sqlite3_bind_int64(stmt, 9, record->tax_amount);
    sqlite3_bind_int64(stmt, 10, total);

    if (!str_is_blank(record->next_service_date)) {
        sqlite3_bind_text(stmt, 11, record->next_service_date, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 11);
    }

    if (record->next_service_odometer_km > 0) {
        sqlite3_bind_int(stmt, 12, record->next_service_odometer_km);
    } else {
        sqlite3_bind_null(stmt, 12);
    }

    sqlite3_bind_text(stmt, 13, record->invoice_doc_path, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 14, record->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    /* Update linked expense if one exists */
    const char *upd_exp = 
        "UPDATE expenses SET amount = ?, expense_date = ?, description = ?, vendor = ? "
        "WHERE maintenance_record_id = ?;";
    sqlite3_stmt *exp_stmt = NULL;
    if (sqlite3_prepare_v2(db, upd_exp, -1, &exp_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(exp_stmt, 1, total);
        sqlite3_bind_text(exp_stmt, 2, record->service_date, -1, SQLITE_STATIC);
        sqlite3_bind_text(exp_stmt, 3, record->description, -1, SQLITE_STATIC);
        sqlite3_bind_text(exp_stmt, 4, record->workshop_name, -1, SQLITE_STATIC);
        sqlite3_bind_int(exp_stmt, 5, record->id);
        sqlite3_step(exp_stmt);
        sqlite3_finalize(exp_stmt);
    }

    char details[MAX_BUFFER_MD];
    snprintf(details, sizeof(details), "Updated maintenance record id %d", record->id);
    audit_log_action(user_id, "MAINTENANCE_UPDATE", "maintenance_records", record->id, details);

    return STATUS_OK;
}

StatusCode maintenance_archive(int id, int user_id) {
    if (id <= 0) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "UPDATE maintenance_records SET archived_at = datetime('now'), updated_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    audit_log_action(user_id, "MAINTENANCE_ARCHIVE", "maintenance_records", id, "Archived maintenance record");
    return STATUS_OK;
}

StatusCode maintenance_list(const MaintenanceFilterOptions *filter, MaintenanceRecord **out_records, int *out_count) {
    if (!out_records || !out_count) return STATUS_INVALID_INPUT;
    *out_records = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    char query[MAX_BUFFER_XL] = 
        "SELECT m.id, m.vehicle_id, v.registration_number, m.service_type, m.service_date, "
        "       m.odometer_km, m.description, m.workshop_name, m.mechanic_name, m.labour_cost, "
        "       m.parts_cost, m.tax_amount, m.total_cost, m.next_service_date, m.next_service_odometer_km, "
        "       m.invoice_doc_path, m.created_by, u.username, m.created_at, m.updated_at, m.archived_at "
        "FROM maintenance_records m "
        "JOIN vehicles v ON m.vehicle_id = v.id "
        "LEFT JOIN users u ON m.created_by = u.id "
        "WHERE m.archived_at IS NULL ";

    if (filter && filter->vehicle_id > 0) {
        strcat(query, "AND m.vehicle_id = ? ");
    }
    if (filter && !str_is_blank(filter->service_type)) {
        strcat(query, "AND m.service_type = ? ");
    }
    if (filter && !str_is_blank(filter->date_from)) {
        strcat(query, "AND m.service_date >= ? ");
    }
    if (filter && !str_is_blank(filter->date_to)) {
        strcat(query, "AND m.service_date <= ? ");
    }

    strcat(query, "ORDER BY m.service_date DESC, m.id DESC;");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    int bind_idx = 1;
    if (filter && filter->vehicle_id > 0) {
        sqlite3_bind_int(stmt, bind_idx++, filter->vehicle_id);
    }
    if (filter && !str_is_blank(filter->service_type)) {
        sqlite3_bind_text(stmt, bind_idx++, filter->service_type, -1, SQLITE_STATIC);
    }
    if (filter && !str_is_blank(filter->date_from)) {
        sqlite3_bind_text(stmt, bind_idx++, filter->date_from, -1, SQLITE_STATIC);
    }
    if (filter && !str_is_blank(filter->date_to)) {
        sqlite3_bind_text(stmt, bind_idx++, filter->date_to, -1, SQLITE_STATIC);
    }

    int capacity = 16;
    int count = 0;
    MaintenanceRecord *list = (MaintenanceRecord *)malloc(capacity * sizeof(MaintenanceRecord));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            MaintenanceRecord *tmp = (MaintenanceRecord *)realloc(list, capacity * sizeof(MaintenanceRecord));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }
        fill_maintenance_from_stmt(stmt, &list[count]);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_records = list;
    *out_count = count;
    return STATUS_OK;
}

StatusCode maintenance_get_vehicle_history(int vehicle_id, MaintenanceRecord **out_records, int *out_count) {
    MaintenanceFilterOptions filter = {0};
    filter.vehicle_id = vehicle_id;
    return maintenance_list(&filter, out_records, out_count);
}

StatusCode maintenance_search(const char *query, MaintenanceRecord **out_records, int *out_count) {
    if (!out_records || !out_count) return STATUS_INVALID_INPUT;
    *out_records = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "SELECT m.id, m.vehicle_id, v.registration_number, m.service_type, m.service_date, "
        "       m.odometer_km, m.description, m.workshop_name, m.mechanic_name, m.labour_cost, "
        "       m.parts_cost, m.tax_amount, m.total_cost, m.next_service_date, m.next_service_odometer_km, "
        "       m.invoice_doc_path, m.created_by, u.username, m.created_at, m.updated_at, m.archived_at "
        "FROM maintenance_records m "
        "JOIN vehicles v ON m.vehicle_id = v.id "
        "LEFT JOIN users u ON m.created_by = u.id "
        "WHERE m.archived_at IS NULL AND "
        "      (v.registration_number LIKE ? OR m.service_type LIKE ? OR m.description LIKE ? OR m.workshop_name LIKE ?) "
        "ORDER BY m.service_date DESC, m.id DESC;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    char pattern[MAX_BUFFER_MD];
    snprintf(pattern, sizeof(pattern), "%%%s%%", query ? query : "");
    sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, pattern, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, pattern, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, pattern, -1, SQLITE_STATIC);

    int capacity = 16;
    int count = 0;
    MaintenanceRecord *list = (MaintenanceRecord *)malloc(capacity * sizeof(MaintenanceRecord));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            MaintenanceRecord *tmp = (MaintenanceRecord *)realloc(list, capacity * sizeof(MaintenanceRecord));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }
        fill_maintenance_from_stmt(stmt, &list[count]);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_records = list;
    *out_count = count;
    return STATUS_OK;
}

void maintenance_free_list(MaintenanceRecord *records) {
    if (records) free(records);
}
