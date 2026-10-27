#ifndef VEHICLE_H
#define VEHICLE_H

#include "common.h"
#include "driver.h"

typedef enum {
    VEHICLE_SORT_REG = 1,
    VEHICLE_SORT_MAKE,
    VEHICLE_SORT_FUEL,
    VEHICLE_SORT_ODOMETER,
    VEHICLE_SORT_STATUS
} VehicleSortField;

typedef struct {
    int id;
    char registration_number[MAX_BUFFER_SM];
    char make[MAX_BUFFER_SM];
    char model[MAX_BUFFER_SM];
    int manufacturing_year;
    char vin[MAX_BUFFER_SM];
    char fuel_type[MAX_BUFFER_SM];
    int fuel_percentage;
    int odometer_km;
    char status[MAX_BUFFER_SM];
    int assigned_driver_id;
    char assigned_driver_name[MAX_BUFFER_MD];
    char insurance_expiry[MAX_BUFFER_SM];
    char notes[MAX_BUFFER_LG];
    char created_at[MAX_BUFFER_SM];
    char updated_at[MAX_BUFFER_SM];
    char archived_at[MAX_BUFFER_SM];
    bool is_archived;
} Vehicle;

typedef struct {
    char status_filter[MAX_BUFFER_SM];
    bool available_only;
    bool maintenance_only;
    bool include_archived;
    VehicleSortField sort_field;
    bool sort_descending;
} VehicleFilterOptions;

StatusCode vehicle_create(const Vehicle *vehicle, int *out_id);
StatusCode vehicle_get_by_id(int id, Vehicle *out_vehicle);
StatusCode vehicle_get_by_reg(const char *reg_number, Vehicle *out_vehicle);
StatusCode vehicle_update(const Vehicle *vehicle);
StatusCode vehicle_update_odometer(int vehicle_id, int new_odometer, const char *correction_reason, int user_id);
StatusCode vehicle_update_fuel(int vehicle_id, int fuel_percentage, int user_id);
StatusCode vehicle_update_status(int vehicle_id, const char *status, int user_id);
StatusCode vehicle_assign_driver(int vehicle_id, int driver_id, int user_id);
StatusCode vehicle_unassign_driver(int vehicle_id, int user_id);
StatusCode vehicle_archive(int vehicle_id, int user_id);
StatusCode vehicle_restore(int vehicle_id, int user_id);

StatusCode vehicle_list(const VehicleFilterOptions *options, Vehicle **out_vehicles, int *out_count);
StatusCode vehicle_search(const char *query, Vehicle **out_vehicles, int *out_count);
void vehicle_free_list(Vehicle *vehicles);

#endif /* VEHICLE_H */
