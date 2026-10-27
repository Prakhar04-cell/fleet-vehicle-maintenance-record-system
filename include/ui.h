#ifndef UI_H
#define UI_H

#include "common.h"
#include "auth.h"

void ui_init(void);
void ui_clear_screen(void);
void ui_print_banner(void);
void ui_print_header(const char *subtitle);
void ui_print_success(const char *msg);
void ui_print_error(const char *msg);
void ui_print_warning(const char *msg);
void ui_print_info(const char *msg);
void ui_print_divider(char ch, int length);

void ui_menu_dashboard(void);
void ui_menu_vehicles(void);
void ui_menu_drivers(void);
void ui_menu_maintenance(void);
void ui_menu_schedules(void);
void ui_menu_expenses(void);
void ui_menu_reports(void);
void ui_menu_notifications(void);
void ui_menu_audit(void);
void ui_menu_users(void);
void ui_menu_backup(void);

void ui_run_main_loop(void);

#endif /* UI_H */
