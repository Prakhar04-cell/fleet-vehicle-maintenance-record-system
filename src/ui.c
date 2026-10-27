#include "ui.h"
#include "input.h"
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
#include "database.h"

void ui_init(void) {
    /* Initialize console or formatting if needed */
}

void ui_clear_screen(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void ui_print_divider(char ch, int length) {
    for (int i = 0; i < length; i++) {
        putchar(ch);
    }
    putchar('\n');
}

void ui_print_banner(void) {
    printf("\n");
    ui_print_divider('=', 78);
    printf("       %s  (v%s)\n", APP_NAME, APP_VERSION);
    printf("   Enterprise Fleet Operations, Preventive Maintenance & Financial Tracking\n");
    ui_print_divider('=', 78);

    const User *u = auth_get_session_user();
    if (u) {
        printf(" User: %-18s | Role: %-10s | Status: ACTIVE\n", u->username, auth_role_to_string(u->role));
        ui_print_divider('-', 78);
    }
}

void ui_print_header(const char *subtitle) {
    printf("\n--- [ %s ] ---\n\n", subtitle);
}

void ui_print_success(const char *msg) {
    printf("\n[SUCCESS] %s\n", msg);
}

void ui_print_error(const char *msg) {
    printf("\n[ERROR] %s\n", msg);
}

void ui_print_warning(const char *msg) {
    printf("\n[WARNING] %s\n", msg);
}

void ui_print_info(const char *msg) {
    printf("\n[INFO] %s\n", msg);
}

/* ==================== DASHBOARD ==================== */
void ui_menu_dashboard(void) {
    ui_clear_screen();
    ui_print_banner();
    ui_print_header("OPERATIONAL FLEET DASHBOARD");

    DashboardStats stats;
    dashboard_get_stats(&stats);

    printf("+-----------------------------------+-----------------------------------+\n");
    printf("| FLEET ASSETS & AVAILABILITY       | MAINTENANCE & REMINDER STATUS     |\n");
    printf("+-----------------------------------+-----------------------------------+\n");
    printf("| Total Active Vehicles : %-9d | Upcoming Services Due : %-9d |\n", stats.total_active_vehicles, stats.upcoming_services_count);
    printf("| Available for Trips   : %-9d | OVERDUE Services      : %-9d |\n", stats.available_vehicles, stats.overdue_services_count);
    printf("| Currently On Trips    : %-9d | Active In-App Alerts  : %-9d |\n", stats.on_trip_vehicles, stats.active_notifications_count);
    printf("| Under Maintenance     : %-9d | Low-Fuel Vehicles     : %-9d |\n", stats.under_maintenance_vehicles, stats.low_fuel_vehicles_count);
    printf("| Out of Service        : %-9d |                                   |\n", stats.out_of_service_vehicles);
    printf("+-----------------------------------+-----------------------------------+\n");

    char total_spend[32], cur_month_spend[32];
    format_currency(stats.total_maintenance_spending, total_spend, sizeof(total_spend));
    format_currency(stats.current_month_spending, cur_month_spend, sizeof(cur_month_spend));

    printf("| FINANCIAL SUMMARY                                                     |\n");
    printf("| Total Maintenance Spend : %-43s |\n", total_spend);
    printf("| Current Month Spend     : %-43s |\n", cur_month_spend);
    printf("+-----------------------------------------------------------------------+\n");

    /* Urgent Schedules Table */
    MaintenanceSchedule *schedules = NULL;
    int s_cnt = 0;
    dashboard_get_urgent_schedules(5, &schedules, &s_cnt);
    if (s_cnt > 0) {
        printf("\nUrgent & Due Service Reminders:\n");
        printf("+------+---------------+----------------------+------------+------------+----------+\n");
        printf("| ID   | Reg Number    | Service Name         | Due Date   | Due KM     | Status   |\n");
        printf("+------+---------------+----------------------+------------+------------+----------+\n");
        for (int i = 0; i < s_cnt; i++) {
            printf("| %-4d | %-13s | %-20s | %-10s | %-10d | %-8s |\n",
                   schedules[i].id, schedules[i].vehicle_reg, schedules[i].service_name,
                   schedules[i].due_date[0] ? schedules[i].due_date : "N/A",
                   schedules[i].due_odometer_km, schedules[i].status);
        }
        printf("+------+---------------+----------------------+------------+------------+----------+\n");
        schedule_free_list(schedules);
    }

    /* Low Fuel Alerts */
    if (stats.low_fuel_vehicles_count > 0) {
        Vehicle *low_fuels = NULL;
        int lf_cnt = 0;
        dashboard_get_low_fuel_vehicles(&low_fuels, &lf_cnt);
        if (lf_cnt > 0) {
            printf("\n[ALERT] Vehicles With Low Fuel (< 20%%):\n");
            for (int i = 0; i < lf_cnt; i++) {
                printf("  ! %s (%s %s) - Fuel: %d%% (%s)\n",
                       low_fuels[i].registration_number, low_fuels[i].make, low_fuels[i].model,
                       low_fuels[i].fuel_percentage, low_fuels[i].fuel_type);
            }
            vehicle_free_list(low_fuels);
        }
    }

    input_pause();
}

/* ==================== VEHICLES ==================== */
static void ui_view_vehicle_details(void) {
    char reg[MAX_BUFFER_SM];
    if (!input_get_string("Enter Vehicle Registration Number: ", reg, sizeof(reg), false)) return;

    Vehicle v;
    if (vehicle_get_by_reg(reg, &v) != STATUS_OK) {
        ui_print_error("Vehicle not found.");
        input_pause();
        return;
    }

    printf("\n======================================================\n");
    printf(" VEHICLE DETAILS: %s\n", v.registration_number);
    printf("======================================================\n");
    printf(" ID             : %d\n", v.id);
    printf(" Make & Model   : %s %s\n", v.make, v.model);
    printf(" Year / VIN     : %d | %s\n", v.manufacturing_year, v.vin);
    printf(" Fuel Type / %% : %s | %d%%\n", v.fuel_type, v.fuel_percentage);
    printf(" Odometer       : %d km\n", v.odometer_km);
    printf(" Current Status : %s\n", v.status);
    printf(" Assigned Driver: %s (Driver ID: %d)\n", v.assigned_driver_name, v.assigned_driver_id);
    printf(" Insurance Exp. : %s\n", v.insurance_expiry[0] ? v.insurance_expiry : "Not recorded");
    printf(" Notes          : %s\n", v.notes[0] ? v.notes : "None");
    printf(" Registered On  : %s\n", v.created_at);
    printf(" Archived       : %s\n", v.is_archived ? v.archived_at : "NO (Active)");
    printf("======================================================\n");

    input_pause();
}

static void ui_add_vehicle(void) {
    const User *u = auth_get_session_user();
    if (!u || !auth_has_permission(u->role, PERM_VEHICLE_WRITE)) {
        ui_print_error("Permission denied. Vehicle Write required.");
        input_pause();
        return;
    }

    ui_print_header("ADD NEW FLEET VEHICLE");
    Vehicle v = {0};

    if (!input_get_string("Registration Number (e.g. MH01AB1234): ", v.registration_number, sizeof(v.registration_number), false)) return;
    str_to_upper(v.registration_number);

    if (!input_get_string("Vehicle Make (e.g. Tata, Mahindra, BharatBenz): ", v.make, sizeof(v.make), false)) return;
    if (!input_get_string("Vehicle Model (e.g. Ace, Bolero Maxi, Prima): ", v.model, sizeof(v.model), false)) return;
    if (!input_get_int("Manufacturing Year (1990 - 2026): ", &v.manufacturing_year, 1990, 2026)) return;
    if (!input_get_string("Chassis / VIN Number: ", v.vin, sizeof(v.vin), false)) return;
    str_to_upper(v.vin);

    printf("Fuel Types: 1. Diesel | 2. Petrol | 3. CNG | 4. Electric | 5. Hybrid\n");
    int f_choice = 1;
    input_get_int("Select Fuel Type [1-5]: ", &f_choice, 1, 5);
    switch (f_choice) {
        case 2: strcpy(v.fuel_type, "Petrol"); break;
        case 3: strcpy(v.fuel_type, "CNG"); break;
        case 4: strcpy(v.fuel_type, "Electric"); break;
        case 5: strcpy(v.fuel_type, "Hybrid"); break;
        case 1:
        default: strcpy(v.fuel_type, "Diesel"); break;
    }

    input_get_int("Current Fuel Level %% [0-100]: ", &v.fuel_percentage, 0, 100);
    input_get_int("Current Odometer Reading (KM >= 0): ", &v.odometer_km, 0, 2000000);
    input_get_date("Insurance Expiry Date (YYYY-MM-DD, or leave blank): ", v.insurance_expiry, sizeof(v.insurance_expiry), true);
    input_get_string("Additional Notes (optional): ", v.notes, sizeof(v.notes), true);

    strcpy(v.status, "Available");

    int new_id = 0;
    StatusCode sc = vehicle_create(&v, &new_id);
    if (sc == STATUS_EXISTS) {
        ui_print_error("A vehicle with this registration number or VIN already exists.");
    } else if (sc == STATUS_OK) {
        char msg[MAX_BUFFER_MD];
        snprintf(msg, sizeof(msg), "Vehicle '%s' added successfully with ID %d.", v.registration_number, new_id);
        ui_print_success(msg);
    } else {
        ui_print_error("Failed to add vehicle.");
    }
    input_pause();
}

static void ui_list_vehicles(void) {
    ui_print_header("FLEET VEHICLE REGISTER");
    VehicleFilterOptions f = {0};
    f.include_archived = false;
    f.sort_field = VEHICLE_SORT_REG;

    printf("Sort by: 1. Reg Number | 2. Make/Model | 3. Fuel Level | 4. Odometer | 5. Status\n");
    int sort_choice = 1;
    input_get_int("Choose sorting [1-5]: ", &sort_choice, 1, 5);
    f.sort_field = (VehicleSortField)sort_choice;

    Vehicle *list = NULL;
    int count = 0;
    if (vehicle_list(&f, &list, &count) != STATUS_OK || count == 0) {
        ui_print_info("No active vehicles found in register.");
        input_pause();
        return;
    }

    printf("\n+-----+---------------+----------------------+------+----------+------------+--------------------+--------------------+\n");
    printf("| ID  | Reg Number    | Make & Model         | Year | Fuel %%   | Odometer   | Status             | Driver             |\n");
    printf("+-----+---------------+----------------------+------+----------+------------+--------------------+--------------------+\n");

    for (int i = 0; i < count; i++) {
        char mm[64];
        snprintf(mm, sizeof(mm), "%s %s", list[i].make, list[i].model);
        printf("| %-3d | %-13s | %-20s | %-4d | %-6d%%  | %-8d km | %-18s | %-18s |\n",
               list[i].id, list[i].registration_number, mm, list[i].manufacturing_year,
               list[i].fuel_percentage, list[i].odometer_km, list[i].status, list[i].assigned_driver_name);
    }
    printf("+-----+---------------+----------------------+------+----------+------------+--------------------+--------------------+\n");
    printf("Total vehicles: %d\n", count);

    vehicle_free_list(list);
    input_pause();
}

static void ui_update_vehicle_odometer(void) {
    const User *u = auth_get_session_user();
    if (!u || !auth_has_permission(u->role, PERM_VEHICLE_WRITE)) {
        ui_print_error("Permission denied.");
        input_pause();
        return;
    }

    int id = 0;
    if (!input_get_int("Enter Vehicle ID: ", &id, 1, 100000)) return;

    Vehicle v;
    if (vehicle_get_by_id(id, &v) != STATUS_OK) {
        ui_print_error("Vehicle not found.");
        input_pause();
        return;
    }

    printf("Vehicle: %s | Current Odometer: %d km\n", v.registration_number, v.odometer_km);
    int new_odom = 0;
    if (!input_get_int("Enter New Odometer Reading (KM): ", &new_odom, 0, 2000000)) return;

    char reason[MAX_BUFFER_LG] = {0};
    if (new_odom < v.odometer_km) {
        ui_print_warning("The new reading is LOWER than current odometer reading.");
        if (!input_get_string("Audited Correction Reason (REQUIRED): ", reason, sizeof(reason), false)) return;
    }

    StatusCode sc = vehicle_update_odometer(id, new_odom, reason, u->id);
    if (sc == STATUS_OK) {
        ui_print_success("Odometer updated successfully.");
    } else {
        ui_print_error("Failed to update odometer.");
    }
    input_pause();
}

static void ui_assign_driver(void) {
    const User *u = auth_get_session_user();
    if (!u || !auth_has_permission(u->role, PERM_VEHICLE_WRITE)) {
        ui_print_error("Permission denied.");
        input_pause();
        return;
    }

    int v_id = 0, d_id = 0;
    if (!input_get_int("Enter Vehicle ID: ", &v_id, 1, 100000)) return;
    if (!input_get_int("Enter Driver ID (or 0 to unassign): ", &d_id, 0, 100000)) return;

    StatusCode sc;
    if (d_id == 0) {
        sc = vehicle_unassign_driver(v_id, u->id);
    } else {
        sc = vehicle_assign_driver(v_id, d_id, u->id);
    }

    if (sc == STATUS_OK) {
        ui_print_success("Driver assignment updated successfully.");
    } else if (sc == STATUS_FORBIDDEN) {
        ui_print_error("Driver is already assigned to another active vehicle.");
    } else {
        ui_print_error("Failed to update driver assignment.");
    }
    input_pause();
}

void ui_menu_vehicles(void) {
    while (1) {
        ui_clear_screen();
        ui_print_banner();
        ui_print_header("VEHICLE ASSET MANAGEMENT");
        printf(" 1. View All Fleet Vehicles\n");
        printf(" 2. Search Vehicles (by Reg No, Make, Model, VIN)\n");
        printf(" 3. View Full Vehicle Details\n");
        printf(" 4. Add New Vehicle\n");
        printf(" 5. Update Vehicle Odometer Reading\n");
        printf(" 6. Update Vehicle Fuel Level\n");
        printf(" 7. Update Operational Status (Available, On Trip, Maintenance...)\n");
        printf(" 8. Assign / Unassign Driver\n");
        printf(" 9. Archive (Decommission) Vehicle\n");
        printf(" 10. Restore Archived Vehicle\n");
        printf(" 0. Return to Main Menu\n");
        ui_print_divider('-', 50);

        int choice = -1;
        if (!input_get_int("Enter Choice [0-10]: ", &choice, 0, 10)) continue;
        if (choice == 0) break;

        const User *u = auth_get_session_user();
        switch (choice) {
            case 1: ui_list_vehicles(); break;
            case 2: {
                char query[MAX_BUFFER_MD];
                if (input_get_string("Search Query: ", query, sizeof(query), false)) {
                    Vehicle *res = NULL;
                    int cnt = 0;
                    if (vehicle_search(query, &res, &cnt) == STATUS_OK && cnt > 0) {
                        printf("\nFound %d matching vehicle(s):\n", cnt);
                        for (int i = 0; i < cnt; i++) {
                            printf(" - ID: %d | %s | %s %s | %d km | Status: %s\n",
                                   res[i].id, res[i].registration_number, res[i].make, res[i].model,
                                   res[i].odometer_km, res[i].status);
                        }
                        vehicle_free_list(res);
                    } else {
                        ui_print_info("No matching vehicles found.");
                    }
                    input_pause();
                }
                break;
            }
            case 3: ui_view_vehicle_details(); break;
            case 4: ui_add_vehicle(); break;
            case 5: ui_update_vehicle_odometer(); break;
            case 6: {
                if (!u || !auth_has_permission(u->role, PERM_VEHICLE_WRITE)) {
                    ui_print_error("Permission denied."); input_pause(); break;
                }
                int vid = 0, fuel = 0;
                input_get_int("Enter Vehicle ID: ", &vid, 1, 100000);
                input_get_int("New Fuel Level %% [0-100]: ", &fuel, 0, 100);
                if (vehicle_update_fuel(vid, fuel, u->id) == STATUS_OK) {
                    ui_print_success("Fuel level updated.");
                } else {
                    ui_print_error("Failed to update fuel level.");
                }
                input_pause();
                break;
            }
            case 7: {
                if (!u || !auth_has_permission(u->role, PERM_VEHICLE_WRITE)) {
                    ui_print_error("Permission denied."); input_pause(); break;
                }
                int vid = 0;
                input_get_int("Enter Vehicle ID: ", &vid, 1, 100000);
                printf("Status: 1. Available | 2. On Trip | 3. Under Maintenance | 4. Out of Service | 5. Decommissioned\n");
                int s_ch = 1;
                input_get_int("Select Status [1-5]: ", &s_ch, 1, 5);
                const char *st_str = "Available";
                if (s_ch == 2) st_str = "On Trip";
                else if (s_ch == 3) st_str = "Under Maintenance";
                else if (s_ch == 4) st_str = "Out of Service";
                else if (s_ch == 5) st_str = "Decommissioned";
                if (vehicle_update_status(vid, st_str, u->id) == STATUS_OK) {
                    ui_print_success("Vehicle status updated.");
                } else {
                    ui_print_error("Failed to update status.");
                }
                input_pause();
                break;
            }
            case 8: ui_assign_driver(); break;
            case 9: {
                if (!u || !auth_has_permission(u->role, PERM_VEHICLE_WRITE)) {
                    ui_print_error("Permission denied."); input_pause(); break;
                }
                int vid = 0;
                input_get_int("Enter Vehicle ID to Archive: ", &vid, 1, 100000);
                if (input_get_confirm("Are you sure you want to archive this vehicle?", false)) {
                    if (vehicle_archive(vid, u->id) == STATUS_OK) {
                        ui_print_success("Vehicle archived. Historical records preserved.");
                    } else {
                        ui_print_error("Failed to archive vehicle.");
                    }
                }
                input_pause();
                break;
            }
            case 10: {
                if (!u || !auth_has_permission(u->role, PERM_VEHICLE_WRITE)) {
                    ui_print_error("Permission denied."); input_pause(); break;
                }
                int vid = 0;
                input_get_int("Enter Vehicle ID to Restore: ", &vid, 1, 100000);
                if (vehicle_restore(vid, u->id) == STATUS_OK) {
                    ui_print_success("Vehicle restored to active register.");
                } else {
                    ui_print_error("Failed to restore vehicle.");
                }
                input_pause();
                break;
            }
        }
    }
}

/* ==================== DRIVERS ==================== */
void ui_menu_drivers(void) {
    while (1) {
        ui_clear_screen();
        ui_print_banner();
        ui_print_header("FLEET DRIVERS MANAGEMENT");
        printf(" 1. View All Registered Drivers\n");
        printf(" 2. Add New Driver\n");
        printf(" 3. Update Driver Contact / License Info\n");
        printf(" 4. Activate / Deactivate Driver\n");
        printf(" 0. Return to Main Menu\n");
        ui_print_divider('-', 50);

        int choice = -1;
        if (!input_get_int("Enter Choice [0-4]: ", &choice, 0, 4)) continue;
        if (choice == 0) break;

        switch (choice) {
            case 1: {
                Driver *list = NULL;
                int count = 0;
                if (driver_list(&list, &count, false) == STATUS_OK && count > 0) {
                    printf("\n+-----+----------------------+----------------+--------------------+------------+----------+\n");
                    printf("| ID  | Driver Name          | Phone Number   | License Number     | Expiry     | Status   |\n");
                    printf("+-----+----------------------+----------------+--------------------+------------+----------+\n");
                    for (int i = 0; i < count; i++) {
                        printf("| %-3d | %-20s | %-14s | %-18s | %-10s | %-8s |\n",
                               list[i].id, list[i].name, list[i].phone, list[i].license_number,
                               list[i].license_expiry, list[i].active ? "ACTIVE" : "INACTIVE");
                    }
                    printf("+-----+----------------------+----------------+--------------------+------------+----------+\n");
                    printf("Total drivers: %d\n", count);
                    driver_free_list(list);
                } else {
                    ui_print_info("No drivers registered.");
                }
                input_pause();
                break;
            }
            case 2: {
                const User *u = auth_get_session_user();
                if (!u || !auth_has_permission(u->role, PERM_VEHICLE_WRITE)) {
                    ui_print_error("Permission denied."); input_pause(); break;
                }
                Driver d = {0};
                if (!input_get_string("Driver Full Name: ", d.name, sizeof(d.name), false)) break;
                if (!input_get_string("Phone Number: ", d.phone, sizeof(d.phone), false)) break;
                if (!input_get_string("Commercial License Number: ", d.license_number, sizeof(d.license_number), false)) break;
                str_to_upper(d.license_number);
                if (!input_get_date("License Expiry Date (YYYY-MM-DD): ", d.license_expiry, sizeof(d.license_expiry), false)) break;
                d.active = true;

                int new_id = 0;
                StatusCode sc = driver_create(&d, &new_id);
                if (sc == STATUS_EXISTS) {
                    ui_print_error("Driver with this license number already exists.");
                } else if (sc == STATUS_OK) {
                    char msg[MAX_BUFFER_MD];
                    snprintf(msg, sizeof(msg), "Driver '%s' created successfully (ID %d).", d.name, new_id);
                    ui_print_success(msg);
                } else {
                    ui_print_error("Failed to add driver.");
                }
                input_pause();
                break;
            }
            case 3: {
                const User *u = auth_get_session_user();
                if (!u || !auth_has_permission(u->role, PERM_VEHICLE_WRITE)) {
                    ui_print_error("Permission denied."); input_pause(); break;
                }
                int id = 0;
                input_get_int("Enter Driver ID: ", &id, 1, 100000);
                Driver d;
                if (driver_get_by_id(id, &d) != STATUS_OK) {
                    ui_print_error("Driver not found."); input_pause(); break;
                }
                printf("Editing Driver: %s (License: %s)\n", d.name, d.license_number);
                input_get_string("New Name (or press Enter to keep current): ", d.name, sizeof(d.name), true);
                input_get_string("New Phone: ", d.phone, sizeof(d.phone), true);
                input_get_date("New License Expiry (YYYY-MM-DD): ", d.license_expiry, sizeof(d.license_expiry), true);

                if (driver_update(&d) == STATUS_OK) {
                    ui_print_success("Driver updated successfully.");
                } else {
                    ui_print_error("Failed to update driver.");
                }
                input_pause();
                break;
            }
            case 4: {
                const User *u = auth_get_session_user();
                if (!u || !auth_has_permission(u->role, PERM_VEHICLE_WRITE)) {
                    ui_print_error("Permission denied."); input_pause(); break;
                }
                int id = 0;
                input_get_int("Enter Driver ID: ", &id, 1, 100000);
                bool act = input_get_confirm("Set status to ACTIVE?", true);
                if (driver_set_active(id, act) == STATUS_OK) {
                    ui_print_success("Driver status changed.");
                } else {
                    ui_print_error("Failed to change driver status.");
                }
                input_pause();
                break;
            }
        }
    }
}

/* ==================== MAINTENANCE ==================== */
static void ui_log_maintenance_record(void) {
    const User *u = auth_get_session_user();
    if (!u || !auth_has_permission(u->role, PERM_MAINTENANCE_WRITE)) {
        ui_print_error("Permission denied. Maintenance Write required.");
        input_pause();
        return;
    }

    ui_print_header("LOG SERVICE / MAINTENANCE RECORD");
    MaintenanceRecord r = {0};

    char reg[MAX_BUFFER_SM];
    if (!input_get_string("Enter Vehicle Registration Number: ", reg, sizeof(reg), false)) return;

    Vehicle v;
    if (vehicle_get_by_reg(reg, &v) != STATUS_OK) {
        ui_print_error("Vehicle not found.");
        input_pause();
        return;
    }
    r.vehicle_id = v.id;
    safe_strncpy(r.vehicle_reg, v.registration_number, sizeof(r.vehicle_reg));

    printf("Logging service for: %s (Current Odometer: %d km)\n\n", v.registration_number, v.odometer_km);

    printf("Service Types: 1. Preventive | 2. Routine Inspection | 3. Oil & Filter Change | 4. Brake Service | 5. Tire Service | 6. Repair / Breakdown | 7. Major Overhaul\n");
    int st_choice = 1;
    input_get_int("Select Service Type [1-7]: ", &st_choice, 1, 7);
    switch (st_choice) {
        case 2: strcpy(r.service_type, "Routine Inspection"); break;
        case 3: strcpy(r.service_type, "Oil & Filter Change"); break;
        case 4: strcpy(r.service_type, "Brake Service"); break;
        case 5: strcpy(r.service_type, "Tire Service"); break;
        case 6: strcpy(r.service_type, "Repair"); break;
        case 7: strcpy(r.service_type, "Major Overhaul"); break;
        case 1:
        default: strcpy(r.service_type, "Preventive"); break;
    }

    char today[16];
    get_current_iso_date(today, sizeof(today));
    char date_prompt[64];
    snprintf(date_prompt, sizeof(date_prompt), "Service Date (YYYY-MM-DD, default %s): ", today);
    if (!input_get_date(date_prompt, r.service_date, sizeof(r.service_date), true)) return;
    if (r.service_date[0] == '\0') strcpy(r.service_date, today);

    if (!input_get_int("Odometer at Service (KM): ", &r.odometer_km, v.odometer_km, 2000000)) return;
    if (!input_get_string("Workshop / Garage Name: ", r.workshop_name, sizeof(r.workshop_name), false)) return;
    input_get_string("Mechanic Name (optional): ", r.mechanic_name, sizeof(r.mechanic_name), true);
    if (!input_get_string("Description of Work Performed: ", r.description, sizeof(r.description), false)) return;

    /* Financial components (minor currency units) */
    input_get_money("Labour Cost: ", &r.labour_cost, 0, 100000000);
    input_get_money("Parts & Materials Cost: ", &r.parts_cost, 0, 100000000);
    input_get_money("Tax Amount (GST / VAT): ", &r.tax_amount, 0, 50000000);
    r.total_cost = r.labour_cost + r.parts_cost + r.tax_amount;

    char tot_buf[32];
    format_currency(r.total_cost, tot_buf, sizeof(tot_buf));
    printf("\nComputed Total Cost: %s\n", tot_buf);

    input_get_date("Next Recommended Service Date (YYYY-MM-DD, optional): ", r.next_service_date, sizeof(r.next_service_date), true);
    input_get_int("Next Recommended Service Odometer (KM, or 0 if none): ", &r.next_service_odometer_km, 0, 2000000);
    input_get_string("Invoice / Document Path (optional): ", r.invoice_doc_path, sizeof(r.invoice_doc_path), true);

    bool link_expense = input_get_confirm("Automatically create matching maintenance expense entry?", true);

    StatusCode sc = maintenance_create(&r, link_expense, u->id);
    if (sc == STATUS_OK) {
        char msg[MAX_BUFFER_MD];
        snprintf(msg, sizeof(msg), "Maintenance Record #%d recorded successfully for %s.", r.id, v.registration_number);
        ui_print_success(msg);
    } else {
        ui_print_error("Failed to record maintenance service.");
    }
    input_pause();
}

static void ui_view_maintenance_history(void) {
    char reg[MAX_BUFFER_SM];
    if (!input_get_string("Enter Vehicle Registration Number (or leave empty for all): ", reg, sizeof(reg), true)) return;

    MaintenanceFilterOptions f = {0};
    if (reg[0] != '\0') {
        Vehicle v;
        if (vehicle_get_by_reg(reg, &v) != STATUS_OK) {
            ui_print_error("Vehicle not found.");
            input_pause();
            return;
        }
        f.vehicle_id = v.id;
    }

    MaintenanceRecord *records = NULL;
    int count = 0;
    if (maintenance_list(&f, &records, &count) != STATUS_OK || count == 0) {
        ui_print_info("No maintenance records found.");
        input_pause();
        return;
    }

    printf("\n+-----+---------------+------------+--------------------+------------+--------------------+--------------------+\n");
    printf("| ID  | Reg Number    | Date       | Service Type       | Odometer   | Workshop           | Total Cost         |\n");
    printf("+-----+---------------+------------+--------------------+------------+--------------------+--------------------+\n");

    int64_t sum = 0;
    for (int i = 0; i < count; i++) {
        sum += records[i].total_cost;
        char c_buf[32];
        format_currency(records[i].total_cost, c_buf, sizeof(c_buf));
        printf("| %-3d | %-13s | %-10s | %-18s | %-8d km| %-18s | %-18s |\n",
               records[i].id, records[i].vehicle_reg, records[i].service_date,
               records[i].service_type, records[i].odometer_km, records[i].workshop_name, c_buf);
    }
    printf("+-----+---------------+------------+--------------------+------------+--------------------+--------------------+\n");
    char sum_buf[32];
    format_currency(sum, sum_buf, sizeof(sum_buf));
    printf("Total records: %d | Combined expenditure: %s\n", count, sum_buf);

    maintenance_free_list(records);
    input_pause();
}

void ui_menu_maintenance(void) {
    while (1) {
        ui_clear_screen();
        ui_print_banner();
        ui_print_header("MAINTENANCE & REPAIR RECORDS");
        printf(" 1. Record New Maintenance / Service\n");
        printf(" 2. View Vehicle Maintenance History\n");
        printf(" 3. Search Maintenance Records (by Vehicle, Workshop, Description)\n");
        printf(" 4. View Maintenance Record Details\n");
        printf(" 5. Archive Maintenance Record\n");
        printf(" 0. Return to Main Menu\n");
        ui_print_divider('-', 50);

        int choice = -1;
        if (!input_get_int("Enter Choice [0-5]: ", &choice, 0, 5)) continue;
        if (choice == 0) break;

        switch (choice) {
            case 1: ui_log_maintenance_record(); break;
            case 2: ui_view_maintenance_history(); break;
            case 3: {
                char q[MAX_BUFFER_MD];
                if (input_get_string("Search Query: ", q, sizeof(q), false)) {
                    MaintenanceRecord *res = NULL;
                    int cnt = 0;
                    if (maintenance_search(q, &res, &cnt) == STATUS_OK && cnt > 0) {
                        printf("\nFound %d record(s):\n", cnt);
                        for (int i = 0; i < cnt; i++) {
                            char c_buf[32];
                            format_currency(res[i].total_cost, c_buf, sizeof(c_buf));
                            printf(" - ID %d | %s | %s | %s | %s | %s\n",
                                   res[i].id, res[i].vehicle_reg, res[i].service_date,
                                   res[i].service_type, res[i].workshop_name, c_buf);
                        }
                        maintenance_free_list(res);
                    } else {
                        ui_print_info("No records matched your query.");
                    }
                    input_pause();
                }
                break;
            }
            case 4: {
                int id = 0;
                input_get_int("Enter Maintenance Record ID: ", &id, 1, 100000);
                MaintenanceRecord r;
                if (maintenance_get_by_id(id, &r) == STATUS_OK) {
                    char lab[32], prt[32], tax[32], tot[32];
                    format_currency(r.labour_cost, lab, sizeof(lab));
                    format_currency(r.parts_cost, prt, sizeof(prt));
                    format_currency(r.tax_amount, tax, sizeof(tax));
                    format_currency(r.total_cost, tot, sizeof(tot));

                    printf("\n======================================================\n");
                    printf(" MAINTENANCE RECORD #%d\n", r.id);
                    printf("======================================================\n");
                    printf(" Vehicle Reg     : %s\n", r.vehicle_reg);
                    printf(" Service Type    : %s\n", r.service_type);
                    printf(" Service Date    : %s\n", r.service_date);
                    printf(" Odometer (KM)   : %d km\n", r.odometer_km);
                    printf(" Workshop Name   : %s\n", r.workshop_name);
                    printf(" Mechanic Name   : %s\n", r.mechanic_name[0] ? r.mechanic_name : "N/A");
                    printf(" Description     : %s\n", r.description);
                    printf(" Labour Cost     : %s\n", lab);
                    printf(" Parts Cost      : %s\n", prt);
                    printf(" Tax Amount      : %s\n", tax);
                    printf(" TOTAL COST      : %s\n", tot);
                    printf(" Next Service    : %s | %d km\n", r.next_service_date[0] ? r.next_service_date : "N/A", r.next_service_odometer_km);
                    printf(" Invoice Doc     : %s\n", r.invoice_doc_path[0] ? r.invoice_doc_path : "None");
                    printf(" Recorded By     : %s on %s\n", r.created_by_username, r.created_at);
                    printf("======================================================\n");
                } else {
                    ui_print_error("Maintenance record not found.");
                }
                input_pause();
                break;
            }
            case 5: {
                const User *u = auth_get_session_user();
                if (!u || !auth_has_permission(u->role, PERM_MAINTENANCE_WRITE)) {
                    ui_print_error("Permission denied."); input_pause(); break;
                }
                int id = 0;
                input_get_int("Enter Record ID to Archive: ", &id, 1, 100000);
                if (input_get_confirm("Are you sure you want to archive this maintenance record?", false)) {
                    if (maintenance_archive(id, u->id) == STATUS_OK) {
                        ui_print_success("Record archived.");
                    } else {
                        ui_print_error("Failed to archive record.");
                    }
                }
                input_pause();
                break;
            }
        }
    }
}

/* ==================== SCHEDULES & REMINDERS ==================== */
static void ui_add_schedule(void) {
    const User *u = auth_get_session_user();
    if (!u || !auth_has_permission(u->role, PERM_SCHEDULE_WRITE)) {
        ui_print_error("Permission denied. Schedule Write required.");
        input_pause();
        return;
    }

    ui_print_header("CREATE PREVENTIVE MAINTENANCE SCHEDULE");
    char reg[MAX_BUFFER_SM];
    if (!input_get_string("Vehicle Registration Number: ", reg, sizeof(reg), false)) return;

    Vehicle v;
    if (vehicle_get_by_reg(reg, &v) != STATUS_OK) {
        ui_print_error("Vehicle not found.");
        input_pause();
        return;
    }

    MaintenanceSchedule s = {0};
    s.vehicle_id = v.id;
    safe_strncpy(s.vehicle_reg, v.registration_number, sizeof(s.vehicle_reg));

    printf("Scheduling service for %s (Current Odometer: %d km)\n\n", v.registration_number, v.odometer_km);
    if (!input_get_string("Service Name (e.g. 10,000 KM Engine Oil & Filter): ", s.service_name, sizeof(s.service_name), false)) return;
    input_get_string("Description (optional): ", s.description, sizeof(s.description), true);
    input_get_date("Due Date (YYYY-MM-DD, or leave blank if purely KM-based): ", s.due_date, sizeof(s.due_date), true);
    input_get_int("Due Odometer Reading (KM, or 0 if purely date-based): ", &s.due_odometer_km, 0, 2000000);
    input_get_int("Reminder Window (Days Before Due Date, default 7): ", &s.reminder_days_before, 1, 90);

    printf("\nRecurring Maintenance Configuration (optional):\n");
    input_get_int("Recurring Interval in Days (e.g. 180 for 6 months, or 0 if one-time): ", &s.recurring_interval_days, 0, 1000);
    input_get_int("Recurring Interval in KM (e.g. 10000, or 0 if one-time): ", &s.recurring_interval_km, 0, 100000);

    StatusCode sc = schedule_create(&s, u->id);
    if (sc == STATUS_OK) {
        char msg[MAX_BUFFER_MD];
        snprintf(msg, sizeof(msg), "Schedule #%d created successfully for vehicle %s.", s.id, v.registration_number);
        ui_print_success(msg);
    } else {
        ui_print_error("Failed to create maintenance schedule.");
    }
    input_pause();
}

static void ui_list_schedules(void) {
    ui_print_header("PREVENTIVE MAINTENANCE SCHEDULES & REMINDERS");
    ScheduleFilterOptions f = {0};
    f.active_only = true;

    MaintenanceSchedule *list = NULL;
    int count = 0;
    if (schedule_list(&f, &list, &count) != STATUS_OK || count == 0) {
        ui_print_info("No active maintenance schedules found.");
        input_pause();
        return;
    }

    printf("\n+-----+---------------+----------------------+------------+------------+----------+--------------+--------------+\n");
    printf("| ID  | Reg Number    | Service Name         | Due Date   | Due KM     | Status   | Days Left    | KM Left      |\n");
    printf("+-----+---------------+----------------------+------------+------------+----------+--------------+--------------+\n");

    for (int i = 0; i < count; i++) {
        char days_str[32], km_str[32];
        if (list[i].days_remaining != 99999) {
            if (list[i].days_remaining < 0) {
                snprintf(days_str, sizeof(days_str), "%d OVERDUE", -list[i].days_remaining);
            } else {
                snprintf(days_str, sizeof(days_str), "%d days", list[i].days_remaining);
            }
        } else {
            strcpy(days_str, "N/A");
        }

        if (list[i].km_remaining != 999999) {
            if (list[i].km_remaining < 0) {
                snprintf(km_str, sizeof(km_str), "%d OVERDUE", -list[i].km_remaining);
            } else {
                snprintf(km_str, sizeof(km_str), "%d km", list[i].km_remaining);
            }
        } else {
            strcpy(km_str, "N/A");
        }

        printf("| %-3d | %-13s | %-20s | %-10s | %-10d | %-8s | %-12s | %-12s |\n",
               list[i].id, list[i].vehicle_reg, list[i].service_name,
               list[i].due_date[0] ? list[i].due_date : "N/A",
               list[i].due_odometer_km, list[i].status, days_str, km_str);
    }
    printf("+-----+---------------+----------------------+------------+------------+----------+--------------+--------------+\n");
    printf("Total active schedules: %d\n", count);

    schedule_free_list(list);
    input_pause();
}

void ui_menu_schedules(void) {
    while (1) {
        ui_clear_screen();
        ui_print_banner();
        ui_print_header("PREVENTIVE MAINTENANCE SCHEDULING");
        printf(" 1. View Upcoming & Overdue Maintenance Schedules\n");
        printf(" 2. Schedule New Preventive Maintenance\n");
        printf(" 3. Run Automated Reminder & Threshold Evaluation\n");
        printf(" 4. Reschedule Maintenance Service\n");
        printf(" 5. Complete Maintenance Schedule (Spawn Next Recurring Service)\n");
        printf(" 6. Cancel Maintenance Schedule\n");
        printf(" 0. Return to Main Menu\n");
        ui_print_divider('-', 50);

        int choice = -1;
        if (!input_get_int("Enter Choice [0-6]: ", &choice, 0, 6)) continue;
        if (choice == 0) break;

        const User *u = auth_get_session_user();
        switch (choice) {
            case 1: ui_list_schedules(); break;
            case 2: ui_add_schedule(); break;
            case 3: {
                int updated = 0, notifs = 0;
                printf("[SYSTEM] Evaluating maintenance schedules against calendar dates and odometer thresholds...\n");
                schedule_evaluate_all(&updated, &notifs);
                char res_msg[MAX_BUFFER_MD];
                snprintf(res_msg, sizeof(res_msg), "Evaluated all schedules: %d status(es) updated, %d new in-app alert(s) generated.", updated, notifs);
                ui_print_success(res_msg);
                input_pause();
                break;
            }
            case 4: {
                if (!u || !auth_has_permission(u->role, PERM_SCHEDULE_WRITE)) {
                    ui_print_error("Permission denied."); input_pause(); break;
                }
                int id = 0;
                input_get_int("Enter Schedule ID: ", &id, 1, 100000);
                char new_d[MAX_BUFFER_SM];
                int new_km = 0;
                input_get_date("New Due Date (YYYY-MM-DD): ", new_d, sizeof(new_d), true);
                input_get_int("New Due Odometer (KM, or 0 if none): ", &new_km, 0, 2000000);
                if (schedule_reschedule(id, new_d, new_km, u->id) == STATUS_OK) {
                    ui_print_success("Service rescheduled.");
                } else {
                    ui_print_error("Failed to reschedule.");
                }
                input_pause();
                break;
            }
            case 5: {
                if (!u || !auth_has_permission(u->role, PERM_SCHEDULE_WRITE)) {
                    ui_print_error("Permission denied."); input_pause(); break;
                }
                int id = 0;
                input_get_int("Enter Schedule ID to Complete: ", &id, 1, 100000);
                int rec_id = 0;
                input_get_int("Enter Associated Maintenance Record ID (or 0 if none): ", &rec_id, 0, 100000);

                int next_id = 0;
                if (schedule_complete(id, rec_id, u->id, &next_id) == STATUS_OK) {
                    if (next_id > 0) {
                        char msg[MAX_BUFFER_MD];
                        snprintf(msg, sizeof(msg), "Schedule marked completed. Automatically spawned next recurring schedule #%d!", next_id);
                        ui_print_success(msg);
                    } else {
                        ui_print_success("Schedule marked completed.");
                    }
                } else {
                    ui_print_error("Failed to complete schedule.");
                }
                input_pause();
                break;
            }
            case 6: {
                if (!u || !auth_has_permission(u->role, PERM_SCHEDULE_WRITE)) {
                    ui_print_error("Permission denied."); input_pause(); break;
                }
                int id = 0;
                input_get_int("Enter Schedule ID to Cancel: ", &id, 1, 100000);
                char reason[MAX_BUFFER_LG];
                input_get_string("Reason for Cancellation: ", reason, sizeof(reason), false);
                if (schedule_cancel(id, reason, u->id) == STATUS_OK) {
                    ui_print_success("Schedule cancelled.");
                } else {
                    ui_print_error("Failed to cancel schedule.");
                }
                input_pause();
                break;
            }
        }
    }
}

/* ==================== EXPENSES ==================== */
void ui_menu_expenses(void) {
    while (1) {
        ui_clear_screen();
        ui_print_banner();
        ui_print_header("FLEET EXPENSE & COST TRACKING");
        printf(" 1. View All Expenses\n");
        printf(" 2. Record New Expense\n");
        printf(" 3. View Category-wise Totals\n");
        printf(" 4. View Monthly Expenditure Trends\n");
        printf(" 5. View Vehicle-wise Cost Summaries\n");
        printf(" 6. Delete Expense Record\n");
        printf(" 0. Return to Main Menu\n");
        ui_print_divider('-', 50);

        int choice = -1;
        if (!input_get_int("Enter Choice [0-6]: ", &choice, 0, 6)) continue;
        if (choice == 0) break;

        const User *u = auth_get_session_user();
        switch (choice) {
            case 1: {
                ExpenseFilterOptions f = {0};
                Expense *list = NULL;
                int count = 0;
                if (expense_list(&f, &list, &count) == STATUS_OK && count > 0) {
                    printf("\n+-----+---------------+----------------+------------+--------------------+--------------------+--------------------+\n");
                    printf("| ID  | Reg Number    | Category       | Date       | Amount             | Vendor             | Description        |\n");
                    printf("+-----+---------------+----------------+------------+--------------------+--------------------+--------------------+\n");

                    int64_t total = 0;
                    for (int i = 0; i < count; i++) {
                        total += list[i].amount;
                        char c_buf[32];
                        format_currency(list[i].amount, c_buf, sizeof(c_buf));
                        printf("| %-3d | %-13s | %-14s | %-10s | %-18s | %-18s | %-18s |\n",
                               list[i].id, list[i].vehicle_reg, list[i].category,
                               list[i].expense_date, c_buf, list[i].vendor, list[i].description);
                    }
                    printf("+-----+---------------+----------------+------------+--------------------+--------------------+--------------------+\n");
                    char tot_buf[32];
                    format_currency(total, tot_buf, sizeof(tot_buf));
                    printf("Total records: %d | Total expense: %s\n", count, tot_buf);
                    expense_free_list(list);
                } else {
                    ui_print_info("No expenses recorded.");
                }
                input_pause();
                break;
            }
            case 2: {
                if (!u || !auth_has_permission(u->role, PERM_EXPENSE_WRITE)) {
                    ui_print_error("Permission denied. Expense Write required."); input_pause(); break;
                }
                char reg[MAX_BUFFER_SM];
                if (!input_get_string("Vehicle Registration Number: ", reg, sizeof(reg), false)) break;
                Vehicle v;
                if (vehicle_get_by_reg(reg, &v) != STATUS_OK) {
                    ui_print_error("Vehicle not found."); input_pause(); break;
                }

                Expense e = {0};
                e.vehicle_id = v.id;
                safe_strncpy(e.vehicle_reg, v.registration_number, sizeof(e.vehicle_reg));

                printf("Category: 1. Fuel | 2. Maintenance | 3. Insurance | 4. Toll/Tax | 5. Permit | 6. Tires | 7. Parts | 8. Miscellaneous\n");
                int cat_ch = 1;
                input_get_int("Select Category [1-8]: ", &cat_ch, 1, 8);
                switch (cat_ch) {
                    case 1: strcpy(e.category, "Fuel"); break;
                    case 2: strcpy(e.category, "Maintenance"); break;
                    case 3: strcpy(e.category, "Insurance"); break;
                    case 4: strcpy(e.category, "Toll/Tax"); break;
                    case 5: strcpy(e.category, "Permit"); break;
                    case 6: strcpy(e.category, "Tires"); break;
                    case 7: strcpy(e.category, "Parts"); break;
                    case 8:
                    default: strcpy(e.category, "Miscellaneous"); break;
                }

                if (!input_get_string("Description: ", e.description, sizeof(e.description), false)) break;
                if (!input_get_money("Amount: ", &e.amount, 0, 100000000)) break;

                char today[16];
                get_current_iso_date(today, sizeof(today));
                char d_prompt[64];
                snprintf(d_prompt, sizeof(d_prompt), "Expense Date (YYYY-MM-DD, default %s): ", today);
                input_get_date(d_prompt, e.expense_date, sizeof(e.expense_date), true);
                if (e.expense_date[0] == '\0') strcpy(e.expense_date, today);

                input_get_string("Vendor / Payee: ", e.vendor, sizeof(e.vendor), true);

                if (expense_create(&e, u->id) == STATUS_OK) {
                    ui_print_success("Expense recorded successfully.");
                } else {
                    ui_print_error("Failed to record expense.");
                }
                input_pause();
                break;
            }
            case 3: {
                CategoryExpenseSummary *cats = NULL;
                int count = 0;
                if (expense_get_category_summary(&cats, &count) == STATUS_OK && count > 0) {
                    printf("\nExpense Breakdown by Category:\n");
                    printf("+----------------------+------------+--------------------+\n");
                    printf("| Category             | Records    | Total Amount       |\n");
                    printf("+----------------------+------------+--------------------+\n");
                    for (int i = 0; i < count; i++) {
                        char c_buf[32];
                        format_currency(cats[i].total_amount, c_buf, sizeof(c_buf));
                        printf("| %-20s | %-10d | %-18s |\n", cats[i].category, cats[i].count, c_buf);
                    }
                    printf("+----------------------+------------+--------------------+\n");
                    free(cats);
                } else {
                    ui_print_info("No expenses recorded yet.");
                }
                input_pause();
                break;
            }
            case 4: {
                MonthlyExpenseSummary *months = NULL;
                int count = 0;
                if (expense_get_monthly_summary(&months, &count) == STATUS_OK && count > 0) {
                    printf("\nMonthly Expenditure Breakdown (Last 12 Months):\n");
                    printf("+-----------------+------------+--------------------+\n");
                    printf("| Month (YYYY-MM) | Records    | Total Amount       |\n");
                    printf("+-----------------+------------+--------------------+\n");
                    for (int i = 0; i < count; i++) {
                        char c_buf[32];
                        format_currency(months[i].total_amount, c_buf, sizeof(c_buf));
                        printf("| %-15s | %-10d | %-18s |\n", months[i].year_month, months[i].count, c_buf);
                    }
                    printf("+-----------------+------------+--------------------+\n");
                    free(months);
                } else {
                    ui_print_info("No expense data available.");
                }
                input_pause();
                break;
            }
            case 5: {
                VehicleCostSummary *v_costs = NULL;
                int count = 0;
                if (expense_get_vehicle_summaries(&v_costs, &count) == STATUS_OK && count > 0) {
                    printf("\nVehicle-wise Maintenance & Operating Costs:\n");
                    printf("+-----+---------------+----------------------+--------------------+--------------------+--------------------+\n");
                    printf("| ID  | Reg Number    | Make & Model         | Maintenance Cost   | Other Expenses     | Total Cost         |\n");
                    printf("+-----+---------------+----------------------+--------------------+--------------------+--------------------+\n");
                    for (int i = 0; i < count; i++) {
                        char m_buf[32], o_buf[32], t_buf[32];
                        format_currency(v_costs[i].maintenance_cost, m_buf, sizeof(m_buf));
                        format_currency(v_costs[i].other_cost, o_buf, sizeof(o_buf));
                        format_currency(v_costs[i].total_cost, t_buf, sizeof(t_buf));
                        printf("| %-3d | %-13s | %-20s | %-18s | %-18s | %-18s |\n",
                               v_costs[i].vehicle_id, v_costs[i].registration_number,
                               v_costs[i].make_model, m_buf, o_buf, t_buf);
                    }
                    printf("+-----+---------------+----------------------+--------------------+--------------------+--------------------+\n");
                    free(v_costs);
                } else {
                    ui_print_info("No vehicle cost data available.");
                }
                input_pause();
                break;
            }
            case 6: {
                if (!u || !auth_has_permission(u->role, PERM_EXPENSE_WRITE)) {
                    ui_print_error("Permission denied."); input_pause(); break;
                }
                int id = 0;
                input_get_int("Enter Expense ID to Delete: ", &id, 1, 100000);
                if (input_get_confirm("Are you sure you want to delete this expense?", false)) {
                    if (expense_delete(id, u->id) == STATUS_OK) {
                        ui_print_success("Expense deleted.");
                    } else {
                        ui_print_error("Failed to delete expense.");
                    }
                }
                input_pause();
                break;
            }
        }
    }
}

/* ==================== NOTIFICATIONS ==================== */
void ui_menu_notifications(void) {
    while (1) {
        ui_clear_screen();
        ui_print_banner();
        ui_print_header("IN-APP MAINTENANCE ALERTS & NOTIFICATIONS");

        Notification *list = NULL;
        int count = 0;
        notification_list_active(&list, &count);

        if (count == 0) {
            ui_print_info("No active maintenance alerts at this time.");
            printf("\nOptions: 1. Run Schedule Evaluation | 0. Return\n");
            int c = 0;
            input_get_int("Choice: ", &c, 0, 1);
            if (c == 1) {
                int upd = 0, n_cnt = 0;
                schedule_evaluate_all(&upd, &n_cnt);
                continue;
            }
            break;
        }

        printf("+-----+---------------+----------------------+----------------------+----------------------+---------------------+\n");
        printf("| ID  | Reg Number    | Service Name         | Alert Type           | Scheduled Date       | Sent At             |\n");
        printf("+-----+---------------+----------------------+----------------------+----------------------+---------------------+\n");

        for (int i = 0; i < count; i++) {
            printf("| %-3d | %-13s | %-20s | %-20s | %-20s | %-19s |\n",
                   list[i].id, list[i].vehicle_reg, list[i].service_name,
                   list[i].notification_type, list[i].scheduled_at, list[i].sent_at);
        }
        printf("+-----+---------------+----------------------+----------------------+----------------------+---------------------+\n");
        printf("Active alerts: %d\n\n", count);
        notification_free_list(list);

        printf("Options:\n");
        printf(" 1. Dismiss Specific Alert by ID\n");
        printf(" 2. Dismiss ALL Active Alerts\n");
        printf(" 3. Re-evaluate Schedules\n");
        printf(" 0. Return to Main Menu\n");
        ui_print_divider('-', 50);

        int opt = 0;
        if (!input_get_int("Enter Option [0-3]: ", &opt, 0, 3)) break;
        if (opt == 0) break;

        const User *u = auth_get_session_user();
        int uid = u ? u->id : 0;

        if (opt == 1) {
            int nid = 0;
            input_get_int("Enter Notification ID to dismiss: ", &nid, 1, 100000);
            notification_dismiss(nid, uid);
            ui_print_success("Alert dismissed.");
            input_pause();
        } else if (opt == 2) {
            notification_dismiss_all(uid);
            ui_print_success("All active alerts dismissed.");
            input_pause();
        } else if (opt == 3) {
            int upd = 0, n_cnt = 0;
            schedule_evaluate_all(&upd, &n_cnt);
        }
    }
}

/* ==================== REPORTS ==================== */
void ui_menu_reports(void) {
    const User *u = auth_get_session_user();
    if (!u || !auth_has_permission(u->role, PERM_REPORTS)) {
        ui_print_error("Permission denied. Reports permission required.");
        input_pause();
        return;
    }

    while (1) {
        ui_clear_screen();
        ui_print_banner();
        ui_print_header("REPORTS & ANALYTICS EXPORT");
        printf(" CSV EXPORTS (RFC 4180 with Formula Injection Protection):\n");
        printf("  1. Export Vehicle Register CSV\n");
        printf("  2. Export Maintenance History CSV\n");
        printf("  3. Export Maintenance Schedules CSV\n");
        printf("  4. Export Expense History CSV\n");
        printf("  5. Export Monthly Expenditure CSV\n");
        printf("  6. Export Vehicle-wise Costs CSV\n");
        printf("\n PDF EXECUTIVE REPORTS (Genuine Vector PDF 1.4 Documents):\n");
        printf("  7. Generate Fleet Executive Summary PDF\n");
        printf("  8. Generate Vehicle Maintenance History PDF\n");
        printf("  9. Generate Expense & Maintenance Cost Analysis PDF\n");
        printf("\n 0. Return to Main Menu\n");
        ui_print_divider('-', 60);

        int choice = -1;
        if (!input_get_int("Enter Choice [0-9]: ", &choice, 0, 9)) continue;
        if (choice == 0) break;

        char path[MAX_BUFFER_LG];
        StatusCode sc = STATUS_OK;

        switch (choice) {
            case 1:
                strcpy(path, "reports/vehicles_register.csv");
                sc = report_csv_vehicle_register(path);
                break;
            case 2: {
                int vid = 0;
                input_get_int("Enter Vehicle ID (or 0 for all vehicles): ", &vid, 0, 100000);
                strcpy(path, "reports/maintenance_history.csv");
                sc = report_csv_maintenance_history(path, vid);
                break;
            }
            case 3:
                strcpy(path, "reports/schedules_report.csv");
                sc = report_csv_schedules(path);
                break;
            case 4:
                strcpy(path, "reports/expense_history.csv");
                sc = report_csv_expenses(path);
                break;
            case 5:
                strcpy(path, "reports/monthly_expenditure.csv");
                sc = report_csv_monthly_spending(path);
                break;
            case 6:
                strcpy(path, "reports/vehicle_costs.csv");
                sc = report_csv_vehicle_costs(path);
                break;
            case 7:
                strcpy(path, "reports/fleet_executive_summary.pdf");
                sc = report_pdf_fleet_summary(path);
                break;
            case 8: {
                int vid = 0;
                input_get_int("Enter Vehicle ID: ", &vid, 1, 100000);
                snprintf(path, sizeof(path), "reports/vehicle_%d_maintenance.pdf", vid);
                sc = report_pdf_vehicle_maintenance(vid, path);
                break;
            }
            case 9:
                strcpy(path, "reports/expense_cost_analysis.pdf");
                sc = report_pdf_expense_summary(path);
                break;
        }

        if (sc == STATUS_OK) {
            char success_msg[MAX_BUFFER_LG];
            snprintf(success_msg, sizeof(success_msg), "Report generated successfully at: %s", path);
            ui_print_success(success_msg);
        } else {
            ui_print_error("Failed to generate report.");
        }
        input_pause();
    }
}

/* ==================== AUDIT TRAIL ==================== */
void ui_menu_audit(void) {
    const User *u = auth_get_session_user();
    if (!u || u->role != ROLE_ADMIN) {
        ui_print_error("Access denied. Administrator privileges required.");
        input_pause();
        return;
    }

    ui_clear_screen();
    ui_print_banner();
    ui_print_header("SECURITY & OPERATIONAL AUDIT TRAIL");

    AuditLog *logs = NULL;
    int count = 0;
    if (audit_list(30, &logs, &count) == STATUS_OK && count > 0) {
        printf("+-----+---------------------+----------------------+---------------------+---------------------------------------------+\n");
        printf("| ID  | Timestamp           | Action               | User                | Details                                     |\n");
        printf("+-----+---------------------+----------------------+---------------------+---------------------------------------------+\n");
        for (int i = 0; i < count; i++) {
            printf("| %-3d | %-19s | %-20s | %-19s | %-43.43s |\n",
                   logs[i].id, logs[i].timestamp, logs[i].action, logs[i].username, logs[i].details);
        }
        printf("+-----+---------------------+----------------------+---------------------+---------------------------------------------+\n");
        printf("Displaying latest %d audit events.\n", count);
        audit_free_list(logs);
    } else {
        ui_print_info("No audit logs found.");
    }
    input_pause();
}

/* ==================== USER MANAGEMENT ==================== */
void ui_menu_users(void) {
    const User *u = auth_get_session_user();
    if (!u || !auth_has_permission(u->role, PERM_USER_MANAGE)) {
        ui_print_error("Access denied. Administrator role required.");
        input_pause();
        return;
    }

    while (1) {
        ui_clear_screen();
        ui_print_banner();
        ui_print_header("USER ACCOUNTS & ROLE PERMISSIONS");
        printf(" 1. List All User Accounts\n");
        printf(" 2. Create New User Account\n");
        printf(" 3. Reset User Password\n");
        printf(" 4. Enable / Disable User Account\n");
        printf(" 0. Return to Main Menu\n");
        ui_print_divider('-', 50);

        int choice = -1;
        if (!input_get_int("Enter Choice [0-4]: ", &choice, 0, 4)) continue;
        if (choice == 0) break;

        switch (choice) {
            case 1:
                auth_list_users();
                input_pause();
                break;
            case 2: {
                char uname[MAX_BUFFER_SM], pwd[MAX_BUFFER_SM];
                if (!input_get_string("Enter Username: ", uname, sizeof(uname), false)) break;
                if (!input_get_password("Enter Password (min 6 chars): ", pwd, sizeof(pwd))) break;
                printf("Roles: 1. Administrator | 2. Fleet Manager | 3. Maintenance Staff | 4. Viewer\n");
                int r_ch = 2;
                input_get_int("Select Role [1-4]: ", &r_ch, 1, 4);
                UserRole role = ROLE_MANAGER;
                if (r_ch == 1) role = ROLE_ADMIN;
                else if (r_ch == 3) role = ROLE_STAFF;
                else if (r_ch == 4) role = ROLE_VIEWER;

                StatusCode sc = auth_create_user(uname, pwd, role);
                if (sc == STATUS_EXISTS) {
                    ui_print_error("A user with this username already exists.");
                } else if (sc == STATUS_OK) {
                    ui_print_success("User account created successfully.");
                } else {
                    ui_print_error("Failed to create user account.");
                }
                input_pause();
                break;
            }
            case 3: {
                int tid = 0;
                input_get_int("Enter Target User ID: ", &tid, 1, 10000);
                char new_p[MAX_BUFFER_SM];
                input_get_password("Enter New Password for user: ", new_p, sizeof(new_p));
                if (auth_admin_reset_password(u->id, tid, new_p) == STATUS_OK) {
                    ui_print_success("Password reset successfully.");
                } else {
                    ui_print_error("Failed to reset password.");
                }
                input_pause();
                break;
            }
            case 4: {
                int tid = 0;
                input_get_int("Enter Target User ID: ", &tid, 1, 10000);
                bool act = input_get_confirm("Set account status to ACTIVE?", true);
                if (auth_set_user_status(u->id, tid, act) == STATUS_OK) {
                    ui_print_success("User account status updated.");
                } else {
                    ui_print_error("Failed to update user status.");
                }
                input_pause();
                break;
            }
        }
    }
}

/* ==================== BACKUP & RESTORE ==================== */
void ui_menu_backup(void) {
    const User *u = auth_get_session_user();
    if (!u || !auth_has_permission(u->role, PERM_BACKUP_RESTORE)) {
        ui_print_error("Access denied. Administrator privileges required for Backup/Restore.");
        input_pause();
        return;
    }

    while (1) {
        ui_clear_screen();
        ui_print_banner();
        ui_print_header("DATABASE BACKUP & DISASTER RECOVERY");
        printf(" 1. Create Online Database Backup Now\n");
        printf(" 2. List Existing Backups\n");
        printf(" 3. Restore Database from Backup File\n");
        printf(" 4. Run SQLite Integrity Check\n");
        printf(" 0. Return to Main Menu\n");
        ui_print_divider('-', 50);

        int choice = -1;
        if (!input_get_int("Enter Choice [0-4]: ", &choice, 0, 4)) continue;
        if (choice == 0) break;

        switch (choice) {
            case 1: {
                char path[MAX_BUFFER_LG];
                printf("[BACKUP] Starting SQLite online hot backup...\n");
                if (backup_create(DEFAULT_BACKUP_DIR, path, sizeof(path)) == STATUS_OK) {
                    char msg[MAX_BUFFER_LG];
                    snprintf(msg, sizeof(msg), "Backup created successfully at: %s", path);
                    ui_print_success(msg);
                } else {
                    ui_print_error("Backup failed.");
                }
                input_pause();
                break;
            }
            case 2:
                backup_list(DEFAULT_BACKUP_DIR);
                input_pause();
                break;
            case 3: {
                backup_list(DEFAULT_BACKUP_DIR);
                char filename[MAX_BUFFER_MD];
                if (!input_get_string("\nEnter Backup Filename (e.g. fleet_backup_20261027_230000.db): ", filename, sizeof(filename), false)) break;

                char full_path[MAX_BUFFER_LG];
                snprintf(full_path, sizeof(full_path), "%s/%s", DEFAULT_BACKUP_DIR, filename);

                printf("\n[WARNING] Restoring will replace the current live database.\n");
                printf("          (An automatic safety backup will be created first).\n");
                if (input_get_confirm("Are you sure you want to proceed with restore?", false)) {
                    if (backup_restore(full_path) == STATUS_OK) {
                        ui_print_success("Database restored and verified successfully!");
                    } else {
                        ui_print_error("Restore operation failed.");
                    }
                }
                input_pause();
                break;
            }
            case 4: {
                char res[64] = {0};
                if (db_check_integrity(res, sizeof(res)) == STATUS_OK) {
                    if (strcmp(res, "ok") == 0) {
                        ui_print_success("Database integrity check: PASSED (ok)");
                    } else {
                        ui_print_error(res);
                    }
                } else {
                    ui_print_error("Failed to run integrity check.");
                }
                input_pause();
                break;
            }
        }
    }
}

/* ==================== MAIN UI LOOP ==================== */
void ui_run_main_loop(void) {
    while (1) {
        ui_clear_screen();
        ui_print_banner();
        ui_print_header("MAIN SYSTEM MENU");

        printf(" 1.  Fleet Dashboard & Operational Metrics\n");
        printf(" 2.  Vehicle Register & Asset Tracking\n");
        printf(" 3.  Driver Management\n");
        printf(" 4.  Maintenance & Repair Records\n");
        printf(" 5.  Preventive Maintenance Schedules & Reminders\n");
        printf(" 6.  Expense & Cost Tracking\n");
        printf(" 7.  In-App Notifications & Alerts\n");
        printf(" 8.  Reports & Analytics (CSV / PDF)\n");

        const User *u = auth_get_session_user();
        if (u && u->role == ROLE_ADMIN) {
            printf(" 9.  Security Audit Trail\n");
            printf(" 10. User Accounts & Access Control\n");
            printf(" 11. Database Backup & Restore\n");
        }

        printf(" 12. Change Password\n");
        printf(" 0.  Logout & Exit\n");
        ui_print_divider('-', 50);

        int choice = -1;
        if (!input_get_int("Enter Option: ", &choice, 0, 12)) continue;
        if (choice == 0) {
            auth_logout();
            printf("\nLogged out successfully. Thank you for using %s.\n", APP_NAME);
            break;
        }

        switch (choice) {
            case 1: ui_menu_dashboard(); break;
            case 2: ui_menu_vehicles(); break;
            case 3: ui_menu_drivers(); break;
            case 4: ui_menu_maintenance(); break;
            case 5: ui_menu_schedules(); break;
            case 6: ui_menu_expenses(); break;
            case 7: ui_menu_notifications(); break;
            case 8: ui_menu_reports(); break;
            case 9:
                if (u && u->role == ROLE_ADMIN) ui_menu_audit();
                else { ui_print_error("Admin access required."); input_pause(); }
                break;
            case 10:
                if (u && u->role == ROLE_ADMIN) ui_menu_users();
                else { ui_print_error("Admin access required."); input_pause(); }
                break;
            case 11:
                if (u && u->role == ROLE_ADMIN) ui_menu_backup();
                else { ui_print_error("Admin access required."); input_pause(); }
                break;
            case 12: {
                if (!u) break;
                char old_p[MAX_BUFFER_SM], new_p[MAX_BUFFER_SM];
                input_get_password("Enter Current Password: ", old_p, sizeof(old_p));
                input_get_password("Enter New Password (min 6 chars): ", new_p, sizeof(new_p));
                if (auth_change_password(u->id, old_p, new_p) == STATUS_OK) {
                    ui_print_success("Password changed successfully.");
                } else {
                    ui_print_error("Failed to change password. Please verify current password.");
                }
                input_pause();
                break;
            }
        }
    }
}
