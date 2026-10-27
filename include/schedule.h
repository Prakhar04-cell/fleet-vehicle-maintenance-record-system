#ifndef SCHEDULE_H
#define SCHEDULE_H

#include "common.h"

typedef struct {
    int id;
    int vehicle_id;
    char vehicle_reg[MAX_BUFFER_SM];
    char service_name[MAX_BUFFER_MD];
    char description[MAX_BUFFER_LG];
    char due_date[MAX_BUFFER_SM];
    int due_odometer_km;
    int current_vehicle_odometer;
    int reminder_days_before;
    char status[MAX_BUFFER_SM]; /* Scheduled, Due, Overdue, Completed, Cancelled */
    int recurring_interval_days;
    int recurring_interval_km;
    char last_notified_at[MAX_BUFFER_SM];
    char created_at[MAX_BUFFER_SM];
    char updated_at[MAX_BUFFER_SM];
    int days_remaining; /* computed */
    int km_remaining;   /* computed */
} MaintenanceSchedule;

typedef struct {
    int vehicle_id;
    char status[MAX_BUFFER_SM];
    bool active_only; /* Scheduled, Due, or Overdue */
} ScheduleFilterOptions;

StatusCode schedule_create(MaintenanceSchedule *schedule, int user_id);
StatusCode schedule_get_by_id(int id, MaintenanceSchedule *out_schedule);
StatusCode schedule_update(const MaintenanceSchedule *schedule, int user_id);
StatusCode schedule_reschedule(int id, const char *new_due_date, int new_due_odometer, int user_id);
StatusCode schedule_complete(int id, int service_record_id, int user_id, int *out_next_schedule_id);
StatusCode schedule_cancel(int id, const char *reason, int user_id);
StatusCode schedule_list(const ScheduleFilterOptions *filter, MaintenanceSchedule **out_schedules, int *out_count);
StatusCode schedule_evaluate_all(int *out_updated_count, int *out_notification_count);
void schedule_free_list(MaintenanceSchedule *schedules);

#endif /* SCHEDULE_H */
