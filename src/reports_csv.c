#include "reports.h"
#include "vehicle.h"
#include "maintenance.h"
#include "schedule.h"
#include "expense.h"
#include "audit.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#else
#include <sys/stat.h>
#define MKDIR(d) mkdir(d, 0755)
#endif

void report_escape_csv_field(const char *src, char *dest, size_t dest_size) {
    if (!dest || dest_size == 0) return;
    if (!src || src[0] == '\0') {
        dest[0] = '\0';
        return;
    }

    /* Protection against CSV / Excel Formula Injection (CWE-1236) */
    bool formula_risk = (src[0] == '=' || src[0] == '+' || src[0] == '-' || src[0] == '@' || src[0] == '\t' || src[0] == '\r');

    bool needs_quotes = formula_risk || strchr(src, ',') || strchr(src, '"') || strchr(src, '\n') || strchr(src, '\r');

    size_t d = 0;
    if (needs_quotes && d + 1 < dest_size) {
        dest[d++] = '"';
    }

    if (formula_risk && d + 1 < dest_size) {
        dest[d++] = '\''; /* prefix single quote */
    }

    for (const char *s = src; *s && d + 2 < dest_size; s++) {
        if (*s == '"') {
            dest[d++] = '"';
            dest[d++] = '"';
        } else {
            dest[d++] = *s;
        }
    }

    if (needs_quotes && d + 1 < dest_size) {
        dest[d++] = '"';
    }
    dest[d] = '\0';
}

static void ensure_reports_dir(const char *filepath) {
    char dir[MAX_BUFFER_MD];
    safe_strncpy(dir, filepath, sizeof(dir));
    char *p = strrchr(dir, '/');
    if (!p) p = strrchr(dir, '\\');
    if (p) {
        *p = '\0';
        MKDIR(dir);
    }
}

StatusCode report_csv_vehicle_register(const char *output_path) {
    const char *path = output_path ? output_path : "reports/vehicles_report.csv";
    ensure_reports_dir(path);

    VehicleFilterOptions f = {0};
    f.include_archived = true;
    Vehicle *list = NULL;
    int count = 0;
    StatusCode sc = vehicle_list(&f, &list, &count);
    if (sc != STATUS_OK) return sc;

    FILE *fp = fopen(path, "w");
    if (!fp) {
        vehicle_free_list(list);
        return STATUS_ERROR;
    }

    /* CSV Header */
    fprintf(fp, "ID,Registration Number,Make,Model,Year,VIN,Fuel Type,Fuel %%,Odometer (KM),Status,Assigned Driver,Insurance Expiry,Archived\n");

    for (int i = 0; i < count; i++) {
        char reg[MAX_BUFFER_MD], make[MAX_BUFFER_MD], model[MAX_BUFFER_MD], vin[MAX_BUFFER_MD];
        char driver[MAX_BUFFER_MD], status[MAX_BUFFER_MD];
        report_escape_csv_field(list[i].registration_number, reg, sizeof(reg));
        report_escape_csv_field(list[i].make, make, sizeof(make));
        report_escape_csv_field(list[i].model, model, sizeof(model));
        report_escape_csv_field(list[i].vin, vin, sizeof(vin));
        report_escape_csv_field(list[i].assigned_driver_name, driver, sizeof(driver));
        report_escape_csv_field(list[i].status, status, sizeof(status));

        fprintf(fp, "%d,%s,%s,%s,%d,%s,%s,%d,%d,%s,%s,%s,%s\n",
                list[i].id, reg, make, model, list[i].manufacturing_year,
                vin, list[i].fuel_type, list[i].fuel_percentage, list[i].odometer_km,
                status, driver, list[i].insurance_expiry, list[i].is_archived ? "YES" : "NO");
    }

    fclose(fp);
    vehicle_free_list(list);
    audit_log_action(0, "REPORT_CSV", "reports", 0, "Generated Vehicle Register CSV");
    return STATUS_OK;
}

StatusCode report_csv_maintenance_history(const char *output_path, int vehicle_id) {
    const char *path = output_path ? output_path : "reports/maintenance_history.csv";
    ensure_reports_dir(path);

    MaintenanceFilterOptions f = {0};
    f.vehicle_id = vehicle_id;
    MaintenanceRecord *list = NULL;
    int count = 0;
    StatusCode sc = maintenance_list(&f, &list, &count);
    if (sc != STATUS_OK) return sc;

    FILE *fp = fopen(path, "w");
    if (!fp) {
        maintenance_free_list(list);
        return STATUS_ERROR;
    }

    fprintf(fp, "ID,Vehicle Reg,Service Type,Date,Odometer (KM),Workshop,Mechanic,Labour (Rs),Parts (Rs),Tax (Rs),Total Cost (Rs),Description,Next Service Date,Next Service KM\n");

    for (int i = 0; i < count; i++) {
        char reg[MAX_BUFFER_MD], stype[MAX_BUFFER_MD], wshop[MAX_BUFFER_MD], mech[MAX_BUFFER_MD], desc[MAX_BUFFER_XL];
        report_escape_csv_field(list[i].vehicle_reg, reg, sizeof(reg));
        report_escape_csv_field(list[i].service_type, stype, sizeof(stype));
        report_escape_csv_field(list[i].workshop_name, wshop, sizeof(wshop));
        report_escape_csv_field(list[i].mechanic_name, mech, sizeof(mech));
        report_escape_csv_field(list[i].description, desc, sizeof(desc));

        fprintf(fp, "%d,%s,%s,%s,%d,%s,%s,%.2f,%.2f,%.2f,%.2f,%s,%s,%d\n",
                list[i].id, reg, stype, list[i].service_date, list[i].odometer_km,
                wshop, mech,
                list[i].labour_cost / 100.0,
                list[i].parts_cost / 100.0,
                list[i].tax_amount / 100.0,
                list[i].total_cost / 100.0,
                desc, list[i].next_service_date, list[i].next_service_odometer_km);
    }

    fclose(fp);
    maintenance_free_list(list);
    audit_log_action(0, "REPORT_CSV", "reports", 0, "Generated Maintenance History CSV");
    return STATUS_OK;
}

StatusCode report_csv_schedules(const char *output_path) {
    const char *path = output_path ? output_path : "reports/maintenance_schedules.csv";
    ensure_reports_dir(path);

    ScheduleFilterOptions f = {0};
    MaintenanceSchedule *list = NULL;
    int count = 0;
    StatusCode sc = schedule_list(&f, &list, &count);
    if (sc != STATUS_OK) return sc;

    FILE *fp = fopen(path, "w");
    if (!fp) {
        schedule_free_list(list);
        return STATUS_ERROR;
    }

    fprintf(fp, "ID,Vehicle Reg,Service Name,Due Date,Due Odometer (KM),Current Odometer (KM),Status,Days Remaining,KM Remaining,Recurring Days,Recurring KM\n");

    for (int i = 0; i < count; i++) {
        char reg[MAX_BUFFER_MD], sname[MAX_BUFFER_MD], status[MAX_BUFFER_MD];
        report_escape_csv_field(list[i].vehicle_reg, reg, sizeof(reg));
        report_escape_csv_field(list[i].service_name, sname, sizeof(sname));
        report_escape_csv_field(list[i].status, status, sizeof(status));

        fprintf(fp, "%d,%s,%s,%s,%d,%d,%s,%d,%d,%d,%d\n",
                list[i].id, reg, sname, list[i].due_date, list[i].due_odometer_km,
                list[i].current_vehicle_odometer, status,
                list[i].days_remaining, list[i].km_remaining,
                list[i].recurring_interval_days, list[i].recurring_interval_km);
    }

    fclose(fp);
    schedule_free_list(list);
    audit_log_action(0, "REPORT_CSV", "reports", 0, "Generated Maintenance Schedules CSV");
    return STATUS_OK;
}

StatusCode report_csv_expenses(const char *output_path) {
    const char *path = output_path ? output_path : "reports/expense_history.csv";
    ensure_reports_dir(path);

    ExpenseFilterOptions f = {0};
    Expense *list = NULL;
    int count = 0;
    StatusCode sc = expense_list(&f, &list, &count);
    if (sc != STATUS_OK) return sc;

    FILE *fp = fopen(path, "w");
    if (!fp) {
        expense_free_list(list);
        return STATUS_ERROR;
    }

    fprintf(fp, "ID,Vehicle Reg,Category,Date,Amount (Rs),Vendor,Description,Linked Maintenance ID,Recorded By\n");

    for (int i = 0; i < count; i++) {
        char reg[MAX_BUFFER_MD], cat[MAX_BUFFER_MD], vendor[MAX_BUFFER_MD], desc[MAX_BUFFER_LG], user[MAX_BUFFER_MD];
        report_escape_csv_field(list[i].vehicle_reg, reg, sizeof(reg));
        report_escape_csv_field(list[i].category, cat, sizeof(cat));
        report_escape_csv_field(list[i].vendor, vendor, sizeof(vendor));
        report_escape_csv_field(list[i].description, desc, sizeof(desc));
        report_escape_csv_field(list[i].created_by_username, user, sizeof(user));

        fprintf(fp, "%d,%s,%s,%s,%.2f,%s,%s,%d,%s\n",
                list[i].id, reg, cat, list[i].expense_date,
                list[i].amount / 100.0, vendor, desc,
                list[i].maintenance_record_id, user);
    }

    fclose(fp);
    expense_free_list(list);
    audit_log_action(0, "REPORT_CSV", "reports", 0, "Generated Expense History CSV");
    return STATUS_OK;
}

StatusCode report_csv_monthly_spending(const char *output_path) {
    const char *path = output_path ? output_path : "reports/monthly_expenditure.csv";
    ensure_reports_dir(path);

    MonthlyExpenseSummary *list = NULL;
    int count = 0;
    StatusCode sc = expense_get_monthly_summary(&list, &count);
    if (sc != STATUS_OK) return sc;

    FILE *fp = fopen(path, "w");
    if (!fp) {
        if (list) free(list);
        return STATUS_ERROR;
    }

    fprintf(fp, "Month,Total Spending (Rs),Transaction Count\n");
    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s,%.2f,%d\n", list[i].year_month, list[i].total_amount / 100.0, list[i].count);
    }

    fclose(fp);
    if (list) free(list);
    audit_log_action(0, "REPORT_CSV", "reports", 0, "Generated Monthly Expenditure CSV");
    return STATUS_OK;
}

StatusCode report_csv_vehicle_costs(const char *output_path) {
    const char *path = output_path ? output_path : "reports/vehicle_costs.csv";
    ensure_reports_dir(path);

    VehicleCostSummary *list = NULL;
    int count = 0;
    StatusCode sc = expense_get_vehicle_summaries(&list, &count);
    if (sc != STATUS_OK) return sc;

    FILE *fp = fopen(path, "w");
    if (!fp) {
        if (list) free(list);
        return STATUS_ERROR;
    }

    fprintf(fp, "Vehicle ID,Registration Number,Make & Model,Maintenance Cost (Rs),Other Expenses (Rs),Total Cost (Rs)\n");
    for (int i = 0; i < count; i++) {
        char reg[MAX_BUFFER_MD], mm[MAX_BUFFER_MD];
        report_escape_csv_field(list[i].registration_number, reg, sizeof(reg));
        report_escape_csv_field(list[i].make_model, mm, sizeof(mm));

        fprintf(fp, "%d,%s,%s,%.2f,%.2f,%.2f\n",
                list[i].vehicle_id, reg, mm,
                list[i].maintenance_cost / 100.0,
                list[i].other_cost / 100.0,
                list[i].total_cost / 100.0);
    }

    fclose(fp);
    if (list) free(list);
    audit_log_action(0, "REPORT_CSV", "reports", 0, "Generated Vehicle Costs CSV");
    return STATUS_OK;
}
