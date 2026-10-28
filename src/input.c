#include "input.h"
#include <ctype.h>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

bool input_get_string(const char *prompt, char *buffer, size_t max_len, bool allow_empty) {
    if (!buffer || max_len == 0) return false;
    while (1) {
        if (prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if (!fgets(buffer, (int)max_len, stdin)) {
            buffer[0] = '\0';
            return false;
        }
        /* remove trailing newline */
        size_t len = strlen(buffer);
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
            buffer[len - 1] = '\0';
            len--;
        }
        str_trim(buffer);
        if (!allow_empty && buffer[0] == '\0') {
            printf("Error: Input cannot be empty. Please try again.\n");
            continue;
        }
        return true;
    }
}

bool input_get_int(const char *prompt, int *out_val, int min_val, int max_val) {
    if (!out_val) return false;
    char line[MAX_BUFFER_SM];
    while (1) {
        if (!input_get_string(prompt, line, sizeof(line), false)) {
            return false;
        }
        char *endptr = NULL;
        long val = strtol(line, &endptr, 10);
        if (endptr == line || *endptr != '\0') {
            printf("Error: Invalid number. Please enter a valid integer.\n");
            continue;
        }
        if (val < min_val || val > max_val) {
            printf("Error: Value must be between %d and %d.\n", min_val, max_val);
            continue;
        }
        *out_val = (int)val;
        return true;
    }
}

bool input_get_double(const char *prompt, double *out_val, double min_val, double max_val) {
    if (!out_val) return false;
    char line[MAX_BUFFER_SM];
    while (1) {
        if (!input_get_string(prompt, line, sizeof(line), false)) {
            return false;
        }
        char *endptr = NULL;
        double val = strtod(line, &endptr);
        if (endptr == line || *endptr != '\0') {
            printf("Error: Invalid number. Please enter a valid decimal number.\n");
            continue;
        }
        if (val < min_val || val > max_val) {
            printf("Error: Value must be between %.2f and %.2f.\n", min_val, max_val);
            continue;
        }
        *out_val = val;
        return true;
    }
}

bool input_get_money(const char *prompt, int64_t *out_cents, int64_t min_cents, int64_t max_cents) {
    if (!out_cents) return false;
    char line[MAX_BUFFER_SM];
    while (1) {
        if (!input_get_string(prompt, line, sizeof(line), false)) {
            return false;
        }
        /* Parse exact decimal money (e.g. "1250" or "1250.75") into integer cents */
        char *dot = strchr(line, '.');
        int64_t whole = 0;
        int64_t frac = 0;

        char *endptr = NULL;
        if (dot) {
            *dot = '\0';
            whole = strtoll(line, &endptr, 10);
            if (endptr == line || *endptr != '\0' || whole < 0) {
                printf("Error: Invalid amount format. Example: 1500 or 1500.50\n");
                continue;
            }
            char frac_str[3] = {'0', '0', '\0'};
            const char *f = dot + 1;
            if (f[0] != '\0') {
                if (!isdigit((unsigned char)f[0])) {
                    printf("Error: Invalid fractional amount.\n");
                    continue;
                }
                frac_str[0] = f[0];
                if (f[1] != '\0') {
                    if (!isdigit((unsigned char)f[1]) || f[2] != '\0') {
                        printf("Error: Only up to 2 decimal places allowed.\n");
                        continue;
                    }
                    frac_str[1] = f[1];
                }
            }
            frac = (frac_str[0] - '0') * 10 + (frac_str[1] - '0');
        } else {
            whole = strtoll(line, &endptr, 10);
            if (endptr == line || *endptr != '\0' || whole < 0) {
                printf("Error: Invalid amount format. Example: 1500 or 1500.50\n");
                continue;
            }
            frac = 0;
        }

        int64_t total_minor = (whole * 100) + frac;
        if (total_minor < min_cents || total_minor > max_cents) {
            char min_buf[32], max_buf[32];
            format_currency(min_cents, min_buf, sizeof(min_buf));
            format_currency(max_cents, max_buf, sizeof(max_buf));
            printf("Error: Amount must be between %s and %s.\n", min_buf, max_buf);
            continue;
        }
        *out_cents = total_minor;
        return true;
    }
}

bool input_get_date(const char *prompt, char *out_iso_date, size_t max_len, bool allow_empty) {
    if (!out_iso_date || max_len < 11) return false;
    char line[MAX_BUFFER_SM];
    while (1) {
        if (!input_get_string(prompt, line, sizeof(line), allow_empty)) {
            return false;
        }
        if (allow_empty && line[0] == '\0') {
            out_iso_date[0] = '\0';
            return true;
        }
        if (!validate_iso_date(line)) {
            printf("Error: Invalid date. Format must be YYYY-MM-DD (e.g. 2026-10-27).\n");
            continue;
        }
        safe_strncpy(out_iso_date, line, max_len);
        return true;
    }
}

bool input_get_confirm(const char *prompt, bool default_yes) {
    char line[MAX_BUFFER_SM];
    char full_prompt[MAX_BUFFER_MD];
    snprintf(full_prompt, sizeof(full_prompt), "%s %s: ", prompt, default_yes ? "[Y/n]" : "[y/N]");

    if (!input_get_string(full_prompt, line, sizeof(line), true)) {
        return default_yes;
    }
    if (line[0] == '\0') return default_yes;
    return (line[0] == 'y' || line[0] == 'Y');
}

bool input_get_password(const char *prompt, char *buffer, size_t max_len) {
    if (!buffer || max_len == 0) return false;
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }

    size_t idx = 0;
#ifdef _WIN32
    while (idx + 1 < max_len) {
        int ch = _getch();
        if (ch == '\r' || ch == '\n') {
            break;
        } else if (ch == '\b') { /* backspace */
            if (idx > 0) {
                idx--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (ch == 0 || ch == 224) {
            _getch(); /* skip special keys */
        } else if (ch >= 32 && ch <= 126) {
            buffer[idx++] = (char)ch;
            putchar('*');
            fflush(stdout);
        }
    }
    buffer[idx] = '\0';
    putchar('\n');
#else
    struct termios old_flags, new_flags;
    tcgetattr(STDIN_FILENO, &old_flags);
    new_flags = old_flags;
    new_flags.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_flags);

    if (fgets(buffer, (int)max_len, stdin)) {
        size_t len = strlen(buffer);
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
            buffer[len - 1] = '\0';
            len--;
        }
    } else {
        buffer[0] = '\0';
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &old_flags);
    putchar('\n');
#endif

    str_trim(buffer);
    return (buffer[0] != '\0');
}

void input_pause(void) {
    printf("\nPress [Enter] to continue...");
    fflush(stdout);
    char buf[16];
    fgets(buf, sizeof(buf), stdin);
}
