#include "common.h"
#include "database.h"
#include "auth.h"
#include "vehicle.h"
#include "driver.h"
#include "maintenance.h"
#include "schedule.h"
#include "expense.h"
#include "notification.h"
#include "audit.h"
#include "dashboard.h"
#include "reports.h"
#include "backup.h"

#include <assert.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (cond) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        printf("  [FAIL] %s (Line %d: %s)\n", msg, __LINE__, #cond); \
    } \
} while(0)

static const char *TEST_DB_PATH = "data/test_fleet.db";

static void setup_test_db(void) {
    remove(TEST_DB_PATH);
    StatusCode sc = db_init(TEST_DB_PATH);
    assert(sc == STATUS_OK);
    sc = db_run_migrations("migrations");
    assert(sc == STATUS_OK);
}

static void teardown_test_db(void) {
    db_close();
    remove(TEST_DB_PATH);
}

/* ==================== 1. AUTHENTICATION TESTS ==================== */
static void run_auth_tests(void) {
    printf("\n=== 1. Running Authentication & RBAC Tests ===\n");

    bool is_first = false;
    auth_is_first_run(&is_first);
    TEST_ASSERT(is_first == true, "Fresh database detects first-run state");

    /* Create Admin User */
    StatusCode sc = auth_create_user("admin_user", "AdminPass123", ROLE_ADMIN);
    TEST_ASSERT(sc == STATUS_OK, "Successfully created Administrator account");

    auth_is_first_run(&is_first);
    TEST_ASSERT(is_first == false, "Database is no longer in first-run state after admin creation");

    /* Create Additional Role Accounts */
    sc = auth_create_user("mgr_user", "ManagerPass123", ROLE_MANAGER);
    TEST_ASSERT(sc == STATUS_OK, "Created Fleet Manager account");

    sc = auth_create_user("staff_user", "StaffPass123", ROLE_STAFF);
    TEST_ASSERT(sc == STATUS_OK, "Created Maintenance Staff account");

    sc = auth_create_user("viewer_user", "ViewerPass123", ROLE_VIEWER);
    TEST_ASSERT(sc == STATUS_OK, "Created Viewer account");

    /* Duplicate Username Prevention */
    sc = auth_create_user("admin_user", "DuplicatePass123", ROLE_ADMIN);
    TEST_ASSERT(sc == STATUS_EXISTS, "Prevent duplicate username creation");

    /* Short Password Rejection */
    sc = auth_create_user("short_user", "123", ROLE_STAFF);
    TEST_ASSERT(sc == STATUS_INVALID_INPUT, "Reject password under 6 characters");

    /* Correct Login */
    User u;
    sc = auth_login("admin_user", "AdminPass123", &u);
    TEST_ASSERT(sc == STATUS_OK, "Correct password authenticates successfully");
    TEST_ASSERT(u.role == ROLE_ADMIN, "Authenticated user has correct ROLE_ADMIN");
    TEST_ASSERT(strcmp(u.username, "admin_user") == 0, "Username matches");

    /* Case-Insensitive Username Login */
    sc = auth_login("ADMIN_USER", "AdminPass123", &u);
    TEST_ASSERT(sc == STATUS_OK, "Username matching is case-insensitive");

    /* Incorrect Password */
    sc = auth_login("admin_user", "WrongPassword", &u);
    TEST_ASSERT(sc == STATUS_AUTH_FAILED, "Incorrect password fails authentication");

    /* Unknown Username */
    sc = auth_login("nonexistent_user", "SomePass123", &u);
    TEST_ASSERT(sc == STATUS_AUTH_FAILED, "Nonexistent username fails authentication");

    /* Password Change */
    sc = auth_change_password(u.id, "AdminPass123", "NewSecretPass456");
    TEST_ASSERT(sc == STATUS_OK, "User can successfully change own password");

    sc = auth_login("admin_user", "AdminPass123", &u);
    TEST_ASSERT(sc == STATUS_AUTH_FAILED, "Old password rejected after password change");

    sc = auth_login("admin_user", "NewSecretPass456", &u);
    TEST_ASSERT(sc == STATUS_OK, "New password authenticates successfully");

    /* Admin Password Reset */
    User mgr;
    auth_login("mgr_user", "ManagerPass123", &mgr);
    sc = auth_admin_reset_password(u.id, mgr.id, "AdminReset999");
    TEST_ASSERT(sc == STATUS_OK, "Administrator can reset another user's password");

    sc = auth_login("mgr_user", "AdminReset999", &mgr);
    TEST_ASSERT(sc == STATUS_OK, "Target user can log in with reset password");

    /* Disable Account */
    sc = auth_set_user_status(u.id, mgr.id, false);
    TEST_ASSERT(sc == STATUS_OK, "Administrator can deactivate an account");

    sc = auth_login("mgr_user", "AdminReset999", &mgr);
    TEST_ASSERT(sc == STATUS_FORBIDDEN, "Disabled user is blocked from logging in");

    /* Re-enable Account */
    sc = auth_set_user_status(u.id, mgr.id, true);
    TEST_ASSERT(sc == STATUS_OK, "Administrator can reactivate an account");

    sc = auth_login("mgr_user", "AdminReset999", &mgr);
    TEST_ASSERT(sc == STATUS_OK, "Reactivated user can log in");

    /* Role-based Permission Flags */
    TEST_ASSERT(auth_has_permission(ROLE_ADMIN, PERM_USER_MANAGE) == true, "Admin has PERM_USER_MANAGE");
    TEST_ASSERT(auth_has_permission(ROLE_ADMIN, PERM_BACKUP_RESTORE) == true, "Admin has PERM_BACKUP_RESTORE");
    TEST_ASSERT(auth_has_permission(ROLE_MANAGER, PERM_USER_MANAGE) == false, "Manager does not have PERM_USER_MANAGE");
    TEST_ASSERT(auth_has_permission(ROLE_MANAGER, PERM_VEHICLE_WRITE) == true, "Manager has PERM_VEHICLE_WRITE");
    TEST_ASSERT(auth_has_permission(ROLE_STAFF, PERM_VEHICLE_WRITE) == false, "Staff does not have PERM_VEHICLE_WRITE");
    TEST_ASSERT(auth_has_permission(ROLE_STAFF, PERM_MAINTENANCE_WRITE) == true, "Staff has PERM_MAINTENANCE_WRITE");
    TEST_ASSERT(auth_has_permission(ROLE_VIEWER, PERM_MAINTENANCE_WRITE) == false, "Viewer cannot write maintenance");
    TEST_ASSERT(auth_has_permission(ROLE_VIEWER, PERM_READ) == true, "Viewer has PERM_READ");
}

/* ==================== 2. VEHICLE & DRIVER TESTS ==================== */
static void run_vehicle_tests(void) {
    printf("\n=== 2. Running Vehicle & Driver Management Tests ===\n");

    /* Create Drivers */
    Driver d1 = {0};
    strcpy(d1.name, "Rajesh Kumar");
    strcpy(d1.phone, "+91-9876543210");
    strcpy(d1.license_number, "DL-01-2019-12345");
    strcpy(d1.license_expiry, "2028-12-31");
    d1.active = true;

    int driver_id1 = 0;
    StatusCode sc = driver_create(&d1, &driver_id1);
    TEST_ASSERT(sc == STATUS_OK && driver_id1 > 0, "Created Driver 1 successfully");

    /* Prevent duplicate driver license */
    sc = driver_create(&d1, NULL);
    TEST_ASSERT(sc == STATUS_EXISTS, "Prevent duplicate driver license registration");

    Driver d2 = {0};
    strcpy(d2.name, "Amit Sharma");
    strcpy(d2.phone, "+91-9123456789");
    strcpy(d2.license_number, "DL-02-2021-98765");
    strcpy(d2.license_expiry, "2030-05-15");
    d2.active = true;
    int driver_id2 = 0;
    driver_create(&d2, &driver_id2);

    /* Create Vehicles */
    Vehicle v1 = {0};
    strcpy(v1.registration_number, "MH-01-AB-1001");
    strcpy(v1.make, "Tata");
    strcpy(v1.model, "Ace Gold");
    v1.manufacturing_year = 2022;
    strcpy(v1.vin, "MAT612001A1001");
    strcpy(v1.fuel_type, "Diesel");
    v1.fuel_percentage = 85;
    v1.odometer_km = 25000;
    strcpy(v1.status, "Available");
    v1.assigned_driver_id = driver_id1;

    int veh_id1 = 0;
    sc = vehicle_create(&v1, &veh_id1);
    TEST_ASSERT(sc == STATUS_OK && veh_id1 > 0, "Created Vehicle 1 with assigned Driver 1");

    /* Prevent duplicate registration number */
    Vehicle v_dup = v1;
    v_dup.assigned_driver_id = 0;
    strcpy(v_dup.vin, "UNIQUEVIN999");
    sc = vehicle_create(&v_dup, NULL);
    TEST_ASSERT(sc == STATUS_EXISTS, "Prevent duplicate vehicle registration number");

    /* Prevent driver double-booking across active vehicles */
    Vehicle v2 = {0};
    strcpy(v2.registration_number, "MH-02-CD-2002");
    strcpy(v2.make, "Mahindra");
    strcpy(v2.model, "Bolero Maxi Truck");
    v2.manufacturing_year = 2023;
    strcpy(v2.vin, "MA1612002B2002");
    strcpy(v2.fuel_type, "Diesel");
    v2.fuel_percentage = 60;
    v2.odometer_km = 18000;
    strcpy(v2.status, "Available");
    v2.assigned_driver_id = driver_id1; /* already assigned to v1! */

    sc = vehicle_create(&v2, NULL);
    TEST_ASSERT(sc == STATUS_FORBIDDEN, "Prevent assigning already-assigned driver to another active vehicle");

    /* Assign unused driver_id2 to v2 */
    v2.assigned_driver_id = driver_id2;
    int veh_id2 = 0;
    sc = vehicle_create(&v2, &veh_id2);
    TEST_ASSERT(sc == STATUS_OK && veh_id2 > 0, "Created Vehicle 2 with available Driver 2");

    /* Read Vehicle */
    Vehicle fetched;
    sc = vehicle_get_by_id(veh_id1, &fetched);
    TEST_ASSERT(sc == STATUS_OK, "Read vehicle by ID");
    TEST_ASSERT(strcmp(fetched.registration_number, "MH-01-AB-1001") == 0, "Vehicle reg matches");
    TEST_ASSERT(strcmp(fetched.assigned_driver_name, "Rajesh Kumar") == 0, "Assigned driver joined correctly");

    /* Negative Odometer Rejection */
    sc = vehicle_update_odometer(veh_id1, -500, NULL, 1);
    TEST_ASSERT(sc == STATUS_INVALID_INPUT, "Reject negative odometer reading");

    /* Decreasing Odometer without Reason Rejection */
    sc = vehicle_update_odometer(veh_id1, 20000, "", 1);
    TEST_ASSERT(sc == STATUS_INVALID_INPUT, "Reject decreasing odometer without authorized reason");

    /* Decreasing Odometer with Authorized Reason */
    sc = vehicle_update_odometer(veh_id1, 24000, "Cluster replacement calibration correction", 1);
    TEST_ASSERT(sc == STATUS_OK, "Accept decreasing odometer with explicit audited reason");

    /* Normal Odometer Update */
    sc = vehicle_update_odometer(veh_id1, 26500, NULL, 1);
    TEST_ASSERT(sc == STATUS_OK, "Accept forward odometer update");

    /* Fuel Level Update */
    sc = vehicle_update_fuel(veh_id1, 45, 1);
    TEST_ASSERT(sc == STATUS_OK, "Fuel level updated to 45%");

    /* Vehicle Search */
    Vehicle *search_res = NULL;
    int search_cnt = 0;
    sc = vehicle_search("Tata", &search_res, &search_cnt);
    TEST_ASSERT(sc == STATUS_OK && search_cnt >= 1, "Vehicle search finds match by Make");
    vehicle_free_list(search_res);

    /* Vehicle Archival (Soft Delete) */
    sc = vehicle_archive(veh_id2, 1);
    TEST_ASSERT(sc == STATUS_OK, "Vehicle archived successfully");

    VehicleFilterOptions f = {0};
    f.include_archived = false;
    Vehicle *active_list = NULL;
    int active_cnt = 0;
    vehicle_list(&f, &active_list, &active_cnt);
    TEST_ASSERT(active_cnt == 1, "Archived vehicle excluded from active vehicle list");
    vehicle_free_list(active_list);

    /* Historical records still accessible */
    Vehicle archived_veh;
    sc = vehicle_get_by_id(veh_id2, &archived_veh);
    TEST_ASSERT(sc == STATUS_OK && archived_veh.is_archived == true, "Archived vehicle persists and remains readable");

    /* Driver 2 can now be reassigned since v2 is decommissioned */
    sc = vehicle_assign_driver(veh_id1, driver_id2, 1);
    TEST_ASSERT(sc == STATUS_OK, "Driver from decommissioned vehicle can be reassigned");
}

/* ==================== 3. MAINTENANCE RECORDS TESTS ==================== */
static void run_maintenance_tests(void) {
    printf("\n=== 3. Running Maintenance & Service Records Tests ===\n");

    Vehicle v;
    vehicle_get_by_reg("MH-01-AB-1001", &v);

    MaintenanceRecord r = {0};
    r.vehicle_id = v.id;
    strcpy(r.service_type, "Preventive Maintenance");
    strcpy(r.service_date, "2026-10-15");
    r.odometer_km = 27000;
    strcpy(r.description, "Complete 25,000 KM Periodic Service: Mobil Delvac Engine Oil, Oil Filter, Fuel Filter Replacement");
    strcpy(r.workshop_name, "Tata Authorized Service Center");
    strcpy(r.mechanic_name, "Suresh");
    r.labour_cost = 150000; /* Rs. 1500.00 */
    r.parts_cost = 320000;  /* Rs. 3200.00 */
    r.tax_amount = 84600;   /* Rs. 846.00 */
    strcpy(r.next_service_date, "2027-04-15");
    r.next_service_odometer_km = 37000;

    StatusCode sc = maintenance_create(&r, true, 1);
    TEST_ASSERT(sc == STATUS_OK && r.id > 0, "Created Maintenance Record with automatic linked expense");
    TEST_ASSERT(r.total_cost == (150000 + 320000 + 84600), "Total cost accurately sums labour, parts, and tax");

    /* Check that vehicle odometer was auto-updated to service odometer */
    Vehicle v_updated;
    vehicle_get_by_id(v.id, &v_updated);
    TEST_ASSERT(v_updated.odometer_km == 27000, "Vehicle odometer auto-advanced to service odometer (27,000 km)");

    /* Verify linked expense exists and matches */
    Expense exp;
    sc = expense_get_by_id(1, &exp);
    TEST_ASSERT(sc == STATUS_OK, "Linked expense created in same transaction");
    TEST_ASSERT(exp.maintenance_record_id == r.id, "Expense is linked to maintenance record ID");
    TEST_ASSERT(exp.amount == r.total_cost, "Expense amount equals maintenance total cost");

    /* Reject Nonexistent Vehicle ID */
    MaintenanceRecord bad_r = r;
    bad_r.vehicle_id = 999999;
    sc = maintenance_create(&bad_r, false, 1);
    TEST_ASSERT(sc == STATUS_NOT_FOUND, "Reject maintenance record for nonexistent vehicle ID");

    /* Reject Invalid Service Date */
    bad_r = r;
    strcpy(bad_r.service_date, "invalid-date");
    sc = maintenance_create(&bad_r, false, 1);
    TEST_ASSERT(sc == STATUS_INVALID_INPUT, "Reject invalid service date format");

    /* Retrieve Vehicle Maintenance History */
    MaintenanceRecord *history = NULL;
    int h_cnt = 0;
    sc = maintenance_get_vehicle_history(v.id, &history, &h_cnt);
    TEST_ASSERT(sc == STATUS_OK && h_cnt == 1, "Retrieved maintenance history for vehicle");
    TEST_ASSERT(strcmp(history[0].workshop_name, "Tata Authorized Service Center") == 0, "Workshop name matches");
    maintenance_free_list(history);
}

/* ==================== 4. PREVENTIVE MAINTENANCE & REMINDERS TESTS ==================== */
static void run_schedule_tests(void) {
    printf("\n=== 4. Running Preventive Maintenance Schedules & Reminders Tests ===\n");

    Vehicle v;
    vehicle_get_by_reg("MH-01-AB-1001", &v);

    /* Schedule 1: Overdue Date Service */
    MaintenanceSchedule s1 = {0};
    s1.vehicle_id = v.id;
    strcpy(s1.service_name, "Brake Fluid Flush");
    strcpy(s1.description, "Safety check and DOT-4 fluid flush");
    strcpy(s1.due_date, "2026-10-01"); /* In the past relative to 2026-10-27 */
    s1.reminder_days_before = 7;
    s1.recurring_interval_days = 365;

    StatusCode sc = schedule_create(&s1, 1);
    TEST_ASSERT(sc == STATUS_OK && s1.id > 0, "Created past due date schedule");

    /* Schedule 2: Upcoming Date Service (due in 4 days) */
    MaintenanceSchedule s2 = {0};
    s2.vehicle_id = v.id;
    strcpy(s2.service_name, "Air Filter & AC Filter Replacement");
    char today[16];
    get_current_iso_date(today, sizeof(today));
    add_days_to_date(today, 4, s2.due_date, sizeof(s2.due_date));
    s2.reminder_days_before = 7;

    sc = schedule_create(&s2, 1);
    TEST_ASSERT(sc == STATUS_OK && s2.id > 0, "Created upcoming date schedule (due in 4 days, window 7)");

    /* Schedule 3: Odometer Threshold Service (vehicle is at 27,000, due at 27,200: within 500 km window!) */
    MaintenanceSchedule s3 = {0};
    s3.vehicle_id = v.id;
    strcpy(s3.service_name, "Tire Rotation & Wheel Alignment");
    s3.due_odometer_km = 27200;
    s3.reminder_days_before = 7;
    s3.recurring_interval_km = 10000;

    sc = schedule_create(&s3, 1);
    TEST_ASSERT(sc == STATUS_OK && s3.id > 0, "Created odometer-based schedule within 500km threshold");

    /* Run Automated Schedule & Reminder Evaluation */
    int updated_cnt = 0, notif_cnt = 0;
    sc = schedule_evaluate_all(&updated_cnt, &notif_cnt);
    TEST_ASSERT(sc == STATUS_OK, "Evaluated all schedules successfully");
    TEST_ASSERT(updated_cnt >= 2, "Updated schedule statuses based on date and odometer");
    TEST_ASSERT(notif_cnt >= 2, "Generated in-app notifications for overdue and upcoming services");

    /* Verify s1 is marked 'Overdue' */
    MaintenanceSchedule check1;
    schedule_get_by_id(s1.id, &check1);
    TEST_ASSERT(strcmp(check1.status, "Overdue") == 0, "Schedule 1 status updated to 'Overdue'");

    /* Verify s2 is marked 'Due' */
    MaintenanceSchedule check2;
    schedule_get_by_id(s2.id, &check2);
    TEST_ASSERT(strcmp(check2.status, "Due") == 0, "Schedule 2 status updated to 'Due'");

    /* Verify Deduplication: Running evaluation again should NOT duplicate notifications */
    int updated2 = 0, notif2 = 0;
    schedule_evaluate_all(&updated2, &notif2);
    TEST_ASSERT(notif2 == 0, "Notification deduplication prevents duplicate alerts for same schedule & window");

    /* Complete Schedule and verify automatic recurring schedule generation */
    int next_sched_id = 0;
    sc = schedule_complete(s3.id, 1, 1, &next_sched_id);
    TEST_ASSERT(sc == STATUS_OK, "Marked Schedule 3 as completed");
    TEST_ASSERT(next_sched_id > 0, "Automatically generated next recurring schedule");

    MaintenanceSchedule recurring;
    schedule_get_by_id(next_sched_id, &recurring);
    TEST_ASSERT(recurring.due_odometer_km == (v.odometer_km + 10000), "Next recurring schedule odometer set to current + interval (37,000 km)");

    /* Reschedule Schedule 1 */
    char future_date[16];
    add_days_to_date(today, 30, future_date, sizeof(future_date));
    sc = schedule_reschedule(s1.id, future_date, 0, 1);
    TEST_ASSERT(sc == STATUS_OK, "Rescheduled Schedule 1 to future date");

    schedule_get_by_id(s1.id, &check1);
    TEST_ASSERT(strcmp(check1.status, "Scheduled") == 0, "Schedule 1 status reset to 'Scheduled'");
}

/* ==================== 5. EXPENSE TRACKING TESTS ==================== */
static void run_expense_tests(void) {
    printf("\n=== 5. Running Expense Tracking & Financial Analytics Tests ===\n");

    Vehicle v;
    vehicle_get_by_reg("MH-01-AB-1001", &v);

    /* Add additional non-maintenance expenses */
    Expense e1 = {0};
    e1.vehicle_id = v.id;
    strcpy(e1.category, "Fuel");
    strcpy(e1.description, "Diesel Tank Full (50 Litres @ Rs. 92/L)");
    e1.amount = 460000; /* Rs. 4,600.00 */
    strcpy(e1.expense_date, "2026-10-18");
    strcpy(e1.vendor, "Indian Oil Retail");

    StatusCode sc = expense_create(&e1, 1);
    TEST_ASSERT(sc == STATUS_OK && e1.id > 0, "Recorded Fuel Expense");

    Expense e2 = {0};
    e2.vehicle_id = v.id;
    strcpy(e2.category, "Toll/Tax");
    strcpy(e2.description, "FASTag Highway Toll Recharge");
    e2.amount = 150000; /* Rs. 1,500.00 */
    strcpy(e2.expense_date, "2026-10-20");
    strcpy(e2.vendor, "NHAI FASTag");

    sc = expense_create(&e2, 1);
    TEST_ASSERT(sc == STATUS_OK && e2.id > 0, "Recorded Toll Expense");

    /* Category Summaries */
    CategoryExpenseSummary *cats = NULL;
    int cat_cnt = 0;
    sc = expense_get_category_summary(&cats, &cat_cnt);
    TEST_ASSERT(sc == STATUS_OK && cat_cnt >= 3, "Category summary aggregates expenses by category");
    free(cats);

    /* Vehicle-wise Summaries */
    VehicleCostSummary *v_costs = NULL;
    int vc_cnt = 0;
    sc = expense_get_vehicle_summaries(&v_costs, &vc_cnt);
    TEST_ASSERT(sc == STATUS_OK && vc_cnt >= 1, "Generated vehicle-wise cost summaries");
    TEST_ASSERT(v_costs[0].maintenance_cost > 0, "Maintenance cost accurately distinguished");
    TEST_ASSERT(v_costs[0].other_cost == (460000 + 150000), "Other operating expenses correctly summed");
    TEST_ASSERT(v_costs[0].total_cost == (v_costs[0].maintenance_cost + v_costs[0].other_cost), "Total cost matches sum");
    free(v_costs);

    /* Overall Stats */
    OverallExpenseStats stats;
    sc = expense_get_overall_stats(&stats);
    TEST_ASSERT(sc == STATUS_OK, "Computed overall fleet expense statistics");
    TEST_ASSERT(stats.total_maintenance_spending > 0, "Maintenance spending non-zero");
    TEST_ASSERT(stats.total_all_expenses > stats.total_maintenance_spending, "Total spending includes maintenance + operational costs");
    TEST_ASSERT(stats.total_active_vehicles == 1, "Accurately counted active vehicles");
}

/* ==================== 6. CSV & PDF REPORTS TESTS ==================== */
static void run_report_tests(void) {
    printf("\n=== 6. Running CSV & PDF Report Generation Tests ===\n");

    /* CSV Formula Injection Protection Test */
    char escaped[64];
    report_escape_csv_field("=cmd|' /C calc'!A0", escaped, sizeof(escaped));
    TEST_ASSERT(escaped[0] == '"' && escaped[1] == '\'', "Formula injection payload prefixed with single quote and quoted");

    report_escape_csv_field("Standard Text", escaped, sizeof(escaped));
    TEST_ASSERT(strcmp(escaped, "Standard Text") == 0, "Safe text does not require quotes or prefixes");

    report_escape_csv_field("Text, with comma", escaped, sizeof(escaped));
    TEST_ASSERT(strcmp(escaped, "\"Text, with comma\"") == 0, "Text with comma quoted properly");

    /* CSV Vehicle Register Report */
    StatusCode sc = report_csv_vehicle_register("reports/test_vehicles.csv");
    TEST_ASSERT(sc == STATUS_OK, "Generated Vehicle Register CSV report");
    FILE *fp = fopen("reports/test_vehicles.csv", "r");
    TEST_ASSERT(fp != NULL, "Vehicle Register CSV file exists and is readable");
    if (fp) fclose(fp);

    /* CSV Maintenance History Report */
    sc = report_csv_maintenance_history("reports/test_maintenance.csv", 0);
    TEST_ASSERT(sc == STATUS_OK, "Generated Maintenance History CSV report");
    fp = fopen("reports/test_maintenance.csv", "r");
    TEST_ASSERT(fp != NULL, "Maintenance History CSV file exists and is readable");
    if (fp) fclose(fp);

    /* CSV Schedules Report */
    sc = report_csv_schedules("reports/test_schedules.csv");
    TEST_ASSERT(sc == STATUS_OK, "Generated Schedules CSV report");
    fp = fopen("reports/test_schedules.csv", "r");
    TEST_ASSERT(fp != NULL, "Schedules CSV file exists and is readable");
    if (fp) fclose(fp);

    /* CSV Expenses Report */
    sc = report_csv_expenses("reports/test_expenses.csv");
    TEST_ASSERT(sc == STATUS_OK, "Generated Expenses CSV report");
    fp = fopen("reports/test_expenses.csv", "r");
    TEST_ASSERT(fp != NULL, "Expenses CSV file exists and is readable");
    if (fp) fclose(fp);

    /* CSV Monthly Expenditure Report */
    sc = report_csv_monthly_spending("reports/test_monthly.csv");
    TEST_ASSERT(sc == STATUS_OK, "Generated Monthly Expenditure CSV report");
    fp = fopen("reports/test_monthly.csv", "r");
    TEST_ASSERT(fp != NULL, "Monthly Expenditure CSV exists and is readable");
    if (fp) fclose(fp);

    /* CSV Vehicle Costs Report */
    sc = report_csv_vehicle_costs("reports/test_vehicle_costs.csv");
    TEST_ASSERT(sc == STATUS_OK, "Generated Vehicle Costs CSV report");
    fp = fopen("reports/test_vehicle_costs.csv", "r");
    TEST_ASSERT(fp != NULL, "Vehicle Costs CSV exists and is readable");
    if (fp) fclose(fp);

    /* PDF Fleet Executive Summary Report */
    sc = report_pdf_fleet_summary("reports/test_fleet_summary.pdf");
    TEST_ASSERT(sc == STATUS_OK, "Generated Fleet Executive Summary PDF");
    fp = fopen("reports/test_fleet_summary.pdf", "rb");
    TEST_ASSERT(fp != NULL, "PDF file exists and is readable");
    if (fp) {
        char header[16] = {0};
        fread(header, 1, 8, fp);
        TEST_ASSERT(memcmp(header, "%PDF-1.4", 8) == 0, "PDF file has genuine %PDF-1.4 magic header");
        fclose(fp);
    }

    /* PDF Vehicle Maintenance History Report */
    sc = report_pdf_vehicle_maintenance(1, "reports/test_vehicle_1_maint.pdf");
    TEST_ASSERT(sc == STATUS_OK, "Generated Vehicle Maintenance History PDF");
    fp = fopen("reports/test_vehicle_1_maint.pdf", "rb");
    TEST_ASSERT(fp != NULL, "Vehicle Maintenance History PDF file exists and is readable");
    if (fp) fclose(fp);

    /* PDF Expense Summary Report */
    sc = report_pdf_expense_summary("reports/test_expense_summary.pdf");
    TEST_ASSERT(sc == STATUS_OK, "Generated Expense Summary PDF");
    fp = fopen("reports/test_expense_summary.pdf", "rb");
    TEST_ASSERT(fp != NULL, "Expense Summary PDF file exists and is readable");
    if (fp) fclose(fp);
}

/* ==================== 7. BACKUP, RESTORE & AUDIT TESTS ==================== */
static void run_backup_and_audit_tests(void) {
    printf("\n=== 7. Running Backup, Restore & Audit Trail Tests ===\n");

    /* Create Online Backup */
    char backup_path[MAX_BUFFER_LG];
    StatusCode sc = backup_create("backups", backup_path, sizeof(backup_path));
    TEST_ASSERT(sc == STATUS_OK, "Created online hot backup using SQLite Backup API");
    FILE *fp = fopen(backup_path, "rb");
    TEST_ASSERT(fp != NULL, "Backup database file exists on disk");
    if (fp) fclose(fp);

    /* Audit Log Verification */
    AuditLog *logs = NULL;
    int log_cnt = 0;
    sc = audit_list(20, &logs, &log_cnt);
    TEST_ASSERT(sc == STATUS_OK && log_cnt > 0, "Audit logs recorded and queried");
    TEST_ASSERT(strcmp(logs[0].action, "BACKUP_CREATE") == 0 || strcmp(logs[0].action, "REPORT_PDF") == 0,
                "Latest audit event correctly recorded");
    audit_free_list(logs);

    /* Database Integrity Check */
    char check_res[64] = {0};
    sc = db_check_integrity(check_res, sizeof(check_res));
    TEST_ASSERT(sc == STATUS_OK && strcmp(check_res, "ok") == 0, "Database passes PRAGMA integrity_check");

    /* Restore from Backup */
    sc = backup_restore(backup_path);
    TEST_ASSERT(sc == STATUS_OK, "Restored database from backup file with pre-restore safety snapshot");

    /* Verify live database integrity after restore */
    sc = db_check_integrity(check_res, sizeof(check_res));
    TEST_ASSERT(sc == STATUS_OK && strcmp(check_res, "ok") == 0, "Database integrity verified after restore");

    /* Verify data persisted */
    Vehicle v;
    sc = vehicle_get_by_reg("MH-01-AB-1001", &v);
    TEST_ASSERT(sc == STATUS_OK, "Vehicle data intact after backup restoration");
}

/* ==================== MAIN TEST ENTRY POINT ==================== */
int main(void) {
    printf("=================================================================\n");
    printf(" FLEET VEHICLE MAINTENANCE RECORD SYSTEM - AUTOMATED TEST SUITE\n");
    printf("=================================================================\n");

    setup_test_db();

    run_auth_tests();
    run_vehicle_tests();
    run_maintenance_tests();
    run_schedule_tests();
    run_expense_tests();
    run_report_tests();
    run_backup_and_audit_tests();

    teardown_test_db();

    printf("\n=================================================================\n");
    printf(" TEST RESULTS SUMMARY: %d / %d Tests Passed (%d%%)\n",
           g_tests_passed, g_tests_run, (g_tests_run > 0 ? (g_tests_passed * 100 / g_tests_run) : 0));
    printf("=================================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
