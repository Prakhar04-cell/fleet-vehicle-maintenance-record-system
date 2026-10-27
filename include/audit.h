#ifndef AUDIT_H
#define AUDIT_H

#include "common.h"

typedef struct {
    int id;
    int user_id;
    char username[MAX_BUFFER_SM];
    char action[MAX_BUFFER_SM];
    char entity_type[MAX_BUFFER_SM];
    int entity_id;
    char timestamp[MAX_BUFFER_SM];
    char details[MAX_BUFFER_XL];
} AuditLog;

StatusCode audit_log_action(int user_id, const char *action, const char *entity_type, int entity_id, const char *details);
StatusCode audit_list(int limit, AuditLog **out_logs, int *out_count);
StatusCode audit_filter(const char *entity_type, int entity_id, AuditLog **out_logs, int *out_count);
void audit_free_list(AuditLog *logs);

#endif /* AUDIT_H */
