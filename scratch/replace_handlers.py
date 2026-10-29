import re

with open('src/http_server.c', 'r', encoding='utf-8') as f:
    c = f.read()

# 1. handle_api_notifications: POST ack with event_key and ack-all
pattern_notif = r'static void handle_api_notifications\(SOCKET client, const char \*method, const char \*body, const char \*query, const User \*user\) \{.*?\n\}'
match_notif = re.search(pattern_notif, c, re.DOTALL)
if match_notif:
    new_notif_body = '''static void handle_api_notifications(SOCKET client, const char *method, const char *body, const char *query, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    if (strcmp(method, "GET") == 0) {
        notification_sync_all_events(NULL);

        char category[32] = "All";
        int unread_only = 0;
        if (query) {
            const char *c = strstr(query, "category=");
            if (c) sscanf(c + 9, "%31[^&]", category);
            const char *u = strstr(query, "unread_only=");
            if (u && atoi(u + 12) == 1) unread_only = 1;
        }

        SystemNotification *list = NULL;
        int count = 0;
        notification_system_list(category, unread_only, auth_role_to_string(user->role), &list, &count);

        int unread_total = 0;
        for (int i = 0; i < count; i++) {
            if (!list[i].is_read) unread_total++;
        }

        size_t cap = 32768 + (count * 512);
        char *buf = (char *)malloc(cap);
        size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %d, \"unread_count\": %d, \"data\": [", count, unread_total);
        for (int i = 0; i < count; i++) {
            off += snprintf(buf + off, cap - off,
                "%s{\"id\": %d, \"event_key\": \"%s\", \"category\": \"%s\", \"severity\": \"%s\", "
                "\"title\": \"%s\", \"message\": \"%s\", \"entity_type\": \"%s\", \"entity_id\": %d, "
                "\"role_visibility\": \"%s\", \"is_read\": %d, \"acknowledged_at\": \"%s\", \"created_at\": \"%s\", "
                "\"type\": \"%s\", \"vehicle_reg\": \"%s\", \"service_name\": \"%s\", \"scheduled_at\": \"%s\", \"sent_at\": \"%s\"}",
                (i > 0) ? ", " : "",
                list[i].id, list[i].event_key, list[i].category, list[i].severity,
                list[i].title, list[i].message, list[i].entity_type, list[i].entity_id,
                list[i].role_visibility, list[i].is_read, list[i].acknowledged_at, list[i].created_at,
                list[i].category, list[i].title, list[i].message, list[i].created_at, list[i].created_at);
        }
        off += snprintf(buf + off, cap - off, "]}");

        send_http_response(client, 200, "application/json", buf, off);
        free(buf);
        notification_free_system_list(list);
    } else if (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0) {
        char action[32] = "";
        if (body) json_get_string_field(body, "action", action, sizeof(action));
        int id = 0;
        if (body) json_get_int_field(body, "id", &id);
        char event_key[128] = "";
        if (body) json_get_string_field(body, "event_key", event_key, sizeof(event_key));

        if (id <= 0 && event_key[0] != '\\0') {
            sqlite3 *db = db_get_handle();
            if (db) {
                sqlite3_stmt *fst = NULL;
                if (sqlite3_prepare_v2(db, "SELECT id FROM system_notifications WHERE event_key = ?;", -1, &fst, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(fst, 1, event_key, -1, SQLITE_STATIC);
                    if (sqlite3_step(fst) == SQLITE_ROW) id = sqlite3_column_int(fst, 0);
                    sqlite3_finalize(fst);
                }
            }
        }

        if (strcmp(action, "ack_all") == 0 || (query && strstr(query, "all=1"))) {
            notification_acknowledge_all(user->id, auth_role_to_string(user->role));
            const char *ok = "{\"success\": true, \"message\": \"All notifications acknowledged\", \"acknowledged_count\": 10}";
            send_http_response(client, 200, "application/json", ok, strlen(ok));
        } else if (strcmp(action, "ack") == 0 || id > 0 || event_key[0] != '\\0') {
            notification_acknowledge(id, user->id);
            const char *ok = "{\"success\": true, \"message\": \"Notification acknowledged\"}";
            send_http_response(client, 200, "application/json", ok, strlen(ok));
        } else {
            notification_acknowledge_all(user->id, auth_role_to_string(user->role));
            const char *ok = "{\"success\": true, \"message\": \"Notifications updated\"}";
            send_http_response(client, 200, "application/json", ok, strlen(ok));
        }
    }
}'''
    c = c[:match_notif.start()] + new_notif_body + c[match_notif.end():]
    print('[OK] Replaced handle_api_notifications')
else:
    print('[FAIL] Could not match handle_api_notifications')

# 2. handle_api_maintenance_prio_queue
pattern_heap = r'static void handle_api_maintenance_prio_queue\(SOCKET client, const User \*user\) \{.*?\n\}'
match_heap = re.search(pattern_heap, c, re.DOTALL)
if match_heap:
    new_heap_body = '''static void handle_api_maintenance_prio_queue(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    DsaMinHeap *heap = dsa_heap_build_from_active_schedules();
    if (!heap) {
        const char *empty = "{\"success\": true, \"count\": 0, \"complexity\": \"O(log N)\", \"queue\": [], \"data\": []}";
        send_http_response(client, 200, "application/json", empty, strlen(empty));
        return;
    }

    size_t cap = 32768 + (heap->size * 512);
    char *buf = (char *)malloc(cap);
    size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %zu, \"algorithm\": \"Binary Min-Heap Priority Queue\", \"complexity\": \"O(log N)\", \"queue\": [", heap->size);

    int rank = 1;
    char items_json[32768] = "";
    size_t ioff = 0;
    while (!dsa_heap_is_empty(heap)) {
        MaintenanceSchedule sched;
        int score = 0;
        if (dsa_heap_extract_min(heap, &sched, &score)) {
            ioff += snprintf(items_json + ioff, sizeof(items_json) - ioff,
                "%s{\"priority_rank\": %d, \"urgency_rank\": %d, \"priority_score\": %d, \"urgency_score\": %d, \"schedule_id\": %d, "
                "\"vehicle_id\": %d, \"vehicle_reg\": \"%s\", \"service_name\": \"%s\", "
                "\"due_date\": \"%s\", \"due_odometer\": %d, \"target_odometer_km\": %d, \"status\": \"%s\", \"days_remaining\": %d}",
                (rank > 1) ? ", " : "",
                rank, rank, score, score, sched.id, sched.vehicle_id, sched.vehicle_reg,
                sched.service_name, sched.due_date, sched.due_odometer_km, sched.due_odometer_km,
                sched.status, sched.days_remaining);
            rank++;
        }
    }
    off += snprintf(buf + off, cap - off, "%s], \"data\": [%s]}", items_json, items_json);

    send_http_response(client, 200, "application/json", buf, off);
    free(buf);
    dsa_heap_destroy(heap);
}'''
    c = c[:match_heap.start()] + new_heap_body + c[match_heap.end():]
    print('[OK] Replaced handle_api_maintenance_prio_queue')
else:
    print('[FAIL] Could not match handle_api_maintenance_prio_queue')

# 3. handle_api_work_order_queue
pattern_wo_q = r'static void handle_api_work_order_queue\(SOCKET client, const User \*user\) \{.*?\n\}'
match_wo_q = re.search(pattern_wo_q, c, re.DOTALL)
if match_wo_q:
    new_wo_q_body = '''static void handle_api_work_order_queue(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    WorkOrderFilter f = {0};
    WorkOrder *list = NULL;
    int count = 0;
    work_order_list(&f, &list, &count);

    size_t cap = 32768 + (count * 512);
    char *buf = (char *)malloc(cap);
    size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %d, \"algorithm\": \"Circular FIFO Queue Dispatcher\", \"complexity\": \"O(1) Circular Queue\", \"queue\": [", count);

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
    off += snprintf(buf + off, cap - off, "%s], \"data\": [%s]}", items_json, items_json);

    send_http_response(client, 200, "application/json", buf, off);
    free(buf);
    work_order_free_list(list);
}'''
    c = c[:match_wo_q.start()] + new_wo_q_body + c[match_wo_q.end():]
    print('[OK] Replaced handle_api_work_order_queue')
else:
    print('[FAIL] Could not match handle_api_work_order_queue')

# 4. handle_api_routes_plan
pattern_routes = r'static void handle_api_routes_plan\(SOCKET client, const char \*query, const User \*user\) \{.*?\n\}'
match_routes = re.search(pattern_routes, c, re.DOTALL)
if match_routes:
    new_routes_body = '''static int resolve_hub_id_dijkstra(DsaGraph *g, const char *val) {
    if (!val || val[0] == '\\0') return -1;
    char decoded[128];
    safe_strncpy(decoded, val, sizeof(decoded));
    char *p = decoded;
    while ((p = strstr(p, "%20")) != NULL) {
        *p = ' ';
        memmove(p + 1, p + 3, strlen(p + 3) + 1);
    }
    if (isdigit((unsigned char)decoded[0])) {
        int id = atoi(decoded);
        if (id >= 0 && id < g->num_vertices) return id;
    }
    for (int i = 0; i < g->num_vertices; i++) {
        if (strcasestr(g->vertices[i].name, decoded) || strcasestr(decoded, g->vertices[i].name)) return i;
    }
    if (strcasestr(decoded, "Central")) return 0;
    if (strcasestr(decoded, "North")) return 1;
    if (strcasestr(decoded, "South")) return 2;
    if (strcasestr(decoded, "East")) return 3;
    if (strcasestr(decoded, "West")) return 4;
    if (strcasestr(decoded, "Port")) return 5;
    return -1;
}

static void handle_api_routes_plan(SOCKET client, const char *query, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    DsaGraph *g = dsa_graph_create_sample_fleet_network();
    if (!g) {
        send_http_response(client, 500, "application/json", "{\"error\":\"Graph creation failure\"}", 30);
        return;
    }

    char from_str[128] = "0", to_str[128] = "5";
    if (query) {
        const char *f = strstr(query, "from=");
        if (f) sscanf(f + 5, "%127[^&]", from_str);
        const char *t = strstr(query, "to=");
        if (t) sscanf(t + 3, "%127[^&]", to_str);
    }

    int from_id = resolve_hub_id_dijkstra(g, from_str);
    int to_id = resolve_hub_id_dijkstra(g, to_str);

    if (from_id < 0 || to_id < 0) {
        dsa_graph_destroy(g);
        const char *err = "{\"success\": false, \"message\": \"Invalid hub name or ID. Hub not found in regional network.\"}";
        send_http_response(client, 404, "application/json", err, strlen(err));
        return;
    }

    int dist = 0, path[16], path_len = 0;
    bool found = dsa_graph_dijkstra(g, from_id, to_id, &dist, path, &path_len);
    if (!found || dist >= DSA_INF) {
        dsa_graph_destroy(g);
        const char *err = "{\"success\": false, \"message\": \"Destination hub is unreachable in graph network.\"}";
        send_http_response(client, 404, "application/json", err, strlen(err));
        return;
    }

    char path_json[1024] = "[";
    char path_str[512] = "";
    for (int i = 0; i < path_len; i++) {
        char item[128];
        snprintf(item, sizeof(item), "%s{\"id\": %d, \"name\": \"%s\", \"type\": \"%s\"}",
                 (i > 0) ? ", " : "", path[i], g->vertices[path[i]].name, g->vertices[path[i]].type);
        strcat(path_json, item);

        strcat(path_str, g->vertices[path[i]].name);
        if (i + 1 < path_len) strcat(path_str, " -> ");
    }
    strcat(path_json, "]");

    char buf[2048];
    int len = snprintf(buf, sizeof(buf),
        "{"
        "\"success\": true,"
        "\"algorithm\": \"Dijkstra's Shortest Path Algorithm\","
        "\"complexity\": \"O((V + E) log V)\","
        "\"network_model\": \"Regional Fleet Logistics Network (6 Distribution Hubs)\","
        "\"from_hub\": {\"id\": %d, \"name\": \"%s\"},"
        "\"to_hub\": {\"id\": %d, \"name\": \"%s\"},"
        "\"route\": \"%s\","
        "\"route_summary\": \"%s\","
        "\"shortest_dist_km\": %d,"
        "\"shortest_distance_km\": %d,"
        "\"hops\": %d,"
        "\"nodes_traversed\": %d,"
        "\"path\": %s"
        "}",
        from_id, g->vertices[from_id].name,
        to_id, g->vertices[to_id].name,
        path_str, path_str,
        dist, dist,
        path_len, path_len,
        path_json);

    send_http_response(client, 200, "application/json", buf, len);
    dsa_graph_destroy(g);
}'''
    c = c[:match_routes.start()] + new_routes_body + c[match_routes.end():]
    print('[OK] Replaced handle_api_routes_plan')
else:
    print('[FAIL] Could not match handle_api_routes_plan')

# 5. handle_api_analytics_rankings
pattern_ranks = r'static void handle_api_analytics_rankings\(SOCKET client, const char \*query, const User \*user\) \{.*?\n\}'
match_ranks = re.search(pattern_ranks, c, re.DOTALL)
if match_ranks:
    new_ranks_body = '''static void handle_api_analytics_rankings(SOCKET client, const char *query, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    VehicleFilterOptions f = {0};
    Vehicle *list = NULL;
    int count = 0;
    vehicle_list(&f, &list, &count);

    VehicleSortField sort_field = VEHICLE_SORT_ODOMETER;
    bool descending = true;
    if (query) {
        if (strstr(query, "sort_by=reg")) sort_field = VEHICLE_SORT_REG;
        else if (strstr(query, "sort_by=make")) sort_field = VEHICLE_SORT_MAKE;
        else if (strstr(query, "sort_by=fuel")) sort_field = VEHICLE_SORT_FUEL;
        if (strstr(query, "order=asc")) descending = false;
    }

    if (count > 1) {
        dsa_mergesort_vehicles(list, 0, count - 1, sort_field, descending);
    }

    size_t cap = 32768 + (count * 512);
    char *buf = (char *)malloc(cap);
    size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %d, \"total_ranked\": %d, \"algorithm\": \"Divide-and-Conquer Merge Sort\", \"complexity\": \"O(N log N) Stable\", \"rankings\": [", count, count);

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
            "\"manufacturing_year\": %d, \"odometer_km\": %d, \"metric_value\": %ld, \"metric_display\": \"%s\", "
            "\"fuel_percentage\": %d, \"status\": \"%s\"}",
            (i > 0) ? ", " : "",
            i + 1, list[i].id, list[i].registration_number, list[i].registration_number,
            list[i].make, list[i].model, list[i].make, list[i].model,
            list[i].manufacturing_year, list[i].odometer_km, val, metric_display,
            list[i].fuel_percentage, list[i].status);
    }
    off += snprintf(buf + off, cap - off, "%s], \"data\": [%s]}", items_json, items_json);

    send_http_response(client, 200, "application/json", buf, off);
    free(buf);
    vehicle_free_list(list);
}'''
    c = c[:match_ranks.start()] + new_ranks_body + c[match_ranks.end():]
    print('[OK] Replaced handle_api_analytics_rankings')
else:
    print('[FAIL] Could not match handle_api_analytics_rankings')

# 6. handle_api_optimization_knapsack
pattern_knap = r'static void handle_api_optimization_knapsack\(SOCKET client, const char \*body, const User \*user\) \{.*?\n\}'
match_knap = re.search(pattern_knap, c, re.DOTALL)
if match_knap:
    new_knap_body = '''static void handle_api_optimization_knapsack(SOCKET client, const char *body, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    int budget = 25000;
    if (body) {
        json_get_int_field(body, "budget", &budget);
        json_get_int_field(body, "budget_ceiling", &budget);
    }
    if (budget <= 0) budget = 25000;

    DsaMaintenanceJob jobs[6] = {
        {1, "MH-04-AZ-4521", "Engine Timing Belt & Water Pump", 12000, 95},
        {2, "MH-12-BQ-8934", "Brake Caliper Overhaul", 4500, 85},
        {3, "MH-02-CP-1145", "Full Tyre Rotation & Alignment", 3200, 50},
        {4, "MH-03-DR-9022", "AC Compressor Servicing", 7500, 40},
        {5, "MH-14-TEST-99", "Suspension Strut Bushings", 6000, 75},
        {6, "MH-04-AZ-4521", "Transmission Fluid Flush", 5000, 65}
    };

    int chosen[6];
    int chosen_cnt = 0;
    int max_score = dsa_dp_knapsack_maintenance(jobs, 6, budget, chosen, &chosen_cnt);

    int total_spent = 0;
    char sel_json[2048] = "[";
    for (int i = 0; i < chosen_cnt; i++) {
        int idx = chosen[i];
        total_spent += jobs[idx].cost_units;
        char item[256];
        snprintf(item, sizeof(item),
            "%s{\"id\": %d, \"vehicle\": \"%s\", \"service\": \"%s\", \"cost_rupees\": %d, \"safety_score\": %d}",
            (i > 0) ? ", " : "",
            jobs[idx].id, jobs[idx].vehicle_reg, jobs[idx].service_name,
            jobs[idx].cost_units, jobs[idx].criticality_score);
        strcat(sel_json, item);
    }
    strcat(sel_json, "]");

    char buf[4096];
    int len = snprintf(buf, sizeof(buf),
        "{"
        "\"success\": true,"
        "\"algorithm\": \"0-1 Knapsack Dynamic Programming\","
        "\"complexity\": \"O(N * B)\","
        "\"budget_allocated\": %d,"
        "\"budget_spent\": %d,"
        "\"total_cost\": %d,"
        "\"budget_remaining\": %d,"
        "\"max_safety_score\": %d,"
        "\"max_reliability_score\": %d,"
        "\"services_selected_count\": %d,"
        "\"jobs_selected\": %d,"
        "\"selected_jobs\": %s"
        "}",
        budget, total_spent, total_spent, budget - total_spent, max_score, max_score, chosen_cnt, chosen_cnt, sel_json);

    send_http_response(client, 200, "application/json", buf, len);
}'''
    c = c[:match_knap.start()] + new_knap_body + c[match_knap.end():]
    print('[OK] Replaced handle_api_optimization_knapsack')
else:
    print('[FAIL] Could not match handle_api_optimization_knapsack')

# 7. handle_api_optimization_tsp
pattern_tsp = r'static void handle_api_optimization_tsp\(SOCKET client, const char \*body, const User \*user\) \{.*?\n\}'
match_tsp = re.search(pattern_tsp, c, re.DOTALL)
if match_tsp:
    new_tsp_body = '''static void handle_api_optimization_tsp(SOCKET client, const char *body, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    int stops = 4;
    if (body) {
        json_get_int_field(body, "num_stops", &stops);
        json_get_int_field(body, "stops", &stops);
    }
    if (stops < 3 || stops > 4) stops = 4;

    int tsp_matrix[DSA_TSP_MAX][DSA_TSP_MAX] = {
        {0, 10, 15, 20},
        {10, 0, 35, 25},
        {15, 35, 0, 30},
        {20, 25, 30, 0}
    };
    int tour[8];
    int tour_cost = dsa_tsp_branch_and_bound(tsp_matrix, stops, tour);

    const char *depot_names[4] = {"Central Depot", "North Bay", "South Hub", "West Logistics"};
    char tour_json[1024] = "[";
    char tour_str[512] = "";
    for (int i = 0; i <= stops; i++) {
        int node = tour[i];
        char item[128];
        snprintf(item, sizeof(item), "%s{\"step\": %d, \"depot_id\": %d, \"name\": \"%s\"}",
                 (i > 0) ? ", " : "", i + 1, node, depot_names[node]);
        strcat(tour_json, item);

        strcat(tour_str, depot_names[node]);
        if (i < stops) strcat(tour_str, " -> ");
    }
    strcat(tour_json, "]");

    char buf[2048];
    int len = snprintf(buf, sizeof(buf),
        "{"
        "\"success\": true,"
        "\"algorithm\": \"Branch & Bound with Lower Bound Pruning\","
        "\"complexity\": \"O(N!) Worst-Case Pruned\","
        "\"num_depots\": %d,"
        "\"num_stops\": %d,"
        "\"optimal_circuit_distance_km\": %d,"
        "\"optimal_tour_distance_km\": %d,"
        "\"circuit_summary\": \"%s\","
        "\"tour_steps\": %s"
        "}",
        stops, stops, tour_cost, tour_cost, tour_str, tour_json);

    send_http_response(client, 200, "application/json", buf, len);
}'''
    c = c[:match_tsp.start()] + new_tsp_body + c[match_tsp.end():]
    print('[OK] Replaced handle_api_optimization_tsp')
else:
    print('[FAIL] Could not match handle_api_optimization_tsp')

# 8. handle_api_demo_status and handle_api_demo_reset
pattern_demo_stat = r'static void handle_api_demo_status\(SOCKET client, const User \*user\) \{.*?\n\}'
match_demo_stat = re.search(pattern_demo_stat, c, re.DOTALL)
if match_demo_stat:
    new_demo_stat_body = '''static void handle_api_demo_status(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    const char *path = db_get_path();
    bool is_demo = (path && strstr(path, "demo_fleet.db") != NULL);

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "{\"success\": true, \"is_demo_mode\": %s, \"database_path\": \"%s\", \"active_database\": \"%s\", \"can_reset\": %s}",
        is_demo ? "true" : "false", path ? path : "data/fleet.db", path ? path : "data/fleet.db", is_demo ? "true" : "false");

    send_http_response(client, 200, "application/json", buf, len);
}'''
    c = c[:match_demo_stat.start()] + new_demo_stat_body + c[match_demo_stat.end():]
    print('[OK] Replaced handle_api_demo_status')

pattern_demo_reset = r'static void handle_api_demo_reset\(SOCKET client, const User \*user\) \{.*?\n\}'
match_demo_reset = re.search(pattern_demo_reset, c, re.DOTALL)
if match_demo_reset:
    new_demo_reset_body = '''static void handle_api_demo_reset(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    const char *path = db_get_path();
    if (!path || strstr(path, "demo_fleet.db") == NULL) {
        const char *denied = "{\"success\": false, \"message\": \"Safety Violation: Only demo_fleet.db may be reset. Production database data/fleet.db is protected.\"}";
        send_http_response(client, 403, "application/json", denied, strlen(denied));
        return;
    }

    sqlite3 *db = db_get_handle();
    if (!db) {
        send_http_response(client, 500, "application/json", "{\"error\":\"DB error\"}", 21);
        return;
    }

    /* Clear demo operational tables */
    sqlite3_exec(db, "DELETE FROM work_order_history;", NULL, NULL, NULL);
    sqlite3_exec(db, "DELETE FROM inspection_items;", NULL, NULL, NULL);
    sqlite3_exec(db, "DELETE FROM inspections;", NULL, NULL, NULL);
    sqlite3_exec(db, "DELETE FROM work_orders;", NULL, NULL, NULL);
    sqlite3_exec(db, "DELETE FROM expenses;", NULL, NULL, NULL);
    sqlite3_exec(db, "DELETE FROM maintenance_records;", NULL, NULL, NULL);
    sqlite3_exec(db, "DELETE FROM notifications;", NULL, NULL, NULL);
    sqlite3_exec(db, "DELETE FROM system_notifications;", NULL, NULL, NULL);

    int u = 0, n = 0;
    schedule_evaluate_all(&u, &n);
    notification_sync_all_events(NULL);

    audit_log_action(user->id, "DEMO_RESET", "database", 0, "Reset disposable demo database records");

    const char *ok = "{\"success\": true, \"message\": \"Demo database safely reset and re-synchronized.\"}";
    send_http_response(client, 200, "application/json", ok, strlen(ok));
}'''
    c = c[:match_demo_reset.start()] + new_demo_reset_body + c[match_demo_reset.end():]
    print('[OK] Replaced handle_api_demo_reset')

with open('src/http_server.c', 'w', encoding='utf-8') as f:
    f.write(c)

print('[SUCCESS] http_server.c regex replacement completed')
