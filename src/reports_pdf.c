#include "reports.h"
#include "pdf_gen.h"
#include "vehicle.h"
#include "maintenance.h"
#include "expense.h"
#include "dashboard.h"
#include "audit.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#else
#include <sys/stat.h>
#define MKDIR(d) mkdir(d, 0755)
#endif

static void ensure_pdf_dir(const char *filepath) {
    char dir[MAX_BUFFER_MD];
    safe_strncpy(dir, filepath, sizeof(dir));
    char *p = strrchr(dir, '/');
    if (!p) p = strrchr(dir, '\\');
    if (p) {
        *p = '\0';
        MKDIR(dir);
    }
}

static void draw_pdf_header(PdfDoc *doc, const char *title, const char *subtitle, int page_num) {
    /* Brand Header Banner */
    pdf_set_fill_color(doc, 0.12, 0.23, 0.38); /* Slate Blue */
    pdf_draw_rect(doc, 40, 780, 515, 35, true, false);

    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 14);
    pdf_set_text_color(doc, 1.0, 1.0, 1.0);
    pdf_draw_text(doc, 50, 792, APP_NAME);

    /* Report Title & Date */
    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 16);
    pdf_set_text_color(doc, 0.1, 0.1, 0.1);
    pdf_draw_text(doc, 40, 750, title);

    char date_buf[64];
    char today[32];
    get_current_iso_datetime(today, sizeof(today));
    snprintf(date_buf, sizeof(date_buf), "Generated on: %s", today);

    pdf_set_font_bold(doc, false);
    pdf_set_font_size(doc, 9);
    pdf_set_text_color(doc, 0.4, 0.4, 0.4);
    pdf_draw_text(doc, 40, 735, date_buf);

    if (subtitle && subtitle[0]) {
        pdf_draw_text(doc, 40, 722, subtitle);
    }

    /* Top Divider */
    pdf_set_stroke_color(doc, 0.8, 0.8, 0.8);
    pdf_draw_line(doc, 40, 715, 555, 715, 1.0);

    /* Footer with Page Number */
    pdf_set_stroke_color(doc, 0.85, 0.85, 0.85);
    pdf_draw_line(doc, 40, 45, 555, 45, 0.5);

    char footer_txt[64];
    snprintf(footer_txt, sizeof(footer_txt), "Page %d - Confidential - Fleet Management System", page_num);
    pdf_set_font_size(doc, 8);
    pdf_set_text_color(doc, 0.5, 0.5, 0.5);
    pdf_draw_text(doc, 40, 32, footer_txt);
}

StatusCode report_pdf_fleet_summary(const char *output_path) {
    const char *path = output_path ? output_path : "reports/fleet_summary.pdf";
    ensure_pdf_dir(path);

    PdfDoc *doc = pdf_create();
    if (!doc) return STATUS_ERROR;

    draw_pdf_header(doc, "Fleet Executive Summary Report", "Comprehensive Fleet Assets, Operational Status, and Maintenance Overview", 1);

    /* Fetch Dashboard Stats */
    DashboardStats stats;
    dashboard_get_stats(&stats);

    /* KPI Cards */
    double card_y = 650;
    double card_h = 50;

    /* Card 1: Active Vehicles */
    pdf_set_fill_color(doc, 0.95, 0.96, 0.98);
    pdf_set_stroke_color(doc, 0.85, 0.88, 0.92);
    pdf_draw_rect(doc, 40, card_y, 120, card_h, true, true);
    pdf_set_font_bold(doc, false);
    pdf_set_font_size(doc, 8);
    pdf_set_text_color(doc, 0.4, 0.4, 0.4);
    pdf_draw_text(doc, 48, card_y + 35, "TOTAL VEHICLES");
    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 16);
    pdf_set_text_color(doc, 0.1, 0.2, 0.4);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", stats.total_active_vehicles);
    pdf_draw_text(doc, 48, card_y + 12, buf);

    /* Card 2: Available Vehicles */
    pdf_draw_rect(doc, 170, card_y, 120, card_h, true, true);
    pdf_set_font_bold(doc, false);
    pdf_set_font_size(doc, 8);
    pdf_set_text_color(doc, 0.4, 0.4, 0.4);
    pdf_draw_text(doc, 178, card_y + 35, "AVAILABLE");
    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 16);
    pdf_set_text_color(doc, 0.1, 0.5, 0.2); /* green */
    snprintf(buf, sizeof(buf), "%d", stats.available_vehicles);
    pdf_draw_text(doc, 178, card_y + 12, buf);

    /* Card 3: In Maintenance */
    pdf_draw_rect(doc, 300, card_y, 120, card_h, true, true);
    pdf_set_font_bold(doc, false);
    pdf_set_font_size(doc, 8);
    pdf_set_text_color(doc, 0.4, 0.4, 0.4);
    pdf_draw_text(doc, 308, card_y + 35, "UNDER MAINTENANCE");
    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 16);
    pdf_set_text_color(doc, 0.8, 0.4, 0.1); /* amber */
    snprintf(buf, sizeof(buf), "%d", stats.under_maintenance_vehicles);
    pdf_draw_text(doc, 308, card_y + 12, buf);

    /* Card 4: Total Spending */
    pdf_draw_rect(doc, 430, card_y, 125, card_h, true, true);
    pdf_set_font_bold(doc, false);
    pdf_set_font_size(doc, 8);
    pdf_set_text_color(doc, 0.4, 0.4, 0.4);
    pdf_draw_text(doc, 438, card_y + 35, "MAINTENANCE SPEND");
    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 12);
    pdf_set_text_color(doc, 0.2, 0.2, 0.2);
    char spend_buf[32];
    format_currency(stats.total_maintenance_spending, spend_buf, sizeof(spend_buf));
    pdf_draw_text(doc, 438, card_y + 14, spend_buf);

    /* Table: Vehicles Register */
    double table_y = 610;
    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 11);
    pdf_set_text_color(doc, 0.15, 0.15, 0.15);
    pdf_draw_text(doc, 40, table_y, "Fleet Vehicles Register");

    table_y -= 15;
    /* Header Row */
    pdf_set_fill_color(doc, 0.9, 0.92, 0.95);
    pdf_draw_rect(doc, 40, table_y - 5, 515, 20, true, false);

    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 9);
    pdf_set_text_color(doc, 0.2, 0.2, 0.2);
    pdf_draw_text(doc, 45, table_y, "Reg Number");
    pdf_draw_text(doc, 130, table_y, "Make & Model");
    pdf_draw_text(doc, 240, table_y, "Year");
    pdf_draw_text(doc, 280, table_y, "Fuel Type");
    pdf_draw_text(doc, 340, table_y, "Odometer");
    pdf_draw_text(doc, 410, table_y, "Status");
    pdf_draw_text(doc, 480, table_y, "Driver");

    VehicleFilterOptions vf = {0};
    Vehicle *vehicles = NULL;
    int v_count = 0;
    vehicle_list(&vf, &vehicles, &v_count);

    table_y -= 20;
    int rows_drawn = 0;
    int page_num = 1;

    for (int i = 0; i < v_count; i++) {
        if (table_y < 70) {
            /* Start new page */
            pdf_new_page(doc);
            page_num++;
            draw_pdf_header(doc, "Fleet Executive Summary Report (Cont.)", "Vehicle Register", page_num);
            table_y = 680;

            /* Redraw table header */
            pdf_set_fill_color(doc, 0.9, 0.92, 0.95);
            pdf_draw_rect(doc, 40, table_y - 5, 515, 20, true, false);
            pdf_set_font_bold(doc, true);
            pdf_set_font_size(doc, 9);
            pdf_set_text_color(doc, 0.2, 0.2, 0.2);
            pdf_draw_text(doc, 45, table_y, "Reg Number");
            pdf_draw_text(doc, 130, table_y, "Make & Model");
            pdf_draw_text(doc, 240, table_y, "Year");
            pdf_draw_text(doc, 280, table_y, "Fuel Type");
            pdf_draw_text(doc, 340, table_y, "Odometer");
            pdf_draw_text(doc, 410, table_y, "Status");
            pdf_draw_text(doc, 480, table_y, "Driver");
            table_y -= 20;
        }

        /* Alternate row background */
        if (rows_drawn % 2 == 1) {
            pdf_set_fill_color(doc, 0.98, 0.98, 0.99);
            pdf_draw_rect(doc, 40, table_y - 4, 515, 18, true, false);
        }

        pdf_set_font_bold(doc, false);
        pdf_set_font_size(doc, 8.5);
        pdf_set_text_color(doc, 0.2, 0.2, 0.2);

        pdf_draw_text(doc, 45, table_y, vehicles[i].registration_number);
        char mm[MAX_BUFFER_MD];
        snprintf(mm, sizeof(mm), "%s %s", vehicles[i].make, vehicles[i].model);
        pdf_draw_text(doc, 130, table_y, mm);

        char yr[16];
        snprintf(yr, sizeof(yr), "%d", vehicles[i].manufacturing_year);
        pdf_draw_text(doc, 240, table_y, yr);

        pdf_draw_text(doc, 280, table_y, vehicles[i].fuel_type);

        char odom[32];
        snprintf(odom, sizeof(odom), "%d km", vehicles[i].odometer_km);
        pdf_draw_text(doc, 340, table_y, odom);

        pdf_draw_text(doc, 410, table_y, vehicles[i].status);
        pdf_draw_text(doc, 480, table_y, vehicles[i].assigned_driver_name);

        table_y -= 18;
        rows_drawn++;
    }

    vehicle_free_list(vehicles);

    StatusCode sc = pdf_save_to_file(doc, path);
    pdf_destroy(doc);

    audit_log_action(0, "REPORT_PDF", "reports", 0, "Generated Fleet Summary PDF");
    return sc;
}

StatusCode report_pdf_vehicle_maintenance(int vehicle_id, const char *output_path) {
    if (vehicle_id <= 0) return STATUS_INVALID_INPUT;
    Vehicle v;
    StatusCode sc = vehicle_get_by_id(vehicle_id, &v);
    if (sc != STATUS_OK) return sc;

    char def_path[MAX_BUFFER_MD];
    snprintf(def_path, sizeof(def_path), "reports/maintenance_%s.pdf", v.registration_number);
    const char *path = output_path ? output_path : def_path;
    ensure_pdf_dir(path);

    PdfDoc *doc = pdf_create();
    if (!doc) return STATUS_ERROR;

    char subtitle[MAX_BUFFER_XL];
    snprintf(subtitle, sizeof(subtitle), "Vehicle: %s (%s %s) | VIN: %s", v.registration_number, v.make, v.model, v.vin);
    draw_pdf_header(doc, "Vehicle Maintenance History", subtitle, 1);

    /* Vehicle Info Card */
    double card_y = 660;
    pdf_set_fill_color(doc, 0.96, 0.97, 0.99);
    pdf_set_stroke_color(doc, 0.85, 0.88, 0.92);
    pdf_draw_rect(doc, 40, card_y, 515, 45, true, true);

    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 9);
    pdf_set_text_color(doc, 0.2, 0.2, 0.2);
    pdf_draw_text(doc, 50, card_y + 28, "Registration:");
    pdf_draw_text(doc, 190, card_y + 28, "Current Odometer:");
    pdf_draw_text(doc, 340, card_y + 28, "Assigned Driver:");

    pdf_draw_text(doc, 50, card_y + 12, "Status:");
    pdf_draw_text(doc, 190, card_y + 12, "Fuel Level:");
    pdf_draw_text(doc, 340, card_y + 12, "Insurance Expiry:");

    pdf_set_font_bold(doc, false);
    pdf_draw_text(doc, 115, card_y + 28, v.registration_number);
    char buf[MAX_BUFFER_MD];
    snprintf(buf, sizeof(buf), "%d KM", v.odometer_km);
    pdf_draw_text(doc, 275, card_y + 28, buf);
    pdf_draw_text(doc, 420, card_y + 28, v.assigned_driver_name);

    pdf_draw_text(doc, 115, card_y + 12, v.status);
    snprintf(buf, sizeof(buf), "%d%% (%s)", v.fuel_percentage, v.fuel_type);
    pdf_draw_text(doc, 275, card_y + 12, buf);
    pdf_draw_text(doc, 420, card_y + 12, str_is_blank(v.insurance_expiry) ? "N/A" : v.insurance_expiry);

    /* Records Table */
    double table_y = 620;
    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 11);
    pdf_set_text_color(doc, 0.15, 0.15, 0.15);
    pdf_draw_text(doc, 40, table_y, "Service & Repair Records");

    table_y -= 15;
    pdf_set_fill_color(doc, 0.9, 0.92, 0.95);
    pdf_draw_rect(doc, 40, table_y - 5, 515, 20, true, false);

    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 8.5);
    pdf_set_text_color(doc, 0.2, 0.2, 0.2);
    pdf_draw_text(doc, 45, table_y, "Date");
    pdf_draw_text(doc, 105, table_y, "Service Type");
    pdf_draw_text(doc, 185, table_y, "Workshop");
    pdf_draw_text(doc, 275, table_y, "Odometer");
    pdf_draw_text(doc, 340, table_y, "Labour");
    pdf_draw_text(doc, 405, table_y, "Parts");
    pdf_draw_text(doc, 475, table_y, "Total Cost");

    MaintenanceFilterOptions mf = {0};
    mf.vehicle_id = vehicle_id;
    MaintenanceRecord *records = NULL;
    int r_count = 0;
    maintenance_list(&mf, &records, &r_count);

    table_y -= 20;
    int rows_drawn = 0;
    int page_num = 1;
    int64_t total_maint_cost = 0;

    for (int i = 0; i < r_count; i++) {
        total_maint_cost += records[i].total_cost;
        if (table_y < 70) {
            pdf_new_page(doc);
            page_num++;
            draw_pdf_header(doc, "Vehicle Maintenance History (Cont.)", subtitle, page_num);
            table_y = 680;
            pdf_set_fill_color(doc, 0.9, 0.92, 0.95);
            pdf_draw_rect(doc, 40, table_y - 5, 515, 20, true, false);
            pdf_set_font_bold(doc, true);
            pdf_set_font_size(doc, 8.5);
            pdf_set_text_color(doc, 0.2, 0.2, 0.2);
            pdf_draw_text(doc, 45, table_y, "Date");
            pdf_draw_text(doc, 105, table_y, "Service Type");
            pdf_draw_text(doc, 185, table_y, "Workshop");
            pdf_draw_text(doc, 275, table_y, "Odometer");
            pdf_draw_text(doc, 340, table_y, "Labour");
            pdf_draw_text(doc, 405, table_y, "Parts");
            pdf_draw_text(doc, 475, table_y, "Total Cost");
            table_y -= 20;
        }

        if (rows_drawn % 2 == 1) {
            pdf_set_fill_color(doc, 0.98, 0.98, 0.99);
            pdf_draw_rect(doc, 40, table_y - 4, 515, 18, true, false);
        }

        pdf_set_font_bold(doc, false);
        pdf_set_font_size(doc, 8);
        pdf_set_text_color(doc, 0.2, 0.2, 0.2);

        pdf_draw_text(doc, 45, table_y, records[i].service_date);
        pdf_draw_text(doc, 105, table_y, records[i].service_type);
        pdf_draw_text(doc, 185, table_y, records[i].workshop_name);

        snprintf(buf, sizeof(buf), "%d km", records[i].odometer_km);
        pdf_draw_text(doc, 275, table_y, buf);

        char cost_buf[32];
        format_currency(records[i].labour_cost, cost_buf, sizeof(cost_buf));
        pdf_draw_text(doc, 340, table_y, cost_buf);

        format_currency(records[i].parts_cost, cost_buf, sizeof(cost_buf));
        pdf_draw_text(doc, 405, table_y, cost_buf);

        format_currency(records[i].total_cost, cost_buf, sizeof(cost_buf));
        pdf_draw_text(doc, 475, table_y, cost_buf);

        table_y -= 18;
        rows_drawn++;
    }

    /* Total Summary Row */
    table_y -= 5;
    pdf_set_stroke_color(doc, 0.7, 0.7, 0.7);
    pdf_draw_line(doc, 40, table_y + 15, 555, table_y + 15, 1.0);

    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 10);
    pdf_draw_text(doc, 340, table_y, "Cumulative Total:");
    char total_buf[32];
    format_currency(total_maint_cost, total_buf, sizeof(total_buf));
    pdf_draw_text(doc, 460, table_y, total_buf);

    maintenance_free_list(records);

    sc = pdf_save_to_file(doc, path);
    pdf_destroy(doc);

    char details[MAX_BUFFER_MD];
    snprintf(details, sizeof(details), "Generated Maintenance PDF for vehicle ID %d", vehicle_id);
    audit_log_action(0, "REPORT_PDF", "reports", vehicle_id, details);

    return sc;
}

StatusCode report_pdf_expense_summary(const char *output_path) {
    const char *path = output_path ? output_path : "reports/expense_summary.pdf";
    ensure_pdf_dir(path);

    PdfDoc *doc = pdf_create();
    if (!doc) return STATUS_ERROR;

    draw_pdf_header(doc, "Fleet Expense & Maintenance Cost Analysis", "Category Breakdown and Historical Monthly Spending", 1);

    /* Fetch overall stats */
    OverallExpenseStats stats;
    expense_get_overall_stats(&stats);

    double card_y = 660;
    pdf_set_fill_color(doc, 0.95, 0.96, 0.98);
    pdf_set_stroke_color(doc, 0.85, 0.88, 0.92);

    /* Card 1: Total Spending */
    pdf_draw_rect(doc, 40, card_y, 160, 45, true, true);
    pdf_set_font_bold(doc, false);
    pdf_set_font_size(doc, 8);
    pdf_set_text_color(doc, 0.4, 0.4, 0.4);
    pdf_draw_text(doc, 48, card_y + 30, "TOTAL FLEET EXPENDITURE");
    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 13);
    pdf_set_text_color(doc, 0.1, 0.2, 0.4);
    char buf[32];
    format_currency(stats.total_all_expenses, buf, sizeof(buf));
    pdf_draw_text(doc, 48, card_y + 12, buf);

    /* Card 2: Maintenance Spending */
    pdf_draw_rect(doc, 215, card_y, 160, 45, true, true);
    pdf_set_font_bold(doc, false);
    pdf_set_font_size(doc, 8);
    pdf_set_text_color(doc, 0.4, 0.4, 0.4);
    pdf_draw_text(doc, 223, card_y + 30, "MAINTENANCE ONLY");
    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 13);
    pdf_set_text_color(doc, 0.1, 0.5, 0.3);
    format_currency(stats.total_maintenance_spending, buf, sizeof(buf));
    pdf_draw_text(doc, 223, card_y + 12, buf);

    /* Card 3: Avg per Active Vehicle */
    pdf_draw_rect(doc, 390, card_y, 165, 45, true, true);
    pdf_set_font_bold(doc, false);
    pdf_set_font_size(doc, 8);
    pdf_set_text_color(doc, 0.4, 0.4, 0.4);
    pdf_draw_text(doc, 398, card_y + 30, "AVG SPEND / VEHICLE");
    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 13);
    pdf_set_text_color(doc, 0.2, 0.2, 0.2);
    format_currency(stats.avg_cost_per_active_vehicle, buf, sizeof(buf));
    pdf_draw_text(doc, 398, card_y + 12, buf);

    /* Category Breakdown Table */
    double table_y = 620;
    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 11);
    pdf_set_text_color(doc, 0.15, 0.15, 0.15);
    pdf_draw_text(doc, 40, table_y, "Expenses by Category");

    table_y -= 15;
    pdf_set_fill_color(doc, 0.9, 0.92, 0.95);
    pdf_draw_rect(doc, 40, table_y - 5, 515, 20, true, false);

    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 8.5);
    pdf_set_text_color(doc, 0.2, 0.2, 0.2);
    pdf_draw_text(doc, 50, table_y, "Expense Category");
    pdf_draw_text(doc, 240, table_y, "Transactions Count");
    pdf_draw_text(doc, 420, table_y, "Total Amount");

    CategoryExpenseSummary *cats = NULL;
    int cat_count = 0;
    expense_get_category_summary(&cats, &cat_count);

    table_y -= 20;
    for (int i = 0; i < cat_count; i++) {
        if (i % 2 == 1) {
            pdf_set_fill_color(doc, 0.98, 0.98, 0.99);
            pdf_draw_rect(doc, 40, table_y - 4, 515, 18, true, false);
        }
        pdf_set_font_bold(doc, false);
        pdf_set_font_size(doc, 8.5);
        pdf_set_text_color(doc, 0.2, 0.2, 0.2);

        pdf_draw_text(doc, 50, table_y, cats[i].category);
        snprintf(buf, sizeof(buf), "%d", cats[i].count);
        pdf_draw_text(doc, 250, table_y, buf);

        char c_buf[32];
        format_currency(cats[i].total_amount, c_buf, sizeof(c_buf));
        pdf_draw_text(doc, 420, table_y, c_buf);

        table_y -= 18;
    }
    if (cats) free(cats);

    /* Monthly Breakdown Table */
    table_y -= 25;
    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 11);
    pdf_set_text_color(doc, 0.15, 0.15, 0.15);
    pdf_draw_text(doc, 40, table_y, "Monthly Spending Trends (Recent 12 Months)");

    table_y -= 15;
    pdf_set_fill_color(doc, 0.9, 0.92, 0.95);
    pdf_draw_rect(doc, 40, table_y - 5, 515, 20, true, false);

    pdf_set_font_bold(doc, true);
    pdf_set_font_size(doc, 8.5);
    pdf_set_text_color(doc, 0.2, 0.2, 0.2);
    pdf_draw_text(doc, 50, table_y, "Month (YYYY-MM)");
    pdf_draw_text(doc, 240, table_y, "Transactions Count");
    pdf_draw_text(doc, 420, table_y, "Total Expenditure");

    MonthlyExpenseSummary *months = NULL;
    int m_count = 0;
    expense_get_monthly_summary(&months, &m_count);

    table_y -= 20;
    for (int i = 0; i < m_count; i++) {
        if (table_y < 70) break;
        if (i % 2 == 1) {
            pdf_set_fill_color(doc, 0.98, 0.98, 0.99);
            pdf_draw_rect(doc, 40, table_y - 4, 515, 18, true, false);
        }
        pdf_set_font_bold(doc, false);
        pdf_set_font_size(doc, 8.5);
        pdf_set_text_color(doc, 0.2, 0.2, 0.2);

        pdf_draw_text(doc, 50, table_y, months[i].year_month);
        snprintf(buf, sizeof(buf), "%d", months[i].count);
        pdf_draw_text(doc, 250, table_y, buf);

        char c_buf[32];
        format_currency(months[i].total_amount, c_buf, sizeof(c_buf));
        pdf_draw_text(doc, 420, table_y, c_buf);

        table_y -= 18;
    }
    if (months) free(months);

    StatusCode sc = pdf_save_to_file(doc, path);
    pdf_destroy(doc);

    audit_log_action(0, "REPORT_PDF", "reports", 0, "Generated Expense Summary PDF");
    return sc;
}
