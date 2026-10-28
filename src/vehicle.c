#include "vehicle.h"
#include "database.h"
#include "audit.h"

StatusCode vehicle_create(const Vehicle *vehicle, int *out_id) {
    if (!vehicle || str_is_blank(vehicle->registration_number) ||
        str_is_blank(vehicle->make) || str_is_blank(vehicle->model) ||
        str_is_blank(vehicle->vin)) {
        return STATUS_INVALID_INPUT;
    }
    if (vehicle->manufacturing_year < 1900 || vehicle->manufacturing_year > 2100) {
        return STATUS_INVALID_INPUT;
    }
    if (vehicle->odometer_km < 0 || vehicle->fuel_percentage < 0 || vehicle->fuel_percentage > 100) {
        return STATUS_INVALID_INPUT;
    }

    /* If driver is assigned, verify driver exists, is active, and is not already assigned */
    if (vehicle->assigned_driver_id > 0) {
        int existing_veh_id = 0;
        if (driver_is_assigned(vehicle->assigned_driver_id, &existing_veh_id)) {
            fprintf(stderr, "Error: Driver is already assigned to active vehicle ID %d.\n", existing_veh_id);
            return STATUS_FORBIDDEN;
        }
    }

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "INSERT INTO vehicles (registration_number, make, model, manufacturing_year, vin, "
        "fuel_type, fuel_percentage, odometer_km, status, assigned_driver_id, insurance_expiry, "
        "notes, created_at, updated_at, archived_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'), datetime('now'), NULL);";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_text(stmt, 1, vehicle->registration_number, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, vehicle->make, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, vehicle->model, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, vehicle->manufacturing_year);
    sqlite3_bind_text(stmt, 5, vehicle->vin, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, str_is_blank(vehicle->fuel_type) ? "Diesel" : vehicle->fuel_type, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, vehicle->fuel_percentage);
    sqlite3_bind_int(stmt, 8, vehicle->odometer_km);
    sqlite3_bind_text(stmt, 9, str_is_blank(vehicle->status) ? "Available" : vehicle->status, -1, SQLITE_STATIC);

    if (vehicle->assigned_driver_id > 0) {
        sqlite3_bind_int(stmt, 10, vehicle->assigned_driver_id);
    } else {
        sqlite3_bind_null(stmt, 10);
    }

    if (!str_is_blank(vehicle->insurance_expiry)) {
        sqlite3_bind_text(stmt, 11, vehicle->insurance_expiry, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 11);
    }

    sqlite3_bind_text(stmt, 12, vehicle->notes, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_CONSTRAINT) return STATUS_EXISTS;
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    int new_id = (int)db_last_insert_id();
    if (out_id) *out_id = new_id;

    char details[MAX_BUFFER_XL];
    snprintf(details, sizeof(details), "Added vehicle %s (%s %s, VIN: %s)",
             vehicle->registration_number, vehicle->make, vehicle->model, vehicle->vin);
    audit_log_action(0, "VEHICLE_CREATE", "vehicles", new_id, details);

    return STATUS_OK;
}

static void fill_vehicle_from_stmt(sqlite3_stmt *stmt, Vehicle *v) {
    memset(v, 0, sizeof(Vehicle));
    v->id = sqlite3_column_int(stmt, 0);
    safe_strncpy(v->registration_number, (const char *)sqlite3_column_text(stmt, 1), sizeof(v->registration_number));
    safe_strncpy(v->make, (const char *)sqlite3_column_text(stmt, 2), sizeof(v->make));
    safe_strncpy(v->model, (const char *)sqlite3_column_text(stmt, 3), sizeof(v->model));
    v->manufacturing_year = sqlite3_column_int(stmt, 4);
    safe_strncpy(v->vin, (const char *)sqlite3_column_text(stmt, 5), sizeof(v->vin));
    safe_strncpy(v->fuel_type, (const char *)sqlite3_column_text(stmt, 6), sizeof(v->fuel_type));
    v->fuel_percentage = sqlite3_column_int(stmt, 7);
    v->odometer_km = sqlite3_column_int(stmt, 8);
    safe_strncpy(v->status, (const char *)sqlite3_column_text(stmt, 9), sizeof(v->status));
    v->assigned_driver_id = sqlite3_column_type(stmt, 10) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 10);
    const char *driver_name = (const char *)sqlite3_column_text(stmt, 11);
    safe_strncpy(v->assigned_driver_name, driver_name ? driver_name : "Unassigned", sizeof(v->assigned_driver_name));
    const char *ins = (const char *)sqlite3_column_text(stmt, 12);
    safe_strncpy(v->insurance_expiry, ins ? ins : "", sizeof(v->insurance_expiry));
    const char *notes = (const char *)sqlite3_column_text(stmt, 13);
    safe_strncpy(v->notes, notes ? notes : "", sizeof(v->notes));
    safe_strncpy(v->created_at, (const char *)sqlite3_column_text(stmt, 14), sizeof(v->created_at));
    safe_strncpy(v->updated_at, (const char *)sqlite3_column_text(stmt, 15), sizeof(v->updated_at));
    const char *arch = (const char *)sqlite3_column_text(stmt, 16);
    if (arch && strlen(arch) > 0) {
        safe_strncpy(v->archived_at, arch, sizeof(v->archived_at));
        v->is_archived = true;
    } else {
        v->archived_at[0] = '\0';
        v->is_archived = false;
    }
}

StatusCode vehicle_get_by_id(int id, Vehicle *out_vehicle) {
    if (id <= 0 || !out_vehicle) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "SELECT v.id, v.registration_number, v.make, v.model, v.manufacturing_year, v.vin, "
        "       v.fuel_type, v.fuel_percentage, v.odometer_km, v.status, v.assigned_driver_id, "
        "       d.name, v.insurance_expiry, v.notes, v.created_at, v.updated_at, v.archived_at "
        "FROM vehicles v "
        "LEFT JOIN drivers d ON v.assigned_driver_id = d.id "
        "WHERE v.id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return STATUS_NOT_FOUND;
    }

    fill_vehicle_from_stmt(stmt, out_vehicle);
    sqlite3_finalize(stmt);
    return STATUS_OK;
}

StatusCode vehicle_get_by_reg(const char *reg_number, Vehicle *out_vehicle) {
    if (str_is_blank(reg_number) || !out_vehicle) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "SELECT v.id, v.registration_number, v.make, v.model, v.manufacturing_year, v.vin, "
        "       v.fuel_type, v.fuel_percentage, v.odometer_km, v.status, v.assigned_driver_id, "
        "       d.name, v.insurance_expiry, v.notes, v.created_at, v.updated_at, v.archived_at "
        "FROM vehicles v "
        "LEFT JOIN drivers d ON v.assigned_driver_id = d.id "
        "WHERE v.registration_number = ? COLLATE NOCASE;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_text(stmt, 1, reg_number, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return STATUS_NOT_FOUND;
    }

    fill_vehicle_from_stmt(stmt, out_vehicle);
    sqlite3_finalize(stmt);
    return STATUS_OK;
}

StatusCode vehicle_update(const Vehicle *vehicle) {
    if (!vehicle || vehicle->id <= 0) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "UPDATE vehicles SET registration_number = ?, make = ?, model = ?, manufacturing_year = ?, "
        "vin = ?, fuel_type = ?, fuel_percentage = ?, status = ?, insurance_expiry = ?, notes = ?, "
        "updated_at = datetime('now') WHERE id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_text(stmt, 1, vehicle->registration_number, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, vehicle->make, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, vehicle->model, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, vehicle->manufacturing_year);
    sqlite3_bind_text(stmt, 5, vehicle->vin, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, vehicle->fuel_type, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, vehicle->fuel_percentage);
    sqlite3_bind_text(stmt, 8, vehicle->status, -1, SQLITE_STATIC);
    if (!str_is_blank(vehicle->insurance_expiry)) {
        sqlite3_bind_text(stmt, 9, vehicle->insurance_expiry, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 9);
    }
    sqlite3_bind_text(stmt, 10, vehicle->notes, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 11, vehicle->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_CONSTRAINT) return STATUS_EXISTS;
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    char details[MAX_BUFFER_MD];
    snprintf(details, sizeof(details), "Updated vehicle id %d (%s)", vehicle->id, vehicle->registration_number);
    audit_log_action(0, "VEHICLE_UPDATE", "vehicles", vehicle->id, details);

    return STATUS_OK;
}

StatusCode vehicle_update_odometer(int vehicle_id, int new_odometer, const char *correction_reason, int user_id) {
    if (vehicle_id <= 0 || new_odometer < 0) return STATUS_INVALID_INPUT;
    Vehicle current;
    StatusCode sc = vehicle_get_by_id(vehicle_id, &current);
    if (sc != STATUS_OK) return sc;

    if (new_odometer < current.odometer_km) {
        if (str_is_blank(correction_reason)) {
            fprintf(stderr, "Error: Decreasing odometer reading requires an explicit authorized reason.\n");
            return STATUS_INVALID_INPUT;
        }
    }

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "UPDATE vehicles SET odometer_km = ?, updated_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, new_odometer);
    sqlite3_bind_int(stmt, 2, vehicle_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    char details[MAX_BUFFER_MD];
    if (new_odometer < current.odometer_km) {
        snprintf(details, sizeof(details), "Odometer rollback from %d km to %d km. Reason: %s",
                 current.odometer_km, new_odometer, correction_reason);
        audit_log_action(user_id, "ODOMETER_CORRECTION", "vehicles", vehicle_id, details);
    } else {
        snprintf(details, sizeof(details), "Odometer updated from %d km to %d km", current.odometer_km, new_odometer);
        audit_log_action(user_id, "ODOMETER_UPDATE", "vehicles", vehicle_id, details);
    }

    return STATUS_OK;
}

StatusCode vehicle_update_fuel(int vehicle_id, int fuel_percentage, int user_id) {
    if (vehicle_id <= 0 || fuel_percentage < 0 || fuel_percentage > 100) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "UPDATE vehicles SET fuel_percentage = ?, updated_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, fuel_percentage);
    sqlite3_bind_int(stmt, 2, vehicle_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    char details[MAX_BUFFER_SM];
    snprintf(details, sizeof(details), "Fuel level updated to %d%%", fuel_percentage);
    audit_log_action(user_id, "FUEL_UPDATE", "vehicles", vehicle_id, details);

    return STATUS_OK;
}

StatusCode vehicle_update_status(int vehicle_id, const char *status, int user_id) {
    if (vehicle_id <= 0 || str_is_blank(status)) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "UPDATE vehicles SET status = ?, updated_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_text(stmt, 1, status, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, vehicle_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    char details[MAX_BUFFER_SM];
    snprintf(details, sizeof(details), "Status changed to '%s'", status);
    audit_log_action(user_id, "VEHICLE_STATUS", "vehicles", vehicle_id, details);

    return STATUS_OK;
}

StatusCode vehicle_assign_driver(int vehicle_id, int driver_id, int user_id) {
    if (vehicle_id <= 0 || driver_id <= 0) return STATUS_INVALID_INPUT;

    /* Verify driver is not already assigned to another active vehicle */
    int existing_veh_id = 0;
    if (driver_is_assigned(driver_id, &existing_veh_id) && existing_veh_id != vehicle_id) {
        fprintf(stderr, "Error: Driver %d is already assigned to active vehicle %d.\n", driver_id, existing_veh_id);
        return STATUS_FORBIDDEN;
    }

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "UPDATE vehicles SET assigned_driver_id = ?, updated_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, driver_id);
    sqlite3_bind_int(stmt, 2, vehicle_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    char details[MAX_BUFFER_SM];
    snprintf(details, sizeof(details), "Assigned driver_id %d", driver_id);
    audit_log_action(user_id, "DRIVER_ASSIGN", "vehicles", vehicle_id, details);

    return STATUS_OK;
}

StatusCode vehicle_unassign_driver(int vehicle_id, int user_id) {
    if (vehicle_id <= 0) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "UPDATE vehicles SET assigned_driver_id = NULL, updated_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, vehicle_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    audit_log_action(user_id, "DRIVER_UNASSIGN", "vehicles", vehicle_id, "Unassigned driver");
    return STATUS_OK;
}

StatusCode vehicle_archive(int vehicle_id, int user_id) {
    if (vehicle_id <= 0) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "UPDATE vehicles SET archived_at = datetime('now'), status = 'Decommissioned', updated_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, vehicle_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    audit_log_action(user_id, "VEHICLE_ARCHIVE", "vehicles", vehicle_id, "Archived and decommissioned vehicle");
    return STATUS_OK;
}

StatusCode vehicle_restore(int vehicle_id, int user_id) {
    if (vehicle_id <= 0) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "UPDATE vehicles SET archived_at = NULL, status = 'Available', updated_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, vehicle_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    audit_log_action(user_id, "VEHICLE_RESTORE", "vehicles", vehicle_id, "Restored archived vehicle");
    return STATUS_OK;
}

StatusCode vehicle_delete(int vehicle_id, int user_id) {
    if (vehicle_id <= 0) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    Vehicle v;
    StatusCode sc = vehicle_get_by_id(vehicle_id, &v);
    if (sc != STATUS_OK) return sc;

    /* Check if vehicle has dependent maintenance records or expenses */
    const char *check_sql = 
        "SELECT "
        "  (SELECT COUNT(*) FROM maintenance_records WHERE vehicle_id = ?) + "
        "  (SELECT COUNT(*) FROM expenses WHERE vehicle_id = ?) + "
        "  (SELECT COUNT(*) FROM maintenance_schedules WHERE vehicle_id = ?);";

    sqlite3_stmt *stmt = NULL;
    int related_count = 0;
    if (sqlite3_prepare_v2(db, check_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, vehicle_id);
        sqlite3_bind_int(stmt, 2, vehicle_id);
        sqlite3_bind_int(stmt, 3, vehicle_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            related_count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (related_count > 0) {
        fprintf(stderr, "Error: Cannot delete vehicle %s: %d dependent maintenance/expense records exist. Archive the vehicle instead.\n",
                v.registration_number, related_count);
        return STATUS_FORBIDDEN;
    }

    const char *del_sql = "DELETE FROM vehicles WHERE id = ?;";
    if (sqlite3_prepare_v2(db, del_sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, vehicle_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_CONSTRAINT) {
        return STATUS_FORBIDDEN;
    }
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    char details[MAX_BUFFER_MD];
    snprintf(details, sizeof(details), "Permanently deleted vehicle %s (ID: %d)", v.registration_number, vehicle_id);
    audit_log_action(user_id, "VEHICLE_DELETE", "vehicles", vehicle_id, details);

    return STATUS_OK;
}

StatusCode vehicle_list(const VehicleFilterOptions *options, Vehicle **out_vehicles, int *out_count) {
    if (!out_vehicles || !out_count) return STATUS_INVALID_INPUT;
    *out_vehicles = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    char query[MAX_BUFFER_XL] = 
        "SELECT v.id, v.registration_number, v.make, v.model, v.manufacturing_year, v.vin, "
        "       v.fuel_type, v.fuel_percentage, v.odometer_km, v.status, v.assigned_driver_id, "
        "       d.name, v.insurance_expiry, v.notes, v.created_at, v.updated_at, v.archived_at "
        "FROM vehicles v "
        "LEFT JOIN drivers d ON v.assigned_driver_id = d.id "
        "WHERE 1=1 ";

    if (!options || !options->include_archived) {
        strcat(query, "AND v.archived_at IS NULL ");
    }
    if (options && !str_is_blank(options->status_filter)) {
        strcat(query, "AND v.status = ? ");
    }
    if (options && options->available_only) {
        strcat(query, "AND v.status = 'Available' ");
    }
    if (options && options->maintenance_only) {
        strcat(query, "AND v.status = 'Under Maintenance' ");
    }

    /* Ordering */
    strcat(query, "ORDER BY ");
    VehicleSortField sort = options ? options->sort_field : VEHICLE_SORT_REG;
    switch (sort) {
        case VEHICLE_SORT_MAKE:
            strcat(query, "v.make, v.model ");
            break;
        case VEHICLE_SORT_FUEL:
            strcat(query, "v.fuel_percentage ");
            break;
        case VEHICLE_SORT_ODOMETER:
            strcat(query, "v.odometer_km ");
            break;
        case VEHICLE_SORT_STATUS:
            strcat(query, "v.status ");
            break;
        case VEHICLE_SORT_REG:
        default:
            strcat(query, "v.registration_number ");
            break;
    }

    if (options && options->sort_descending) {
        strcat(query, "DESC;");
    } else {
        strcat(query, "ASC;");
    }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    if (options && !str_is_blank(options->status_filter)) {
        sqlite3_bind_text(stmt, 1, options->status_filter, -1, SQLITE_STATIC);
    }

    int capacity = 16;
    int count = 0;
    Vehicle *list = (Vehicle *)malloc(capacity * sizeof(Vehicle));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            Vehicle *tmp = (Vehicle *)realloc(list, capacity * sizeof(Vehicle));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }
        fill_vehicle_from_stmt(stmt, &list[count]);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_vehicles = list;
    *out_count = count;
    return STATUS_OK;
}

StatusCode vehicle_search(const char *query, Vehicle **out_vehicles, int *out_count) {
    if (!out_vehicles || !out_count) return STATUS_INVALID_INPUT;
    *out_vehicles = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "SELECT v.id, v.registration_number, v.make, v.model, v.manufacturing_year, v.vin, "
        "       v.fuel_type, v.fuel_percentage, v.odometer_km, v.status, v.assigned_driver_id, "
        "       d.name, v.insurance_expiry, v.notes, v.created_at, v.updated_at, v.archived_at "
        "FROM vehicles v "
        "LEFT JOIN drivers d ON v.assigned_driver_id = d.id "
        "WHERE (v.registration_number LIKE ? OR v.make LIKE ? OR v.model LIKE ? OR v.vin LIKE ?) "
        "ORDER BY v.registration_number ASC;";

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
    Vehicle *list = (Vehicle *)malloc(capacity * sizeof(Vehicle));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            Vehicle *tmp = (Vehicle *)realloc(list, capacity * sizeof(Vehicle));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }
        fill_vehicle_from_stmt(stmt, &list[count]);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_vehicles = list;
    *out_count = count;
    return STATUS_OK;
}

void vehicle_free_list(Vehicle *vehicles) {
    if (vehicles) free(vehicles);
}
