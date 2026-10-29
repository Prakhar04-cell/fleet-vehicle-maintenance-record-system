#include "inspection.h"
#include "database.h"
#include "vehicle.h"
#include "work_order.h"
#include "audit.h"

StatusCode inspection_create(Inspection *insp, const InspectionItem *items, int item_count, int user_id) {
    if (!insp || insp->vehicle_id <= 0 || str_is_blank(insp->inspector_name)) {
        return STATUS_INVALID_INPUT;
    }

    Vehicle v;
    StatusCode sc = vehicle_get_by_id(insp->vehicle_id, &v);
    if (sc != STATUS_OK) return sc;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    if (str_is_blank(insp->inspection_date)) {
        get_current_iso_date(insp->inspection_date, sizeof(insp->inspection_date));
    }
    if (insp->odometer_km <= 0) {
        insp->odometer_km = v.odometer_km;
    }
    if (str_is_blank(insp->inspection_number)) {
        snprintf(insp->inspection_number, sizeof(insp->inspection_number), "INS-%ld-%d", (long)time(NULL), rand() % 900 + 100);
    }

    /* Compute overall status and defect count */
    int defects = 0;
    bool has_critical = false;
    for (int i = 0; i < item_count; i++) {
        char norm_status[16] = "Pass";
        if (strcmp(items[i].status, "Fail") == 0) {
            safe_strncpy(norm_status, "Fail", sizeof(norm_status));
        } else if (strcmp(items[i].status, "N/A") == 0 || strcmp(items[i].status, "NA") == 0 ||
                   strcmp(items[i].status, "Not Applicable") == 0) {
            safe_strncpy(norm_status, "N/A", sizeof(norm_status));
        }

        char norm_sev[32] = "Normal";
        if (strstr(items[i].severity, "Critical") != NULL) {
            safe_strncpy(norm_sev, "Critical Defect", sizeof(norm_sev));
        } else if (strstr(items[i].severity, "Minor") != NULL) {
            safe_strncpy(norm_sev, "Minor Defect", sizeof(norm_sev));
        }

        if (strcmp(norm_status, "Fail") == 0) {
            defects++;
            if (strcmp(norm_sev, "Critical Defect") == 0) {
                has_critical = true;
            }
        }
    }
    insp->defect_count = defects;
    if (has_critical) {
        safe_strncpy(insp->overall_status, "Fail", sizeof(insp->overall_status));
    } else if (defects > 0) {
        safe_strncpy(insp->overall_status, "Defective", sizeof(insp->overall_status));
    } else {
        safe_strncpy(insp->overall_status, "Pass", sizeof(insp->overall_status));
    }

    if (db_begin_transaction() != STATUS_OK) return STATUS_DB_ERROR;

    const char *sql = 
        "INSERT INTO inspections (inspection_number, vehicle_id, inspector_id, inspector_name, "
        "inspection_date, odometer_km, overall_status, defect_count, notes, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'));";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_rollback_transaction();
        return STATUS_DB_ERROR;
    }

    sqlite3_bind_text(stmt, 1, insp->inspection_number, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, insp->vehicle_id);

    if (insp->inspector_id > 0) sqlite3_bind_int(stmt, 3, insp->inspector_id);
    else if (user_id > 0) sqlite3_bind_int(stmt, 3, user_id);
    else sqlite3_bind_null(stmt, 3);

    sqlite3_bind_text(stmt, 4, insp->inspector_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, insp->inspection_date, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, insp->odometer_km);
    sqlite3_bind_text(stmt, 7, insp->overall_status, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 8, insp->defect_count);
    sqlite3_bind_text(stmt, 9, insp->notes, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        db_rollback_transaction();
        return STATUS_DB_ERROR;
    }

    insp->id = (int)db_last_insert_id();

    /* Insert checklist items */
    const char *item_sql = 
        "INSERT INTO inspection_items (inspection_id, item_name, status, severity, notes, work_order_id) "
        "VALUES (?, ?, ?, ?, ?, NULL);";

    for (int i = 0; i < item_count; i++) {
        char norm_status[16] = "Pass";
        if (strcmp(items[i].status, "Fail") == 0) {
            safe_strncpy(norm_status, "Fail", sizeof(norm_status));
        } else if (strcmp(items[i].status, "N/A") == 0 || strcmp(items[i].status, "NA") == 0 ||
                   strcmp(items[i].status, "Not Applicable") == 0) {
            safe_strncpy(norm_status, "N/A", sizeof(norm_status));
        }

        char norm_sev[32] = "Normal";
        if (strstr(items[i].severity, "Critical") != NULL) {
            safe_strncpy(norm_sev, "Critical Defect", sizeof(norm_sev));
        } else if (strstr(items[i].severity, "Minor") != NULL) {
            safe_strncpy(norm_sev, "Minor Defect", sizeof(norm_sev));
        }

        sqlite3_stmt *i_stmt = NULL;
        if (sqlite3_prepare_v2(db, item_sql, -1, &i_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(i_stmt, 1, insp->id);
            sqlite3_bind_text(i_stmt, 2, items[i].item_name, -1, SQLITE_STATIC);
            sqlite3_bind_text(i_stmt, 3, norm_status, -1, SQLITE_STATIC);
            sqlite3_bind_text(i_stmt, 4, norm_sev, -1, SQLITE_STATIC);
            sqlite3_bind_text(i_stmt, 5, items[i].notes, -1, SQLITE_STATIC);
            sqlite3_step(i_stmt);
            sqlite3_finalize(i_stmt);
        }
    }

    if (db_commit_transaction() != STATUS_OK) {
        return STATUS_DB_ERROR;
    }

    char details[MAX_BUFFER_XL];
    snprintf(details, sizeof(details), "Submitted Inspection %s for %s: Status %s (%d defects)",
             insp->inspection_number, v.registration_number, insp->overall_status, insp->defect_count);
    audit_log_action(user_id, "INSPECTION_CREATE", "inspections", insp->id, details);

    return STATUS_OK;
}

StatusCode inspection_get_by_id(int id, Inspection *out_insp, InspectionItem **out_items, int *out_item_count) {
    if (id <= 0 || !out_insp) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "SELECT i.id, i.inspection_number, i.vehicle_id, v.registration_number, i.inspector_id, "
        "       i.inspector_name, i.inspection_date, i.odometer_km, i.overall_status, i.defect_count, "
        "       i.notes, i.created_at "
        "FROM inspections i "
        "JOIN vehicles v ON i.vehicle_id = v.id "
        "WHERE i.id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return STATUS_NOT_FOUND;
    }

    memset(out_insp, 0, sizeof(Inspection));
    out_insp->id = sqlite3_column_int(stmt, 0);
    safe_strncpy(out_insp->inspection_number, (const char *)sqlite3_column_text(stmt, 1), sizeof(out_insp->inspection_number));
    out_insp->vehicle_id = sqlite3_column_int(stmt, 2);
    safe_strncpy(out_insp->vehicle_reg, (const char *)sqlite3_column_text(stmt, 3), sizeof(out_insp->vehicle_reg));
    out_insp->inspector_id = sqlite3_column_type(stmt, 4) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 4);
    safe_strncpy(out_insp->inspector_name, (const char *)sqlite3_column_text(stmt, 5), sizeof(out_insp->inspector_name));
    safe_strncpy(out_insp->inspection_date, (const char *)sqlite3_column_text(stmt, 6), sizeof(out_insp->inspection_date));
    out_insp->odometer_km = sqlite3_column_int(stmt, 7);
    safe_strncpy(out_insp->overall_status, (const char *)sqlite3_column_text(stmt, 8), sizeof(out_insp->overall_status));
    out_insp->defect_count = sqlite3_column_int(stmt, 9);
    const char *notes = (const char *)sqlite3_column_text(stmt, 10);
    safe_strncpy(out_insp->notes, notes ? notes : "", sizeof(out_insp->notes));
    safe_strncpy(out_insp->created_at, (const char *)sqlite3_column_text(stmt, 11), sizeof(out_insp->created_at));

    sqlite3_finalize(stmt);

    if (out_items && out_item_count) {
        *out_items = NULL;
        *out_item_count = 0;

        const char *i_sql = 
            "SELECT id, inspection_id, item_name, status, severity, notes, work_order_id "
            "FROM inspection_items WHERE inspection_id = ? ORDER BY id ASC;";
        sqlite3_stmt *i_stmt = NULL;
        if (sqlite3_prepare_v2(db, i_sql, -1, &i_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(i_stmt, 1, id);
            int cap = 8;
            int cnt = 0;
            InspectionItem *it_list = (InspectionItem *)malloc(cap * sizeof(InspectionItem));
            while (sqlite3_step(i_stmt) == SQLITE_ROW) {
                if (cnt >= cap) {
                    cap *= 2;
                    InspectionItem *tmp = (InspectionItem *)realloc(it_list, cap * sizeof(InspectionItem));
                    if (!tmp) break;
                    it_list = tmp;
                }
                memset(&it_list[cnt], 0, sizeof(InspectionItem));
                it_list[cnt].id = sqlite3_column_int(i_stmt, 0);
                it_list[cnt].inspection_id = sqlite3_column_int(i_stmt, 1);
                safe_strncpy(it_list[cnt].item_name, (const char *)sqlite3_column_text(i_stmt, 2), sizeof(it_list[cnt].item_name));
                safe_strncpy(it_list[cnt].status, (const char *)sqlite3_column_text(i_stmt, 3), sizeof(it_list[cnt].status));
                safe_strncpy(it_list[cnt].severity, (const char *)sqlite3_column_text(i_stmt, 4), sizeof(it_list[cnt].severity));
                const char *inotes = (const char *)sqlite3_column_text(i_stmt, 5);
                safe_strncpy(it_list[cnt].notes, inotes ? inotes : "", sizeof(it_list[cnt].notes));
                it_list[cnt].work_order_id = sqlite3_column_type(i_stmt, 6) == SQLITE_NULL ? 0 : sqlite3_column_int(i_stmt, 6);
                cnt++;
            }
            sqlite3_finalize(i_stmt);
            *out_items = it_list;
            *out_item_count = cnt;
        }
    }

    return STATUS_OK;
}

StatusCode inspection_list(const InspectionFilter *filter, Inspection **out_list, int *out_count) {
    if (!out_list || !out_count) return STATUS_INVALID_INPUT;
    *out_list = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    char query[MAX_BUFFER_XL] = 
        "SELECT i.id, i.inspection_number, i.vehicle_id, v.registration_number, i.inspector_id, "
        "       i.inspector_name, i.inspection_date, i.odometer_km, i.overall_status, i.defect_count, "
        "       i.notes, i.created_at "
        "FROM inspections i "
        "JOIN vehicles v ON i.vehicle_id = v.id "
        "WHERE 1=1 ";

    if (filter && filter->vehicle_id > 0) {
        strcat(query, "AND i.vehicle_id = ? ");
    }
    if (filter && !str_is_blank(filter->overall_status)) {
        strcat(query, "AND i.overall_status = ? ");
    }

    strcat(query, "ORDER BY i.id DESC;");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    int bind_idx = 1;
    if (filter && filter->vehicle_id > 0) {
        sqlite3_bind_int(stmt, bind_idx++, filter->vehicle_id);
    }
    if (filter && !str_is_blank(filter->overall_status)) {
        sqlite3_bind_text(stmt, bind_idx++, filter->overall_status, -1, SQLITE_STATIC);
    }

    int capacity = 16;
    int count = 0;
    Inspection *list = (Inspection *)malloc(capacity * sizeof(Inspection));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            Inspection *tmp = (Inspection *)realloc(list, capacity * sizeof(Inspection));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }
        memset(&list[count], 0, sizeof(Inspection));
        list[count].id = sqlite3_column_int(stmt, 0);
        safe_strncpy(list[count].inspection_number, (const char *)sqlite3_column_text(stmt, 1), sizeof(list[count].inspection_number));
        list[count].vehicle_id = sqlite3_column_int(stmt, 2);
        safe_strncpy(list[count].vehicle_reg, (const char *)sqlite3_column_text(stmt, 3), sizeof(list[count].vehicle_reg));
        list[count].inspector_id = sqlite3_column_type(stmt, 4) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 4);
        safe_strncpy(list[count].inspector_name, (const char *)sqlite3_column_text(stmt, 5), sizeof(list[count].inspector_name));
        safe_strncpy(list[count].inspection_date, (const char *)sqlite3_column_text(stmt, 6), sizeof(list[count].inspection_date));
        list[count].odometer_km = sqlite3_column_int(stmt, 7);
        safe_strncpy(list[count].overall_status, (const char *)sqlite3_column_text(stmt, 8), sizeof(list[count].overall_status));
        list[count].defect_count = sqlite3_column_int(stmt, 9);
        const char *notes = (const char *)sqlite3_column_text(stmt, 10);
        safe_strncpy(list[count].notes, notes ? notes : "", sizeof(list[count].notes));
        safe_strncpy(list[count].created_at, (const char *)sqlite3_column_text(stmt, 11), sizeof(list[count].created_at));
        count++;
    }

    sqlite3_finalize(stmt);
    *out_list = list;
    *out_count = count;
    return STATUS_OK;
}

StatusCode inspection_get_defects(int vehicle_id, InspectionItem **out_defects, int *out_count) {
    if (!out_defects || !out_count) return STATUS_INVALID_INPUT;
    *out_defects = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    char query[MAX_BUFFER_XL] = 
        "SELECT it.id, it.inspection_id, it.item_name, it.status, it.severity, it.notes, it.work_order_id "
        "FROM inspection_items it "
        "JOIN inspections i ON it.inspection_id = i.id "
        "WHERE it.status = 'Fail' ";

    if (vehicle_id > 0) {
        strcat(query, "AND i.vehicle_id = ? ");
    }
    strcat(query, "ORDER BY it.id DESC;");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    if (vehicle_id > 0) {
        sqlite3_bind_int(stmt, 1, vehicle_id);
    }

    int capacity = 16;
    int count = 0;
    InspectionItem *list = (InspectionItem *)malloc(capacity * sizeof(InspectionItem));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            InspectionItem *tmp = (InspectionItem *)realloc(list, capacity * sizeof(InspectionItem));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }
        memset(&list[count], 0, sizeof(InspectionItem));
        list[count].id = sqlite3_column_int(stmt, 0);
        list[count].inspection_id = sqlite3_column_int(stmt, 1);
        safe_strncpy(list[count].item_name, (const char *)sqlite3_column_text(stmt, 2), sizeof(list[count].item_name));
        safe_strncpy(list[count].status, (const char *)sqlite3_column_text(stmt, 3), sizeof(list[count].status));
        safe_strncpy(list[count].severity, (const char *)sqlite3_column_text(stmt, 4), sizeof(list[count].severity));
        const char *notes = (const char *)sqlite3_column_text(stmt, 5);
        safe_strncpy(list[count].notes, notes ? notes : "", sizeof(list[count].notes));
        list[count].work_order_id = sqlite3_column_type(stmt, 6) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 6);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_defects = list;
    *out_count = count;
    return STATUS_OK;
}

StatusCode inspection_create_work_order_from_defect(int inspection_item_id, int user_id, const char *user_name, int *out_work_order_id) {
    if (inspection_item_id <= 0) return STATUS_INVALID_INPUT;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sel_sql = 
        "SELECT it.item_name, it.severity, it.notes, it.work_order_id, i.id, i.vehicle_id, i.odometer_km "
        "FROM inspection_items it "
        "JOIN inspections i ON it.inspection_id = i.id "
        "WHERE it.id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sel_sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, inspection_item_id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return STATUS_NOT_FOUND;
    }

    char item_name[MAX_BUFFER_MD];
    char severity[MAX_BUFFER_SM];
    char notes[MAX_BUFFER_LG];
    safe_strncpy(item_name, (const char *)sqlite3_column_text(stmt, 0), sizeof(item_name));
    safe_strncpy(severity, (const char *)sqlite3_column_text(stmt, 1), sizeof(severity));
    const char *n = (const char *)sqlite3_column_text(stmt, 2);
    safe_strncpy(notes, n ? n : "", sizeof(notes));
    int existing_wo = sqlite3_column_type(stmt, 3) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 3);
    int insp_id = sqlite3_column_int(stmt, 4);
    int vehicle_id = sqlite3_column_int(stmt, 5);
    int odom = sqlite3_column_int(stmt, 6);
    sqlite3_finalize(stmt);

    if (existing_wo > 0) {
        if (out_work_order_id) *out_work_order_id = existing_wo;
        return STATUS_OK; /* Work order already created */
    }

    WorkOrder wo = {0};
    wo.vehicle_id = vehicle_id;
    wo.inspection_id = insp_id;
    snprintf(wo.title, sizeof(wo.title), "Repair Inspection Defect: %.100s", item_name);
    snprintf(wo.description, sizeof(wo.description), "Identified during Inspection #%d (%.32s): %.400s",
             insp_id, severity, str_is_blank(notes) ? "Mechanical attention required" : notes);

    if (strcmp(severity, "Critical Defect") == 0) {
        safe_strncpy(wo.priority, "Critical", sizeof(wo.priority));
    } else {
        safe_strncpy(wo.priority, "High", sizeof(wo.priority));
    }
    safe_strncpy(wo.status, "Open", sizeof(wo.status));
    wo.odometer_km = odom;

    StatusCode sc = work_order_create(&wo, user_id, user_name);
    if (sc != STATUS_OK) return sc;

    /* Link created work order in inspection_items */
    const char *upd_sql = "UPDATE inspection_items SET work_order_id = ? WHERE id = ?;";
    sqlite3_stmt *upd_stmt = NULL;
    if (sqlite3_prepare_v2(db, upd_sql, -1, &upd_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(upd_stmt, 1, wo.id);
        sqlite3_bind_int(upd_stmt, 2, inspection_item_id);
        sqlite3_step(upd_stmt);
        sqlite3_finalize(upd_stmt);
    }

    if (out_work_order_id) *out_work_order_id = wo.id;
    return STATUS_OK;
}

void inspection_free_list(Inspection *list) {
    if (list) free(list);
}

void inspection_free_items(InspectionItem *items) {
    if (items) free(items);
}
