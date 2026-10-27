#ifndef EXPENSE_H
#define EXPENSE_H

#include "common.h"

typedef struct {
    int id;
    int vehicle_id;
    char vehicle_reg[MAX_BUFFER_SM];
    int maintenance_record_id;
    char category[MAX_BUFFER_SM];
    char description[MAX_BUFFER_LG];
    int64_t amount; /* in minor currency units (paise/cents) */
    char currency[MAX_BUFFER_XS];
    char expense_date[MAX_BUFFER_SM];
    char vendor[MAX_BUFFER_MD];
    int created_by;
    char created_by_username[MAX_BUFFER_SM];
    char created_at[MAX_BUFFER_SM];
} Expense;

typedef struct {
    int vehicle_id;
    char category[MAX_BUFFER_SM];
    char date_from[MAX_BUFFER_SM];
    char date_to[MAX_BUFFER_SM];
} ExpenseFilterOptions;

typedef struct {
    char category[MAX_BUFFER_SM];
    int64_t total_amount;
    int count;
} CategoryExpenseSummary;

typedef struct {
    char year_month[MAX_BUFFER_SM]; /* "2026-10" */
    int64_t total_amount;
    int count;
} MonthlyExpenseSummary;

typedef struct {
    int vehicle_id;
    char registration_number[MAX_BUFFER_SM];
    char make_model[MAX_BUFFER_MD];
    int64_t maintenance_cost;
    int64_t other_cost;
    int64_t total_cost;
} VehicleCostSummary;

typedef struct {
    int64_t total_maintenance_spending;
    int64_t total_all_expenses;
    int64_t avg_cost_per_active_vehicle;
    int total_active_vehicles;
    int expense_records_count;
} OverallExpenseStats;

StatusCode expense_create(Expense *expense, int user_id);
StatusCode expense_get_by_id(int id, Expense *out_expense);
StatusCode expense_update(const Expense *expense, int user_id);
StatusCode expense_delete(int id, int user_id);
StatusCode expense_list(const ExpenseFilterOptions *filter, Expense **out_expenses, int *out_count);
StatusCode expense_get_category_summary(CategoryExpenseSummary **out_summaries, int *out_count);
StatusCode expense_get_monthly_summary(MonthlyExpenseSummary **out_summaries, int *out_count);
StatusCode expense_get_vehicle_summaries(VehicleCostSummary **out_summaries, int *out_count);
StatusCode expense_get_overall_stats(OverallExpenseStats *out_stats);
void expense_free_list(Expense *expenses);

#endif /* EXPENSE_H */
