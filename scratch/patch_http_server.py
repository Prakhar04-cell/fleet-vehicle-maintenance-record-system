import os

with open('src/http_server.c', 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Replace send_http_response and serve_static_file
old_send_block = """static void send_http_response(SOCKET client, int status_code, const char *content_type, const char *body, size_t body_len) {
    char header[512];
    const char *status_text = (status_code == 200) ? "OK" :
                              (status_code == 201) ? "Created" :
                              (status_code == 400) ? "Bad Request" :
                              (status_code == 403) ? "Forbidden" :
                              (status_code == 404) ? "Not Found" : "Internal Server Error";

    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\\r\\n"
        "Content-Type: %s; charset=utf-8\\r\\n"
        "Content-Length: %zu\\r\\n"
        "Access-Control-Allow-Origin: *\\r\\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\\r\\n"
        "Access-Control-Allow-Headers: Content-Type\\r\\n"
        "Connection: close\\r\\n\\r\\n",
        status_code, status_text, content_type, body_len);

    send(client, header, hlen, 0);
    if (body && body_len > 0) {
        send(client, body, (int)body_len, 0);
    }
}

static void serve_static_file(SOCKET client, const char *filepath, const char *mime_type) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        const char *err = "{\\"error\\": \\"Static file not found\\"}";
        send_http_response(client, 404, "application/json", err, strlen(err));
        return;
    }

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (sz < 0) {
        fclose(fp);
        const char *err = "{\\"error\\": \\"Failed reading file\\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    char *buf = (char *)malloc(sz + 1);
    if (!buf) {
        fclose(fp);
        const char *err = "{\\"error\\": \\"Out of memory\\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    size_t read_bytes = fread(buf, 1, sz, fp);
    fclose(fp);

    send_http_response(client, 200, mime_type, buf, read_bytes);
    free(buf);
}"""

new_send_block = """static int send_all(SOCKET sock, const char *data, size_t total_len) {
    size_t sent_bytes = 0;
    while (sent_bytes < total_len) {
        int to_send = (int)(total_len - sent_bytes);
        if (to_send > 16384) to_send = 16384;
        int n = send(sock, data + sent_bytes, to_send, 0);
        if (n <= 0) {
            return -1;
        }
        sent_bytes += (size_t)n;
    }
    return 0;
}

static void send_http_response_ext(SOCKET client, int status_code, const char *content_type, const char *extra_headers, const char *body, size_t body_len) {
    char header[1024];
    const char *status_text = (status_code == 200) ? "OK" :
                              (status_code == 201) ? "Created" :
                              (status_code == 204) ? "No Content" :
                              (status_code == 400) ? "Bad Request" :
                              (status_code == 401) ? "Unauthorized" :
                              (status_code == 403) ? "Forbidden" :
                              (status_code == 404) ? "Not Found" : "Internal Server Error";

    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\\r\\n"
        "Content-Type: %s; charset=utf-8\\r\\n"
        "Content-Length: %zu\\r\\n"
        "Access-Control-Allow-Origin: *\\r\\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\\r\\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\\r\\n"
        "%s"
        "Connection: close\\r\\n\\r\\n",
        status_code, status_text, content_type, body_len,
        extra_headers ? extra_headers : "");

    send_all(client, header, (size_t)hlen);
    if (body && body_len > 0) {
        send_all(client, body, body_len);
    }
#ifdef _WIN32
    shutdown(client, SD_SEND);
#else
    shutdown(client, SHUT_WR);
#endif
}

static void send_http_response(SOCKET client, int status_code, const char *content_type, const char *body, size_t body_len) {
    send_http_response_ext(client, status_code, content_type, NULL, body, body_len);
}

static void serve_static_file(SOCKET client, const char *filepath, const char *mime_type) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        const char *err = "{\\"error\\": \\"Static file not found\\"}";
        send_http_response(client, 404, "application/json", err, strlen(err));
        return;
    }

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (sz < 0) {
        fclose(fp);
        const char *err = "{\\"error\\": \\"Failed reading file\\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        const char *err = "{\\"error\\": \\"Out of memory\\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    size_t read_bytes = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);

    send_http_response(client, 200, mime_type, buf, read_bytes);
    free(buf);
}"""

if old_send_block in code:
    code = code.replace(old_send_block, new_send_block)
    print("Replaced send block successfully")
else:
    print("Could not find old_send_block!")

# 2. Add Reports and Auth handlers before handle_client_request
new_handlers = """static void handle_api_reports_csv(SOCKET client, const char *query) {
    char type[64] = "vehicles";
    if (query) {
        const char *t = strstr(query, "type=");
        if (t) {
            sscanf(t + 5, "%63[^&]", type);
        }
    }

    char out_path[MAX_BUFFER_MD];
    snprintf(out_path, sizeof(out_path), "reports/export_%s_%ld.csv", type, (long)time(NULL));

    StatusCode sc = STATUS_ERROR;
    if (strcmp(type, "vehicles") == 0) {
        sc = report_csv_vehicle_register(out_path);
    } else if (strcmp(type, "maintenance") == 0) {
        sc = report_csv_maintenance_history(out_path, 0);
    } else if (strcmp(type, "schedules") == 0) {
        sc = report_csv_schedules(out_path);
    } else if (strcmp(type, "expenses") == 0) {
        sc = report_csv_expenses(out_path);
    } else if (strcmp(type, "spending") == 0) {
        sc = report_csv_monthly_spending(out_path);
    } else if (strcmp(type, "vehicle_costs") == 0) {
        sc = report_csv_vehicle_costs(out_path);
    }

    if (sc != STATUS_OK) {
        const char *err = "{\\"success\\": false, \\"message\\": \\"Failed to generate CSV report\\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    FILE *fp = fopen(out_path, "rb");
    if (!fp) {
        const char *err = "{\\"success\\": false, \\"message\\": \\"Report file unreadable\\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        const char *err = "{\\"success\\": false, \\"message\\": \\"Out of memory\\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    size_t rd = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);

    char disp_header[256];
    snprintf(disp_header, sizeof(disp_header), "Content-Disposition: attachment; filename=\\"fleet_%s.csv\\"\\r\\n", type);
    send_http_response_ext(client, 200, "text/csv", disp_header, buf, rd);
    free(buf);
}

static void handle_api_reports_pdf(SOCKET client, const char *query) {
    char type[64] = "fleet";
    int vehicle_id = 0;
    if (query) {
        const char *t = strstr(query, "type=");
        if (t) {
            sscanf(t + 5, "%63[^&]", type);
        }
        const char *v = strstr(query, "vehicle_id=");
        if (v) {
            vehicle_id = atoi(v + 11);
        }
    }

    char out_path[MAX_BUFFER_MD];
    snprintf(out_path, sizeof(out_path), "reports/export_%s_%ld.pdf", type, (long)time(NULL));

    StatusCode sc = STATUS_ERROR;
    if (strcmp(type, "fleet") == 0) {
        sc = report_pdf_fleet_summary(out_path);
    } else if (strcmp(type, "expenses") == 0) {
        sc = report_pdf_expense_summary(out_path);
    } else if (strcmp(type, "maintenance") == 0) {
        sc = report_pdf_vehicle_maintenance(vehicle_id, out_path);
    }

    if (sc != STATUS_OK) {
        const char *err = "{\\"success\\": false, \\"message\\": \\"Failed to generate PDF report\\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    FILE *fp = fopen(out_path, "rb");
    if (!fp) {
        const char *err = "{\\"success\\": false, \\"message\\": \\"PDF file unreadable\\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        const char *err = "{\\"success\\": false, \\"message\\": \\"Out of memory\\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    size_t rd = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);

    char disp_header[256];
    snprintf(disp_header, sizeof(disp_header), "Content-Disposition: attachment; filename=\\"fleet_%s.pdf\\"\\r\\n", type);
    send_http_response_ext(client, 200, "application/pdf", disp_header, buf, rd);
    free(buf);
}

static void handle_api_auth(SOCKET client, const char *method, const char *body, const char *subpath) {
    if (strcmp(method, "POST") == 0 && (strcmp(subpath, "/login") == 0 || strcmp(subpath, "") == 0)) {
        char username[MAX_BUFFER_SM] = {0};
        char password[MAX_BUFFER_SM] = {0};
        json_get_string_field(body, "username", username, sizeof(username));
        json_get_string_field(body, "password", password, sizeof(password));

        if (str_is_blank(username) || str_is_blank(password)) {
            const char *err = "{\\"success\\": false, \\"message\\": \\"Username and password are required\\"}";
            send_http_response(client, 400, "application/json", err, strlen(err));
            return;
        }

        User u = {0};
        StatusCode sc = auth_login(username, password, &u);
        if (sc == STATUS_OK) {
            char resp[512];
            snprintf(resp, sizeof(resp),
                "{\\"success\\": true, \\"message\\": \\"Authentication successful\\", "
                "\\"user\\": {\\"id\\": %d, \\"username\\": \\"%s\\", \\"role\\": \\"%s\\", \\"role_id\\": %d, "
                "\\"can_write\\": %s, \\"can_admin\\": %s}}",
                u.id, u.username, auth_role_to_string(u.role), (int)u.role,
                auth_has_permission(u.role, PERM_VEHICLE_WRITE) ? "true" : "false",
                auth_has_permission(u.role, PERM_USER_MANAGE) ? "true" : "false");
            send_http_response(client, 200, "application/json", resp, strlen(resp));
        } else {
            const char *err = "{\\"success\\": false, \\"message\\": \\"Invalid username or password\\"}";
            send_http_response(client, 401, "application/json", err, strlen(err));
        }
    } else if (strcmp(method, "GET") == 0 && strcmp(subpath, "/users") == 0) {
        User *users = NULL;
        int count = 0;
        StatusCode sc = auth_get_users(&users, &count);
        if (sc != STATUS_OK || !users) {
            const char *empty = "{\\"success\\": true, \\"count\\": 0, \\"data\\": []}";
            send_http_response(client, 200, "application/json", empty, strlen(empty));
            return;
        }

        size_t cap = 4096 + (count * 256);
        char *buf = (char *)malloc(cap);
        if (!buf) {
            auth_free_users(users);
            const char *err = "{\\"error\\": \\"OOM\\"}";
            send_http_response(client, 500, "application/json", err, strlen(err));
            return;
        }

        size_t off = snprintf(buf, cap, "{\\"success\\": true, \\"count\\": %d, \\"data\\": [", count);
        for (int i = 0; i < count; i++) {
            off += snprintf(buf + off, cap - off,
                "%s{\\"id\\": %d, \\"username\\": \\"%s\\", \\"role\\": \\"%s\\", \\"active\\": %s}",
                (i > 0) ? ", " : "",
                users[i].id, users[i].username, auth_role_to_string(users[i].role),
                users[i].active ? "true" : "false");
        }
        off += snprintf(buf + off, cap - off, "]}");

        send_http_response(client, 200, "application/json", buf, off);
        free(buf);
        auth_free_users(users);
    } else {
        const char *err = "{\\"success\\": true, \\"message\\": \\"Auth session active\\"}";
        send_http_response(client, 200, "application/json", err, strlen(err));
    }
}

/* ==================== HTTP REQUEST ROUTER ==================== */"""

router_marker = "/* ==================== HTTP REQUEST ROUTER ==================== */"
if router_marker in code:
    code = code.replace(router_marker, new_handlers)
    print("Added new report & auth handlers")
else:
    print("Could not find router_marker!")

# 3. Add routes in handle_client_request
old_routes = """    /* Routing */
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        serve_static_file(client, "web/index.html", "text/html");
    } else if (strcmp(path, "/dashboard.css") == 0 || strcmp(path, "/css/dashboard.css") == 0) {
        serve_static_file(client, "web/dashboard.css", "text/css");
    } else if (strcmp(path, "/app.js") == 0 || strcmp(path, "/js/app.js") == 0) {
        serve_static_file(client, "web/app.js", "application/javascript");
    } else if (strcmp(path, "/api/dashboard") == 0) {
        handle_api_dashboard(client);
    } else if (strcmp(path, "/api/vehicles") == 0) {
        handle_api_vehicles(client, method, body, query);
    } else if (strcmp(path, "/api/drivers") == 0) {
        handle_api_drivers(client, method, body, query);
    } else if (strcmp(path, "/api/maintenance") == 0) {
        handle_api_maintenance(client, method, body);
    } else if (strcmp(path, "/api/schedules") == 0) {
        handle_api_schedules(client, method, body);
    } else if (strcmp(path, "/api/expenses") == 0) {
        handle_api_expenses(client, method, body);
    } else if (strcmp(path, "/api/notifications") == 0) {
        handle_api_notifications(client);
    } else if (strcmp(path, "/api/dsa/run") == 0) {
        handle_api_dsa_run(client);
    } else if (strcmp(path, "/api/backup") == 0) {
        handle_api_backup(client);
    } else if (strcmp(path, "/api/evaluate") == 0) {
        handle_api_evaluate(client);
    } else if (strcmp(path, "/api/audit") == 0) {
        handle_api_audit(client);
    } else {
        const char *not_found = "{\\"error\\": \\"Endpoint not found\\"}";
        send_http_response(client, 404, "application/json", not_found, strlen(not_found));
    }"""

new_routes = """    /* Routing */
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        serve_static_file(client, "web/index.html", "text/html");
    } else if (strcmp(path, "/dashboard.css") == 0 || strcmp(path, "/css/dashboard.css") == 0) {
        serve_static_file(client, "web/dashboard.css", "text/css");
    } else if (strcmp(path, "/app.js") == 0 || strcmp(path, "/js/app.js") == 0) {
        serve_static_file(client, "web/app.js", "application/javascript");
    } else if (strcmp(path, "/favicon.ico") == 0) {
        send_http_response(client, 204, "image/x-icon", "", 0);
    } else if (strcmp(path, "/api/dashboard") == 0) {
        handle_api_dashboard(client);
    } else if (strcmp(path, "/api/vehicles") == 0) {
        handle_api_vehicles(client, method, body, query);
    } else if (strcmp(path, "/api/drivers") == 0) {
        handle_api_drivers(client, method, body, query);
    } else if (strcmp(path, "/api/maintenance") == 0) {
        handle_api_maintenance(client, method, body);
    } else if (strcmp(path, "/api/schedules") == 0) {
        handle_api_schedules(client, method, body);
    } else if (strcmp(path, "/api/expenses") == 0) {
        handle_api_expenses(client, method, body);
    } else if (strcmp(path, "/api/notifications") == 0) {
        handle_api_notifications(client);
    } else if (strcmp(path, "/api/dsa/run") == 0) {
        handle_api_dsa_run(client);
    } else if (strcmp(path, "/api/backup") == 0) {
        handle_api_backup(client);
    } else if (strcmp(path, "/api/evaluate") == 0) {
        handle_api_evaluate(client);
    } else if (strcmp(path, "/api/audit") == 0) {
        handle_api_audit(client);
    } else if (strcmp(path, "/api/reports/csv") == 0) {
        handle_api_reports_csv(client, query);
    } else if (strcmp(path, "/api/reports/pdf") == 0) {
        handle_api_reports_pdf(client, query);
    } else if (strncmp(path, "/api/auth", 9) == 0) {
        handle_api_auth(client, method, body, path + 9);
    } else if (strcmp(path, "/api/users") == 0) {
        handle_api_auth(client, "GET", body, "/users");
    } else {
        const char *not_found = "{\\"error\\": \\"Endpoint not found\\"}";
        send_http_response(client, 404, "application/json", not_found, strlen(not_found));
    }"""

if old_routes in code:
    code = code.replace(old_routes, new_routes)
    print("Added new routes successfully")
else:
    print("Could not find old_routes!")

# 4. Bind localhost loopback for security
old_bind = "addr.sin_addr.s_addr = htonl(INADDR_ANY);"
new_bind = "addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* Bound to localhost 127.0.0.1 for security */"
if old_bind in code:
    code = code.replace(old_bind, new_bind)
    print("Updated bind address to localhost loopback")
else:
    print("Could not find old_bind!")

with open('src/http_server.c', 'w', encoding='utf-8') as f:
    f.write(code)

print("Saved updated src/http_server.c")
