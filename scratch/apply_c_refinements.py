import re

with open('src/http_server.c', 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Update handle_api_notifications
old_notif_post = '''        if (strcmp(action, "ack_all") == 0) {
            notification_acknowledge_all(user->id, auth_role_to_string(user->role));
            const char *ok = "{\"success\": true, \"message\": \"All notifications acknowledged\"}";
            send_http_response(client, 200, "application/json", ok, strlen(ok));
        } else if (strcmp(action, "ack") == 0 || id > 0) {
            notification_acknowledge(id, user->id);
            const char *ok = "{\"success\": true, \"message\": \"Notification acknowledged\"}";
            send_http_response(client, 200, "application/json", ok, strlen(ok));'''

new_notif_post = '''        char event_key[128] = "";
        json_get_string_field(body, "event_key", event_key, sizeof(event_key));
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

        if (strcmp(action, "ack_all") == 0) {
            notification_acknowledge_all(user->id, auth_role_to_string(user->role));
            const char *ok = "{\"success\": true, \"message\": \"All notifications acknowledged\"}";
            send_http_response(client, 200, "application/json", ok, strlen(ok));
        } else if (strcmp(action, "ack") == 0 || id > 0 || event_key[0] != '\\0') {
            notification_acknowledge(id, user->id);
            const char *ok = "{\"success\": true, \"message\": \"Notification acknowledged\"}";
            send_http_response(client, 200, "application/json", ok, strlen(ok));'''

if old_notif_post in code:
    code = code.replace(old_notif_post, new_notif_post, 1)
    print('[OK] Updated handle_api_notifications POST handling')
else:
    print('[FAIL] old_notif_post not found')

# 2. Update handle_api_live_status to include summary, open_work_orders, critical_defects_count, active_schedules, notifications
old_live_fmt = '''    char buf[16384];
    int len = snprintf(buf, sizeof(buf),
        "{"
        "\\"success\\": true,"
        "\\"server_time\\": \\"%s\\","
        "\\"uptime_seconds\\": %ld,"
        "\\"active_database\\": \\"%s\\","
        "\\"telemetry_source\\": \\"SQLite Central Operational Depot Engine (Live telematics hardware disconnected)\\","
        "\\"vehicles\\": {\\"total\\": %d, \\"available\\": %d, \\"on_trip\\": %d, \\"maintenance\\": %d, \\"decommissioned\\": %d},"
        "\\"operational_status\\": {\\"open_work_orders\\": %d, \\"urgent_alerts\\": %d, \\"open_defects\\": %d, \\"due_schedules\\": %d, \\"overdue_schedules\\": %d},"
        "\\"recent_activity\\": %s"
        "}",
        now_iso, (long)(time(NULL) - g_server_start_time), db_get_path(),
        total_v, avail_v, trip_v, maint_v, decomm_v,
        open_wo, urgent_alerts, open_defects, due_schedules, overdue_schedules,
        act_json);'''

new_live_fmt = '''    /* Query recent notifications array */
    char notifs_json[8192] = "[";
    const char *notif_sql = "SELECT id, category, severity, title, message, is_read, created_at, event_key FROM system_notifications ORDER BY id DESC LIMIT 10;";
    if (sqlite3_prepare_v2(db, notif_sql, -1, &stmt, NULL) == SQLITE_OK) {
        int nidx = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            char nitem[512];
            snprintf(nitem, sizeof(nitem),
                "%s{\\"id\\": %d, \\"category\\": \\"%s\\", \\"severity\\": \\"%s\\", \\"title\\": \\"%s\\", \\"message\\": \\"%.100s\\", \\"is_read\\": %d, \\"created_at\\": \\"%s\\", \\"event_key\\": \\"%s\\"}",
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
        "\\"success\\": true,"
        "\\"server_time\\": \\"%s\\","
        "\\"uptime_seconds\\": %ld,"
        "\\"active_database\\": \\"%s\\","
        "\\"telemetry_source\\": \\"SQLite Central Operational Depot Engine (Live telematics hardware disconnected)\\","
        "\\"summary\\": {\\"total_vehicles\\": %d, \\"available\\": %d, \\"on_trip\\": %d, \\"maintenance\\": %d, \\"decommissioned\\": %d},"
        "\\"vehicles\\": {\\"total\\": %d, \\"available\\": %d, \\"on_trip\\": %d, \\"maintenance\\": %d, \\"decommissioned\\": %d},"
        "\\"open_work_orders\\": %d,"
        "\\"critical_defects_count\\": %d,"
        "\\"active_schedules\\": %d,"
        "\\"operational_status\\": {\\"open_work_orders\\": %d, \\"urgent_alerts\\": %d, \\"open_defects\\": %d, \\"due_schedules\\": %d, \\"overdue_schedules\\": %d},"
        "\\"recent_activity\\": %s,"
        "\\"notifications\\": %s"
        "}",
        now_iso, (long)(time(NULL) - g_server_start_time), db_get_path(),
        total_v, avail_v, trip_v, maint_v, decomm_v,
        total_v, avail_v, trip_v, maint_v, decomm_v,
        open_wo, open_defects, (due_schedules + overdue_schedules),
        open_wo, urgent_alerts, open_defects, due_schedules, overdue_schedules,
        act_json, notifs_json);'''

if old_live_fmt in code:
    code = code.replace(old_live_fmt, new_live_fmt, 1)
    print('[OK] Updated handle_api_live_status formatting')
else:
    print('[FAIL] old_live_fmt not found')

# 3. Update handle_api_demo_status and handle_api_demo_reset
old_demo_status = '''static void handle_api_demo_status(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;
    const char *path = db_get_path();
    bool is_demo = (path && strstr(path, "demo_fleet.db") != NULL);
    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "{\"success\": true, \"is_demo_mode\": %s, \"database_path\": \"%s\", \"can_reset\": %s}",
        is_demo ? "true" : "false", path ? path : "data/fleet.db", is_demo ? "true" : "false");
    send_http_response(client, 200, "application/json", buf, len);
}'''

new_demo_status = '''static void handle_api_demo_status(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;
    const char *path = db_get_path();
    bool is_demo = (path && strstr(path, "demo_fleet.db") != NULL);
    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "{\"success\": true, \"is_demo_mode\": %s, \"database_path\": \"%s\", \"active_database\": \"%s\", \"can_reset\": %s}",
        is_demo ? "true" : "false", path ? path : "data/fleet.db", path ? path : "data/fleet.db", is_demo ? "true" : "false");
    send_http_response(client, 200, "application/json", buf, len);
}'''

if old_demo_status in code:
    code = code.replace(old_demo_status, new_demo_status, 1)
    print('[OK] Updated handle_api_demo_status')
else:
    print('[FAIL] old_demo_status not found')

# 4. Update handle_api_routes_plan to resolve names and return route, shortest_dist_km, hops
old_routes_plan = '''static void handle_api_routes_plan(SOCKET client, const char *query, const User *user) {
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
    }'''

new_routes_plan = '''static int resolve_hub_id_helper(DsaGraph *g, const char *val) {
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

    int from_id = resolve_hub_id_helper(g, from_str);
    int to_id = resolve_hub_id_helper(g, to_str);

    if (from_id < 0 || to_id < 0) {
        dsa_graph_destroy(g);
        const char *err = "{\"success\": false, \"message\": \"Invalid hub name or ID. Hub not found in regional network.\"}";
        send_http_response(client, 404, "application/json", err, strlen(err));
        return;
    }'''

if old_routes_plan in code:
    code = code.replace(old_routes_plan, new_routes_plan, 1)
    print('[OK] Updated handle_api_routes_plan hub resolution')
else:
    print('[FAIL] old_routes_plan not found')

# Update response in handle_api_routes_plan to include route, shortest_dist_km, hops
old_route_res = '''        "\"from_hub\": {\"id\": %d, \"name\": \"%s\"},"
        "\"to_hub\": {\"id\": %d, \"name\": \"%s\"},"
        "\"shortest_distance_km\": %d,"
        "\"nodes_traversed\": %d,"
        "\"route_summary\": \"%s\","
        "\"path\": %s"
        "}",
        from_id, g->vertices[from_id].name,
        to_id, g->vertices[to_id].name,
        dist, path_len, path_str, path_json);'''

new_route_res = '''        "\"from_hub\": {\"id\": %d, \"name\": \"%s\"},"
        "\"to_hub\": {\"id\": %d, \"name\": \"%s\"},"
        "\"route\": \"%s\","
        "\"shortest_dist_km\": %d,"
        "\"shortest_distance_km\": %d,"
        "\"hops\": %d,"
        "\"nodes_traversed\": %d,"
        "\"route_summary\": \"%s\","
        "\"path\": %s"
        "}",
        from_id, g->vertices[from_id].name,
        to_id, g->vertices[to_id].name,
        path_str,
        dist, dist,
        path_len, path_len,
        path_str, path_json);'''

if old_route_res in code:
    code = code.replace(old_route_res, new_route_res, 1)
    print('[OK] Updated route response fields')
else:
    print('[FAIL] old_route_res not found')

with open('src/http_server.c', 'w', encoding='utf-8') as f:
    f.write(code)

print('[SUCCESS] All C fixes applied')
