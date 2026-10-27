#ifndef REPORTS_H
#define REPORTS_H

#include "common.h"

/* CSV escaping and formula injection protection */
void report_escape_csv_field(const char *src, char *dest, size_t dest_size);

/* CSV reports */
StatusCode report_csv_vehicle_register(const char *output_path);
StatusCode report_csv_maintenance_history(const char *output_path, int vehicle_id);
StatusCode report_csv_schedules(const char *output_path);
StatusCode report_csv_expenses(const char *output_path);
StatusCode report_csv_monthly_spending(const char *output_path);
StatusCode report_csv_vehicle_costs(const char *output_path);

/* PDF reports */
StatusCode report_pdf_fleet_summary(const char *output_path);
StatusCode report_pdf_vehicle_maintenance(int vehicle_id, const char *output_path);
StatusCode report_pdf_expense_summary(const char *output_path);

#endif /* REPORTS_H */
