#ifndef WORK_ORDER_H
#define WORK_ORDER_H

#include "common.h"

typedef struct {
    int id;
    char wo_number[MAX_BUFFER_SM];
    int vehicle_id;
    char vehicle_reg[MAX_BUFFER_SM];
    int schedule_id;
    int inspection_id;
    char title[MAX_BUFFER_MD];
    char description[MAX_BUFFER_LG];
    char priority[MAX_BUFFER_SM]; /* Low, Medium, High, Critical */
    char status[MAX_BUFFER_SM];   /* Open, Assigned, In Progress, Waiting for Parts, Completed, Cancelled */
    int assigned_to_user_id;
    char assigned_to_name[MAX_BUFFER_SM];
    int odometer_km;
    char start_time[MAX_BUFFER_SM];
    char completion_time[MAX_BUFFER_SM];
    int64_t labour_cost;
    int64_t parts_cost;
    int64_t total_cost;
    int maintenance_record_id;
    char notes[MAX_BUFFER_LG];
    int created_by;
    char created_at[MAX_BUFFER_SM];
    char updated_at[MAX_BUFFER_SM];
} WorkOrder;

typedef struct {
    int id;
    int work_order_id;
    char from_status[MAX_BUFFER_SM];
    char to_status[MAX_BUFFER_SM];
    int changed_by;
    char changed_by_name[MAX_BUFFER_SM];
    char notes[MAX_BUFFER_MD];
    char timestamp[MAX_BUFFER_SM];
} WorkOrderHistory;

typedef struct {
    int vehicle_id;
    char status[MAX_BUFFER_SM];
    char priority[MAX_BUFFER_SM];
    int assigned_to_user_id;
} WorkOrderFilter;

StatusCode work_order_create(WorkOrder *wo, int user_id, const char *user_name);
StatusCode work_order_get_by_id(int id, WorkOrder *out_wo);
StatusCode work_order_update_status(int id, const char *new_status, const char *notes, int user_id, const char *user_name);
StatusCode work_order_assign(int id, int technician_id, const char *technician_name, int user_id, const char *user_name);
StatusCode work_order_complete(int id, int odometer, int64_t labour_cost, int64_t parts_cost, const char *notes, int user_id, const char *user_name, int *out_maint_id);
StatusCode work_order_list(const WorkOrderFilter *filter, WorkOrder **out_list, int *out_count);
StatusCode work_order_get_history(int work_order_id, WorkOrderHistory **out_history, int *out_count);
void work_order_free_list(WorkOrder *list);
void work_order_free_history(WorkOrderHistory *history);

#endif /* WORK_ORDER_H */
