#ifndef DRIVER_H
#define DRIVER_H

#include "common.h"

typedef struct {
    int id;
    char name[MAX_BUFFER_MD];
    char phone[MAX_BUFFER_SM];
    char license_number[MAX_BUFFER_SM];
    char license_expiry[MAX_BUFFER_SM];
    bool active;
    char created_at[MAX_BUFFER_SM];
    char updated_at[MAX_BUFFER_SM];
} Driver;

StatusCode driver_create(const Driver *driver, int *out_id);
StatusCode driver_get_by_id(int id, Driver *out_driver);
StatusCode driver_get_by_license(const char *license_no, Driver *out_driver);
StatusCode driver_update(const Driver *driver);
StatusCode driver_set_active(int id, bool active);
StatusCode driver_list(Driver **out_drivers, int *out_count, bool active_only);
StatusCode driver_search(const char *query, Driver **out_drivers, int *out_count);
StatusCode driver_delete(int driver_id, int user_id);
StatusCode driver_free_list(Driver *drivers);
bool driver_is_assigned(int driver_id, int *out_vehicle_id);

#endif /* DRIVER_H */
