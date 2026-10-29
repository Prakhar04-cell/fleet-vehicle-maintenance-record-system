with open('src/http_server.c', 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Fix v->year and v->fuel_level_percent
old_veh_snprintf = '''        v->year, v->vin, v->fuel_type, v->fuel_level_percent,'''
new_veh_snprintf = '''        v->manufacturing_year, v->vin, v->fuel_type, v->fuel_percentage,'''
assert old_veh_snprintf in code, "old_veh_snprintf not found"
code = code.replace(old_veh_snprintf, new_veh_snprintf, 1)
print("[OK] Fixed Vehicle members in handle_api_vehicle_lookup")

# 2. Add str_contains_ignore_case helper right before resolve_hub_id_dijkstra
helper = '''static bool str_contains_ignore_case(const char *haystack, const char *needle) {
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

'''

old_resolve = '''static int resolve_hub_id_dijkstra(DsaGraph *g, const char *val) {
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
}'''

new_resolve = helper + '''static int resolve_hub_id_dijkstra(DsaGraph *g, const char *val) {
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
        if (str_contains_ignore_case(g->vertices[i].name, decoded) || str_contains_ignore_case(decoded, g->vertices[i].name)) return i;
    }
    if (str_contains_ignore_case(decoded, "Central")) return 0;
    if (str_contains_ignore_case(decoded, "North")) return 1;
    if (str_contains_ignore_case(decoded, "South")) return 2;
    if (str_contains_ignore_case(decoded, "East")) return 3;
    if (str_contains_ignore_case(decoded, "West")) return 4;
    if (str_contains_ignore_case(decoded, "Port")) return 5;
    return -1;
}'''

assert old_resolve in code, "old_resolve not found"
code = code.replace(old_resolve, new_resolve, 1)
print("[OK] Replaced strcasestr with portable str_contains_ignore_case")

with open('src/http_server.c', 'w', encoding='utf-8') as f:
    f.write(code)

print("[SUCCESS] All C fixes applied to src/http_server.c")
