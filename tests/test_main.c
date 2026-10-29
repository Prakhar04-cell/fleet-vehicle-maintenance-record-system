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
#include "dsa.h"
#include "work_order.h"
#include "inspection.h"

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
    TEST_ASSERT(auth_has_permission(ROLE_ADMIN, PERM_VEHICLE_DELETE) == true, "Admin has PERM_VEHICLE_DELETE");
    TEST_ASSERT(auth_has_permission(ROLE_ADMIN, PERM_AUDIT_VIEW) == true, "Admin has PERM_AUDIT_VIEW");
    
    TEST_ASSERT(auth_has_permission(ROLE_MANAGER, PERM_USER_MANAGE) == false, "Manager does not have PERM_USER_MANAGE");
    TEST_ASSERT(auth_has_permission(ROLE_MANAGER, PERM_BACKUP_RESTORE) == false, "Manager does not have PERM_BACKUP_RESTORE");
    TEST_ASSERT(auth_has_permission(ROLE_MANAGER, PERM_VEHICLE_DELETE) == false, "Manager does not have PERM_VEHICLE_DELETE");
    TEST_ASSERT(auth_has_permission(ROLE_MANAGER, PERM_AUDIT_VIEW) == false, "Manager does not have PERM_AUDIT_VIEW");
    TEST_ASSERT(auth_has_permission(ROLE_MANAGER, PERM_VEHICLE_WRITE) == true, "Manager has PERM_VEHICLE_WRITE");
    TEST_ASSERT(auth_has_permission(ROLE_MANAGER, PERM_DRIVER_WRITE) == true, "Manager has PERM_DRIVER_WRITE");
    TEST_ASSERT(auth_has_permission(ROLE_MANAGER, PERM_EXPENSE_WRITE) == true, "Manager has PERM_EXPENSE_WRITE");
    TEST_ASSERT(auth_has_permission(ROLE_MANAGER, PERM_REPORTS) == true, "Manager has PERM_REPORTS");

    TEST_ASSERT(auth_has_permission(ROLE_STAFF, PERM_VEHICLE_WRITE) == false, "Staff does not have PERM_VEHICLE_WRITE");
    TEST_ASSERT(auth_has_permission(ROLE_STAFF, PERM_VEHICLE_DELETE) == false, "Staff does not have PERM_VEHICLE_DELETE");
    TEST_ASSERT(auth_has_permission(ROLE_STAFF, PERM_DRIVER_WRITE) == false, "Staff does not have PERM_DRIVER_WRITE");
    TEST_ASSERT(auth_has_permission(ROLE_STAFF, PERM_EXPENSE_WRITE) == false, "Staff does not have PERM_EXPENSE_WRITE");
    TEST_ASSERT(auth_has_permission(ROLE_STAFF, PERM_REPORTS) == false, "Staff does not have PERM_REPORTS");
    TEST_ASSERT(auth_has_permission(ROLE_STAFF, PERM_BACKUP_RESTORE) == false, "Staff does not have PERM_BACKUP_RESTORE");
    TEST_ASSERT(auth_has_permission(ROLE_STAFF, PERM_AUDIT_VIEW) == false, "Staff does not have PERM_AUDIT_VIEW");
    TEST_ASSERT(auth_has_permission(ROLE_STAFF, PERM_MAINTENANCE_WRITE) == true, "Staff has PERM_MAINTENANCE_WRITE");
    TEST_ASSERT(auth_has_permission(ROLE_STAFF, PERM_SCHEDULE_WRITE) == true, "Staff has PERM_SCHEDULE_WRITE");
    TEST_ASSERT(auth_has_permission(ROLE_STAFF, PERM_READ) == true, "Staff has PERM_READ");

    TEST_ASSERT(auth_has_permission(ROLE_VIEWER, PERM_MAINTENANCE_WRITE) == false, "Viewer cannot write maintenance");
    TEST_ASSERT(auth_has_permission(ROLE_VIEWER, PERM_READ) == true, "Viewer has PERM_READ");

    /* HTTP Cryptographic Session Token Lifecycle */
    char admin_token[AUTH_SESSION_TOKEN_LEN] = {0};
    sc = auth_session_create(&u, admin_token, sizeof(admin_token));
    TEST_ASSERT(sc == STATUS_OK, "Successfully generated 64-hex char cryptographic session token");
    TEST_ASSERT(strlen(admin_token) == 64, "Session token is exactly 64 characters (256-bit hex)");

    User validated_user = {0};
    sc = auth_session_validate(admin_token, &validated_user);
    TEST_ASSERT(sc == STATUS_OK, "Cryptographic session token validates successfully");
    TEST_ASSERT(validated_user.id == u.id, "Validated session user ID matches");
    TEST_ASSERT(validated_user.role == ROLE_ADMIN, "Validated session user has ROLE_ADMIN");

    /* Invalid and corrupt tokens */
    sc = auth_session_validate("invalid_or_truncated_token", &validated_user);
    TEST_ASSERT(sc == STATUS_AUTH_FAILED, "Malformed session token is rejected");
    sc = auth_session_validate(NULL, &validated_user);
    TEST_ASSERT(sc == STATUS_AUTH_FAILED, "NULL session token is rejected");

    /* Session destruction on logout */
    sc = auth_session_destroy(admin_token);
    TEST_ASSERT(sc == STATUS_OK, "Session destroyed successfully on logout");
    sc = auth_session_validate(admin_token, &validated_user);
    TEST_ASSERT(sc == STATUS_AUTH_FAILED, "Destroyed session token is immediately rejected");

    /* Live account deactivation session check */
    User staff_user = {0};
    auth_login("staff_user", "StaffPass123", &staff_user);
    char staff_token[AUTH_SESSION_TOKEN_LEN] = {0};
    auth_session_create(&staff_user, staff_token, sizeof(staff_token));
    TEST_ASSERT(auth_session_validate(staff_token, &validated_user) == STATUS_OK, "Staff session valid before deactivation");
    
    /* Deactivate staff account */
    auth_set_user_status(u.id, staff_user.id, false);
    TEST_ASSERT(auth_session_validate(staff_token, &validated_user) == STATUS_AUTH_FAILED, "Session validation rejects deactivated account");
    auth_set_user_status(u.id, staff_user.id, true); /* Reactivate */

    /* Programmatic User Retrieval */
    User *all_users = NULL;
    int user_cnt = 0;
    sc = auth_get_users(&all_users, &user_cnt);
    TEST_ASSERT(sc == STATUS_OK && user_cnt >= 4, "auth_get_users returns all active and configured accounts");
    auth_free_users(all_users);
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

    /* Driver Search */
    Driver *found_drivers = NULL;
    int f_cnt = 0;
    sc = driver_search("Rajesh", &found_drivers, &f_cnt);
    TEST_ASSERT(sc == STATUS_OK && f_cnt >= 1, "driver_search finds driver by name substring");
    driver_free_list(found_drivers);

    /* Create Driver 3 for deletion & unassign tests */
    Driver d3 = {0};
    strcpy(d3.name, "Vikram Singh");
    strcpy(d3.phone, "+91-9988776655");
    strcpy(d3.license_number, "DL-03-2022-55555");
    strcpy(d3.license_expiry, "2029-01-01");
    d3.active = true;
    int driver_id3 = 0;
    driver_create(&d3, &driver_id3);

    /* Assign Driver 3 to temporary Vehicle 3 */
    Vehicle v3 = {0};
    strcpy(v3.registration_number, "MH-03-EF-3003");
    strcpy(v3.make, "Ashok Leyland");
    strcpy(v3.model, "Dost+");
    v3.manufacturing_year = 2021;
    strcpy(v3.vin, "AL1612003C3003");
    strcpy(v3.fuel_type, "Diesel");
    v3.fuel_percentage = 90;
    v3.odometer_km = 12000;
    strcpy(v3.status, "Available");
    v3.assigned_driver_id = driver_id3;
    int veh_id3 = 0;
    sc = vehicle_create(&v3, &veh_id3);
    TEST_ASSERT(sc == STATUS_OK && veh_id3 > 0, "Created temporary Vehicle 3 assigned to Driver 3");

    /* Attempt to delete Driver 3 while assigned to an active vehicle -> Must fail with FORBIDDEN */
    sc = driver_delete(driver_id3, 1);
    TEST_ASSERT(sc == STATUS_FORBIDDEN, "Prevent deleting driver actively assigned to an active vehicle");

    /* Deactivate Driver 3 -> Automatically unassigns Driver 3 from Vehicle 3 */
    sc = driver_set_active(driver_id3, false);
    TEST_ASSERT(sc == STATUS_OK, "Deactivated Driver 3");

    Vehicle v3_check;
    vehicle_get_by_id(veh_id3, &v3_check);
    TEST_ASSERT(v3_check.assigned_driver_id == 0, "Deactivating driver automatically clears vehicle assignment");

    /* Now delete unassigned Driver 3 -> Must succeed */
    sc = driver_delete(driver_id3, 1);
    TEST_ASSERT(sc == STATUS_OK, "Successfully deleted unassigned driver");

    /* Clean Vehicle Deletion: Vehicle 3 has no maintenance/expense/schedule records -> delete should succeed */
    sc = vehicle_delete(veh_id3, 1);
    TEST_ASSERT(sc == STATUS_OK, "Clean vehicle deletion succeeds when no linked records exist");

    /* Verify Vehicle 3 no longer exists */
    sc = vehicle_get_by_id(veh_id3, &v3_check);
    TEST_ASSERT(sc == STATUS_NOT_FOUND, "Deleted vehicle is no longer in database");
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

    /* Prevent deleting vehicle with dependent maintenance/expense records */
    sc = vehicle_delete(v.id, 1);
    TEST_ASSERT(sc == STATUS_FORBIDDEN, "Prevent deleting vehicle with active maintenance or expense records");
}

/* ==================== 8. DATA STRUCTURES & ALGORITHMS (DSA) TESTS ==================== */
static void run_dsa_tests(void) {
    printf("\n=== 8. Running Data Structures & Algorithms (C11) Tests ===\n");

    /* 1. Hash Table Tests */
    DsaHashTable *ht = dsa_ht_create(16);
    TEST_ASSERT(ht != NULL, "DSA Hash Table initialized");
    Vehicle hv1 = { .id = 101, .odometer_km = 15000 };
    strcpy(hv1.registration_number, "DL-01-AA-1111");
    strcpy(hv1.make, "Tata");
    Vehicle hv2 = { .id = 102, .odometer_km = 32000 };
    strcpy(hv2.registration_number, "MH-02-BB-2222");
    strcpy(hv2.make, "Mahindra");
    Vehicle hv3 = { .id = 103, .odometer_km = 45000 };
    strcpy(hv3.registration_number, "KA-03-CC-3333");
    strcpy(hv3.make, "Eicher");

    dsa_ht_insert(ht, &hv1);
    dsa_ht_insert(ht, &hv2);
    dsa_ht_insert(ht, &hv3);
    TEST_ASSERT(dsa_ht_size(ht) == 3, "DSA Hash Table size is 3 after insertions");

    const Vehicle *found_ht = dsa_ht_search(ht, "MH-02-BB-2222");
    TEST_ASSERT(found_ht != NULL && found_ht->id == 102, "DSA Hash Table O(1) lookup found correct vehicle");

    const Vehicle *not_found_ht = dsa_ht_search(ht, "NONEXISTENT");
    TEST_ASSERT(not_found_ht == NULL, "DSA Hash Table lookup returns NULL for absent key");

    bool removed = dsa_ht_remove(ht, "MH-02-BB-2222");
    TEST_ASSERT(removed && dsa_ht_size(ht) == 2, "DSA Hash Table successfully removed key");
    dsa_ht_destroy(ht);

    /* 2. Binary Search Tree (BST) Tests */
    DsaBSTNode *bst_root = NULL;
    bst_root = dsa_bst_insert(bst_root, &hv2); /* 32000 */
    bst_root = dsa_bst_insert(bst_root, &hv1); /* 15000 */
    bst_root = dsa_bst_insert(bst_root, &hv3); /* 45000 */

    const DsaBSTNode *bst_search_node = dsa_bst_search(bst_root, 32000);
    TEST_ASSERT(bst_search_node != NULL && strcmp(bst_search_node->vehicle.registration_number, "MH-02-BB-2222") == 0,
                "DSA BST O(log N) search finds vehicle by odometer key");

    const DsaBSTNode *bst_min_node = dsa_bst_min(bst_root);
    TEST_ASSERT(bst_min_node != NULL && bst_min_node->key == 15000, "DSA BST min key is 15,000 km");

    const DsaBSTNode *bst_max_node = dsa_bst_max(bst_root);
    TEST_ASSERT(bst_max_node != NULL && bst_max_node->key == 45000, "DSA BST max key is 45,000 km");

    Vehicle range_results[10];
    int range_cnt = 0;
    dsa_bst_range_query(bst_root, 10000, 35000, range_results, &range_cnt, 10);
    TEST_ASSERT(range_cnt == 2, "DSA BST range query [10000, 35000] returns 2 vehicles");

    bst_root = dsa_bst_delete(bst_root, 32000);
    TEST_ASSERT(dsa_bst_search(bst_root, 32000) == NULL, "DSA BST successfully deleted node");
    dsa_bst_destroy(bst_root);

    /* 3. Min-Heap (Priority Queue) Tests */
    DsaMinHeap *heap = dsa_heap_create(10);
    MaintenanceSchedule sched1 = { .id = 1 };
    strcpy(sched1.service_name, "Oil Change");
    MaintenanceSchedule sched2 = { .id = 2 };
    strcpy(sched2.service_name, "Brake Inspection");
    MaintenanceSchedule sched3 = { .id = 3 };
    strcpy(sched3.service_name, "Tire Rotation");

    dsa_heap_insert(heap, &sched1, 5);   /* due in 5 days */
    dsa_heap_insert(heap, &sched2, -3);  /* overdue by 3 days -> highest priority */
    dsa_heap_insert(heap, &sched3, 14);  /* due in 14 days */

    const DsaHeapItem *top = dsa_heap_peek(heap);
    TEST_ASSERT(top != NULL && top->priority_score == -3, "DSA Min-Heap peek returns highest priority (-3 days overdue)");

    MaintenanceSchedule ext_sched;
    int ext_prio;
    dsa_heap_extract_min(heap, &ext_sched, &ext_prio);
    TEST_ASSERT(ext_sched.id == 2 && ext_prio == -3, "DSA Min-Heap extract min pops most overdue schedule");
    dsa_heap_extract_min(heap, &ext_sched, &ext_prio);
    TEST_ASSERT(ext_sched.id == 1 && ext_prio == 5, "DSA Min-Heap second extraction is next in priority");
    dsa_heap_destroy(heap);

    /* 4. FIFO Queue Tests */
    DsaQueue *q = dsa_queue_create();
    MaintenanceRecord t1 = { .id = 501 };
    MaintenanceRecord t2 = { .id = 502 };
    dsa_queue_enqueue(q, &t1);
    dsa_queue_enqueue(q, &t2);
    TEST_ASSERT(dsa_queue_size(q) == 2, "DSA Queue size is 2");

    MaintenanceRecord dq_task;
    dsa_queue_dequeue(q, &dq_task);
    TEST_ASSERT(dq_task.id == 501, "DSA Queue maintains strict FIFO order (first in is first out)");
    dsa_queue_dequeue(q, &dq_task);
    TEST_ASSERT(dq_task.id == 502 && dsa_queue_is_empty(q), "DSA Queue is empty after 2 dequeues");
    dsa_queue_destroy(q);

    /* 5. LIFO Stack Tests */
    DsaStack *stk = dsa_stack_create(10);
    dsa_stack_push(stk, "INSERT", "VEHICLE", 1, "Added MH-01");
    dsa_stack_push(stk, "UPDATE", "VEHICLE", 1, "Odometer updated");
    TEST_ASSERT(dsa_stack_size(stk) == 2, "DSA Stack size is 2");

    DsaStackItem popped;
    dsa_stack_pop(stk, &popped);
    TEST_ASSERT(strcmp(popped.action, "UPDATE") == 0, "DSA Stack maintains LIFO order (last in is first out)");
    dsa_stack_pop(stk, &popped);
    TEST_ASSERT(strcmp(popped.action, "INSERT") == 0 && dsa_stack_is_empty(stk), "DSA Stack empty after pops");
    dsa_stack_destroy(stk);

    /* 6. Merge Sort Tests */
    Vehicle sort_arr[4];
    sort_arr[0] = (Vehicle){ .id = 1, .odometer_km = 40000 };
    sort_arr[1] = (Vehicle){ .id = 2, .odometer_km = 10000 };
    sort_arr[2] = (Vehicle){ .id = 3, .odometer_km = 25000 };
    sort_arr[3] = (Vehicle){ .id = 4, .odometer_km = 18000 };

    dsa_mergesort_vehicles(sort_arr, 0, 3, VEHICLE_SORT_ODOMETER, false); /* ascending */
    TEST_ASSERT(sort_arr[0].odometer_km == 10000 &&
                sort_arr[1].odometer_km == 18000 &&
                sort_arr[2].odometer_km == 25000 &&
                sort_arr[3].odometer_km == 40000,
                "DSA Merge Sort correctly orders vehicles by odometer ascending in O(N log N)");

    dsa_mergesort_vehicles(sort_arr, 0, 3, VEHICLE_SORT_ODOMETER, true); /* descending */
    TEST_ASSERT(sort_arr[0].odometer_km == 40000 && sort_arr[3].odometer_km == 10000,
                "DSA Merge Sort correctly orders vehicles by odometer descending");

    /* 7. Graph & Dijkstra Shortest Path Tests */
    DsaGraph *g = dsa_graph_create_sample_fleet_network();
    TEST_ASSERT(g != NULL && g->num_vertices == 6, "DSA Fleet Graph initialized with 6 nodes");
    int shortest_dist = 0;
    int path[DSA_MAX_GRAPH_VERTICES];
    int path_len = 0;
    bool has_path = dsa_graph_dijkstra(g, 0, 3, &shortest_dist, path, &path_len);
    TEST_ASSERT(has_path && shortest_dist == 26 && path_len >= 3,
                "DSA Dijkstra computes correct shortest path distance (26 km from Depot to East Cargo)");
    dsa_graph_destroy(g);

    /* 8. Dynamic Programming: 0-1 Knapsack Fleet Budget Allocation */
    DsaMaintenanceJob jobs[4] = {
        { .id = 1, .cost_units = 15, .criticality_score = 40 },
        { .id = 2, .cost_units = 10, .criticality_score = 30 },
        { .id = 3, .cost_units = 20, .criticality_score = 50 },
        { .id = 4, .cost_units = 25, .criticality_score = 70 }
    };
    int selected_idx[4];
    int sel_cnt = 0;
    int max_score = dsa_dp_knapsack_maintenance(jobs, 4, 30, selected_idx, &sel_cnt);
    TEST_ASSERT(max_score == 80 && sel_cnt == 2,
                "DSA DP Knapsack correctly optimizes fleet maintenance safety under budget");

    /* 9. Branch and Bound: Traveling Salesperson Dispatch Tour */
    int tsp_matrix[DSA_TSP_MAX][DSA_TSP_MAX] = {
        {  0, 10, 15, 20 },
        { 10,  0, 35, 25 },
        { 15, 35,  0, 30 },
        { 20, 25, 30,  0 }
    };
    int best_tour[DSA_TSP_MAX + 1];
    int min_tour_cost = dsa_tsp_branch_and_bound(tsp_matrix, 4, best_tour);
    TEST_ASSERT(min_tour_cost == 80 && best_tour[0] == 0 && best_tour[4] == 0,
                "DSA Branch & Bound TSP finds optimal inspection circuit cost (80 km)");
}

/* ==================== 9. SMART FLEET WORK ORDERS & INSPECTIONS ==================== */
static void run_smart_fleet_tests(void) {
    printf("\n=== 9. Running Smart Fleet Work Orders & Inspection Tests ===\n");

    /* 1. Create Work Order */
    WorkOrder wo = {0};
    wo.vehicle_id = 1;
    safe_strncpy(wo.title, "Transmission Flush & Filter", sizeof(wo.title));
    safe_strncpy(wo.description, "Complete synthetic transmission fluid flush", sizeof(wo.description));
    safe_strncpy(wo.priority, "High", sizeof(wo.priority));
    safe_strncpy(wo.status, "Open", sizeof(wo.status));
    wo.labour_cost = 250000; /* Rs. 2,500 */
    wo.parts_cost = 350000;  /* Rs. 3,500 */

    StatusCode sc = work_order_create(&wo, 1, "admin");
    TEST_ASSERT(sc == STATUS_OK && wo.id > 0, "Created Work Order successfully");
    TEST_ASSERT(strlen(wo.wo_number) > 5, "Work Order auto-generated unique tracking number");

    /* 2. Retrieve Work Order by ID */
    WorkOrder fetched_wo;
    sc = work_order_get_by_id(wo.id, &fetched_wo);
    TEST_ASSERT(sc == STATUS_OK && strcmp(fetched_wo.title, wo.title) == 0, "Fetched Work Order by ID");
    TEST_ASSERT(strlen(fetched_wo.vehicle_reg) > 0, "Work Order joined vehicle registration number");

    /* 3. Status Transition Validation */
    sc = work_order_update_status(wo.id, "Completed", "Trying to skip in progress", 1, "admin");
    TEST_ASSERT(sc == STATUS_INVALID_INPUT, "Reject invalid transition Open -> Completed");

    /* 4. Assign Technician */
    sc = work_order_assign(wo.id, 2, "Senior Tech Rajesh", 1, "admin");
    TEST_ASSERT(sc == STATUS_OK, "Assigned technician to Work Order");

    work_order_get_by_id(wo.id, &fetched_wo);
    TEST_ASSERT(strcmp(fetched_wo.status, "Assigned") == 0, "Work Order status advanced to 'Assigned'");
    TEST_ASSERT(strcmp(fetched_wo.assigned_to_name, "Senior Tech Rajesh") == 0, "Assigned technician name updated");

    /* 5. Advance to In Progress */
    sc = work_order_update_status(wo.id, "In Progress", "Work started on bay 3", 2, "staff");
    TEST_ASSERT(sc == STATUS_OK, "Advanced Work Order to 'In Progress'");

    /* 6. Complete Work Order */
    int out_maint_id = 0;
    sc = work_order_complete(wo.id, 28500, 250000, 350000, "Flush completed successfully", 2, "staff", &out_maint_id);
    TEST_ASSERT(sc == STATUS_OK && out_maint_id > 0, "Completed Work Order and created Maintenance Record");

    work_order_get_by_id(wo.id, &fetched_wo);
    TEST_ASSERT(strcmp(fetched_wo.status, "Completed") == 0, "Work Order status is 'Completed'");
    TEST_ASSERT(fetched_wo.maintenance_record_id == out_maint_id, "Work Order attached linked Maintenance Record ID");

    /* 7. Prevent Duplicate Completion */
    sc = work_order_complete(wo.id, 28500, 250000, 350000, "Duplicate try", 2, "staff", NULL);
    TEST_ASSERT(sc == STATUS_INVALID_INPUT, "Prevent duplicate completion on already completed Work Order");

    /* 8. Work Order History */
    WorkOrderHistory *hist = NULL;
    int hist_cnt = 0;
    sc = work_order_get_history(wo.id, &hist, &hist_cnt);
    TEST_ASSERT(sc == STATUS_OK && hist_cnt >= 4, "Work Order tracks full status transition history");
    work_order_free_history(hist);

    /* 9. Digital Vehicle Inspection */
    Inspection insp = {0};
    insp.vehicle_id = 1;
    safe_strncpy(insp.inspector_name, "Inspector Verma", sizeof(insp.inspector_name));
    insp.odometer_km = 28500;
    safe_strncpy(insp.notes, "Comprehensive 40-point depot inspection", sizeof(insp.notes));

    const char *std_items[8] = {
        "Brakes", "Tyres", "Lights", "Engine",
        "Battery", "Steering", "Fluids", "Registration & Insurance Documents"
    };
    InspectionItem items[8];
    memset(items, 0, sizeof(items));
    for (int i = 0; i < 8; i++) {
        safe_strncpy(items[i].item_name, std_items[i], sizeof(items[i].item_name));
        if (i == 0) {
            safe_strncpy(items[i].status, "Fail", sizeof(items[i].status));
            safe_strncpy(items[i].severity, "Critical Defect", sizeof(items[i].severity));
            safe_strncpy(items[i].notes, "Front right brake pad worn down to metal", sizeof(items[i].notes));
        } else if (i == 1) {
            safe_strncpy(items[i].status, "Fail", sizeof(items[i].status));
            safe_strncpy(items[i].severity, "Minor Defect", sizeof(items[i].severity));
            safe_strncpy(items[i].notes, "Tyre pressure low by 4 PSI", sizeof(items[i].notes));
        } else {
            safe_strncpy(items[i].status, "Pass", sizeof(items[i].status));
            safe_strncpy(items[i].severity, "Normal", sizeof(items[i].severity));
        }
    }

    sc = inspection_create(&insp, items, 8, 1);
    TEST_ASSERT(sc == STATUS_OK && insp.id > 0, "Created Digital Vehicle Inspection with 8 checklist items");
    TEST_ASSERT(insp.defect_count == 2, "Inspection accurately counted 2 defects");
    TEST_ASSERT(strcmp(insp.overall_status, "Fail") == 0, "Critical defect correctly escalated overall status to 'Fail'");

    /* 10. Inspection Retrieval & Detail */
    Inspection fetched_insp;
    InspectionItem *fetched_items = NULL;
    int f_count = 0;
    sc = inspection_get_by_id(insp.id, &fetched_insp, &fetched_items, &f_count);
    TEST_ASSERT(sc == STATUS_OK && f_count == 8, "Retrieved inspection details with all 8 checklist items");
    int failed_item_id = (f_count > 0) ? fetched_items[0].id : 0;
    inspection_free_items(fetched_items);

    /* 11. Create Work Order from Defect */
    int repair_wo_id = 0;
    sc = inspection_create_work_order_from_defect(failed_item_id, 1, "admin", &repair_wo_id);
    TEST_ASSERT(sc == STATUS_OK && repair_wo_id > 0, "Generated Repair Work Order directly from inspection defect");

    WorkOrder repair_wo;
    work_order_get_by_id(repair_wo_id, &repair_wo);
    TEST_ASSERT(strcmp(repair_wo.priority, "Critical") == 0, "Repair Work Order inherited Critical priority from inspection item");
    TEST_ASSERT(repair_wo.inspection_id == insp.id, "Repair Work Order linked to originating inspection ID");

    /* 12. Query Defects */
    InspectionItem *defects = NULL;
    int def_count = 0;
    sc = inspection_get_defects(1, &defects, &def_count);
    TEST_ASSERT(sc == STATUS_OK && def_count >= 2, "Query vehicle defects returned all recorded failure items");
    inspection_free_items(defects);
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
    run_dsa_tests();
    run_smart_fleet_tests();

    teardown_test_db();

    printf("\n=================================================================\n");
    printf(" TEST RESULTS SUMMARY: %d / %d Tests Passed (%d%%)\n",
           g_tests_passed, g_tests_run, (g_tests_run > 0 ? (g_tests_passed * 100 / g_tests_run) : 0));
    printf("=================================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
