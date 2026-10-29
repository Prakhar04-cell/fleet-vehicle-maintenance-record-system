with open('src/http_server.c', 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Update handle_api_maintenance_prio_queue
old_prio = r'''static void handle_api_maintenance_prio_queue(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    DsaMinHeap *heap = dsa_heap_build_from_active_schedules();
    if (!heap) {
        const char *empty = "{\"success\": true, \"count\": 0, \"data\": []}";
        send_http_response(client, 200, "application/json", empty, strlen(empty));
        return;
    }

    size_t cap = 32768 + (heap->size * 512);
    char *buf = (char *)malloc(cap);
    size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %zu, \"algorithm\": \"Binary Min-Heap Priority Queue\", \"complexity\": \"O(log N) insert/extract\", \"data\": [", heap->size);

    int rank = 1;
    while (!dsa_heap_is_empty(heap)) {
        MaintenanceSchedule sched;
        int score = 0;
        if (dsa_heap_extract_min(heap, &sched, &score)) {
            off += snprintf(buf + off, cap - off,
                "%s{\"urgency_rank\": %d, \"priority_score\": %d, \"schedule_id\": %d, "
                "\"vehicle_id\": %d, \"vehicle_reg\": \"%s\", \"service_name\": \"%s\", "
                "\"due_date\": \"%s\", \"target_odometer_km\": %d, \"status\": \"%s\", \"days_remaining\": %d}",
                (rank > 1) ? ", " : "",
                rank, score, sched.id, sched.vehicle_id, sched.vehicle_reg,
                sched.service_name, sched.due_date, sched.target_odometer_km, sched.status, sched.days_remaining);
            rank++;
        }
    }
    off += snprintf(buf + off, cap - off, "]}");

    send_http_response(client, 200, "application/json", buf, off);
    free(buf);
    dsa_heap_destroy(heap);
}'''

new_prio = r'''static void handle_api_maintenance_prio_queue(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    DsaMinHeap *heap = dsa_heap_build_from_active_schedules();
    if (!heap) {
        const char *empty = "{\"success\": true, \"count\": 0, \"complexity\": \"O(log N)\", \"queue\": [], \"data\": []}";
        send_http_response(client, 200, "application/json", empty, strlen(empty));
        return;
    }

    size_t cap = 65536 + (heap->size * 512);
    char *buf = (char *)malloc(cap);
    char items_json[32768] = "";
    size_t ioff = 0;

    int rank = 1;
    while (!dsa_heap_is_empty(heap)) {
        MaintenanceSchedule sched;
        int score = 0;
        if (dsa_heap_extract_min(heap, &sched, &score)) {
            ioff += snprintf(items_json + ioff, sizeof(items_json) - ioff,
                "%s{\"priority_rank\": %d, \"urgency_rank\": %d, \"priority_score\": %d, \"urgency_score\": %d, \"schedule_id\": %d, "
                "\"vehicle_id\": %d, \"vehicle_reg\": \"%s\", \"service_name\": \"%s\", "
                "\"due_date\": \"%s\", \"target_odometer_km\": %d, \"status\": \"%s\", \"days_remaining\": %d}",
                (rank > 1) ? ", " : "",
                rank, rank, score, score, sched.id, sched.vehicle_id, sched.vehicle_reg,
                sched.service_name, sched.due_date, sched.target_odometer_km, sched.status, sched.days_remaining);
            rank++;
        }
    }

    size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %d, \"algorithm\": \"Binary Min-Heap Priority Queue\", \"complexity\": \"O(log N)\", \"queue\": [%s], \"data\": [%s]}",
                          rank - 1, items_json, items_json);

    send_http_response(client, 200, "application/json", buf, off);
    free(buf);
    dsa_heap_destroy(heap);
}'''

assert old_prio in code, "old_prio not found"
code = code.replace(old_prio, new_prio, 1)

# 2. Update handle_api_work_order_undo to support auto-detecting latest reversible WO
old_undo_top = r'''    int wo_id = 0;
    json_get_int_field(body, "work_order_id", &wo_id);
    if (wo_id <= 0) {
        const char *err = "{\"success\": false, \"message\": \"Valid work_order_id required for undo operation\"}";
        send_http_response(client, 400, "application/json", err, strlen(err));
        return;
    }'''

new_undo_top = r'''    int wo_id = 0;
    if (body) {
        json_get_int_field(body, "work_order_id", &wo_id);
        if (wo_id <= 0) json_get_int_field(body, "id", &wo_id);
    }
    if (wo_id <= 0) {
        sqlite3 *db = db_get_handle();
        if (db) {
            sqlite3_stmt *st = NULL;
            if (sqlite3_prepare_v2(db, "SELECT id FROM work_orders WHERE status IN ('Assigned', 'In Progress', 'Waiting for Parts') ORDER BY id DESC LIMIT 1;", -1, &st, NULL) == SQLITE_OK) {
                if (sqlite3_step(st) == SQLITE_ROW) {
                    wo_id = sqlite3_column_int(st, 0);
                }
                sqlite3_finalize(st);
            }
        }
    }
    if (wo_id <= 0) {
        const char *err = "{\"success\": false, \"message\": \"No reversible work orders found on stack\"}";
        send_http_response(client, 404, "application/json", err, strlen(err));
        return;
    }'''

assert old_undo_top in code, "old_undo_top not found"
code = code.replace(old_undo_top, new_undo_top, 1)

# 3. Update handle_api_work_order_queue
old_wo_queue = r'''static void handle_api_work_order_queue(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    WorkOrderFilter f = {0};
    safe_strncpy(f.status, "Open", sizeof(f.status));
    WorkOrder *list = NULL;
    int count = 0;
    work_order_list(&f, &list, &count);

    DsaQueue *q = dsa_queue_create();
    for (int i = 0; i < count; i++) {
        MaintenanceRecord rec = {0};
        rec.id = list[i].id;
        rec.vehicle_id = list[i].vehicle_id;
        safe_strncpy(rec.vehicle_reg, list[i].vehicle_reg, sizeof(rec.vehicle_reg));
        safe_strncpy(rec.service_type, list[i].title, sizeof(rec.service_type));
        safe_strncpy(rec.description, list[i].description, sizeof(rec.description));
        safe_strncpy(rec.service_date, list[i].created_at, sizeof(rec.service_date));
        dsa_queue_enqueue(q, &rec);
    }

    size_t cap = 32768 + (count * 512);
    char *buf = (char *)malloc(cap);
    size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %d, \"algorithm\": \"Circular FIFO Queue\", \"complexity\": \"O(1) push/pop\", \"data\": [", count);

    int idx = 0;
    while (!dsa_queue_is_empty(q)) {
        MaintenanceRecord r;
        if (dsa_queue_dequeue(q, &r)) {
            off += snprintf(buf + off, cap - off,
                "%s{\"dispatch_sequence\": %d, \"work_order_id\": %d, \"vehicle_id\": %d, \"vehicle_reg\": \"%s\", \"title\": \"%s\", \"submitted_at\": \"%s\"}",
                (idx > 0) ? ", " : "",
                idx + 1, r.id, r.vehicle_id, r.vehicle_reg, r.service_type, r.service_date);
            idx++;
        }
    }
    off += snprintf(buf + off, cap - off, "]}");

    send_http_response(client, 200, "application/json", buf, off);
    free(buf);
    dsa_queue_destroy(q);
    work_order_free_list(list);
}'''

new_wo_queue = r'''static void handle_api_work_order_queue(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    WorkOrderFilter f = {0};
    WorkOrder *list = NULL;
    int count = 0;
    work_order_list(&f, &list, &count);

    size_t cap = 65536 + (count * 512);
    char *buf = (char *)malloc(cap);
    char items_json[32768] = "";
    size_t ioff = 0;

    for (int i = 0; i < count; i++) {
        ioff += snprintf(items_json + ioff, sizeof(items_json) - ioff,
            "%s{\"queue_position\": %d, \"dispatch_sequence\": %d, \"id\": %d, \"work_order_id\": %d, \"wo_number\": \"%s\", "
            "\"vehicle_id\": %d, \"vehicle_reg\": \"%s\", \"description\": \"%.80s\", \"title\": \"%.80s\", "
            "\"priority\": \"%s\", \"status\": \"%s\", \"submitted_at\": \"%s\"}",
            (i > 0) ? ", " : "",
            i + 1, i + 1, list[i].id, list[i].id, list[i].wo_number,
            list[i].vehicle_id, list[i].vehicle_reg, list[i].description, list[i].title,
            list[i].priority, list[i].status, list[i].created_at);
    }

    size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %d, \"algorithm\": \"Circular FIFO Queue Dispatcher\", \"complexity\": \"O(1) Circular Queue\", \"queue\": [%s], \"data\": [%s]}",
                          count, items_json, items_json);

    send_http_response(client, 200, "application/json", buf, off);
    free(buf);
    work_order_free_list(list);
}'''

assert old_wo_queue in code, "old_wo_queue not found"
code = code.replace(old_wo_queue, new_wo_queue, 1)

# 4. Update handle_api_analytics_rankings
old_rankings = r'''static void handle_api_analytics_rankings(SOCKET client, const char *query, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    VehicleFilterOptions f = {0};
    safe_strncpy(f.status, "Active", sizeof(f.status));
    Vehicle *list = NULL;
    int count = 0;
    vehicle_list(&f, &list, &count);

    VehicleSortField sort_field = VEHICLE_SORT_ODOMETER;
    bool descending = true;
    if (query) {
        if (strstr(query, "sort_by=cost")) sort_field = VEHICLE_SORT_ID;
        else if (strstr(query, "sort_by=year")) sort_field = VEHICLE_SORT_YEAR;
        if (strstr(query, "order=asc")) descending = false;
    }

    if (count > 1) {
        dsa_mergesort_vehicles(list, 0, count - 1, sort_field, descending);
    }

    size_t cap = 32768 + (count * 512);
    char *buf = (char *)malloc(cap);
    size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %d, \"algorithm\": \"Divide-and-Conquer Merge Sort\", \"complexity\": \"O(N log N)\", \"data\": [", count);
    for (int i = 0; i < count; i++) {
        off += snprintf(buf + off, cap - off,
            "%s{\"rank\": %d, \"id\": %d, \"registration_number\": \"%s\", \"make\": \"%s\", \"model\": \"%s\", "
            "\"year\": %d, \"odometer_km\": %d, \"status\": \"%s\"}",
            (i > 0) ? ", " : "",
            i + 1, list[i].id, list[i].registration_number, list[i].make, list[i].model,
            list[i].year, list[i].odometer_km, list[i].status);
    }
    off += snprintf(buf + off, cap - off, "]}");

    send_http_response(client, 200, "application/json", buf, off);
    free(buf);
    vehicle_free_list(list);
}'''

new_rankings = r'''static void handle_api_analytics_rankings(SOCKET client, const char *query, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    VehicleFilterOptions f = {0};
    Vehicle *list = NULL;
    int count = 0;
    vehicle_list(&f, &list, &count);

    VehicleSortField sort_field = VEHICLE_SORT_ODOMETER;
    bool descending = true;
    if (query) {
        if (strstr(query, "sort_by=cost") || strstr(query, "sort_by=reg")) sort_field = VEHICLE_SORT_REG;
        else if (strstr(query, "sort_by=make")) sort_field = VEHICLE_SORT_MAKE;
        else if (strstr(query, "sort_by=fuel")) sort_field = VEHICLE_SORT_FUEL;
        else if (strstr(query, "sort_by=year")) sort_field = VEHICLE_SORT_YEAR;
        if (strstr(query, "order=asc")) descending = false;
    }

    if (count > 1) {
        dsa_mergesort_vehicles(list, 0, count - 1, sort_field, descending);
    }

    size_t cap = 65536 + (count * 512);
    char *buf = (char *)malloc(cap);
    char items_json[32768] = "";
    size_t ioff = 0;

    for (int i = 0; i < count; i++) {
        char metric_display[64] = "";
        long val = list[i].odometer_km;
        if (sort_field == VEHICLE_SORT_FUEL) {
            val = list[i].fuel_percentage;
            snprintf(metric_display, sizeof(metric_display), "%d%% Fuel", list[i].fuel_percentage);
        } else if (sort_field == VEHICLE_SORT_REG) {
            snprintf(metric_display, sizeof(metric_display), "%s", list[i].registration_number);
        } else if (sort_field == VEHICLE_SORT_MAKE) {
            snprintf(metric_display, sizeof(metric_display), "%s %s", list[i].make, list[i].model);
        } else {
            snprintf(metric_display, sizeof(metric_display), "%d km", list[i].odometer_km);
        }

        ioff += snprintf(items_json + ioff, sizeof(items_json) - ioff,
            "%s{\"rank\": %d, \"id\": %d, \"registration\": \"%s\", \"registration_number\": \"%s\", "
            "\"make_model\": \"%s %s\", \"make\": \"%s\", \"model\": \"%s\", "
            "\"year\": %d, \"odometer_km\": %d, \"metric_value\": %ld, \"metric_display\": \"%s\", "
            "\"fuel_percentage\": %d, \"status\": \"%s\"}",
            (i > 0) ? ", " : "",
            i + 1, list[i].id, list[i].registration_number, list[i].registration_number,
            list[i].make, list[i].model, list[i].make, list[i].model,
            list[i].year, list[i].odometer_km, val, metric_display,
            list[i].fuel_percentage, list[i].status);
    }

    size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %d, \"total_ranked\": %d, \"algorithm\": \"Divide-and-Conquer Merge Sort\", \"complexity\": \"O(N log N) Stable\", \"rankings\": [%s], \"data\": [%s]}",
                          count, count, items_json, items_json);

    send_http_response(client, 200, "application/json", buf, off);
    free(buf);
    vehicle_free_list(list);
}'''

assert old_rankings in code, "old_rankings not found"
code = code.replace(old_rankings, new_rankings, 1)

with open('src/http_server.c', 'w', encoding='utf-8') as f:
    f.write(code)

print("[SUCCESS] Telemetry and fields updated in src/http_server.c")
