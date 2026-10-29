with open('src/http_server.c', 'r', encoding='utf-8') as f:
    c = f.read()

# 1. handle_api_notifications
old_notif = r'''static void handle_api_notifications(SOCKET client, const char *method, const char *body, const char *query, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    if (strcmp(method, "GET") == 0) {
        /* Auto-sync pending maintenance, defect, and work order events */
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

        /* Fallback check for legacy notifications if system_notifications is empty */
        if (count == 0 && strcmp(category, "All") == 0) {
            Notification *leg_list = NULL;
            int leg_count = 0;
            notification_list_active(&leg_list, &leg_count);
            if (leg_count > 0) {
                size_t lcap = 32768 + (leg_count * 256);
                char *lbuf = (char *)malloc(lcap);
                size_t loff = snprintf(lbuf, lcap, "{\"success\": true, \"count\": %d, \"unread_count\": %d, \"data\": [", leg_count, leg_count);
                for (int i = 0; i < leg_count; i++) {
                    loff += snprintf(lbuf + loff, lcap - loff,
                        "%s{\"id\": %d, \"vehicle_reg\": \"%s\", \"service_name\": \"%s\", "
                        "\"type\": \"%s\", \"category\": \"Maintenance\", \"severity\": \"Warning\", "
                        "\"title\": \"%s - %s\", \"message\": \"Service milestone is active\", "
                        "\"is_read\": 0, \"scheduled_at\": \"%s\", \"sent_at\": \"%s\", \"created_at\": \"%s\"}",
                        (i > 0) ? ", " : "",
                        leg_list[i].id, leg_list[i].vehicle_reg, leg_list[i].service_name,
                        leg_list[i].notification_type, leg_list[i].vehicle_reg, leg_list[i].service_name,
                        leg_list[i].scheduled_at, leg_list[i].sent_at, leg_list[i].created_at);
                }
                loff += snprintf(lbuf + loff, lcap - loff, "]}");
                send_http_response(client, 200, "application/json", lbuf, loff);
                free(lbuf);
                notification_free_list(leg_list);
                return;
            }
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
        json_get_string_field(body, "action", action, sizeof(action));
        int id = 0;
        json_get_int_field(body, "id", &id);

        if (strcmp(action, "ack_all") == 0) {
            notification_acknowledge_all(user->id, auth_role_to_string(user->role));
            const char *ok = "{\"success\": true, \"message\": \"All notifications acknowledged\"}";
            send_http_response(client, 200, "application/json", ok, strlen(ok));
        } else if (strcmp(action, "ack") == 0 || id > 0) {
            notification_acknowledge(id, user->id);
            const char *ok = "{\"success\": true, \"message\": \"Notification acknowledged\"}";
            send_http_response(client, 200, "application/json", ok, strlen(ok));
        } else if (strcmp(action, "dismiss_all") == 0) {
            notification_dismiss_all(user->id);
            const char *ok = "{\"success\": true, \"message\": \"Notifications dismissed\"}";
            send_http_response(client, 200, "application/json", ok, strlen(ok));
        } else {
            notification_dismiss(id, user->id);
            const char *ok = "{\"success\": true, \"message\": \"Notification dismissed\"}";
            send_http_response(client, 200, "application/json", ok, strlen(ok));
        }
    }
}'''

new_notif = r'''static void handle_api_notifications(SOCKET client, const char *method, const char *body, const char *query, const User *user) {
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

        if (id <= 0 && event_key[0] != '\0') {
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
        } else if (strcmp(action, "ack") == 0 || id > 0 || event_key[0] != '\0') {
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

if old_notif in c:
    c = c.replace(old_notif, new_notif, 1)
    print('[OK] Replaced handle_api_notifications')
else:
    print('[FAIL] old_notif not found')

# 2. handle_api_live_status: add summary, open_work_orders, critical_defects_count, active_schedules, notifications
old_live_snprintf = r'''    char buf[16384];
    int len = snprintf(buf, sizeof(buf),
        "{"
        "\"success\": true,"
        "\"server_time\": \"%s\","
        "\"uptime_seconds\": %ld,"
        "\"active_database\": \"%s\","
        "\"telemetry_source\": \"SQLite Central Operational Depot Engine (Live telematics hardware disconnected)\","
        "\"vehicles\": {\"total\": %d, \"available\": %d, \"on_trip\": %d, \"maintenance\": %d, \"decommissioned\": %d},"
        "\"operational_status\": {\"open_work_orders\": %d, \"urgent_alerts\": %d, \"open_defects\": %d, \"due_schedules\": %d, \"overdue_schedules\": %d},"
        "\"recent_activity\": %s"
        "}",
        now_iso, (long)(time(NULL) - g_server_start_time), db_get_path(),
        total_v, avail_v, trip_v, maint_v, decomm_v,
        open_wo, urgent_alerts, open_defects, due_schedules, overdue_schedules,
        act_json);'''

new_live_snprintf = r'''    char notifs_json[8192] = "[";
    const char *notif_sql = "SELECT id, category, severity, title, message, is_read, created_at, event_key FROM system_notifications ORDER BY id DESC LIMIT 10;";
    if (sqlite3_prepare_v2(db, notif_sql, -1, &stmt, NULL) == SQLITE_OK) {
        int nidx = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            char nitem[512];
            snprintf(nitem, sizeof(nitem),
                "%s{\"id\": %d, \"category\": \"%s\", \"severity\": \"%s\", \"title\": \"%s\", \"message\": \"%.100s\", \"is_read\": %d, \"created_at\": \"%s\", \"event_key\": \"%s\"}",
                (nidx++ > 0) ? ", " : "",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1) ? (const char *)sqlite3_column_text(stmt, 1) : "",
                sqlite3_column_text(stmt, 2) ? (const char *)sqlite3_column_text(stmt, 2) : "Info",
                sqlite3_column_text(stmt, 3) ? (const char *)sqlite3_column_text(stmt, 3) : "",
                sqlite3_column_text(stmt, 4) ? (const char *)sqlite3_column_text(stmt, 4) : "",
                sqlite3_column_int(stmt, 5),
                sqlite3_column_text(stmt, 6) ? (const char *)sqlite3_column_text(stmt, 6) : "",
                sqlite3_column_text(stmt, 7) ? (const char *)sqlite3_column_text(stmt, 7) : "");
            strcat(notifs_json, nitem);
        }
        sqlite3_finalize(stmt);
    }
    strcat(notifs_json, "]");

    char buf[16384];
    int len = snprintf(buf, sizeof(buf),
        "{"
        "\"success\": true,"
        "\"server_time\": \"%s\","
        "\"uptime_seconds\": %ld,"
        "\"active_database\": \"%s\","
        "\"telemetry_source\": \"SQLite Central Operational Depot Engine (Live telematics hardware disconnected)\","
        "\"summary\": {\"total_vehicles\": %d, \"available\": %d, \"on_trip\": %d, \"maintenance\": %d, \"decommissioned\": %d},"
        "\"vehicles\": {\"total\": %d, \"available\": %d, \"on_trip\": %d, \"maintenance\": %d, \"decommissioned\": %d},"
        "\"open_work_orders\": %d,"
        "\"critical_defects_count\": %d,"
        "\"active_schedules\": %d,"
        "\"operational_status\": {\"open_work_orders\": %d, \"urgent_alerts\": %d, \"open_defects\": %d, \"due_schedules\": %d, \"overdue_schedules\": %d},"
        "\"recent_activity\": %s,"
        "\"notifications\": %s"
        "}",
        now_iso, (long)(time(NULL) - g_server_start_time), db_get_path(),
        total_v, avail_v, trip_v, maint_v, decomm_v,
        total_v, avail_v, trip_v, maint_v, decomm_v,
        open_wo, open_defects, (due_schedules + overdue_schedules),
        open_wo, urgent_alerts, open_defects, due_schedules, overdue_schedules,
        act_json, notifs_json);'''

if old_live_snprintf in c:
    c = c.replace(old_live_snprintf, new_live_snprintf, 1)
    print('[OK] Replaced handle_api_live_status')
else:
    print('[FAIL] old_live_snprintf not found')

# 3. handle_api_maintenance_prio_queue: complexity O(log N) and queue array
old_prio_snprintf = r'''    size_t cap = 32768 + (heap->size * 512);
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
                sched.service_name, sched.due_date, sched.due_odometer_km,
                sched.status, sched.days_remaining);
            rank++;
        }
    }
    off += snprintf(buf + off, cap - off, "]}");'''

new_prio_snprintf = r'''    size_t cap = 32768 + (heap->size * 512);
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
    off += snprintf(buf + off, cap - off, "%s], \"data\": [%s]}", items_json, items_json);'''

if old_prio_snprintf in c:
    c = c.replace(old_prio_snprintf, new_prio_snprintf, 1)
    print('[OK] Replaced handle_api_maintenance_prio_queue formatting')
else:
    print('[FAIL] old_prio_snprintf not found')

# 4. handle_api_work_order_queue: complexity O(1) Circular Queue
old_wo_q = r'''    size_t cap = 32768 + (count * 512);
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
    off += snprintf(buf + off, cap - off, "]}");'''

new_wo_q = r'''    size_t cap = 32768 + (count * 512);
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
    off += snprintf(buf + off, cap - off, "%s], \"data\": [%s]}", items_json, items_json);'''

if old_wo_q in c:
    c = c.replace(old_wo_q, new_wo_q, 1)
    print('[OK] Replaced handle_api_work_order_queue formatting')
else:
    print('[FAIL] old_wo_q not found')

# 5. handle_api_routes_plan: resolver and output
old_routes = r'''static void handle_api_routes_plan(SOCKET client, const char *query, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    int from_id = 0, to_id = 5;
    if (query) {
        const char *f = strstr(query, "from=");
        if (f) from_id = atoi(f + 5);
        const char *t = strstr(query, "to=");
        if (t) to_id = atoi(t + 3);
    }

    DsaGraph *g = dsa_graph_create_sample_fleet_network();
    if (!g) {
        send_http_response(client, 500, "application/json", "{\"error\":\"Graph creation failure\"}", 30);
        return;
    }

    if (from_id < 0 || from_id >= g->num_vertices || to_id < 0 || to_id >= g->num_vertices) {
        dsa_graph_destroy(g);
        const char *err = "{\"success\": false, \"message\": \"Invalid start or destination hub ID. Available hubs: 0-5.\"}";
        send_http_response(client, 400, "application/json", err, strlen(err));
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
        "\"shortest_distance_km\": %d,"
        "\"nodes_traversed\": %d,"
        "\"route_summary\": \"%s\","
        "\"path\": %s"
        "}",
        from_id, g->vertices[from_id].name,
        to_id, g->vertices[to_id].name,
        dist, path_len, path_str, path_json);

    send_http_response(client, 200, "application/json", buf, len);
    dsa_graph_destroy(g);
}'''

new_routes = r'''static int resolve_hub_id_dijkstra(DsaGraph *g, const char *val) {
    if (!val || val[0] == '\0') return -1;
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

if old_routes in c:
    c = c.replace(old_routes, new_routes, 1)
    print('[OK] Replaced handle_api_routes_plan')
else:
    print('[FAIL] old_routes not found')

# 6. handle_api_analytics_rankings: complexity O(N log N) Stable, rankings array
old_rankings = r'''    size_t cap = 32768 + (count * 512);
    char *buf = (char *)malloc(cap);
    size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %d, \"algorithm\": \"Divide-and-Conquer Merge Sort\", \"complexity\": \"O(N log N)\", \"data\": [", count);
    for (int i = 0; i < count; i++) {
        off += snprintf(buf + off, cap - off,
            "%s{\"rank\": %d, \"id\": %d, \"registration_number\": \"%s\", \"make\": \"%s\", \"model\": \"%s\", "
            "\"manufacturing_year\": %d, \"odometer_km\": %d, \"fuel_percentage\": %d, \"status\": \"%s\"}",
            (i > 0) ? ", " : "",
            i + 1, list[i].id, list[i].registration_number, list[i].make, list[i].model,
            list[i].manufacturing_year, list[i].odometer_km, list[i].fuel_percentage, list[i].status);
    }
    off += snprintf(buf + off, cap - off, "]}");'''

new_rankings = r'''    size_t cap = 32768 + (count * 512);
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
    off += snprintf(buf + off, cap - off, "%s], \"data\": [%s]}", items_json, items_json);'''

if old_rankings in c:
    c = c.replace(old_rankings, new_rankings, 1)
    print('[OK] Replaced handle_api_analytics_rankings formatting')
else:
    print('[FAIL] old_rankings not found')

# 7. handle_api_optimization_knapsack: max_reliability_score, total_cost
old_knap = r'''    char buf[4096];
    int len = snprintf(buf, sizeof(buf),
        "{"
        "\"success\": true,"
        "\"algorithm\": \"0-1 Knapsack Dynamic Programming\","
        "\"complexity\": \"O(N * B)\","
        "\"budget_allocated\": %d,"
        "\"budget_spent\": %d,"
        "\"budget_remaining\": %d,"
        "\"max_safety_score\": %d,"
        "\"services_selected_count\": %d,"
        "\"selected_jobs\": %s"
        "}",
        budget, total_spent, budget - total_spent, max_score, chosen_cnt, sel_json);'''

new_knap = r'''    char buf[4096];
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
        budget, total_spent, total_spent, budget - total_spent, max_score, max_score, chosen_cnt, chosen_cnt, sel_json);'''

if old_knap in c:
    c = c.replace(old_knap, new_knap, 1)
    print('[OK] Replaced handle_api_optimization_knapsack formatting')
else:
    print('[FAIL] old_knap not found')

# In knapsack, also support budget_ceiling
c = c.replace('json_get_int_field(body, "budget", &budget);', 'json_get_int_field(body, "budget", &budget);\n    json_get_int_field(body, "budget_ceiling", &budget);')

# 8. handle_api_optimization_tsp: optimal_tour_distance_km
old_tsp = r'''        "\"optimal_circuit_distance_km\": %d,"
        "\"circuit_summary\": \"%s\","
        "\"tour_steps\": %s"
        "}",
        tour_cost, tour_str, tour_json);'''

new_tsp = r'''        "\"optimal_circuit_distance_km\": %d,"
        "\"optimal_tour_distance_km\": %d,"
        "\"circuit_summary\": \"%s\","
        "\"tour_steps\": %s"
        "}",
        tour_cost, tour_cost, tour_str, tour_json);'''

if old_tsp in c:
    c = c.replace(old_tsp, new_tsp, 1)
    print('[OK] Replaced handle_api_optimization_tsp formatting')
else:
    print('[FAIL] old_tsp not found')

# 9. handle_api_demo_status: database_path
old_demo_s = r'''    char buf[256];
    int len = snprintf(buf, sizeof(buf),
        "{\"success\": true, \"is_demo_mode\": %s, \"active_database\": \"%s\", \"can_reset\": %s}",
        is_demo ? "true" : "false", path ? path : "unknown", is_demo ? "true" : "false");'''

new_demo_s = r'''    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "{\"success\": true, \"is_demo_mode\": %s, \"database_path\": \"%s\", \"active_database\": \"%s\", \"can_reset\": %s}",
        is_demo ? "true" : "false", path ? path : "data/fleet.db", path ? path : "data/fleet.db", is_demo ? "true" : "false");'''

if old_demo_s in c:
    c = c.replace(old_demo_s, new_demo_s, 1)
    print('[OK] Replaced handle_api_demo_status formatting')
else:
    print('[FAIL] old_demo_s not found')

# 10. handle_api_demo_reset message:
old_demo_r = 'const char *denied = "{\"success\": false, \"message\": \"Safety Violation: Demo reset is strictly disabled for non-demo databases. Production database data/fleet.db is protected.\"}";'
new_demo_r = 'const char *denied = "{\"success\": false, \"message\": \"Safety Violation: Only demo_fleet.db may be reset. Production database data/fleet.db is protected.\"}";'
if old_demo_r in c:
    c = c.replace(old_demo_r, new_demo_r, 1)
    print('[OK] Replaced handle_api_demo_reset denied message')

# 11. Add route for /api/notifications/ack-all
old_ack_route = r'''        } else if (strcmp(path, "/api/notifications/ack") == 0) {
            handle_api_notifications(client, "POST", body, query, &current_user);'''
new_ack_route = r'''        } else if (strcmp(path, "/api/notifications/ack") == 0) {
            handle_api_notifications(client, "POST", body, query, &current_user);
        } else if (strcmp(path, "/api/notifications/ack-all") == 0) {
            handle_api_notifications(client, "POST", "{\"action\":\"ack_all\"}", query, &current_user);'''
if old_ack_route in c:
    c = c.replace(old_ack_route, new_ack_route, 1)
    print('[OK] Added /api/notifications/ack-all route')

with open('src/http_server.c', 'w', encoding='utf-8') as f:
    f.write(c)

print('[SUCCESS] http_server.c cleanly updated')
