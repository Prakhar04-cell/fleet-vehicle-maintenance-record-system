#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include "common.h"

/* Legacy Notification structure */
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

/* Enterprise Deduplicated System Notification */
typedef struct {
    int id;
    char event_key[MAX_BUFFER_MD];
    char category[MAX_BUFFER_SM];       /* Maintenance, Defect, WorkOrder, System */
    char severity[MAX_BUFFER_SM];       /* Info, Warning, Critical */
    char title[MAX_BUFFER_MD];
    char message[MAX_BUFFER_LG];
    char entity_type[MAX_BUFFER_SM];
    int entity_id;
    char role_visibility[MAX_BUFFER_SM];/* All, Admin, Manager, Staff */
    int is_read;
    int acknowledged_by;
    char acknowledged_at[MAX_BUFFER_SM];
    char created_at[MAX_BUFFER_SM];
} SystemNotification;

/* Legacy API functions */
StatusCode notification_create(int schedule_id, const char *type, const char *scheduled_at, const char *failure_reason);
StatusCode notification_list_active(Notification **out_notifications, int *out_count);
StatusCode notification_dismiss(int id, int user_id);
StatusCode notification_dismiss_all(int user_id);
void notification_free_list(Notification *notifications);

/* Enhanced System Notification Engine */
StatusCode notification_event_create(const char *event_key, const char *category, const char *severity,
                                     const char *title, const char *message, const char *entity_type,
                                     int entity_id, const char *role_visibility);
StatusCode notification_system_list(const char *category, int unread_only, const char *user_role,
                                    SystemNotification **out_list, int *out_count);
StatusCode notification_acknowledge(int id, int user_id);
StatusCode notification_acknowledge_all(int user_id, const char *user_role);
StatusCode notification_sync_all_events(int *out_synced_count);
void notification_free_system_list(SystemNotification *list);

#endif /* NOTIFICATION_H */
