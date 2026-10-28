#include "common.h"
#include <ctype.h>

void str_trim(char *str) {
    if (!str) return;
    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    *end = '\0';
    if (start != str) {
        memmove(str, start, (size_t)(end - start + 1));
    }
}

void str_to_upper(char *str) {
    if (!str) return;
    for (char *p = str; *p; p++) {
        *p = (char)toupper((unsigned char)*p);
    }
}

void str_to_lower(char *str) {
    if (!str) return;
    for (char *p = str; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
    }
}

bool str_is_blank(const char *str) {
    if (!str) return true;
    while (*str) {
        if (!isspace((unsigned char)*str)) return false;
        str++;
    }
    return true;
}

void safe_strncpy(char *dest, const char *src, size_t dest_size) {
    if (!dest || dest_size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

void get_current_iso_datetime(char *buffer, size_t max_len) {
    if (!buffer || max_len == 0) return;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, max_len, "%Y-%m-%d %H:%M:%S", tm_info);
}

void get_current_iso_date(char *buffer, size_t max_len) {
    if (!buffer || max_len == 0) return;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, max_len, "%Y-%m-%d", tm_info);
}

bool validate_iso_date(const char *date_str) {
    if (!date_str || strlen(date_str) != 10) return false;
    if (date_str[4] != '-' || date_str[7] != '-') return false;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (!isdigit((unsigned char)date_str[i])) return false;
    }
    int year = atoi(date_str);
    int month = atoi(date_str + 5);
    int day = atoi(date_str + 8);
    if (year < 1900 || year > 2100) return false;
    if (month < 1 || month > 12) return false;
    if (day < 1 || day > 31) return false;
    if ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30) return false;
    if (month == 2) {
        bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        if (day > (is_leap ? 29 : 28)) return false;
    }
    return true;
}

int days_between_dates(const char *date_from, const char *date_to) {
    if (!validate_iso_date(date_from) || !validate_iso_date(date_to)) return 0;
    struct tm tm_from = {0};
    struct tm tm_to = {0};
    sscanf(date_from, "%d-%d-%d", &tm_from.tm_year, &tm_from.tm_mon, &tm_from.tm_mday);
    sscanf(date_to, "%d-%d-%d", &tm_to.tm_year, &tm_to.tm_mon, &tm_to.tm_mday);
    tm_from.tm_year -= 1900;
    tm_from.tm_mon -= 1;
    tm_to.tm_year -= 1900;
    tm_to.tm_mon -= 1;
    tm_from.tm_isdst = -1;
    tm_to.tm_isdst = -1;

    time_t t1 = mktime(&tm_from);
    time_t t2 = mktime(&tm_to);
    if (t1 == (time_t)-1 || t2 == (time_t)-1) return 0;
    double seconds = difftime(t2, t1);
    return (int)(seconds / (60 * 60 * 24));
}

void add_days_to_date(const char *base_date, int days, char *out_date, size_t max_len) {
    if (!validate_iso_date(base_date) || !out_date || max_len < 11) {
        if (out_date && max_len > 0) out_date[0] = '\0';
        return;
    }
    struct tm tm_val = {0};
    sscanf(base_date, "%d-%d-%d", &tm_val.tm_year, &tm_val.tm_mon, &tm_val.tm_mday);
    tm_val.tm_year -= 1900;
    tm_val.tm_mon -= 1;
    tm_val.tm_mday += days;
    tm_val.tm_isdst = -1;

    time_t t = mktime(&tm_val);
    if (t == (time_t)-1) {
        safe_strncpy(out_date, base_date, max_len);
        return;
    }
    struct tm *res = localtime(&t);
    strftime(out_date, max_len, "%Y-%m-%d", res);
}

void format_currency(int64_t amount_minor, char *buffer, size_t max_len) {
    if (!buffer || max_len == 0) return;
    bool negative = (amount_minor < 0);
    int64_t abs_amt = negative ? -amount_minor : amount_minor;
    int64_t whole = abs_amt / 100;
    int paise = (int)(abs_amt % 100);

    /* Format with thousands separators */
    char whole_str[64];
    snprintf(whole_str, sizeof(whole_str), "%lld", (long long)whole);
    int len = (int)strlen(whole_str);

    char formatted[128] = {0};
    int f_idx = 0;
    for (int i = 0; i < len; i++) {
        if (i > 0 && (len - i) % 3 == 0) {
            formatted[f_idx++] = ',';
        }
        formatted[f_idx++] = whole_str[i];
    }
    formatted[f_idx] = '\0';

    if (negative) {
        snprintf(buffer, max_len, "-%s %s.%02d", CURRENCY_SYMBOL, formatted, paise);
    } else {
        snprintf(buffer, max_len, "%s %s.%02d", CURRENCY_SYMBOL, formatted, paise);
    }
}
