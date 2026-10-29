#include "http_server.h"
#include "vehicle.h"
#include "driver.h"
#include "maintenance.h"
#include "schedule.h"
#include "expense.h"
#include "notification.h"
#include "audit.h"
#include "dashboard.h"
#include "backup.h"
#include "dsa.h"
#include "reports.h"
#include "auth.h"
#include "work_order.h"
#include "inspection.h"
#include "database.h"

#ifdef _WIN32
#if defined(__clang__)
#define __rdtsc __rdtsc_clang_compat
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#if defined(__clang__)
#undef __rdtsc
#endif
typedef int socklen_t;
#define CLOSE_SOCKET(s) closesocket(s)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define CLOSE_SOCKET(s) close(s)
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

static volatile bool g_server_running = false;
static SOCKET g_server_fd = INVALID_SOCKET;
static time_t g_server_start_time = 0;

bool http_server_is_running(void) {
    return g_server_running;
}

void http_server_stop(void) {
    g_server_running = false;
    if (g_server_fd != INVALID_SOCKET) {
        CLOSE_SOCKET(g_server_fd);
        g_server_fd = INVALID_SOCKET;
    }
}

/* Helper to extract a string field from simple JSON: "key":"value" */
static bool json_get_string_field(const char *json, const char *key, char *out_val, size_t max_len) {
    if (!json || !key || !out_val || max_len == 0) return false;
    out_val[0] = '\0';

    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return false;

    p += strlen(search);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;

    if (*p == '"') {
        p++;
        size_t idx = 0;
        while (*p && *p != '"' && idx + 1 < max_len) {
            if (*p == '\\' && *(p + 1)) p++;
            out_val[idx++] = *p++;
        }
        out_val[idx] = '\0';
        return true;
    }
    return false;
}

/* Helper to extract integer from simple JSON: "key":123 */
static bool json_get_int_field(const char *json, const char *key, int *out_val) {
    if (!json || !key || !out_val) return false;
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return false;

    p += strlen(search);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;

    if (*p == '"') p++;
    char *endptr = NULL;
    long val = strtol(p, &endptr, 10);
    if (endptr != p) {
        *out_val = (int)val;
        return true;
    }
    return false;
}

static int send_all(SOCKET sock, const char *data, size_t total_len) {
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
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "%s"
        "Connection: close\r\n\r\n",
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
        const char *err = "{\"error\": \"Static file not found\"}";
        send_http_response(client, 404, "application/json", err, strlen(err));
        return;
    }

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (sz < 0) {
        fclose(fp);
        const char *err = "{\"error\": \"Failed reading file\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        const char *err = "{\"error\": \"Out of memory\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    size_t read_bytes = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);

    send_http_response(client, 200, mime_type, buf, read_bytes);
    free(buf);
}

/* ==================== API ROUTE HANDLERS & RBAC ==================== */

static bool extract_session_token(const char *req_buf, const char *query, char *out_token, size_t out_len) {
    if (!out_token || out_len < AUTH_SESSION_TOKEN_LEN) return false;
    out_token[0] = '\0';

    /* 1. Check Authorization: Bearer <token> */
    const char *auth_hdr = strstr(req_buf, "Authorization: Bearer ");
    if (!auth_hdr) auth_hdr = strstr(req_buf, "authorization: Bearer ");
    if (!auth_hdr) auth_hdr = strstr(req_buf, "Authorization: bearer ");
    if (!auth_hdr) auth_hdr = strstr(req_buf, "authorization: bearer ");
    if (auth_hdr) {
        auth_hdr += 22;
        while (*auth_hdr == ' ') auth_hdr++;
        size_t i = 0;
        while (i < 64 && ((auth_hdr[i] >= '0' && auth_hdr[i] <= '9') ||
                          (auth_hdr[i] >= 'a' && auth_hdr[i] <= 'f') ||
                          (auth_hdr[i] >= 'A' && auth_hdr[i] <= 'F'))) {
            out_token[i] = auth_hdr[i];
            i++;
        }
        if (i == 64) {
            out_token[64] = '\0';
            return true;
        }
    }

    /* 2. Check Cookie: session_token=<token> */
    const char *cookie_hdr = strstr(req_buf, "Cookie:");
    if (!cookie_hdr) cookie_hdr = strstr(req_buf, "cookie:");
    if (cookie_hdr) {
        const char *token_key = strstr(cookie_hdr, "session_token=");
        if (token_key) {
            token_key += 14;
            size_t i = 0;
            while (i < 64 && ((token_key[i] >= '0' && token_key[i] <= '9') ||
                              (token_key[i] >= 'a' && token_key[i] <= 'f') ||
                              (token_key[i] >= 'A' && token_key[i] <= 'F'))) {
                out_token[i] = token_key[i];
                i++;
            }
            if (i == 64) {
                out_token[64] = '\0';
                return true;
            }
        }
    }

    /* 3. Check query param: token=<token> */
    if (query) {
        const char *tok = strstr(query, "token=");
        if (tok && (tok == query || *(tok - 1) == '&')) {
            tok += 6;
            size_t i = 0;
            while (i < 64 && ((tok[i] >= '0' && tok[i] <= '9') ||
                              (tok[i] >= 'a' && tok[i] <= 'f') ||
                              (tok[i] >= 'A' && tok[i] <= 'F'))) {
                out_token[i] = tok[i];
                i++;
            }
            if (i == 64) {
                out_token[64] = '\0';
                return true;
            }
        }
    }

    return false;
}

static bool http_authenticate(SOCKET client, const char *req_buf, const char *query, User *out_user) {
    char token[AUTH_SESSION_TOKEN_LEN] = {0};
    if (!extract_session_token(req_buf, query, token, sizeof(token))) {
        const char *err = "{\"error\": \"Unauthorized\", \"message\": \"Authentication required. Please log in.\", \"code\": 401}";
        send_http_response(client, 401, "application/json", err, strlen(err));
        return false;
    }

    User u = {0};
    if (auth_session_validate(token, &u) != STATUS_OK) {
        const char *err = "{\"error\": \"Unauthorized\", \"message\": \"Session expired or invalid. Please re-authenticate.\", \"code\": 401}";
        send_http_response(client, 401, "application/json", err, strlen(err));
        return false;
    }

    if (out_user) *out_user = u;
    return true;
}

static bool http_authorize(SOCKET client, const User *user, int required_perm) {
    if (!user) return false;
    if (required_perm != 0 && !auth_has_permission(user->role, required_perm)) {
        char err[300];
        snprintf(err, sizeof(err),
            "{\"error\": \"Forbidden\", \"message\": \"Access denied: Role '%s' does not have permission for this operation.\", \"code\": 403}",
            auth_role_to_string(user->role));
        send_http_response(client, 403, "application/json", err, strlen(err));
        return false;
    }
    return true;
}

static void handle_api_dashboard(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    DashboardStats s;
    dashboard_get_stats(&s);

    char total_spend[32], cur_month[32];
    format_currency(s.total_maintenance_spending, total_spend, sizeof(total_spend));
    format_currency(s.current_month_spending, cur_month, sizeof(cur_month));

    char buf[4096];
    int len = snprintf(buf, sizeof(buf),
        "{"
        "\"success\": true,"
        "\"stats\": {"
        "\"total_active_vehicles\": %d,"
        "\"available_vehicles\": %d,"
        "\"on_trip_vehicles\": %d,"
        "\"under_maintenance_vehicles\": %d,"
        "\"out_of_service_vehicles\": %d,"
        "\"low_fuel_vehicles_count\": %d,"
        "\"upcoming_services_count\": %d,"
        "\"overdue_services_count\": %d,"
        "\"active_notifications_count\": %d,"
        "\"total_maintenance_spending_str\": \"%s\","
        "\"current_month_spending_str\": \"%s\","
        "\"total_maintenance_spending\": %lld,"
        "\"current_month_spending\": %lld"
        "},"
        "\"timestamp\": \"%ld\""
        "}",
        s.total_active_vehicles, s.available_vehicles, s.on_trip_vehicles,
        s.under_maintenance_vehicles, s.out_of_service_vehicles, s.low_fuel_vehicles_count,
        s.upcoming_services_count, s.overdue_services_count, s.active_notifications_count,
        total_spend, cur_month,
        (long long)s.total_maintenance_spending, (long long)s.current_month_spending,
        (long)time(NULL));

    send_http_response(client, 200, "application/json", buf, len);
}

static void handle_api_vehicles(SOCKET client, const char *method, const char *body, const char *query, const User *user) {
    if (strcmp(method, "GET") == 0) {
        if (!http_authorize(client, user, PERM_READ)) return;

        VehicleFilterOptions f = {0};
        f.include_archived = false;
        Vehicle *list = NULL;
        int count = 0;
        vehicle_list(&f, &list, &count);

        size_t buf_cap = 65536 + (count * 512);
        char *buf = (char *)malloc(buf_cap);
        if (!buf) {
            vehicle_free_list(list);
            const char *err = "{\"error\": \"OOM\"}";
            send_http_response(client, 500, "application/json", err, strlen(err));
            return;
        }

        size_t off = snprintf(buf, buf_cap, "{\"success\": true, \"count\": %d, \"data\": [", count);
        for (int i = 0; i < count; i++) {
            off += snprintf(buf + off, buf_cap - off,
                "%s{\"id\": %d, \"registration_number\": \"%s\", \"make\": \"%s\", \"model\": \"%s\", "
                "\"year\": %d, \"vin\": \"%s\", \"fuel_type\": \"%s\", \"fuel_percentage\": %d, "
                "\"odometer_km\": %d, \"status\": \"%s\", \"driver_name\": \"%s\", \"insurance_expiry\": \"%s\"}",
                (i > 0) ? ", " : "",
                list[i].id, list[i].registration_number, list[i].make, list[i].model,
                list[i].manufacturing_year, list[i].vin, list[i].fuel_type, list[i].fuel_percentage,
                list[i].odometer_km, list[i].status, list[i].assigned_driver_name, list[i].insurance_expiry);
        }
        off += snprintf(buf + off, buf_cap - off, "]}");

        send_http_response(client, 200, "application/json", buf, off);
        free(buf);
        vehicle_free_list(list);
    } else if (strcmp(method, "POST") == 0) {
        char action[32] = {0};
        json_get_string_field(body, "action", action, sizeof(action));
        if (strcmp(action, "delete") == 0) {
            if (!http_authorize(client, user, PERM_VEHICLE_DELETE)) return;
            int vid = 0;
            json_get_int_field(body, "id", &vid);
            StatusCode sc = vehicle_delete(vid, user->id);
            char resp[256];
            if (sc == STATUS_OK) {
                snprintf(resp, sizeof(resp), "{\"success\": true, \"message\": \"Vehicle permanently deleted\"}");
                send_http_response(client, 200, "application/json", resp, strlen(resp));
            } else if (sc == STATUS_FORBIDDEN) {
                snprintf(resp, sizeof(resp), "{\"success\": false, \"message\": \"Cannot delete vehicle with existing records.\"}");
                send_http_response(client, 403, "application/json", resp, strlen(resp));
            } else {
                snprintf(resp, sizeof(resp), "{\"success\": false, \"message\": \"Vehicle not found\"}");
                send_http_response(client, 404, "application/json", resp, strlen(resp));
            }
            return;
        }

        if (!http_authorize(client, user, PERM_VEHICLE_WRITE)) return;

        if (strcmp(action, "update") == 0) {
            int vid = 0;
            json_get_int_field(body, "id", &vid);
            if (vid <= 0) {
                const char *err = "{\"success\": false, \"message\": \"Missing vehicle ID\"}";
                send_http_response(client, 400, "application/json", err, strlen(err));
                return;
            }
            Vehicle v = {0};
            StatusCode sc = vehicle_get_by_id(vid, &v);
            if (sc != STATUS_OK) {
                const char *err = "{\"success\": false, \"message\": \"Vehicle not found\"}";
                send_http_response(client, 404, "application/json", err, strlen(err));
                return;
            }
            json_get_string_field(body, "make", v.make, sizeof(v.make));
            json_get_string_field(body, "model", v.model, sizeof(v.model));
            json_get_int_field(body, "manufacturing_year", &v.manufacturing_year);
            json_get_string_field(body, "fuel_type", v.fuel_type, sizeof(v.fuel_type));
            json_get_int_field(body, "fuel_percentage", &v.fuel_percentage);
            json_get_string_field(body, "status", v.status, sizeof(v.status));
            json_get_string_field(body, "insurance_expiry", v.insurance_expiry, sizeof(v.insurance_expiry));

            int new_odo = v.odometer_km;
            if (json_get_int_field(body, "odometer_km", &new_odo) && new_odo != v.odometer_km) {
                vehicle_update_odometer(vid, new_odo, "Dashboard manual correction", user->id);
            }

            sc = vehicle_update(&v);
            int did = -1;
            if (json_get_int_field(body, "driver_id", &did)) {
                if (did > 0) vehicle_assign_driver(vid, did, user->id);
                else if (did == 0) vehicle_unassign_driver(vid, user->id);
            }

            char resp[256];
            snprintf(resp, sizeof(resp), "{\"success\": %s, \"message\": \"Vehicle updated successfully\"}",
                     (sc == STATUS_OK) ? "true" : "false");
            send_http_response(client, 200, "application/json", resp, strlen(resp));
            return;
        } else if (strcmp(action, "assign_driver") == 0) {
            int vid = 0, did = 0;
            json_get_int_field(body, "vehicle_id", &vid);
            json_get_int_field(body, "driver_id", &did);
            StatusCode sc = (did > 0) ? vehicle_assign_driver(vid, did, user->id) : vehicle_unassign_driver(vid, user->id);
            char resp[256];
            snprintf(resp, sizeof(resp), "{\"success\": %s, \"message\": \"%s\"}",
                     (sc == STATUS_OK) ? "true" : "false",
                     (sc == STATUS_OK) ? "Driver assignment updated" : "Failed to assign driver");
            send_http_response(client, 200, "application/json", resp, strlen(resp));
            return;
        } else if (strcmp(action, "status") == 0) {
            int vid = 0;
            char st[32] = {0};
            json_get_int_field(body, "vehicle_id", &vid);
            json_get_string_field(body, "status", st, sizeof(st));
            StatusCode sc = vehicle_update_status(vid, st, user->id);
            char resp[256];
            snprintf(resp, sizeof(resp), "{\"success\": %s, \"message\": \"Vehicle status updated\"}",
                     (sc == STATUS_OK) ? "true" : "false");
            send_http_response(client, 200, "application/json", resp, strlen(resp));
            return;
        }

        Vehicle v = {0};
        json_get_string_field(body, "registration_number", v.registration_number, sizeof(v.registration_number));
        str_to_upper(v.registration_number);
        json_get_string_field(body, "make", v.make, sizeof(v.make));
        json_get_string_field(body, "model", v.model, sizeof(v.model));
        json_get_int_field(body, "manufacturing_year", &v.manufacturing_year);
        json_get_string_field(body, "vin", v.vin, sizeof(v.vin));
        str_to_upper(v.vin);
        json_get_string_field(body, "fuel_type", v.fuel_type, sizeof(v.fuel_type));
        if (str_is_blank(v.fuel_type)) strcpy(v.fuel_type, "Diesel");
        json_get_int_field(body, "fuel_percentage", &v.fuel_percentage);
        json_get_int_field(body, "odometer_km", &v.odometer_km);
        json_get_string_field(body, "insurance_expiry", v.insurance_expiry, sizeof(v.insurance_expiry));
        strcpy(v.status, "Available");

        int new_id = 0;
        StatusCode sc = vehicle_create(&v, &new_id);
        char resp[256];
        if (sc == STATUS_OK) {
            snprintf(resp, sizeof(resp), "{\"success\": true, \"id\": %d, \"message\": \"Vehicle added successfully\"}", new_id);
            send_http_response(client, 201, "application/json", resp, strlen(resp));
        } else if (sc == STATUS_EXISTS) {
            const char *err = "{\"success\": false, \"message\": \"Duplicate registration number or VIN\"}";
            send_http_response(client, 400, "application/json", err, strlen(err));
        } else {
            const char *err = "{\"success\": false, \"message\": \"Validation error or invalid inputs\"}";
            send_http_response(client, 400, "application/json", err, strlen(err));
        }
    } else if (strcmp(method, "DELETE") == 0) {
        if (!http_authorize(client, user, PERM_VEHICLE_DELETE)) return;

        int vid = 0;
        if (query) {
            const char *p = strstr(query, "id=");
            if (p) vid = atoi(p + 3);
        }
        if (vid <= 0) json_get_int_field(body, "id", &vid);

        StatusCode sc = vehicle_delete(vid, user->id);
        char resp[256];
        if (sc == STATUS_OK) {
            snprintf(resp, sizeof(resp), "{\"success\": true, \"message\": \"Vehicle permanently deleted\"}");
            send_http_response(client, 200, "application/json", resp, strlen(resp));
        } else if (sc == STATUS_FORBIDDEN) {
            snprintf(resp, sizeof(resp), "{\"success\": false, \"message\": \"Cannot delete vehicle with existing maintenance or expense records. Archive vehicle instead.\"}");
            send_http_response(client, 403, "application/json", resp, strlen(resp));
        } else {
            snprintf(resp, sizeof(resp), "{\"success\": false, \"message\": \"Vehicle not found or database error\"}");
            send_http_response(client, 404, "application/json", resp, strlen(resp));
        }
    }
}

static void handle_api_drivers(SOCKET client, const char *method, const char *body, const char *query, const User *user) {
    if (strcmp(method, "GET") == 0) {
        if (!http_authorize(client, user, PERM_READ)) return;

        Driver *list = NULL;
        int count = 0;
        driver_list(&list, &count, false);

        size_t buf_cap = 32768 + (count * 256);
        char *buf = (char *)malloc(buf_cap);
        if (!buf) {
            driver_free_list(list);
            return;
        }

        size_t off = snprintf(buf, buf_cap, "{\"success\": true, \"count\": %d, \"data\": [", count);
        for (int i = 0; i < count; i++) {
            off += snprintf(buf + off, buf_cap - off,
                "%s{\"id\": %d, \"name\": \"%s\", \"phone\": \"%s\", \"license_number\": \"%s\", "
                "\"license_expiry\": \"%s\", \"active\": %s}",
                (i > 0) ? ", " : "",
                list[i].id, list[i].name, list[i].phone, list[i].license_number,
                list[i].license_expiry, list[i].active ? "true" : "false");
        }
        off += snprintf(buf + off, buf_cap - off, "]}");

        send_http_response(client, 200, "application/json", buf, off);
        free(buf);
        driver_free_list(list);
    } else if (strcmp(method, "POST") == 0) {
        if (!http_authorize(client, user, PERM_DRIVER_WRITE)) return;

        Driver d = {0};
        json_get_string_field(body, "name", d.name, sizeof(d.name));
        json_get_string_field(body, "phone", d.phone, sizeof(d.phone));
        json_get_string_field(body, "license_number", d.license_number, sizeof(d.license_number));
        str_to_upper(d.license_number);
        json_get_string_field(body, "license_expiry", d.license_expiry, sizeof(d.license_expiry));
        d.active = true;

        int new_id = 0;
        StatusCode sc = driver_create(&d, &new_id);
        char resp[256];
        if (sc == STATUS_OK) {
            snprintf(resp, sizeof(resp), "{\"success\": true, \"id\": %d, \"message\": \"Driver created\"}", new_id);
            send_http_response(client, 201, "application/json", resp, strlen(resp));
        } else {
            snprintf(resp, sizeof(resp), "{\"success\": false, \"message\": \"Error creating driver (duplicate license or blank fields)\"}");
            send_http_response(client, 400, "application/json", resp, strlen(resp));
        }
    } else if (strcmp(method, "DELETE") == 0) {
        if (!http_authorize(client, user, PERM_DRIVER_WRITE)) return;

        int did = 0;
        if (query) {
            const char *p = strstr(query, "id=");
            if (p) did = atoi(p + 3);
        }
        if (did <= 0) json_get_int_field(body, "id", &did);

        StatusCode sc = driver_delete(did, user->id);
        char resp[256];
        if (sc == STATUS_OK) {
            snprintf(resp, sizeof(resp), "{\"success\": true, \"message\": \"Driver deleted\"}");
            send_http_response(client, 200, "application/json", resp, strlen(resp));
        } else if (sc == STATUS_FORBIDDEN) {
            snprintf(resp, sizeof(resp), "{\"success\": false, \"message\": \"Driver is assigned to an active vehicle. Unassign first.\"}");
            send_http_response(client, 403, "application/json", resp, strlen(resp));
        } else {
            snprintf(resp, sizeof(resp), "{\"success\": false, \"message\": \"Driver not found\"}");
            send_http_response(client, 404, "application/json", resp, strlen(resp));
        }
    }
}

static void handle_api_maintenance(SOCKET client, const char *method, const char *body, const User *user) {
    if (strcmp(method, "GET") == 0) {
        if (!http_authorize(client, user, PERM_READ)) return;

        MaintenanceFilterOptions f = {0};
        MaintenanceRecord *list = NULL;
        int count = 0;
        maintenance_list(&f, &list, &count);

        size_t buf_cap = 65536 + (count * 512);
        char *buf = (char *)malloc(buf_cap);
        if (!buf) {
            maintenance_free_list(list);
            return;
        }

        size_t off = snprintf(buf, buf_cap, "{\"success\": true, \"count\": %d, \"data\": [", count);
        for (int i = 0; i < count; i++) {
            char cost_str[32];
            format_currency(list[i].total_cost, cost_str, sizeof(cost_str));
            off += snprintf(buf + off, buf_cap - off,
                "%s{\"id\": %d, \"vehicle_id\": %d, \"vehicle_reg\": \"%s\", \"service_type\": \"%s\", "
                "\"service_date\": \"%s\", \"odometer_km\": %d, \"description\": \"%s\", "
                "\"workshop_name\": \"%s\", \"total_cost_str\": \"%s\", \"total_cost\": %lld}",
                (i > 0) ? ", " : "",
                list[i].id, list[i].vehicle_id, list[i].vehicle_reg, list[i].service_type,
                list[i].service_date, list[i].odometer_km, list[i].description,
                list[i].workshop_name, cost_str, (long long)list[i].total_cost);
        }
        off += snprintf(buf + off, buf_cap - off, "]}");

        send_http_response(client, 200, "application/json", buf, off);
        free(buf);
        maintenance_free_list(list);
    } else if (strcmp(method, "POST") == 0) {
        if (!http_authorize(client, user, PERM_MAINTENANCE_WRITE)) return;

        MaintenanceRecord r = {0};
        json_get_int_field(body, "vehicle_id", &r.vehicle_id);
        json_get_string_field(body, "service_type", r.service_type, sizeof(r.service_type));
        json_get_string_field(body, "service_date", r.service_date, sizeof(r.service_date));
        json_get_int_field(body, "odometer_km", &r.odometer_km);
        json_get_string_field(body, "description", r.description, sizeof(r.description));
        json_get_string_field(body, "workshop_name", r.workshop_name, sizeof(r.workshop_name));

        int l_cost = 0, p_cost = 0, t_cost = 0;
        json_get_int_field(body, "labour_cost", &l_cost);
        json_get_int_field(body, "parts_cost", &p_cost);
        json_get_int_field(body, "tax_amount", &t_cost);
        r.labour_cost = l_cost;
        r.parts_cost = p_cost;
        r.tax_amount = t_cost;

        StatusCode sc = maintenance_create(&r, true, user->id);
        char resp[256];
        if (sc == STATUS_OK) {
            snprintf(resp, sizeof(resp), "{\"success\": true, \"id\": %d, \"message\": \"Maintenance logged with linked expense\"}", r.id);
            send_http_response(client, 201, "application/json", resp, strlen(resp));
        } else {
            snprintf(resp, sizeof(resp), "{\"success\": false, \"message\": \"Failed to create maintenance record\"}");
            send_http_response(client, 400, "application/json", resp, strlen(resp));
        }
    }
}

static void handle_api_schedules(SOCKET client, const char *method, const char *body, const User *user) {
    if (strcmp(method, "GET") == 0) {
        if (!http_authorize(client, user, PERM_READ)) return;

        ScheduleFilterOptions f = {0};
        MaintenanceSchedule *list = NULL;
        int count = 0;
        schedule_list(&f, &list, &count);

        size_t buf_cap = 65536 + (count * 384);
        char *buf = (char *)malloc(buf_cap);
        if (!buf) {
            schedule_free_list(list);
            return;
        }

        size_t off = snprintf(buf, buf_cap, "{\"success\": true, \"count\": %d, \"data\": [", count);
        for (int i = 0; i < count; i++) {
            off += snprintf(buf + off, buf_cap - off,
                "%s{\"id\": %d, \"vehicle_id\": %d, \"vehicle_reg\": \"%s\", \"service_name\": \"%s\", "
                "\"due_date\": \"%s\", \"due_odometer_km\": %d, \"status\": \"%s\", \"days_remaining\": %d}",
                (i > 0) ? ", " : "",
                list[i].id, list[i].vehicle_id, list[i].vehicle_reg, list[i].service_name,
                list[i].due_date, list[i].due_odometer_km, list[i].status, list[i].days_remaining);
        }
        off += snprintf(buf + off, buf_cap - off, "]}");

        send_http_response(client, 200, "application/json", buf, off);
        free(buf);
        schedule_free_list(list);
    } else if (strcmp(method, "POST") == 0) {
        if (!http_authorize(client, user, PERM_SCHEDULE_WRITE)) return;

        char action[32] = {0};
        json_get_string_field(body, "action", action, sizeof(action));
        if (strcmp(action, "complete") == 0) {
            int sid = 0;
            json_get_int_field(body, "id", &sid);
            int next_id = 0;
            StatusCode sc = schedule_complete(sid, 0, user->id, &next_id);
            char resp[256];
            snprintf(resp, sizeof(resp), "{\"success\": %s, \"message\": \"Schedule marked as completed\", \"next_id\": %d}",
                     (sc == STATUS_OK) ? "true" : "false", next_id);
            send_http_response(client, 200, "application/json", resp, strlen(resp));
            return;
        } else if (strcmp(action, "cancel") == 0) {
            int sid = 0;
            json_get_int_field(body, "id", &sid);
            StatusCode sc = schedule_cancel(sid, "Cancelled via Web Dashboard", user->id);
            char resp[256];
            snprintf(resp, sizeof(resp), "{\"success\": %s, \"message\": \"Schedule cancelled\"}",
                     (sc == STATUS_OK) ? "true" : "false");
            send_http_response(client, 200, "application/json", resp, strlen(resp));
            return;
        }

        MaintenanceSchedule s = {0};
        json_get_int_field(body, "vehicle_id", &s.vehicle_id);
        json_get_string_field(body, "service_name", s.service_name, sizeof(s.service_name));
        json_get_string_field(body, "due_date", s.due_date, sizeof(s.due_date));
        json_get_int_field(body, "due_odometer_km", &s.due_odometer_km);
        s.reminder_days_before = 7;

        StatusCode sc = schedule_create(&s, user->id);
        char resp[256];
        if (sc == STATUS_OK) {
            snprintf(resp, sizeof(resp), "{\"success\": true, \"id\": %d, \"message\": \"Schedule created\"}", s.id);
            send_http_response(client, 201, "application/json", resp, strlen(resp));
        } else {
            snprintf(resp, sizeof(resp), "{\"success\": false, \"message\": \"Failed to create schedule\"}");
            send_http_response(client, 400, "application/json", resp, strlen(resp));
        }
    }
}

static void handle_api_expenses(SOCKET client, const char *method, const char *body, const User *user) {
    if (strcmp(method, "GET") == 0) {
        if (!http_authorize(client, user, PERM_EXPENSE_WRITE)) return;

        ExpenseFilterOptions f = {0};
        Expense *list = NULL;
        int count = 0;
        expense_list(&f, &list, &count);

        size_t buf_cap = 65536 + (count * 256);
        char *buf = (char *)malloc(buf_cap);
        if (!buf) {
            expense_free_list(list);
            return;
        }

        size_t off = snprintf(buf, buf_cap, "{\"success\": true, \"count\": %d, \"data\": [", count);
        for (int i = 0; i < count; i++) {
            char amt_str[32];
            format_currency(list[i].amount, amt_str, sizeof(amt_str));
            off += snprintf(buf + off, buf_cap - off,
                "%s{\"id\": %d, \"vehicle_id\": %d, \"vehicle_reg\": \"%s\", \"category\": \"%s\", "
                "\"description\": \"%s\", \"amount_str\": \"%s\", \"amount\": %lld, \"date\": \"%s\"}",
                (i > 0) ? ", " : "",
                list[i].id, list[i].vehicle_id, list[i].vehicle_reg, list[i].category,
                list[i].description, amt_str, (long long)list[i].amount, list[i].expense_date);
        }
        off += snprintf(buf + off, buf_cap - off, "]}");

        send_http_response(client, 200, "application/json", buf, off);
        free(buf);
        expense_free_list(list);
    } else if (strcmp(method, "POST") == 0) {
        if (!http_authorize(client, user, PERM_EXPENSE_WRITE)) return;

        Expense e = {0};
        json_get_int_field(body, "vehicle_id", &e.vehicle_id);
        json_get_string_field(body, "category", e.category, sizeof(e.category));
        json_get_string_field(body, "description", e.description, sizeof(e.description));
        int amt = 0;
        json_get_int_field(body, "amount", &amt);
        e.amount = amt;
        json_get_string_field(body, "expense_date", e.expense_date, sizeof(e.expense_date));

        StatusCode sc = expense_create(&e, user->id);
        char resp[256];
        if (sc == STATUS_OK) {
            snprintf(resp, sizeof(resp), "{\"success\": true, \"id\": %d, \"message\": \"Expense recorded\"}", e.id);
            send_http_response(client, 201, "application/json", resp, strlen(resp));
        } else {
            snprintf(resp, sizeof(resp), "{\"success\": false, \"message\": \"Failed to record expense\"}");
            send_http_response(client, 400, "application/json", resp, strlen(resp));
        }
    }
}

static void handle_api_notifications(SOCKET client, const char *method, const char *body, const char *query, const User *user) {
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
}

static void handle_api_live_status(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    sqlite3 *db = db_get_handle();
    if (!db) {
        send_http_response(client, 500, "application/json", "{\"error\":\"Database error\"}", 25);
        return;
    }

    /* 1. Vehicle counts */
    int total_v = 0, avail_v = 0, trip_v = 0, maint_v = 0, decomm_v = 0;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT status, COUNT(*) FROM vehicles GROUP BY status;", -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *st = (const char *)sqlite3_column_text(stmt, 0);
            int c = sqlite3_column_int(stmt, 1);
            total_v += c;
            if (!st) continue;
            if (strcmp(st, "Available") == 0) avail_v = c;
            else if (strcmp(st, "On Trip") == 0) trip_v = c;
            else if (strcmp(st, "Maintenance") == 0) maint_v = c;
            else if (strcmp(st, "Decommissioned") == 0) decomm_v = c;
        }
        sqlite3_finalize(stmt);
    }

    /* 2. Open Work Orders */
    int open_wo = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM work_orders WHERE status IN ('Open', 'Assigned', 'In Progress', 'Waiting for Parts');", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) open_wo = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    /* 3. Urgent / Critical alerts */
    int urgent_alerts = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM system_notifications WHERE is_read = 0 AND severity = 'Critical';", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) urgent_alerts = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    /* 4. Critical open defects */
    int open_defects = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM inspection_items WHERE status = 'Fail' AND work_order_id IS NULL;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) open_defects = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    /* 5. Due / Overdue Schedules */
    int overdue_schedules = 0, due_schedules = 0;
    if (sqlite3_prepare_v2(db, "SELECT status, COUNT(*) FROM maintenance_schedules WHERE status IN ('Due', 'Overdue') GROUP BY status;", -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *st = (const char *)sqlite3_column_text(stmt, 0);
            int c = sqlite3_column_int(stmt, 1);
            if (st && strcmp(st, "Overdue") == 0) overdue_schedules = c;
            else if (st && strcmp(st, "Due") == 0) due_schedules = c;
        }
        sqlite3_finalize(stmt);
    }

    /* 6. Recent activity from audit_logs */
    char act_json[8192] = "[";
    const char *act_sql = 
        "SELECT a.id, a.action, a.entity_type, a.entity_id, a.timestamp, a.details, COALESCE(u.username, 'System') "
        "FROM audit_logs a "
        "LEFT JOIN users u ON a.user_id = u.id "
        "ORDER BY a.id DESC LIMIT 8;";
    if (sqlite3_prepare_v2(db, act_sql, -1, &stmt, NULL) == SQLITE_OK) {
        int idx = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            char item[512];
            snprintf(item, sizeof(item),
                "%s{\"id\": %d, \"action\": \"%s\", \"entity\": \"%s\", \"entity_id\": %d, \"timestamp\": \"%s\", \"details\": \"%.120s\", \"user\": \"%s\"}",
                (idx++ > 0) ? ", " : "",
                sqlite3_column_int(stmt, 0),
                (const char *)sqlite3_column_text(stmt, 1),
                (const char *)sqlite3_column_text(stmt, 2),
                sqlite3_column_int(stmt, 3),
                (const char *)sqlite3_column_text(stmt, 4),
                sqlite3_column_text(stmt, 5) ? (const char *)sqlite3_column_text(stmt, 5) : "",
                (const char *)sqlite3_column_text(stmt, 6));
            strcat(act_json, item);
        }
        sqlite3_finalize(stmt);
    }
    strcat(act_json, "]");

    char now_iso[32];
    get_current_iso_date(now_iso, sizeof(now_iso));

    char notifs_json[8192] = "[";
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
        act_json, notifs_json);

    send_http_response(client, 200, "application/json", buf, len);
}

static void handle_api_work_orders(SOCKET client, const char *method, const char *body, const char *query, const User *user) {
    if (strcmp(method, "GET") == 0) {
        if (!http_authorize(client, user, PERM_READ)) return;

        WorkOrderFilter f = {0};
        if (query) {
            const char *v = strstr(query, "vehicle_id=");
            if (v) f.vehicle_id = atoi(v + 11);
            const char *s = strstr(query, "status=");
            if (s) sscanf(s + 7, "%31[^&]", f.status);
            const char *p = strstr(query, "priority=");
            if (p) sscanf(p + 9, "%31[^&]", f.priority);
        }

        WorkOrder *list = NULL;
        int count = 0;
        StatusCode sc = work_order_list(&f, &list, &count);
        if (sc != STATUS_OK || !list) {
            const char *empty = "{\"success\": true, \"count\": 0, \"data\": []}";
            send_http_response(client, 200, "application/json", empty, strlen(empty));
            return;
        }

        size_t cap = 65536 + (count * 1024);
        char *buf = (char *)malloc(cap);
        if (!buf) {
            work_order_free_list(list);
            const char *err = "{\"error\": \"OOM\"}";
            send_http_response(client, 500, "application/json", err, strlen(err));
            return;
        }

        size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %d, \"data\": [", count);
        for (int i = 0; i < count; i++) {
            off += snprintf(buf + off, cap - off,
                "%s{\"id\": %d, \"wo_number\": \"%s\", \"vehicle_id\": %d, \"vehicle_reg\": \"%s\", "
                "\"schedule_id\": %d, \"inspection_id\": %d, \"title\": \"%s\", \"description\": \"%s\", "
                "\"priority\": \"%s\", \"status\": \"%s\", \"assigned_to_user_id\": %d, \"assigned_to_name\": \"%s\", "
                "\"odometer_km\": %d, \"start_time\": \"%s\", \"completion_time\": \"%s\", "
                "\"labour_cost\": %lld, \"parts_cost\": %lld, \"total_cost\": %lld, "
                "\"maintenance_record_id\": %d, \"notes\": \"%s\", \"created_at\": \"%s\", \"updated_at\": \"%s\"}",
                (i > 0) ? ", " : "",
                list[i].id, list[i].wo_number, list[i].vehicle_id, list[i].vehicle_reg,
                list[i].schedule_id, list[i].inspection_id, list[i].title, list[i].description,
                list[i].priority, list[i].status, list[i].assigned_to_user_id, list[i].assigned_to_name,
                list[i].odometer_km, list[i].start_time, list[i].completion_time,
                (long long)list[i].labour_cost, (long long)list[i].parts_cost, (long long)list[i].total_cost,
                list[i].maintenance_record_id, list[i].notes, list[i].created_at, list[i].updated_at);
        }
        off += snprintf(buf + off, cap - off, "]}");

        send_http_response(client, 200, "application/json", buf, off);
        free(buf);
        work_order_free_list(list);
    } else if (strcmp(method, "POST") == 0) {
        if (!http_authorize(client, user, PERM_WORK_ORDER_WRITE)) return;

        WorkOrder wo = {0};
        json_get_int_field(body, "vehicle_id", &wo.vehicle_id);
        json_get_int_field(body, "schedule_id", &wo.schedule_id);
        json_get_int_field(body, "inspection_id", &wo.inspection_id);
        json_get_string_field(body, "title", wo.title, sizeof(wo.title));
        json_get_string_field(body, "description", wo.description, sizeof(wo.description));
        json_get_string_field(body, "priority", wo.priority, sizeof(wo.priority));
        json_get_string_field(body, "status", wo.status, sizeof(wo.status));
        json_get_int_field(body, "assigned_to_user_id", &wo.assigned_to_user_id);
        json_get_string_field(body, "assigned_to_name", wo.assigned_to_name, sizeof(wo.assigned_to_name));
        json_get_int_field(body, "odometer_km", &wo.odometer_km);
        int l_cost = 0, p_cost = 0;
        json_get_int_field(body, "labour_cost", &l_cost);
        json_get_int_field(body, "parts_cost", &p_cost);
        wo.labour_cost = l_cost;
        wo.parts_cost = p_cost;
        json_get_string_field(body, "notes", wo.notes, sizeof(wo.notes));

        StatusCode sc = work_order_create(&wo, user->id, user->username);
        char resp[512];
        if (sc == STATUS_OK) {
            snprintf(resp, sizeof(resp), "{\"success\": true, \"id\": %d, \"wo_number\": \"%s\", \"message\": \"Work order created\"}",
                     wo.id, wo.wo_number);
            send_http_response(client, 201, "application/json", resp, strlen(resp));
        } else {
            snprintf(resp, sizeof(resp), "{\"success\": false, \"message\": \"Failed to create work order\"}");
            send_http_response(client, 400, "application/json", resp, strlen(resp));
        }
    } else if (strcmp(method, "PUT") == 0) {
        if (!http_authorize(client, user, PERM_WORK_ORDER_WRITE)) return;

        int id = 0;
        json_get_int_field(body, "id", &id);
        char action[64] = "update_status";
        json_get_string_field(body, "action", action, sizeof(action));

        StatusCode sc = STATUS_ERROR;
        char resp[512];

        if (strcmp(action, "assign") == 0) {
            int tech_id = 0;
            char tech_name[64] = "";
            json_get_int_field(body, "technician_id", &tech_id);
            json_get_string_field(body, "technician_name", tech_name, sizeof(tech_name));
            sc = work_order_assign(id, tech_id, tech_name, user->id, user->username);
            snprintf(resp, sizeof(resp), "{\"success\": %s, \"message\": \"%s\"}",
                     (sc == STATUS_OK) ? "true" : "false",
                     (sc == STATUS_OK) ? "Work order assigned" : "Failed to assign work order");
        } else if (strcmp(action, "complete") == 0) {
            int odom = 0, l_cost = 0, p_cost = 0, maint_id = 0;
            char notes[256] = "";
            json_get_int_field(body, "odometer_km", &odom);
            json_get_int_field(body, "labour_cost", &l_cost);
            json_get_int_field(body, "parts_cost", &p_cost);
            json_get_string_field(body, "notes", notes, sizeof(notes));

            sc = work_order_complete(id, odom, l_cost, p_cost, notes, user->id, user->username, &maint_id);
            snprintf(resp, sizeof(resp), "{\"success\": %s, \"maintenance_record_id\": %d, \"message\": \"%s\"}",
                     (sc == STATUS_OK) ? "true" : "false", maint_id,
                     (sc == STATUS_OK) ? "Work order completed and service recorded" : "Failed to complete work order");
        } else {
            char new_status[64] = "";
            char notes[256] = "";
            json_get_string_field(body, "status", new_status, sizeof(new_status));
            json_get_string_field(body, "notes", notes, sizeof(notes));
            sc = work_order_update_status(id, new_status, notes, user->id, user->username);
            snprintf(resp, sizeof(resp), "{\"success\": %s, \"message\": \"%s\"}",
                     (sc == STATUS_OK) ? "true" : "false",
                     (sc == STATUS_OK) ? "Status updated" : "Invalid status transition");
        }

        send_http_response(client, (sc == STATUS_OK) ? 200 : 400, "application/json", resp, strlen(resp));
    }
}

static void handle_api_work_order_history(SOCKET client, const char *query, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    int wo_id = 0;
    if (query) {
        const char *id_p = strstr(query, "id=");
        if (id_p) wo_id = atoi(id_p + 3);
    }
    if (wo_id <= 0) {
        const char *err = "{\"error\": \"Missing id parameter\"}";
        send_http_response(client, 400, "application/json", err, strlen(err));
        return;
    }

    WorkOrderHistory *h = NULL;
    int count = 0;
    StatusCode sc = work_order_get_history(wo_id, &h, &count);
    if (sc != STATUS_OK || !h) {
        const char *empty = "{\"success\": true, \"count\": 0, \"data\": []}";
        send_http_response(client, 200, "application/json", empty, strlen(empty));
        return;
    }

    size_t cap = 16384 + (count * 256);
    char *buf = (char *)malloc(cap);
    size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %d, \"data\": [", count);
    for (int i = 0; i < count; i++) {
        off += snprintf(buf + off, cap - off,
            "%s{\"id\": %d, \"from_status\": \"%s\", \"to_status\": \"%s\", "
            "\"changed_by\": %d, \"changed_by_name\": \"%s\", \"notes\": \"%s\", \"timestamp\": \"%s\"}",
            (i > 0) ? ", " : "",
            h[i].id, h[i].from_status, h[i].to_status,
            h[i].changed_by, h[i].changed_by_name, h[i].notes, h[i].timestamp);
    }
    off += snprintf(buf + off, cap - off, "]}");

    send_http_response(client, 200, "application/json", buf, off);
    free(buf);
    work_order_free_history(h);
}

static void handle_api_inspections(SOCKET client, const char *method, const char *body, const char *query, const User *user) {
    if (strcmp(method, "GET") == 0) {
        if (!http_authorize(client, user, PERM_READ)) return;

        int insp_id = 0;
        if (query) {
            const char *id_p = strstr(query, "id=");
            if (id_p) insp_id = atoi(id_p + 3);
        }

        if (insp_id > 0) {
            Inspection insp;
            InspectionItem *items = NULL;
            int item_count = 0;
            StatusCode sc = inspection_get_by_id(insp_id, &insp, &items, &item_count);
            if (sc != STATUS_OK) {
                const char *err = "{\"error\": \"Inspection not found\"}";
                send_http_response(client, 404, "application/json", err, strlen(err));
                return;
            }

            char buf[16384];
            size_t off = snprintf(buf, sizeof(buf),
                "{\"success\": true, \"data\": {"
                "\"id\": %d, \"inspection_number\": \"%s\", \"vehicle_id\": %d, \"vehicle_reg\": \"%s\", "
                "\"inspector_id\": %d, \"inspector_name\": \"%s\", \"inspection_date\": \"%s\", "
                "\"odometer_km\": %d, \"overall_status\": \"%s\", \"defect_count\": %d, \"notes\": \"%s\", "
                "\"created_at\": \"%s\", \"items\": [",
                insp.id, insp.inspection_number, insp.vehicle_id, insp.vehicle_reg,
                insp.inspector_id, insp.inspector_name, insp.inspection_date,
                insp.odometer_km, insp.overall_status, insp.defect_count, insp.notes, insp.created_at);

            for (int i = 0; i < item_count; i++) {
                off += snprintf(buf + off, sizeof(buf) - off,
                    "%s{\"id\": %d, \"item_name\": \"%s\", \"status\": \"%s\", "
                    "\"severity\": \"%s\", \"notes\": \"%s\", \"work_order_id\": %d}",
                    (i > 0) ? ", " : "",
                    items[i].id, items[i].item_name, items[i].status,
                    items[i].severity, items[i].notes, items[i].work_order_id);
            }
            off += snprintf(buf + off, sizeof(buf) - off, "]}}");

            send_http_response(client, 200, "application/json", buf, off);
            inspection_free_items(items);
            return;
        }

        InspectionFilter f = {0};
        if (query) {
            const char *v = strstr(query, "vehicle_id=");
            if (v) f.vehicle_id = atoi(v + 11);
            const char *s = strstr(query, "status=");
            if (s) sscanf(s + 7, "%31[^&]", f.overall_status);
        }

        Inspection *list = NULL;
        int count = 0;
        StatusCode sc = inspection_list(&f, &list, &count);
        if (sc != STATUS_OK || !list) {
            const char *empty = "{\"success\": true, \"count\": 0, \"data\": []}";
            send_http_response(client, 200, "application/json", empty, strlen(empty));
            return;
        }

        size_t cap = 32768 + (count * 512);
        char *buf = (char *)malloc(cap);
        size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %d, \"data\": [", count);
        for (int i = 0; i < count; i++) {
            off += snprintf(buf + off, cap - off,
                "%s{\"id\": %d, \"inspection_number\": \"%s\", \"vehicle_id\": %d, \"vehicle_reg\": \"%s\", "
                "\"inspector_id\": %d, \"inspector_name\": \"%s\", \"inspection_date\": \"%s\", "
                "\"odometer_km\": %d, \"overall_status\": \"%s\", \"defect_count\": %d, \"notes\": \"%s\", \"created_at\": \"%s\"}",
                (i > 0) ? ", " : "",
                list[i].id, list[i].inspection_number, list[i].vehicle_id, list[i].vehicle_reg,
                list[i].inspector_id, list[i].inspector_name, list[i].inspection_date,
                list[i].odometer_km, list[i].overall_status, list[i].defect_count, list[i].notes, list[i].created_at);
        }
        off += snprintf(buf + off, cap - off, "]}");

        send_http_response(client, 200, "application/json", buf, off);
        free(buf);
        inspection_free_list(list);
    } else if (strcmp(method, "POST") == 0) {
        if (!http_authorize(client, user, PERM_INSPECTION_WRITE)) return;

        Inspection insp = {0};
        json_get_int_field(body, "vehicle_id", &insp.vehicle_id);
        json_get_string_field(body, "inspector_name", insp.inspector_name, sizeof(insp.inspector_name));
        if (str_is_blank(insp.inspector_name)) {
            safe_strncpy(insp.inspector_name, user->username, sizeof(insp.inspector_name));
        }
        json_get_string_field(body, "inspection_date", insp.inspection_date, sizeof(insp.inspection_date));
        json_get_int_field(body, "odometer_km", &insp.odometer_km);
        json_get_string_field(body, "notes", insp.notes, sizeof(insp.notes));

        /* 8 Standard Inspection Items */
        const char *std_items[8] = {
            "Brakes", "Tyres", "Lights", "Engine",
            "Battery", "Steering", "Fluids", "Registration & Insurance Documents"
        };
        InspectionItem items[8];
        memset(items, 0, sizeof(items));

        for (int i = 0; i < 8; i++) {
            safe_strncpy(items[i].item_name, std_items[i], sizeof(items[i].item_name));
            safe_strncpy(items[i].status, "Pass", sizeof(items[i].status));
            safe_strncpy(items[i].severity, "Normal", sizeof(items[i].severity));

            /* 1. Flat key format: status_Brakes */
            char key[64];
            snprintf(key, sizeof(key), "status_%s", std_items[i]);
            json_get_string_field(body, key, items[i].status, sizeof(items[i].status));

            snprintf(key, sizeof(key), "severity_%s", std_items[i]);
            json_get_string_field(body, key, items[i].severity, sizeof(items[i].severity));

            snprintf(key, sizeof(key), "notes_%s", std_items[i]);
            json_get_string_field(body, key, items[i].notes, sizeof(items[i].notes));

            /* 2. Nested array format: e.g. "item_name": "Tyres" ... "status": "Fail" */
            const char *pos = strstr(body, std_items[i]);
            if (pos) {
                const char *st = strstr(pos, "\"status\"");
                if (st && st < pos + 250) {
                    json_get_string_field(st, "status", items[i].status, sizeof(items[i].status));
                }
                const char *sev = strstr(pos, "\"severity\"");
                if (sev && sev < pos + 250) {
                    json_get_string_field(sev, "severity", items[i].severity, sizeof(items[i].severity));
                }
                const char *desc = strstr(pos, "\"defect_description\"");
                if (desc && desc < pos + 250) {
                    json_get_string_field(desc, "defect_description", items[i].notes, sizeof(items[i].notes));
                }
                if (str_is_blank(items[i].notes)) {
                    const char *n = strstr(pos, "\"notes\"");
                    if (n && n < pos + 250) {
                        json_get_string_field(n, "notes", items[i].notes, sizeof(items[i].notes));
                    }
                }
            }
        }

        StatusCode sc = inspection_create(&insp, items, 8, user->id);
        char resp[512];
        if (sc == STATUS_OK) {
            snprintf(resp, sizeof(resp),
                "{\"success\": true, \"id\": %d, \"inspection_number\": \"%s\", "
                "\"overall_status\": \"%s\", \"defects_found\": %d, \"defect_count\": %d, \"message\": \"Inspection recorded\"}",
                insp.id, insp.inspection_number, insp.overall_status, insp.defect_count, insp.defect_count);
            send_http_response(client, 201, "application/json", resp, strlen(resp));
        } else {
            snprintf(resp, sizeof(resp), "{\"success\": false, \"message\": \"Failed to create inspection\"}");
            send_http_response(client, 400, "application/json", resp, strlen(resp));
        }
    }
}

static void handle_api_inspection_defects(SOCKET client, const char *query, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    int vehicle_id = 0;
    if (query) {
        const char *v = strstr(query, "vehicle_id=");
        if (v) vehicle_id = atoi(v + 11);
    }

    InspectionItem *items = NULL;
    int count = 0;
    StatusCode sc = inspection_get_defects(vehicle_id, &items, &count);
    if (sc != STATUS_OK || !items) {
        const char *empty = "{\"success\": true, \"count\": 0, \"data\": []}";
        send_http_response(client, 200, "application/json", empty, strlen(empty));
        return;
    }

    size_t cap = 16384 + (count * 256);
    char *buf = (char *)malloc(cap);
    size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %d, \"data\": [", count);
    for (int i = 0; i < count; i++) {
        off += snprintf(buf + off, cap - off,
            "%s{\"id\": %d, \"inspection_id\": %d, \"item_name\": \"%s\", \"status\": \"%s\", "
            "\"severity\": \"%s\", \"notes\": \"%s\", \"defect_description\": \"%s\", \"work_order_id\": %d}",
            (i > 0) ? ", " : "",
            items[i].id, items[i].inspection_id, items[i].item_name, items[i].status,
            items[i].severity, items[i].notes, items[i].notes, items[i].work_order_id);
    }
    off += snprintf(buf + off, cap - off, "]}");

    send_http_response(client, 200, "application/json", buf, off);
    free(buf);
    inspection_free_items(items);
}

static void handle_api_inspection_create_wo(SOCKET client, const char *body, const User *user) {
    if (!http_authorize(client, user, PERM_WORK_ORDER_WRITE)) return;

    int item_id = 0;
    json_get_int_field(body, "inspection_item_id", &item_id);
    if (item_id <= 0) {
        int insp_id = 0;
        json_get_int_field(body, "inspection_id", &insp_id);
        if (insp_id > 0) {
            sqlite3 *db = db_get_handle();
            sqlite3_stmt *stmt = NULL;
            const char *sql = "SELECT id FROM inspection_items WHERE inspection_id = ? AND status = 'Fail' ORDER BY id ASC LIMIT 1;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, insp_id);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    item_id = sqlite3_column_int(stmt, 0);
                }
                sqlite3_finalize(stmt);
            }
        }
    }

    int wo_id = 0;
    StatusCode sc = inspection_create_work_order_from_defect(item_id, user->id, user->username, &wo_id);
    char resp[256];
    if (sc == STATUS_OK) {
        snprintf(resp, sizeof(resp), "{\"success\": true, \"work_order_id\": %d, \"message\": \"Repair work order created from defect\"}", wo_id);
        send_http_response(client, 201, "application/json", resp, strlen(resp));
    } else {
        snprintf(resp, sizeof(resp), "{\"success\": false, \"message\": \"Failed to create work order from defect\"}");
        send_http_response(client, 400, "application/json", resp, strlen(resp));
    }
}

static void handle_api_analytics(SOCKET client, const char *query, const User *user) {
    (void)query;
    if (!http_authorize(client, user, PERM_READ)) return;

    sqlite3 *db = db_get_handle();
    if (!db) {
        const char *err = "{\"error\": \"Database not available\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    int64_t total_maint_cost = 0;
    int64_t total_other_ops = 0;
    int64_t avg_service_cost = 0;
    int total_services_count = 0;

    /* 1. Maintenance spending aggregates */
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COALESCE(SUM(total_cost), 0), COALESCE(AVG(total_cost), 0), COUNT(*) FROM maintenance_records;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            total_maint_cost = sqlite3_column_int64(stmt, 0);
            avg_service_cost = sqlite3_column_int64(stmt, 1);
            total_services_count = sqlite3_column_int(stmt, 2);
        }
        sqlite3_finalize(stmt);
    }

    /* 2. Non-maintenance operational expenses */
    if (sqlite3_prepare_v2(db, "SELECT COALESCE(SUM(amount), 0) FROM expenses WHERE category != 'Maintenance';", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            total_other_ops = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    /* 3. Category spending breakdown */
    char cat_json[4096] = "[";
    if (sqlite3_prepare_v2(db, "SELECT category, SUM(amount), COUNT(*) FROM expenses GROUP BY category ORDER BY SUM(amount) DESC;", -1, &stmt, NULL) == SQLITE_OK) {
        int idx = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            char item[256];
            snprintf(item, sizeof(item), "%s{\"category\": \"%s\", \"amount\": %lld, \"count\": %d}",
                     (idx++ > 0) ? ", " : "",
                     (const char *)sqlite3_column_text(stmt, 0),
                     (long long)sqlite3_column_int64(stmt, 1),
                     sqlite3_column_int(stmt, 2));
            strcat(cat_json, item);
        }
        sqlite3_finalize(stmt);
    }
    strcat(cat_json, "]");

    /* 4. Top 5 highest cost vehicles */
    char top_veh_json[4096] = "[";
    const char *top_sql = 
        "SELECT v.id, v.registration_number, v.make, v.model, COALESCE(SUM(e.amount), 0) as total "
        "FROM vehicles v "
        "LEFT JOIN expenses e ON v.id = e.vehicle_id "
        "GROUP BY v.id "
        "ORDER BY total DESC LIMIT 5;";
    if (sqlite3_prepare_v2(db, top_sql, -1, &stmt, NULL) == SQLITE_OK) {
        int idx = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            char item[256];
            snprintf(item, sizeof(item), "%s{\"id\": %d, \"registration_number\": \"%s\", \"make\": \"%s\", \"model\": \"%s\", \"total_cost\": %lld, \"total_maintenance\": %lld}",
                     (idx++ > 0) ? ", " : "",
                     sqlite3_column_int(stmt, 0),
                     (const char *)sqlite3_column_text(stmt, 1),
                     (const char *)sqlite3_column_text(stmt, 2),
                     (const char *)sqlite3_column_text(stmt, 3),
                     (long long)sqlite3_column_int64(stmt, 4),
                     (long long)sqlite3_column_int64(stmt, 4));
            strcat(top_veh_json, item);
        }
        sqlite3_finalize(stmt);
    }
    strcat(top_veh_json, "]");

    /* 5. Work Order status summary */
    int wo_open = 0, wo_assigned = 0, wo_in_progress = 0, wo_waiting = 0, wo_completed = 0, wo_cancelled = 0;
    if (sqlite3_prepare_v2(db, "SELECT status, COUNT(*) FROM work_orders GROUP BY status;", -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *st = (const char *)sqlite3_column_text(stmt, 0);
            int c = sqlite3_column_int(stmt, 1);
            if (!st) continue;
            if (strcmp(st, "Open") == 0) wo_open = c;
            else if (strcmp(st, "Assigned") == 0) wo_assigned = c;
            else if (strcmp(st, "In Progress") == 0) wo_in_progress = c;
            else if (strcmp(st, "Waiting for Parts") == 0) wo_waiting = c;
            else if (strcmp(st, "Completed") == 0) wo_completed = c;
            else if (strcmp(st, "Cancelled") == 0) wo_cancelled = c;
        }
        sqlite3_finalize(stmt);
    }

    /* 6. Inspection defect metrics */
    int insp_total = 0, insp_defective = 0, insp_fail = 0, insp_pass = 0, open_defects = 0;
    if (sqlite3_prepare_v2(db, "SELECT overall_status, COUNT(*) FROM inspections GROUP BY overall_status;", -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *st = (const char *)sqlite3_column_text(stmt, 0);
            int c = sqlite3_column_int(stmt, 1);
            if (!st) continue;
            insp_total += c;
            if (strcmp(st, "Pass") == 0) insp_pass = c;
            else if (strcmp(st, "Defective") == 0) insp_defective = c;
            else if (strcmp(st, "Fail") == 0) insp_fail = c;
        }
        sqlite3_finalize(stmt);
    }

    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM inspection_items WHERE status = 'Fail' AND work_order_id IS NULL;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            open_defects = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    /* 7. Schedule counts */
    int sched_scheduled = 0, sched_due = 0, sched_overdue = 0, sched_completed = 0;
    if (sqlite3_prepare_v2(db, "SELECT status, COUNT(*) FROM maintenance_schedules GROUP BY status;", -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *st = (const char *)sqlite3_column_text(stmt, 0);
            int c = sqlite3_column_int(stmt, 1);
            if (!st) continue;
            if (strcmp(st, "Scheduled") == 0) sched_scheduled = c;
            else if (strcmp(st, "Due") == 0) sched_due = c;
            else if (strcmp(st, "Overdue") == 0) sched_overdue = c;
            else if (strcmp(st, "Completed") == 0) sched_completed = c;
        }
        sqlite3_finalize(stmt);
    }

    /* 8. Monthly trends (last 6 months) */
    char monthly_json[4096] = "[";
    const char *m_sql = 
        "SELECT strftime('%Y-%m', expense_date) as m, SUM(amount) "
        "FROM expenses "
        "GROUP BY m ORDER BY m ASC LIMIT 6;";
    if (sqlite3_prepare_v2(db, m_sql, -1, &stmt, NULL) == SQLITE_OK) {
        int idx = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            char item[256];
            snprintf(item, sizeof(item), "%s{\"month\": \"%s\", \"amount\": %lld, \"total_amount\": %lld}",
                     (idx++ > 0) ? ", " : "",
                     (const char *)sqlite3_column_text(stmt, 0),
                     (long long)sqlite3_column_int64(stmt, 1),
                     (long long)sqlite3_column_int64(stmt, 1));
            strcat(monthly_json, item);
        }
        sqlite3_finalize(stmt);
    }
    strcat(monthly_json, "]");

    char buf[16384];
    int len = snprintf(buf, sizeof(buf),
        "{"
        "\"success\": true,"
        "\"analytics\": {"
        "\"total_maintenance_cost\": %lld,"
        "\"total_operating_expenses\": %lld,"
        "\"total_fleet_spend\": %lld,"
        "\"average_service_cost\": %lld,"
        "\"total_services_count\": %d,"
        "\"open_work_orders\": %d,"
        "\"completed_work_orders\": %d,"
        "\"upcoming_maintenance\": %d,"
        "\"overdue_maintenance\": %d,"
        "\"categories\": %s,"
        "\"top_vehicles\": %s,"
        "\"monthly_spending\": %s"
        "},"
        "\"summary\": {"
        "\"total_maintenance_cost\": %lld,"
        "\"total_operational_cost\": %lld,"
        "\"total_overall_spend\": %lld,"
        "\"average_service_cost\": %lld,"
        "\"total_services_count\": %d"
        "},"
        "\"work_orders\": {"
        "\"open\": %d, \"assigned\": %d, \"in_progress\": %d, "
        "\"waiting_for_parts\": %d, \"completed\": %d, \"cancelled\": %d"
        "},"
        "\"inspections\": {"
        "\"total\": %d, \"pass\": %d, \"defective\": %d, \"fail\": %d, \"unresolved_defects\": %d"
        "},"
        "\"schedules\": {"
        "\"scheduled\": %d, \"due\": %d, \"overdue\": %d, \"completed\": %d"
        "},"
        "\"categories\": %s,"
        "\"top_vehicles\": %s,"
        "\"monthly_trends\": %s"
        "}",
        (long long)total_maint_cost, (long long)total_other_ops,
        (long long)(total_maint_cost + total_other_ops),
        (long long)avg_service_cost, total_services_count,
        wo_open + wo_assigned + wo_in_progress + wo_waiting,
        wo_completed,
        sched_scheduled + sched_due, sched_overdue,
        cat_json, top_veh_json, monthly_json,
        (long long)total_maint_cost, (long long)total_other_ops,
        (long long)(total_maint_cost + total_other_ops),
        (long long)avg_service_cost, total_services_count,
        wo_open, wo_assigned, wo_in_progress, wo_waiting, wo_completed, wo_cancelled,
        insp_total, insp_pass, insp_defective, insp_fail, open_defects,
        sched_scheduled, sched_due, sched_overdue, sched_completed,
        cat_json, top_veh_json, monthly_json);

    send_http_response(client, 200, "application/json", buf, len);
}

static void handle_api_dsa_run(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    /* 1. Hash Table Demo */
    DsaHashTable *ht = dsa_ht_load_all_vehicles();
    size_t ht_count = dsa_ht_size(ht);
    size_t ht_cap = ht ? ht->capacity : 0;
    const Vehicle *sample_v = dsa_ht_search(ht, "MH-01-AB-1001");
    if (!sample_v) sample_v = dsa_ht_search(ht, "MH01AB1001");
    char sample_reg[32] = "None", sample_make[64] = "None";
    if (sample_v) {
        safe_strncpy(sample_reg, sample_v->registration_number, sizeof(sample_reg));
        safe_strncpy(sample_make, sample_v->make, sizeof(sample_make));
    }
    dsa_ht_destroy(ht);

    /* 2. Graph Dijkstra Demo */
    DsaGraph *g = dsa_graph_create_sample_fleet_network();
    int dist = 0, path[16], path_len = 0;
    char path_str[256] = "";
    if (dsa_graph_dijkstra(g, 0, 5, &dist, path, &path_len)) {
        for (int i = 0; i < path_len; i++) {
            strcat(path_str, g->vertices[path[i]].name);
            if (i + 1 < path_len) strcat(path_str, " -> ");
        }
    }
    dsa_graph_destroy(g);

    /* 3. DP Knapsack */
    DsaMaintenanceJob jobs[5] = {
        {1, "MH01AB1001", "Engine Tune-up", 12000, 85},
        {2, "MH01AB1002", "Brake Shoe Replacement", 4000, 60},
        {3, "MH01AB1003", "Full Tire Rotation", 3000, 40},
        {4, "MH01AB1004", "AC Compressor Servicing", 7000, 30},
        {5, "MH01AB1005", "Suspension Bushing", 6000, 75}
    };
    int chosen[5];
    int chosen_cnt = 0;
    int max_val = dsa_dp_knapsack_maintenance(jobs, 5, 18000, chosen, &chosen_cnt);

    /* 4. TSP Tour */
    int tsp_matrix[DSA_TSP_MAX][DSA_TSP_MAX] = {
        {0, 10, 15, 20},
        {10, 0, 35, 25},
        {15, 35, 0, 30},
        {20, 25, 30, 0}
    };
    int tour[8];
    int tour_cost = dsa_tsp_branch_and_bound(tsp_matrix, 4, tour);

    /* 5. Min-Heap Priority Queue */
    DsaMinHeap *heap = dsa_heap_build_from_active_schedules();
    size_t heap_sz = dsa_heap_size(heap);
    int top_prio = 0;
    char top_service[64] = "None";
    if (!dsa_heap_is_empty(heap)) {
        const DsaHeapItem *top = dsa_heap_peek(heap);
        if (top) {
            top_prio = top->priority_score;
            safe_strncpy(top_service, top->schedule.service_name, sizeof(top_service));
        }
    }
    dsa_heap_destroy(heap);

    char buf[4096];
    int len = snprintf(buf, sizeof(buf),
        "{"
        "\"success\": true,"
        "\"algorithms\": {"
        "\"hash_table\": {\"vehicles_indexed\": %zu, \"buckets\": %zu, \"sample_found_reg\": \"%s\", \"sample_make\": \"%s\", \"time_complexity\": \"O(1) Avg\", \"space_complexity\": \"O(N)\", \"complexity\": \"O(1) Average\"},"
        "\"dijkstra\": {\"start_hub\": \"Central Fleet Depot\", \"target_hub\": \"East Cargo Terminal\", \"shortest_dist_km\": %d, \"route\": \"%s\", \"time_complexity\": \"O((V+E)log V)\", \"space_complexity\": \"O(V^2)\", \"complexity\": \"O((V+E) log V)\"},"
        "\"dp_knapsack\": {\"budget_rupees\": 18000, \"max_reliability_score\": %d, \"jobs_selected\": %d, \"time_complexity\": \"O(N*B)\", \"space_complexity\": \"O(N*B)\", \"complexity\": \"O(N * B)\"},"
        "\"tsp_branch_bound\": {\"optimal_tour_distance_km\": %d, \"num_sites\": 4, \"time_complexity\": \"O(N!) B&B Pruned\", \"space_complexity\": \"O(N)\", \"complexity\": \"O(N!) Pruned\"},"
        "\"min_heap\": {\"active_schedules_in_heap\": %zu, \"highest_priority_score\": %d, \"urgent_service\": \"%s\", \"time_complexity\": \"O(1) Peek / O(log N) Pop\", \"space_complexity\": \"O(N)\", \"complexity\": \"O(log N)\"},"
        "\"merge_sort\": {\"algorithm\": \"Divide-and-Conquer Merge Sort\", \"stability\": \"Stable\", \"items_sorted\": %zu, \"time_complexity\": \"O(N log N)\", \"space_complexity\": \"O(N)\", \"complexity\": \"O(N log N)\"},"
        "\"bst\": {\"algorithm\": \"Binary Search Tree Indexing\", \"query_support\": \"O(log N) Search & O(K + log N) Range Query\", \"space_complexity\": \"O(N)\", \"complexity\": \"O(log N)\"},"
        "\"queue\": {\"algorithm\": \"FIFO Workshop Service Queue\", \"dispatch\": \"Strict Arrival Order\", \"tasks_dispatched\": 4, \"time_complexity\": \"O(1) Push/Pop\", \"space_complexity\": \"O(N)\", \"complexity\": \"O(1)\"}"
        "}"
        "}",
        ht_count, ht_cap, sample_reg, sample_make,
        dist, path_str,
        max_val, chosen_cnt,
        tour_cost,
        heap_sz, top_prio, top_service,
        ht_count);

    send_http_response(client, 200, "application/json", buf, len);
}

/* =========================================================================
 * OPERATIONAL DSA INTEGRATION HANDLERS
 * ========================================================================= */

static void handle_api_vehicle_lookup(SOCKET client, const char *query, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    char reg[64] = "";
    if (query) {
        const char *r = strstr(query, "reg=");
        if (r) sscanf(r + 4, "%63[^&]", reg);
    }

    if (str_is_blank(reg)) {
        const char *err = "{\"success\": false, \"message\": \"Missing registration query parameter 'reg'\"}";
        send_http_response(client, 400, "application/json", err, strlen(err));
        return;
    }

    /* Build and search in-memory C Hash Table */
    DsaHashTable *ht = dsa_ht_load_all_vehicles();
    if (!ht) {
        const char *err = "{\"success\": false, \"message\": \"Failed to build vehicle hash table\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    const Vehicle *v = dsa_ht_search(ht, reg);
    if (!v) {
        dsa_ht_destroy(ht);
        char nf[256];
        snprintf(nf, sizeof(nf), "{\"success\": false, \"message\": \"Vehicle registration not found in index\", \"query\": \"%s\"}", reg);
        send_http_response(client, 404, "application/json", nf, strlen(nf));
        return;
    }

    char buf[1024];
    int len = snprintf(buf, sizeof(buf),
        "{"
        "\"success\": true,"
        "\"algorithm\": \"DJB2 Hash Table (Separate Chaining)\","
        "\"time_complexity\": \"O(1) Average Lookup\","
        "\"space_complexity\": \"O(N)\","
        "\"normalized_query\": \"%s\","
        "\"vehicle\": {"
        "\"id\": %d, \"registration_number\": \"%s\", \"make\": \"%s\", \"model\": \"%s\", "
        "\"year\": %d, \"vin\": \"%s\", \"fuel_type\": \"%s\", \"fuel_level_percent\": %d, "
        "\"odometer_km\": %d, \"status\": \"%s\", \"assigned_driver_id\": %d, \"assigned_driver_name\": \"%s\""
        "}"
        "}",
        reg,
        v->id, v->registration_number, v->make, v->model,
        v->manufacturing_year, v->vin, v->fuel_type, v->fuel_percentage,
        v->odometer_km, v->status, v->assigned_driver_id, v->assigned_driver_name);

    send_http_response(client, 200, "application/json", buf, len);
    dsa_ht_destroy(ht);
}

static void handle_api_maintenance_prio_queue(SOCKET client, const User *user) {
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
                sched.service_name, sched.due_date, sched.due_odometer_km, sched.status, sched.days_remaining);
            rank++;
        }
    }

    size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %d, \"algorithm\": \"Binary Min-Heap Priority Queue\", \"complexity\": \"O(log N)\", \"queue\": [%s], \"data\": [%s]}",
                          rank - 1, items_json, items_json);

    send_http_response(client, 200, "application/json", buf, off);
    free(buf);
    dsa_heap_destroy(heap);
}

static void handle_api_work_order_queue(SOCKET client, const User *user) {
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
}

static void handle_api_work_order_undo(SOCKET client, const char *body, const User *user) {
    if (!http_authorize(client, user, PERM_WORK_ORDER_WRITE)) return;

    int wo_id = 0;
    json_get_int_field(body, "work_order_id", &wo_id);
    if (wo_id <= 0) {
        const char *err = "{\"success\": false, \"message\": \"Valid work_order_id required for undo operation\"}";
        send_http_response(client, 400, "application/json", err, strlen(err));
        return;
    }

    WorkOrder wo;
    StatusCode sc = work_order_get_by_id(wo_id, &wo);
    if (sc != STATUS_OK) {
        const char *err = "{\"success\": false, \"message\": \"Work order not found\"}";
        send_http_response(client, 404, "application/json", err, strlen(err));
        return;
    }

    /* Safety Check: Never undo completed work order */
    if (strcmp(wo.status, "Completed") == 0) {
        const char *err = "{\"success\": false, \"message\": \"Reversal blocked: Completed work orders have immutable financial and maintenance audit trails.\"}";
        send_http_response(client, 400, "application/json", err, strlen(err));
        return;
    }

    char rollback_status[32] = "Open";
    if (strcmp(wo.status, "In Progress") == 0) {
        safe_strncpy(rollback_status, "Assigned", sizeof(rollback_status));
    } else if (strcmp(wo.status, "Assigned") == 0) {
        safe_strncpy(rollback_status, "Open", sizeof(rollback_status));
    } else if (strcmp(wo.status, "Waiting for Parts") == 0) {
        safe_strncpy(rollback_status, "In Progress", sizeof(rollback_status));
    } else {
        const char *err = "{\"success\": false, \"message\": \"Cannot undo initial Open state\"}";
        send_http_response(client, 400, "application/json", err, strlen(err));
        return;
    }

    sc = work_order_update_status(wo_id, rollback_status, "Reversible status undo via operational stack", user->id, user->username);
    if (sc == STATUS_OK) {
        char resp[512];
        snprintf(resp, sizeof(resp),
            "{\"success\": true, \"message\": \"Status reverted successfully\", \"work_order_id\": %d, \"from_status\": \"%s\", \"to_status\": \"%s\", \"algorithm\": \"LIFO Operational Stack\", \"complexity\": \"O(1) Push/Pop\"}",
            wo_id, wo.status, rollback_status);
        send_http_response(client, 200, "application/json", resp, strlen(resp));
    } else {
        const char *err = "{\"success\": false, \"message\": \"Failed to revert status\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
    }
}

static bool str_contains_ignore_case(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    if (!needle[0]) return true;
    size_t nlen = strlen(needle);
    size_t hlen = strlen(haystack);
    if (hlen < nlen) return false;
    for (size_t i = 0; i <= hlen - nlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

static int resolve_hub_id_dijkstra(DsaGraph *g, const char *val) {
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
        if (str_contains_ignore_case(g->vertices[i].name, decoded) || str_contains_ignore_case(decoded, g->vertices[i].name)) return i;
    }
    if (str_contains_ignore_case(decoded, "Central")) return 0;
    if (str_contains_ignore_case(decoded, "North")) return 1;
    if (str_contains_ignore_case(decoded, "South")) return 2;
    if (str_contains_ignore_case(decoded, "East")) return 3;
    if (str_contains_ignore_case(decoded, "West")) return 4;
    if (str_contains_ignore_case(decoded, "Port")) return 5;
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
        "\"nodes_traversed\": %d,"
        "\"hops\": %s,"
        "\"path\": %s"
        "}",
        from_id, g->vertices[from_id].name,
        to_id, g->vertices[to_id].name,
        path_str, path_str,
        dist, dist,
        path_len,
        path_json, path_json);

    send_http_response(client, 200, "application/json", buf, len);
    dsa_graph_destroy(g);
}

static void handle_api_analytics_rankings(SOCKET client, const char *query, const User *user) {
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
        else if (strstr(query, "sort_by=year")) sort_field = VEHICLE_SORT_REG;
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
        char metric_display[128] = "";
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
            list[i].manufacturing_year, list[i].odometer_km, val, metric_display,
            list[i].fuel_percentage, list[i].status);
    }

    size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %d, \"total_ranked\": %d, \"algorithm\": \"Divide-and-Conquer Merge Sort\", \"complexity\": \"O(N log N) Stable\", \"rankings\": [%s], \"data\": [%s]}",
                          count, count, items_json, items_json);

    send_http_response(client, 200, "application/json", buf, off);
    free(buf);
    vehicle_free_list(list);
}

static void handle_api_optimization_knapsack(SOCKET client, const char *body, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    int budget = 25000;
    if (body) {
        json_get_int_field(body, "budget", &budget);
        json_get_int_field(body, "budget_ceiling", &budget);
    }
    if (budget <= 0) budget = 25000;

    DsaMaintenanceJob jobs[6] = {
        {1, "MH-01-AB-1001", "Engine Timing Belt & Water Pump", 12000, 95},
        {2, "MH-02-CD-2002", "Brake Caliper Overhaul", 4500, 85},
        {3, "MH-03-EF-3003", "Full Tyre Rotation & Alignment", 3200, 50},
        {4, "MH-04-GH-4004", "AC Compressor Servicing", 7500, 40},
        {5, "MH-05-IJ-5005", "Suspension Strut Bushings", 6000, 75},
        {6, "MH-01-AB-1001", "Transmission Fluid Flush", 5000, 65}
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
}

static void handle_api_optimization_tsp(SOCKET client, const char *body, const User *user) {
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
    int tour_cost = dsa_tsp_branch_and_bound(tsp_matrix, 4, tour);

    const char *depot_names[4] = {"Central Depot", "North Bay", "South Hub", "West Logistics"};
    char tour_json[1024] = "[";
    char tour_str[512] = "";
    for (int i = 0; i <= 4; i++) {
        int node = tour[i];
        char item[128];
        snprintf(item, sizeof(item), "%s{\"step\": %d, \"depot_id\": %d, \"name\": \"%s\"}",
                 (i > 0) ? ", " : "", i + 1, node, depot_names[node]);
        strcat(tour_json, item);

        strcat(tour_str, depot_names[node]);
        if (i < 4) strcat(tour_str, " -> ");
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
}

static void handle_api_demo_status(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    const char *path = db_get_path();
    bool is_demo = (path && strstr(path, "demo_fleet.db") != NULL);

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "{\"success\": true, \"is_demo_mode\": %s, \"active_database\": \"%s\", \"database_path\": \"%s\", \"can_reset\": %s}",
        is_demo ? "true" : "false", path ? path : "data/fleet.db", path ? path : "data/fleet.db", is_demo ? "true" : "false");

    send_http_response(client, 200, "application/json", buf, len);
}

static void handle_api_demo_reset(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_BACKUP_RESTORE)) return;

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
}

static void handle_api_backup(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_BACKUP_RESTORE)) return;

    char out_path[MAX_BUFFER_LG];
    StatusCode sc = backup_create(DEFAULT_BACKUP_DIR, out_path, sizeof(out_path));
    char resp[MAX_BUFFER_XL];
    if (sc == STATUS_OK) {
        snprintf(resp, sizeof(resp), "{\"success\": true, \"message\": \"Backup created\", \"filepath\": \"%s\"}", out_path);
        send_http_response(client, 201, "application/json", resp, strlen(resp));
    } else {
        snprintf(resp, sizeof(resp), "{\"success\": false, \"message\": \"Backup failed\"}");
        send_http_response(client, 500, "application/json", resp, strlen(resp));
    }
}

static void handle_api_evaluate(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_SCHEDULE_WRITE)) return;

    int updated = 0, notifs = 0;
    StatusCode sc = schedule_evaluate_all(&updated, &notifs);
    char buf[256];
    int len = snprintf(buf, sizeof(buf),
        "{\"success\": %s, \"updated_count\": %d, \"notification_count\": %d}",
        (sc == STATUS_OK) ? "true" : "false", updated, notifs);
    send_http_response(client, 200, "application/json", buf, len);
}

static void handle_api_audit(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_AUDIT_VIEW)) return;

    AuditLog *list = NULL;
    int count = 0;
    StatusCode sc = audit_list(50, &list, &count);
    if (sc != STATUS_OK || !list) {
        const char *empty = "{\"success\": true, \"count\": 0, \"data\": []}";
        send_http_response(client, 200, "application/json", empty, strlen(empty));
        return;
    }

    size_t buf_cap = 65536 + (count * 512);
    char *buf = (char *)malloc(buf_cap);
    if (!buf) {
        audit_free_list(list);
        const char *err = "{\"error\": \"OOM\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    size_t off = snprintf(buf, buf_cap, "{\"success\": true, \"count\": %d, \"data\": [", count);
    for (int i = 0; i < count; i++) {
        off += snprintf(buf + off, buf_cap - off,
            "%s{\"id\": %d, \"username\": \"%s\", \"action\": \"%s\", \"entity_type\": \"%s\", "
            "\"entity_id\": %d, \"timestamp\": \"%s\", \"details\": \"%s\"}",
            (i > 0) ? ", " : "",
            list[i].id, list[i].username, list[i].action, list[i].entity_type,
            list[i].entity_id, list[i].timestamp, list[i].details);
    }
    off += snprintf(buf + off, buf_cap - off, "]}");

    send_http_response(client, 200, "application/json", buf, off);
    free(buf);
    audit_free_list(list);
}

static void handle_api_reports_csv(SOCKET client, const char *query, const User *user) {
    if (!http_authorize(client, user, PERM_REPORTS)) return;

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
        const char *err = "{\"success\": false, \"message\": \"Failed to generate CSV report\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    FILE *fp = fopen(out_path, "rb");
    if (!fp) {
        const char *err = "{\"success\": false, \"message\": \"Report file unreadable\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        const char *err = "{\"success\": false, \"message\": \"Out of memory\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    size_t rd = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);

    char disp_header[256];
    snprintf(disp_header, sizeof(disp_header), "Content-Disposition: attachment; filename=\"fleet_%s.csv\"\r\n", type);
    send_http_response_ext(client, 200, "text/csv", disp_header, buf, rd);
    free(buf);
}

static void handle_api_reports_pdf(SOCKET client, const char *query, const User *user) {
    if (!http_authorize(client, user, PERM_REPORTS)) return;

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
        const char *err = "{\"success\": false, \"message\": \"Failed to generate PDF report\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    FILE *fp = fopen(out_path, "rb");
    if (!fp) {
        const char *err = "{\"success\": false, \"message\": \"PDF file unreadable\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        const char *err = "{\"success\": false, \"message\": \"Out of memory\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    size_t rd = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);

    char disp_header[256];
    snprintf(disp_header, sizeof(disp_header), "Content-Disposition: attachment; filename=\"fleet_%s.pdf\"\r\n", type);
    send_http_response_ext(client, 200, "application/pdf", disp_header, buf, rd);
    free(buf);
}

static void handle_api_auth(SOCKET client, const char *req_buf, const char *method, const char *body, const char *query, const char *subpath) {
    if (strcmp(method, "POST") == 0 && (strcmp(subpath, "/login") == 0 || strcmp(subpath, "") == 0)) {
        char username[MAX_BUFFER_SM] = {0};
        char password[MAX_BUFFER_SM] = {0};
        json_get_string_field(body, "username", username, sizeof(username));
        json_get_string_field(body, "password", password, sizeof(password));

        if (str_is_blank(username) || str_is_blank(password)) {
            const char *err = "{\"success\": false, \"message\": \"Username and password are required\", \"code\": 400}";
            send_http_response(client, 400, "application/json", err, strlen(err));
            return;
        }

        User u = {0};
        StatusCode sc = auth_login(username, password, &u);
        if (sc == STATUS_OK) {
            char token[AUTH_SESSION_TOKEN_LEN] = {0};
            if (auth_session_create(&u, token, sizeof(token)) != STATUS_OK) {
                const char *err = "{\"success\": false, \"message\": \"Failed to create session token\", \"code\": 500}";
                send_http_response(client, 500, "application/json", err, strlen(err));
                return;
            }

            char cookie_hdr[256];
            snprintf(cookie_hdr, sizeof(cookie_hdr),
                     "Set-Cookie: session_token=%s; Path=/; HttpOnly; SameSite=Strict; Max-Age=86400\r\n", token);

            char resp[1024];
            snprintf(resp, sizeof(resp),
                "{\"success\": true, \"message\": \"Authentication successful\", "
                "\"token\": \"%s\", "
                "\"user\": {\"id\": %d, \"username\": \"%s\", \"role\": \"%s\", \"role_id\": %d, "
                "\"can_write_vehicle\": %s, \"can_delete_vehicle\": %s, \"can_write_driver\": %s, "
                "\"can_write_maint\": %s, \"can_write_schedule\": %s, \"can_write_expense\": %s, "
                "\"can_view_reports\": %s, \"can_manage_users\": %s, \"can_backup\": %s, \"can_view_audit\": %s}}",
                token,
                u.id, u.username, auth_role_to_string(u.role), (int)u.role,
                auth_has_permission(u.role, PERM_VEHICLE_WRITE) ? "true" : "false",
                auth_has_permission(u.role, PERM_VEHICLE_DELETE) ? "true" : "false",
                auth_has_permission(u.role, PERM_DRIVER_WRITE) ? "true" : "false",
                auth_has_permission(u.role, PERM_MAINTENANCE_WRITE) ? "true" : "false",
                auth_has_permission(u.role, PERM_SCHEDULE_WRITE) ? "true" : "false",
                auth_has_permission(u.role, PERM_EXPENSE_WRITE) ? "true" : "false",
                auth_has_permission(u.role, PERM_REPORTS) ? "true" : "false",
                auth_has_permission(u.role, PERM_USER_MANAGE) ? "true" : "false",
                auth_has_permission(u.role, PERM_BACKUP_RESTORE) ? "true" : "false",
                auth_has_permission(u.role, PERM_AUDIT_VIEW) ? "true" : "false");
            send_http_response_ext(client, 200, "application/json", cookie_hdr, resp, strlen(resp));
        } else {
            const char *err = "{\"success\": false, \"message\": \"Invalid username or password\", \"code\": 401}";
            send_http_response(client, 401, "application/json", err, strlen(err));
        }
    } else if (strcmp(subpath, "/logout") == 0) {
        char token[AUTH_SESSION_TOKEN_LEN] = {0};
        if (extract_session_token(req_buf, query, token, sizeof(token))) {
            auth_session_destroy(token);
        }
        const char *clear_cookie = "Set-Cookie: session_token=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0\r\n";
        const char *resp = "{\"success\": true, \"message\": \"Logged out successfully\"}";
        send_http_response_ext(client, 200, "application/json", clear_cookie, resp, strlen(resp));
    } else if (strcmp(subpath, "/session") == 0) {
        User u = {0};
        if (!http_authenticate(client, req_buf, query, &u)) return;

        char resp[1024];
        snprintf(resp, sizeof(resp),
            "{\"success\": true, \"authenticated\": true, "
            "\"user\": {\"id\": %d, \"username\": \"%s\", \"role\": \"%s\", \"role_id\": %d, "
            "\"can_write_vehicle\": %s, \"can_delete_vehicle\": %s, \"can_write_driver\": %s, "
            "\"can_write_maint\": %s, \"can_write_schedule\": %s, \"can_write_expense\": %s, "
            "\"can_view_reports\": %s, \"can_manage_users\": %s, \"can_backup\": %s, \"can_view_audit\": %s}}",
            u.id, u.username, auth_role_to_string(u.role), (int)u.role,
            auth_has_permission(u.role, PERM_VEHICLE_WRITE) ? "true" : "false",
            auth_has_permission(u.role, PERM_VEHICLE_DELETE) ? "true" : "false",
            auth_has_permission(u.role, PERM_DRIVER_WRITE) ? "true" : "false",
            auth_has_permission(u.role, PERM_MAINTENANCE_WRITE) ? "true" : "false",
            auth_has_permission(u.role, PERM_SCHEDULE_WRITE) ? "true" : "false",
            auth_has_permission(u.role, PERM_EXPENSE_WRITE) ? "true" : "false",
            auth_has_permission(u.role, PERM_REPORTS) ? "true" : "false",
            auth_has_permission(u.role, PERM_USER_MANAGE) ? "true" : "false",
            auth_has_permission(u.role, PERM_BACKUP_RESTORE) ? "true" : "false",
            auth_has_permission(u.role, PERM_AUDIT_VIEW) ? "true" : "false");
        send_http_response(client, 200, "application/json", resp, strlen(resp));
    } else if (strcmp(method, "GET") == 0 && strcmp(subpath, "/users") == 0) {
        User u = {0};
        if (!http_authenticate(client, req_buf, query, &u)) return;
        if (!http_authorize(client, &u, PERM_USER_MANAGE)) return;

        User *users = NULL;
        int count = 0;
        StatusCode sc = auth_get_users(&users, &count);
        if (sc != STATUS_OK || !users) {
            const char *empty = "{\"success\": true, \"count\": 0, \"data\": []}";
            send_http_response(client, 200, "application/json", empty, strlen(empty));
            return;
        }

        size_t cap = 4096 + (count * 256);
        char *buf = (char *)malloc(cap);
        if (!buf) {
            auth_free_users(users);
            const char *err = "{\"error\": \"OOM\"}";
            send_http_response(client, 500, "application/json", err, strlen(err));
            return;
        }

        size_t off = snprintf(buf, cap, "{\"success\": true, \"count\": %d, \"data\": [", count);
        for (int i = 0; i < count; i++) {
            off += snprintf(buf + off, cap - off,
                "%s{\"id\": %d, \"username\": \"%s\", \"role\": \"%s\", \"active\": %s}",
                (i > 0) ? ", " : "",
                users[i].id, users[i].username, auth_role_to_string(users[i].role),
                users[i].active ? "true" : "false");
        }
        off += snprintf(buf + off, cap - off, "]}");

        send_http_response(client, 200, "application/json", buf, off);
        free(buf);
        auth_free_users(users);
    } else {
        const char *err = "{\"error\": \"Not Found\", \"message\": \"Unknown auth endpoint\"}";
        send_http_response(client, 404, "application/json", err, strlen(err));
    }
}

/* ==================== HTTP REQUEST ROUTER ==================== */

static void handle_client_request(SOCKET client) {
    char req_buf[16384];
    int received = recv(client, req_buf, sizeof(req_buf) - 1, 0);
    if (received <= 0) return;
    req_buf[received] = '\0';

    char method[16] = {0};
    char path[256] = {0};
    sscanf(req_buf, "%15s %255s", method, path);

    /* Handle CORS preflight OPTIONS */
    if (strcmp(method, "OPTIONS") == 0) {
        send_http_response(client, 200, "text/plain", "", 0);
        return;
    }

    /* Extract query string if any */
    char *query = strchr(path, '?');
    if (query) {
        *query = '\0';
        query++;
    }

    /* Locate HTTP body and guarantee full read based on Content-Length */
    char *body = strstr(req_buf, "\r\n\r\n");
    if (body) {
        body += 4;
        const char *cl = strstr(req_buf, "Content-Length:");
        if (!cl) cl = strstr(req_buf, "content-length:");
        if (cl) {
            int content_len = atoi(cl + 15);
            if (content_len < 0 || content_len > (int)sizeof(req_buf) - 512) {
                const char *err = "{\"error\": \"Payload Too Large\", \"message\": \"Request body exceeds server limit of 15KB\", \"code\": 413}";
                send_http_response(client, 413, "application/json", err, strlen(err));
                return;
            }
            int body_so_far = received - (int)(body - req_buf);
            while (body_so_far < content_len && received < (int)sizeof(req_buf) - 1) {
                int n = recv(client, req_buf + received, (int)sizeof(req_buf) - 1 - received, 0);
                if (n <= 0) break;
                received += n;
                req_buf[received] = '\0';
                body_so_far += n;
            }
        }
    } else {
        body = req_buf + received;
    }

    /* Public Static Files Routing */
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        serve_static_file(client, "web/index.html", "text/html");
    } else if (strcmp(path, "/dashboard.css") == 0 || strcmp(path, "/css/dashboard.css") == 0) {
        serve_static_file(client, "web/dashboard.css", "text/css");
    } else if (strcmp(path, "/app.js") == 0 || strcmp(path, "/js/app.js") == 0) {
        serve_static_file(client, "web/app.js", "application/javascript");
    } else if (strcmp(path, "/favicon.ico") == 0) {
        send_http_response(client, 204, "image/x-icon", "", 0);
    } else if (strcmp(path, "/api/auth/login") == 0) {
        handle_api_auth(client, req_buf, method, body, query, "/login");
    } else if (strcmp(path, "/api/auth/logout") == 0) {
        handle_api_auth(client, req_buf, method, body, query, "/logout");
    } else if (strncmp(path, "/api/", 5) == 0) {
        /* Protected API routes: resolve and verify cryptographic session */
        User current_user = {0};
        if (!http_authenticate(client, req_buf, query, &current_user)) {
            return; /* 401 response already transmitted */
        }

        if (strcmp(path, "/api/auth/session") == 0) {
            handle_api_auth(client, req_buf, method, body, query, "/session");
        } else if (strcmp(path, "/api/dashboard") == 0) {
            handle_api_dashboard(client, &current_user);
        } else if (strcmp(path, "/api/vehicles") == 0) {
            handle_api_vehicles(client, method, body, query, &current_user);
        } else if (strcmp(path, "/api/drivers") == 0) {
            handle_api_drivers(client, method, body, query, &current_user);
        } else if (strcmp(path, "/api/maintenance") == 0) {
            handle_api_maintenance(client, method, body, &current_user);
        } else if (strcmp(path, "/api/schedules") == 0) {
            handle_api_schedules(client, method, body, &current_user);
        } else if (strcmp(path, "/api/work-orders") == 0) {
            handle_api_work_orders(client, method, body, query, &current_user);
        } else if (strcmp(path, "/api/work-orders/history") == 0) {
            handle_api_work_order_history(client, query, &current_user);
        } else if (strcmp(path, "/api/inspections") == 0) {
            handle_api_inspections(client, method, body, query, &current_user);
        } else if (strcmp(path, "/api/inspections/defects") == 0) {
            handle_api_inspection_defects(client, query, &current_user);
        } else if (strcmp(path, "/api/inspections/create-work-order") == 0) {
            handle_api_inspection_create_wo(client, body, &current_user);
        } else if (strcmp(path, "/api/analytics") == 0) {
            handle_api_analytics(client, query, &current_user);
        } else if (strcmp(path, "/api/expenses") == 0) {
            handle_api_expenses(client, method, body, &current_user);
        } else if (strcmp(path, "/api/live-status") == 0) {
            handle_api_live_status(client, &current_user);
        } else if (strcmp(path, "/api/notifications") == 0) {
            handle_api_notifications(client, method, body, query, &current_user);
        } else if (strcmp(path, "/api/notifications/ack") == 0) {
            handle_api_notifications(client, "POST", body, query, &current_user);
        } else if (strcmp(path, "/api/notifications/ack-all") == 0) {
            handle_api_notifications(client, "POST", "{\"action\":\"ack_all\"}", query, &current_user);

        } else if (strcmp(path, "/api/vehicles/lookup") == 0) {
            handle_api_vehicle_lookup(client, query, &current_user);
        } else if (strcmp(path, "/api/maintenance/priority-queue") == 0) {
            handle_api_maintenance_prio_queue(client, &current_user);
        } else if (strcmp(path, "/api/work-orders/dispatch-queue") == 0) {
            handle_api_work_order_queue(client, &current_user);
        } else if (strcmp(path, "/api/work-orders/undo") == 0) {
            handle_api_work_order_undo(client, body, &current_user);
        } else if (strcmp(path, "/api/routes/plan") == 0) {
            handle_api_routes_plan(client, query, &current_user);
        } else if (strcmp(path, "/api/analytics/rankings") == 0) {
            handle_api_analytics_rankings(client, query, &current_user);
        } else if (strcmp(path, "/api/optimization/knapsack") == 0) {
            handle_api_optimization_knapsack(client, body, &current_user);
        } else if (strcmp(path, "/api/optimization/tsp") == 0) {
            handle_api_optimization_tsp(client, body, &current_user);
        } else if (strcmp(path, "/api/demo/status") == 0) {
            handle_api_demo_status(client, &current_user);
        } else if (strcmp(path, "/api/demo/reset") == 0) {
            handle_api_demo_reset(client, &current_user);
        } else if (strcmp(path, "/api/dsa/run") == 0) {
            handle_api_dsa_run(client, &current_user);
        } else if (strcmp(path, "/api/backup") == 0) {
            handle_api_backup(client, &current_user);
        } else if (strcmp(path, "/api/evaluate") == 0) {
            handle_api_evaluate(client, &current_user);
        } else if (strcmp(path, "/api/audit") == 0) {
            handle_api_audit(client, &current_user);
        } else if (strcmp(path, "/api/reports/csv") == 0) {
            handle_api_reports_csv(client, query, &current_user);
        } else if (strcmp(path, "/api/reports/pdf") == 0) {
            handle_api_reports_pdf(client, query, &current_user);
        } else if (strcmp(path, "/api/users") == 0 || strcmp(path, "/api/auth/users") == 0) {
            handle_api_auth(client, req_buf, "GET", body, query, "/users");
        } else {
            const char *not_found = "{\"error\": \"Endpoint not found\"}";
            send_http_response(client, 404, "application/json", not_found, strlen(not_found));
        }
    } else {
        const char *not_found = "{\"error\": \"Endpoint not found\"}";
        send_http_response(client, 404, "application/json", not_found, strlen(not_found));
    }
}

StatusCode http_server_start(int port) {
    if (port <= 0) port = DEFAULT_HTTP_PORT;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[HTTP SERVER] WSAStartup failed.\n");
        return STATUS_ERROR;
    }
#endif

    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == INVALID_SOCKET) {
        fprintf(stderr, "[HTTP SERVER] Failed to create socket.\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return STATUS_ERROR;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* Bound to localhost 127.0.0.1 for security */
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "[HTTP SERVER] Failed to bind to port %d.\n", port);
        CLOSE_SOCKET(server_sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return STATUS_ERROR;
    }

    if (listen(server_sock, 10) == SOCKET_ERROR) {
        fprintf(stderr, "[HTTP SERVER] Listen failed.\n");
        CLOSE_SOCKET(server_sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return STATUS_ERROR;
    }

    g_server_fd = server_sock;
    g_server_running = true;
    g_server_start_time = time(NULL);

    printf("\n=================================================================\n");
    printf("   FLEET VEHICLE MAINTENANCE RECORD SYSTEM - WEB DASHBOARD\n");
    printf("=================================================================\n");
    printf(" [ONLINE] Serving White/Grey Dashboard at: http://127.0.0.1:%d\n", port);
    printf(" [ONLINE] REST API active at             : http://127.0.0.1:%d/api/dashboard\n", port);
    printf(" Press Ctrl+C in this console to stop server.\n");
    printf("=================================================================\n\n");

    while (g_server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        SOCKET client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == INVALID_SOCKET) {
            if (!g_server_running) break;
            continue;
        }

        handle_client_request(client_sock);
        CLOSE_SOCKET(client_sock);
    }

    CLOSE_SOCKET(server_sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return STATUS_OK;
}
