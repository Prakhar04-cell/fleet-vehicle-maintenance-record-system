#include "driver.h"
#include "database.h"
#include "audit.h"

StatusCode driver_create(const Driver *driver, int *out_id) {
    if (!driver || str_is_blank(driver->name) || str_is_blank(driver->license_number)) {
        return STATUS_INVALID_INPUT;
    }
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "INSERT INTO drivers (name, phone, license_number, license_expiry, active, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, datetime('now'), datetime('now'));";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_text(stmt, 1, driver->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, driver->phone, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, driver->license_number, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, driver->license_expiry, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, driver->active ? 1 : 0);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_CONSTRAINT) return STATUS_EXISTS;
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    int new_id = (int)db_last_insert_id();
    if (out_id) *out_id = new_id;

    char details[MAX_BUFFER_XL];
    snprintf(details, sizeof(details), "Added driver '%s' (License: %s)", driver->name, driver->license_number);
    audit_log_action(0, "DRIVER_CREATE", "drivers", new_id, details);

    return STATUS_OK;
}

StatusCode driver_get_by_id(int id, Driver *out_driver) {
    if (id <= 0 || !out_driver) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "SELECT id, name, phone, license_number, license_expiry, active, created_at, updated_at FROM drivers WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return STATUS_NOT_FOUND;
    }

    memset(out_driver, 0, sizeof(Driver));
    out_driver->id = sqlite3_column_int(stmt, 0);
    safe_strncpy(out_driver->name, (const char *)sqlite3_column_text(stmt, 1), sizeof(out_driver->name));
    safe_strncpy(out_driver->phone, (const char *)sqlite3_column_text(stmt, 2), sizeof(out_driver->phone));
    safe_strncpy(out_driver->license_number, (const char *)sqlite3_column_text(stmt, 3), sizeof(out_driver->license_number));
    safe_strncpy(out_driver->license_expiry, (const char *)sqlite3_column_text(stmt, 4), sizeof(out_driver->license_expiry));
    out_driver->active = (sqlite3_column_int(stmt, 5) != 0);
    safe_strncpy(out_driver->created_at, (const char *)sqlite3_column_text(stmt, 6), sizeof(out_driver->created_at));
    safe_strncpy(out_driver->updated_at, (const char *)sqlite3_column_text(stmt, 7), sizeof(out_driver->updated_at));

    sqlite3_finalize(stmt);
    return STATUS_OK;
}

StatusCode driver_get_by_license(const char *license_no, Driver *out_driver) {
    if (str_is_blank(license_no) || !out_driver) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "SELECT id, name, phone, license_number, license_expiry, active, created_at, updated_at FROM drivers WHERE license_number = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_text(stmt, 1, license_no, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return STATUS_NOT_FOUND;
    }

    memset(out_driver, 0, sizeof(Driver));
    out_driver->id = sqlite3_column_int(stmt, 0);
    safe_strncpy(out_driver->name, (const char *)sqlite3_column_text(stmt, 1), sizeof(out_driver->name));
    safe_strncpy(out_driver->phone, (const char *)sqlite3_column_text(stmt, 2), sizeof(out_driver->phone));
    safe_strncpy(out_driver->license_number, (const char *)sqlite3_column_text(stmt, 3), sizeof(out_driver->license_number));
    safe_strncpy(out_driver->license_expiry, (const char *)sqlite3_column_text(stmt, 4), sizeof(out_driver->license_expiry));
    out_driver->active = (sqlite3_column_int(stmt, 5) != 0);
    safe_strncpy(out_driver->created_at, (const char *)sqlite3_column_text(stmt, 6), sizeof(out_driver->created_at));
    safe_strncpy(out_driver->updated_at, (const char *)sqlite3_column_text(stmt, 7), sizeof(out_driver->updated_at));

    sqlite3_finalize(stmt);
    return STATUS_OK;
}

StatusCode driver_update(const Driver *driver) {
    if (!driver || driver->id <= 0 || str_is_blank(driver->name)) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "UPDATE drivers SET name = ?, phone = ?, license_number = ?, license_expiry = ?, active = ?, updated_at = datetime('now') "
        "WHERE id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_text(stmt, 1, driver->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, driver->phone, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, driver->license_number, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, driver->license_expiry, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, driver->active ? 1 : 0);
    sqlite3_bind_int(stmt, 6, driver->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_CONSTRAINT) return STATUS_EXISTS;
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    char details[MAX_BUFFER_XL];
    snprintf(details, sizeof(details), "Updated driver id %d ('%s')", driver->id, driver->name);
    audit_log_action(0, "DRIVER_UPDATE", "drivers", driver->id, details);

    return STATUS_OK;
}

StatusCode driver_set_active(int id, bool active) {
    if (id <= 0) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    if (!active) {
        /* If deactivating, unassign from any active vehicles */
        const char *unassign_sql = "UPDATE vehicles SET assigned_driver_id = NULL, updated_at = datetime('now') WHERE assigned_driver_id = ? AND archived_at IS NULL;";
        sqlite3_stmt *u_stmt = NULL;
        if (sqlite3_prepare_v2(db, unassign_sql, -1, &u_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(u_stmt, 1, id);
            sqlite3_step(u_stmt);
            sqlite3_finalize(u_stmt);
        }
    }

    const char *sql = "UPDATE drivers SET active = ?, updated_at = datetime('now') WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, active ? 1 : 0);
    sqlite3_bind_int(stmt, 2, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;
    return STATUS_OK;
}

StatusCode driver_search(const char *query, Driver **out_drivers, int *out_count) {
    if (!out_drivers || !out_count) return STATUS_INVALID_INPUT;
    *out_drivers = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    if (str_is_blank(query)) {
        return driver_list(out_drivers, out_count, false);
    }

    char pattern[MAX_BUFFER_MD];
    snprintf(pattern, sizeof(pattern), "%%%s%%", query);

    const char *sql = 
        "SELECT id, name, phone, license_number, license_expiry, active, created_at, updated_at "
        "FROM drivers "
        "WHERE name LIKE ? OR license_number LIKE ? OR phone LIKE ? "
        "ORDER BY name ASC;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, pattern, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, pattern, -1, SQLITE_STATIC);

    int capacity = 16;
    int count = 0;
    Driver *list = (Driver *)malloc(capacity * sizeof(Driver));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            Driver *tmp = (Driver *)realloc(list, capacity * sizeof(Driver));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }

        memset(&list[count], 0, sizeof(Driver));
        list[count].id = sqlite3_column_int(stmt, 0);
        safe_strncpy(list[count].name, (const char *)sqlite3_column_text(stmt, 1), sizeof(list[count].name));
        safe_strncpy(list[count].phone, (const char *)sqlite3_column_text(stmt, 2), sizeof(list[count].phone));
        safe_strncpy(list[count].license_number, (const char *)sqlite3_column_text(stmt, 3), sizeof(list[count].license_number));
        safe_strncpy(list[count].license_expiry, (const char *)sqlite3_column_text(stmt, 4), sizeof(list[count].license_expiry));
        list[count].active = (sqlite3_column_int(stmt, 5) != 0);
        safe_strncpy(list[count].created_at, (const char *)sqlite3_column_text(stmt, 6), sizeof(list[count].created_at));
        safe_strncpy(list[count].updated_at, (const char *)sqlite3_column_text(stmt, 7), sizeof(list[count].updated_at));
        count++;
    }

    sqlite3_finalize(stmt);
    *out_drivers = list;
    *out_count = count;
    return STATUS_OK;
}

StatusCode driver_delete(int driver_id, int user_id) {
    if (driver_id <= 0) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    int veh_id = 0;
    if (driver_is_assigned(driver_id, &veh_id)) {
        fprintf(stderr, "Error: Driver ID %d is assigned to active vehicle ID %d. Reassign or unassign before deleting.\n", driver_id, veh_id);
        return STATUS_FORBIDDEN;
    }

    Driver d;
    if (driver_get_by_id(driver_id, &d) != STATUS_OK) return STATUS_NOT_FOUND;

    const char *sql = "DELETE FROM drivers WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, driver_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    char details[MAX_BUFFER_LG];
    snprintf(details, sizeof(details), "Deleted driver '%s' (ID: %d)", d.name, driver_id);
    audit_log_action(user_id, "DRIVER_DELETE", "drivers", driver_id, details);

    return STATUS_OK;
}

StatusCode driver_list(Driver **out_drivers, int *out_count, bool active_only) {
    if (!out_drivers || !out_count) return STATUS_INVALID_INPUT;
    *out_drivers = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = active_only ?
        "SELECT id, name, phone, license_number, license_expiry, active, created_at, updated_at FROM drivers WHERE active = 1 ORDER BY name ASC;" :
        "SELECT id, name, phone, license_number, license_expiry, active, created_at, updated_at FROM drivers ORDER BY name ASC;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    int capacity = 16;
    int count = 0;
    Driver *list = (Driver *)malloc(capacity * sizeof(Driver));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            Driver *tmp = (Driver *)realloc(list, capacity * sizeof(Driver));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }

        memset(&list[count], 0, sizeof(Driver));
        list[count].id = sqlite3_column_int(stmt, 0);
        safe_strncpy(list[count].name, (const char *)sqlite3_column_text(stmt, 1), sizeof(list[count].name));
        safe_strncpy(list[count].phone, (const char *)sqlite3_column_text(stmt, 2), sizeof(list[count].phone));
        safe_strncpy(list[count].license_number, (const char *)sqlite3_column_text(stmt, 3), sizeof(list[count].license_number));
        safe_strncpy(list[count].license_expiry, (const char *)sqlite3_column_text(stmt, 4), sizeof(list[count].license_expiry));
        list[count].active = (sqlite3_column_int(stmt, 5) != 0);
        safe_strncpy(list[count].created_at, (const char *)sqlite3_column_text(stmt, 6), sizeof(list[count].created_at));
        safe_strncpy(list[count].updated_at, (const char *)sqlite3_column_text(stmt, 7), sizeof(list[count].updated_at));
        count++;
    }

    sqlite3_finalize(stmt);
    *out_drivers = list;
    *out_count = count;
    return STATUS_OK;
}

StatusCode driver_free_list(Driver *drivers) {
    if (drivers) free(drivers);
    return STATUS_OK;
}

bool driver_is_assigned(int driver_id, int *out_vehicle_id) {
    if (driver_id <= 0) return false;
    sqlite3 *db = db_get_handle();
    if (!db) return false;

    const char *sql = "SELECT id FROM vehicles WHERE assigned_driver_id = ? AND archived_at IS NULL AND status != 'Decommissioned' LIMIT 1;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, driver_id);

    bool assigned = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        assigned = true;
        if (out_vehicle_id) {
            *out_vehicle_id = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    return assigned;
}
