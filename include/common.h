#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define APP_NAME "Fleet Vehicle Maintenance Record System"
#define APP_VERSION "1.0.0"
#define DEFAULT_DB_PATH "data/fleet.db"
#define DEFAULT_MIGRATIONS_DIR "migrations"
#define DEFAULT_BACKUP_DIR "backups"
#define DEFAULT_REPORTS_DIR "reports"

#define MAX_BUFFER_XS    64
#define MAX_BUFFER_SM    128
#define MAX_BUFFER_MD    256
#define MAX_BUFFER_LG    512
#define MAX_BUFFER_XL    1024
#define MAX_BUFFER_XXL   4096

#define CURRENCY_CODE    "INR"
#define CURRENCY_SYMBOL  "Rs."

/* Return status codes */
typedef enum {
    STATUS_OK = 0,
    STATUS_ERROR = -1,
    STATUS_NOT_FOUND = -2,
    STATUS_INVALID_INPUT = -3,
    STATUS_AUTH_FAILED = -4,
    STATUS_FORBIDDEN = -5,
    STATUS_EXISTS = -6,
    STATUS_DB_ERROR = -7
} StatusCode;

/* String utilities */
void str_trim(char *str);
void str_to_upper(char *str);
void str_to_lower(char *str);
bool str_is_blank(const char *str);
void safe_strncpy(char *dest, const char *src, size_t dest_size);

/* Time / Date utilities (ISO 8601: YYYY-MM-DD and YYYY-MM-DD HH:MM:SS) */
void get_current_iso_datetime(char *buffer, size_t max_len);
void get_current_iso_date(char *buffer, size_t max_len);
bool validate_iso_date(const char *date_str);
int days_between_dates(const char *date_from, const char *date_to);
void add_days_to_date(const char *base_date, int days, char *out_date, size_t max_len);

/* Currency formatting (converts minor units [paise/cents] to "Rs. 1,234.50") */
void format_currency(int64_t amount_minor, char *buffer, size_t max_len);

#endif /* COMMON_H */
