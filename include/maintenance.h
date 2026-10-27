#ifndef MAINTENANCE_H
#define MAINTENANCE_H

#include "common.h"

typedef struct {
    int id;
    int vehicle_id;
    char vehicle_reg[MAX_BUFFER_SM];
    char service_type[MAX_BUFFER_SM];
    char service_date[MAX_BUFFER_SM];
    int odometer_km;
    char description[MAX_BUFFER_XL];
    char workshop_name[MAX_BUFFER_MD];
    char mechanic_name[MAX_BUFFER_MD];
    int64_t labour_cost; /* minor currency unit */
    int64_t parts_cost;
    int64_t tax_amount;
    int64_t total_cost;
    char next_service_date[MAX_BUFFER_SM];
    int next_service_odometer_km;
    char invoice_doc_path[MAX_BUFFER_LG];
    int created_by;
    char created_by_username[MAX_BUFFER_SM];
    char created_at[MAX_BUFFER_SM];
    char updated_at[MAX_BUFFER_SM];
    char archived_at[MAX_BUFFER_SM];
    bool is_archived;
} MaintenanceRecord;

typedef struct {
    int vehicle_id;
    char service_type[MAX_BUFFER_SM];
    char date_from[MAX_BUFFER_SM];
    char date_to[MAX_BUFFER_SM];
} MaintenanceFilterOptions;

StatusCode maintenance_create(MaintenanceRecord *record, bool create_linked_expense, int user_id);
StatusCode maintenance_get_by_id(int id, MaintenanceRecord *out_record);
StatusCode maintenance_update(const MaintenanceRecord *record, int user_id);
StatusCode maintenance_archive(int id, int user_id);
StatusCode maintenance_list(const MaintenanceFilterOptions *filter, MaintenanceRecord **out_records, int *out_count);
StatusCode maintenance_get_vehicle_history(int vehicle_id, MaintenanceRecord **out_records, int *out_count);
StatusCode maintenance_search(const char *query, MaintenanceRecord **out_records, int *out_count);
void maintenance_free_list(MaintenanceRecord *records);

#endif /* MAINTENANCE_H */
