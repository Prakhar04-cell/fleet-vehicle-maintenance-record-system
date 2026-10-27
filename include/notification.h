#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include "common.h"

typedef struct {
    int id;
    int schedule_id;
    int vehicle_id;
    char vehicle_reg[MAX_BUFFER_SM];
    char service_name[MAX_BUFFER_MD];
    char notification_type[MAX_BUFFER_SM];
    char status[MAX_BUFFER_SM]; /* Active, Dismissed */
    char scheduled_at[MAX_BUFFER_SM];
    char sent_at[MAX_BUFFER_SM];
    char failure_reason[MAX_BUFFER_MD];
    char created_at[MAX_BUFFER_SM];
    int days_overdue_or_remaining;
} Notification;

StatusCode notification_create(int schedule_id, const char *type, const char *scheduled_at, const char *failure_reason);
StatusCode notification_list_active(Notification **out_notifications, int *out_count);
StatusCode notification_dismiss(int id, int user_id);
StatusCode notification_dismiss_all(int user_id);
void notification_free_list(Notification *notifications);

#endif /* NOTIFICATION_H */
