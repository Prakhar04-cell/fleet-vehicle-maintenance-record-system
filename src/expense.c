#include "expense.h"
#include "database.h"
#include "vehicle.h"
#include "audit.h"

static void fill_expense_from_stmt(sqlite3_stmt *stmt, Expense *e) {
    memset(e, 0, sizeof(Expense));
    e->id = sqlite3_column_int(stmt, 0);
    e->vehicle_id = sqlite3_column_int(stmt, 1);
    safe_strncpy(e->vehicle_reg, (const char *)sqlite3_column_text(stmt, 2), sizeof(e->vehicle_reg));
    e->maintenance_record_id = sqlite3_column_type(stmt, 3) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 3);
    safe_strncpy(e->category, (const char *)sqlite3_column_text(stmt, 4), sizeof(e->category));
    safe_strncpy(e->description, (const char *)sqlite3_column_text(stmt, 5), sizeof(e->description));
    e->amount = sqlite3_column_int64(stmt, 6);
    safe_strncpy(e->currency, (const char *)sqlite3_column_text(stmt, 7), sizeof(e->currency));
    safe_strncpy(e->expense_date, (const char *)sqlite3_column_text(stmt, 8), sizeof(e->expense_date));
    const char *v = (const char *)sqlite3_column_text(stmt, 9);
    safe_strncpy(e->vendor, v ? v : "", sizeof(e->vendor));
    e->created_by = sqlite3_column_int(stmt, 10);
    const char *creator = (const char *)sqlite3_column_text(stmt, 11);
    safe_strncpy(e->created_by_username, creator ? creator : "system", sizeof(e->created_by_username));
    safe_strncpy(e->created_at, (const char *)sqlite3_column_text(stmt, 12), sizeof(e->created_at));
}

StatusCode expense_create(Expense *expense, int user_id) {
    if (!expense || expense->vehicle_id <= 0 || str_is_blank(expense->category) ||
        str_is_blank(expense->description) || expense->amount < 0) {
        return STATUS_INVALID_INPUT;
    }
    if (!validate_iso_date(expense->expense_date)) {
        fprintf(stderr, "Error: Invalid expense date. Must be YYYY-MM-DD.\n");
        return STATUS_INVALID_INPUT;
    }

    Vehicle v;
    StatusCode sc = vehicle_get_by_id(expense->vehicle_id, &v);
    if (sc != STATUS_OK) return sc;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "INSERT INTO expenses (vehicle_id, maintenance_record_id, category, description, "
        "amount, currency, expense_date, vendor, created_by, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'));";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_int(stmt, 1, expense->vehicle_id);
    if (expense->maintenance_record_id > 0) {
        sqlite3_bind_int(stmt, 2, expense->maintenance_record_id);
    } else {
        sqlite3_bind_null(stmt, 2);
    }
    sqlite3_bind_text(stmt, 3, expense->category, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, expense->description, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, expense->amount);
    sqlite3_bind_text(stmt, 6, str_is_blank(expense->currency) ? "INR" : expense->currency, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, expense->expense_date, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, expense->vendor, -1, SQLITE_STATIC);
    if (user_id > 0) {
        sqlite3_bind_int(stmt, 9, user_id);
    } else {
        sqlite3_bind_null(stmt, 9);
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    expense->id = (int)db_last_insert_id();

    char details[MAX_BUFFER_XL];
    snprintf(details, sizeof(details), "Recorded %s expense for %s: Rs. %lld.%02d (%s)",
             expense->category, v.registration_number,
             (long long)(expense->amount / 100), (int)(expense->amount % 100), expense->description);
    audit_log_action(user_id, "EXPENSE_CREATE", "expenses", expense->id, details);

    return STATUS_OK;
}

StatusCode expense_get_by_id(int id, Expense *out_expense) {
    if (id <= 0 || !out_expense) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "SELECT e.id, e.vehicle_id, v.registration_number, e.maintenance_record_id, e.category, "
        "       e.description, e.amount, e.currency, e.expense_date, e.vendor, e.created_by, "
        "       u.username, e.created_at "
        "FROM expenses e "
        "JOIN vehicles v ON e.vehicle_id = v.id "
        "LEFT JOIN users u ON e.created_by = u.id "
        "WHERE e.id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return STATUS_NOT_FOUND;
    }

    fill_expense_from_stmt(stmt, out_expense);
    sqlite3_finalize(stmt);
    return STATUS_OK;
}

StatusCode expense_update(const Expense *expense, int user_id) {
    if (!expense || expense->id <= 0) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "UPDATE expenses SET category = ?, description = ?, amount = ?, currency = ?, "
        "expense_date = ?, vendor = ? WHERE id = ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    sqlite3_bind_text(stmt, 1, expense->category, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, expense->description, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, expense->amount);
    sqlite3_bind_text(stmt, 4, str_is_blank(expense->currency) ? "INR" : expense->currency, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, expense->expense_date, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, expense->vendor, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, expense->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    audit_log_action(user_id, "EXPENSE_UPDATE", "expenses", expense->id, "Updated expense details");
    return STATUS_OK;
}

StatusCode expense_delete(int id, int user_id) {
    if (id <= 0) return STATUS_INVALID_INPUT;
    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = "DELETE FROM expenses WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;
    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return STATUS_DB_ERROR;

    audit_log_action(user_id, "EXPENSE_DELETE", "expenses", id, "Deleted expense record");
    return STATUS_OK;
}

StatusCode expense_list(const ExpenseFilterOptions *filter, Expense **out_expenses, int *out_count) {
    if (!out_expenses || !out_count) return STATUS_INVALID_INPUT;
    *out_expenses = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    char query[MAX_BUFFER_XL] = 
        "SELECT e.id, e.vehicle_id, v.registration_number, e.maintenance_record_id, e.category, "
        "       e.description, e.amount, e.currency, e.expense_date, e.vendor, e.created_by, "
        "       u.username, e.created_at "
        "FROM expenses e "
        "JOIN vehicles v ON e.vehicle_id = v.id "
        "LEFT JOIN users u ON e.created_by = u.id "
        "WHERE 1=1 ";

    if (filter && filter->vehicle_id > 0) {
        strcat(query, "AND e.vehicle_id = ? ");
    }
    if (filter && !str_is_blank(filter->category)) {
        strcat(query, "AND e.category = ? ");
    }
    if (filter && !str_is_blank(filter->date_from)) {
        strcat(query, "AND e.expense_date >= ? ");
    }
    if (filter && !str_is_blank(filter->date_to)) {
        strcat(query, "AND e.expense_date <= ? ");
    }

    strcat(query, "ORDER BY e.expense_date DESC, e.id DESC;");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    int bind_idx = 1;
    if (filter && filter->vehicle_id > 0) {
        sqlite3_bind_int(stmt, bind_idx++, filter->vehicle_id);
    }
    if (filter && !str_is_blank(filter->category)) {
        sqlite3_bind_text(stmt, bind_idx++, filter->category, -1, SQLITE_STATIC);
    }
    if (filter && !str_is_blank(filter->date_from)) {
        sqlite3_bind_text(stmt, bind_idx++, filter->date_from, -1, SQLITE_STATIC);
    }
    if (filter && !str_is_blank(filter->date_to)) {
        sqlite3_bind_text(stmt, bind_idx++, filter->date_to, -1, SQLITE_STATIC);
    }

    int capacity = 16;
    int count = 0;
    Expense *list = (Expense *)malloc(capacity * sizeof(Expense));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            Expense *tmp = (Expense *)realloc(list, capacity * sizeof(Expense));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }
        fill_expense_from_stmt(stmt, &list[count]);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_expenses = list;
    *out_count = count;
    return STATUS_OK;
}

StatusCode expense_get_category_summary(CategoryExpenseSummary **out_summaries, int *out_count) {
    if (!out_summaries || !out_count) return STATUS_INVALID_INPUT;
    *out_summaries = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "SELECT category, SUM(amount) AS total, COUNT(*) AS cnt "
        "FROM expenses "
        "GROUP BY category "
        "ORDER BY total DESC;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    int capacity = 8;
    int count = 0;
    CategoryExpenseSummary *list = (CategoryExpenseSummary *)malloc(capacity * sizeof(CategoryExpenseSummary));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            CategoryExpenseSummary *tmp = (CategoryExpenseSummary *)realloc(list, capacity * sizeof(CategoryExpenseSummary));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }
        safe_strncpy(list[count].category, (const char *)sqlite3_column_text(stmt, 0), sizeof(list[count].category));
        list[count].total_amount = sqlite3_column_int64(stmt, 1);
        list[count].count = sqlite3_column_int(stmt, 2);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_summaries = list;
    *out_count = count;
    return STATUS_OK;
}

StatusCode expense_get_monthly_summary(MonthlyExpenseSummary **out_summaries, int *out_count) {
    if (!out_summaries || !out_count) return STATUS_INVALID_INPUT;
    *out_summaries = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "SELECT substr(expense_date, 1, 7) AS ym, SUM(amount) AS total, COUNT(*) AS cnt "
        "FROM expenses "
        "GROUP BY ym "
        "ORDER BY ym DESC LIMIT 12;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    int capacity = 12;
    int count = 0;
    MonthlyExpenseSummary *list = (MonthlyExpenseSummary *)malloc(capacity * sizeof(MonthlyExpenseSummary));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            MonthlyExpenseSummary *tmp = (MonthlyExpenseSummary *)realloc(list, capacity * sizeof(MonthlyExpenseSummary));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }
        safe_strncpy(list[count].year_month, (const char *)sqlite3_column_text(stmt, 0), sizeof(list[count].year_month));
        list[count].total_amount = sqlite3_column_int64(stmt, 1);
        list[count].count = sqlite3_column_int(stmt, 2);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_summaries = list;
    *out_count = count;
    return STATUS_OK;
}

StatusCode expense_get_vehicle_summaries(VehicleCostSummary **out_summaries, int *out_count) {
    if (!out_summaries || !out_count) return STATUS_INVALID_INPUT;
    *out_summaries = NULL;
    *out_count = 0;

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    const char *sql = 
        "SELECT v.id, v.registration_number, (v.make || ' ' || v.model) AS mm, "
        "       COALESCE(SUM(CASE WHEN e.category = 'Maintenance' THEN e.amount ELSE 0 END), 0) AS maint_cost, "
        "       COALESCE(SUM(CASE WHEN e.category != 'Maintenance' THEN e.amount ELSE 0 END), 0) AS other_cost, "
        "       COALESCE(SUM(e.amount), 0) AS total_cost "
        "FROM vehicles v "
        "LEFT JOIN expenses e ON v.id = e.vehicle_id "
        "WHERE v.archived_at IS NULL "
        "GROUP BY v.id "
        "ORDER BY total_cost DESC;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    int capacity = 16;
    int count = 0;
    VehicleCostSummary *list = (VehicleCostSummary *)malloc(capacity * sizeof(VehicleCostSummary));
    if (!list) {
        sqlite3_finalize(stmt);
        return STATUS_ERROR;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            VehicleCostSummary *tmp = (VehicleCostSummary *)realloc(list, capacity * sizeof(VehicleCostSummary));
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return STATUS_ERROR;
            }
            list = tmp;
        }
        list[count].vehicle_id = sqlite3_column_int(stmt, 0);
        safe_strncpy(list[count].registration_number, (const char *)sqlite3_column_text(stmt, 1), sizeof(list[count].registration_number));
        safe_strncpy(list[count].make_model, (const char *)sqlite3_column_text(stmt, 2), sizeof(list[count].make_model));
        list[count].maintenance_cost = sqlite3_column_int64(stmt, 3);
        list[count].other_cost = sqlite3_column_int64(stmt, 4);
        list[count].total_cost = sqlite3_column_int64(stmt, 5);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_summaries = list;
    *out_count = count;
    return STATUS_OK;
}

StatusCode expense_get_overall_stats(OverallExpenseStats *out_stats) {
    if (!out_stats) return STATUS_INVALID_INPUT;
    memset(out_stats, 0, sizeof(OverallExpenseStats));

    sqlite3 *db = db_get_handle();
    if (!db) return STATUS_DB_ERROR;

    /* Total active vehicles */
    sqlite3_stmt *v_stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM vehicles WHERE archived_at IS NULL;", -1, &v_stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(v_stmt) == SQLITE_ROW) {
            out_stats->total_active_vehicles = sqlite3_column_int(v_stmt, 0);
        }
        sqlite3_finalize(v_stmt);
    }

    /* Total maintenance spending & all expenses */
    const char *sql = 
        "SELECT "
        "  COALESCE(SUM(CASE WHEN category = 'Maintenance' THEN amount ELSE 0 END), 0) AS maint_total, "
        "  COALESCE(SUM(amount), 0) AS total_all, "
        "  COUNT(*) AS cnt "
        "FROM expenses;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return STATUS_DB_ERROR;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out_stats->total_maintenance_spending = sqlite3_column_int64(stmt, 0);
        out_stats->total_all_expenses = sqlite3_column_int64(stmt, 1);
        out_stats->expense_records_count = sqlite3_column_int(stmt, 2);
    }
    sqlite3_finalize(stmt);

    if (out_stats->total_active_vehicles > 0) {
        out_stats->avg_cost_per_active_vehicle = out_stats->total_maintenance_spending / out_stats->total_active_vehicles;
    }

    return STATUS_OK;
}

void expense_free_list(Expense *expenses) {
    if (expenses) free(expenses);
}
