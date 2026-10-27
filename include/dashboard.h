#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "common.h"
#include "vehicle.h"
#include "maintenance.h"
#include "schedule.h"
#include "notification.h"

typedef struct {
    int total_active_vehicles;
    int available_vehicles;
    int on_trip_vehicles;
    int under_maintenance_vehicles;
    int out_of_service_vehicles;
    int upcoming_services_count;
    int overdue_services_count;
    int64_t total_maintenance_spending;
    int64_t current_month_spending;
    int low_fuel_vehicles_count;
    int active_notifications_count;
} DashboardStats;

StatusCode dashboard_get_stats(DashboardStats *out_stats);
StatusCode dashboard_get_low_fuel_vehicles(Vehicle **out_vehicles, int *out_count);
StatusCode dashboard_get_recent_maintenance(int limit, MaintenanceRecord **out_records, int *out_count);
StatusCode dashboard_get_urgent_schedules(int limit, MaintenanceSchedule **out_schedules, int *out_count);

#endif /* DASHBOARD_H */
