#ifndef INPUT_H
#define INPUT_H

#include "common.h"

/* Safe interactive input functions with fgets and bounded buffers */
bool input_get_string(const char *prompt, char *buffer, size_t max_len, bool allow_empty);
bool input_get_int(const char *prompt, int *out_val, int min_val, int max_val);
bool input_get_double(const char *prompt, double *out_val, double min_val, double max_val);
bool input_get_money(const char *prompt, int64_t *out_cents, int64_t min_cents, int64_t max_cents);
bool input_get_date(const char *prompt, char *out_iso_date, size_t max_len, bool allow_empty);
bool input_get_confirm(const char *prompt, bool default_yes);
bool input_get_password(const char *prompt, char *buffer, size_t max_len);
void input_pause(void);

#endif /* INPUT_H */
