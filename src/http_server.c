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

static void handle_api_notifications(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_READ)) return;

    Notification *list = NULL;
    int count = 0;
    notification_list_active(&list, &count);

    size_t buf_cap = 32768 + (count * 256);
    char *buf = (char *)malloc(buf_cap);
    if (!buf) {
        notification_free_list(list);
        return;
    }

    size_t off = snprintf(buf, buf_cap, "{\"success\": true, \"count\": %d, \"data\": [", count);
    for (int i = 0; i < count; i++) {
        off += snprintf(buf + off, buf_cap - off,
            "%s{\"id\": %d, \"vehicle_reg\": \"%s\", \"service_name\": \"%s\", "
            "\"type\": \"%s\", \"scheduled_at\": \"%s\", \"sent_at\": \"%s\"}",
            (i > 0) ? ", " : "",
            list[i].id, list[i].vehicle_reg, list[i].service_name,
            list[i].notification_type, list[i].scheduled_at, list[i].sent_at);
    }
    off += snprintf(buf + off, buf_cap - off, "]}");

    send_http_response(client, 200, "application/json", buf, off);
    free(buf);
    notification_free_list(list);
}

static void handle_api_dsa_run(SOCKET client, const User *user) {
    if (!http_authorize(client, user, PERM_USER_MANAGE)) return;

    /* Execute DSA benchmarks live and serialize structured result */
    DsaHashTable *ht = dsa_ht_load_all_vehicles();
    size_t ht_count = dsa_ht_size(ht);
    size_t ht_cap = ht ? ht->capacity : 0;
    dsa_ht_destroy(ht);

    /* Graph Dijkstra demo */
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

    /* DP Knapsack */
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

    /* TSP Tour */
    int tsp_matrix[DSA_TSP_MAX][DSA_TSP_MAX] = {
        {0, 10, 15, 20},
        {10, 0, 35, 25},
        {15, 35, 0, 30},
        {20, 25, 30, 0}
    };
    int tour[8];
    int tour_cost = dsa_tsp_branch_and_bound(tsp_matrix, 4, tour);

    char buf[4096];
    int len = snprintf(buf, sizeof(buf),
        "{"
        "\"success\": true,"
        "\"algorithms\": {"
        "\"hash_table\": {\"vehicles_indexed\": %zu, \"buckets\": %zu, \"complexity\": \"O(1) Avg\"},"
        "\"dijkstra\": {\"shortest_dist_km\": %d, \"route\": \"%s\", \"complexity\": \"O((V+E)log V)\"},"
        "\"dp_knapsack\": {\"budget_rupees\": 18000, \"max_reliability_score\": %d, \"jobs_selected\": %d, \"complexity\": \"O(N*B)\"},"
        "\"tsp_branch_bound\": {\"optimal_tour_distance_km\": %d, \"complexity\": \"O(N!) B&B Pruned\"}"
        "}"
        "}",
        ht_count, ht_cap, dist, path_str, max_val, chosen_cnt, tour_cost);

    send_http_response(client, 200, "application/json", buf, len);
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
        } else if (strcmp(path, "/api/expenses") == 0) {
            handle_api_expenses(client, method, body, &current_user);
        } else if (strcmp(path, "/api/notifications") == 0) {
            handle_api_notifications(client, &current_user);
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
