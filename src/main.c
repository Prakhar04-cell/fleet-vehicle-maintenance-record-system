#include "common.h"
#include "database.h"
#include "auth.h"
#include "input.h"
#include "ui.h"
#include "schedule.h"
#include "dsa.h"
#include "http_server.h"

static void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("Options:\n");
    printf("  --demo               Start in Safe Demonstration Mode using isolated 'data/demo_fleet.db'\n");
    printf("  --check-reminders    Run headless maintenance reminder evaluation and exit (for OS task scheduler / cron)\n");
    printf("  --web, --server [p]  Start embedded C HTTP server and white/grey web dashboard (default: %d)\n", DEFAULT_HTTP_PORT);
    printf("  --dsa                Run Academic DSA demonstration suite (Hash Table, BST, Min-Heap, etc.)\n");
    printf("  --db <path>          Use specified SQLite database file (default: %s)\n", DEFAULT_DB_PATH);
    printf("  --help, -h           Show this help message and exit\n");
    printf("  --version, -v        Show program version\n");
}

static void handle_first_run(void) {
    ui_clear_screen();
    ui_print_banner();
    ui_print_header("INITIAL SYSTEM CONFIGURATION");
    printf("Welcome to the Fleet Vehicle Maintenance Record System!\n");
    printf("No administrator account exists. Please create the initial Administrator account.\n");
    ui_print_divider('-', 65);

    char username[MAX_BUFFER_SM];
    char password[MAX_BUFFER_SM];
    char confirm_pwd[MAX_BUFFER_SM];

    while (1) {
        if (!input_get_string("Enter Administrator Username: ", username, sizeof(username), false)) {
            continue;
        }
        if (!input_get_password("Enter Master Administrator Password (min 6 chars): ", password, sizeof(password))) {
            continue;
        }
        if (strlen(password) < 6) {
            ui_print_error("Password must be at least 6 characters long.");
            continue;
        }
        if (!input_get_password("Confirm Administrator Password: ", confirm_pwd, sizeof(confirm_pwd))) {
            continue;
        }
        if (strcmp(password, confirm_pwd) != 0) {
            ui_print_error("Passwords do not match. Please try again.");
            continue;
        }

        StatusCode sc = auth_create_user(username, password, ROLE_ADMIN);
        if (sc == STATUS_OK) {
            ui_print_success("Administrator account configured successfully!");
            input_pause();
            break;
        } else {
            ui_print_error("Failed to create administrator account. Please try again.");
        }
    }
}

static bool perform_login(void) {
    ui_clear_screen();
    ui_print_banner();
    ui_print_header("SECURE OPERATOR LOGIN");

    char username[MAX_BUFFER_SM];
    char password[MAX_BUFFER_SM];

    while (1) {
        if (!input_get_string("Username (or 'exit' to quit): ", username, sizeof(username), false)) {
            return false;
        }
        if (strcmp(username, "exit") == 0 || strcmp(username, "quit") == 0) {
            return false;
        }

        if (!input_get_password("Password: ", password, sizeof(password))) {
            continue;
        }

        User authenticated_user;
        StatusCode sc = auth_login(username, password, &authenticated_user);
        if (sc == STATUS_OK) {
            auth_set_session_user(&authenticated_user);
            ui_print_success("Authentication successful! Welcome.");
            return true;
        } else if (sc == STATUS_FORBIDDEN) {
            ui_print_error("This account has been deactivated. Contact an administrator.");
        } else {
            ui_print_error("Invalid username or password. Please try again.");
        }
    }
}

int main(int argc, char *argv[]) {
    const char *db_path = DEFAULT_DB_PATH;
    bool headless_reminders = false;
    bool run_web = false;
    int web_port = DEFAULT_HTTP_PORT;
    bool run_dsa = false;

    /* Parse command-line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("%s version %s\n", APP_NAME, APP_VERSION);
            return 0;
        } else if (strcmp(argv[i], "--check-reminders") == 0) {
            headless_reminders = true;
        } else if (strcmp(argv[i], "--dsa") == 0) {
            run_dsa = true;
        } else if (strcmp(argv[i], "--demo") == 0) {
            db_path = "data/demo_fleet.db";
            /* Ensure demo database exists by bootstrapping from fleet.db if absent or empty */
            FILE *f_check = fopen("data/demo_fleet.db", "rb");
            long demo_sz = 0;
            if (f_check) {
                fseek(f_check, 0, SEEK_END);
                demo_sz = ftell(f_check);
                fclose(f_check);
            }
            if (!f_check || demo_sz < 4096) {
                FILE *f_src = fopen("data/fleet.db", "rb");
                if (f_src) {
                    FILE *f_dst = fopen("data/demo_fleet.db", "wb");
                    if (f_dst) {
                        char copy_buf[8192];
                        size_t bytes;
                        while ((bytes = fread(copy_buf, 1, sizeof(copy_buf), f_src)) > 0) {
                            fwrite(copy_buf, 1, bytes, f_dst);
                        }
                        fclose(f_dst);
                    }
                    fclose(f_src);
                }
            }
        } else if (strcmp(argv[i], "--web") == 0) {
            run_web = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                web_port = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "--server") == 0) {
            run_web = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                web_port = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* Initialize database and execute migrations */
    if (db_init(db_path) != STATUS_OK) {
        fprintf(stderr, "Fatal: Could not initialize database '%s'.\n", db_path);
        return 1;
    }

    if (db_run_migrations(DEFAULT_MIGRATIONS_DIR) != STATUS_OK) {
        fprintf(stderr, "Fatal: Schema migration failed.\n");
        db_close();
        return 1;
    }

    /* DSA Algorithm Demonstrations */
    if (run_dsa) {
        dsa_run_full_demonstration();
        db_close();
        return 0;
    }

    /* Embedded C HTTP Web Server */
    if (run_web) {
        http_server_start(web_port);
        db_close();
        return 0;
    }

    /* Headless background mode (e.g. cron / Windows Task Scheduler) */
    if (headless_reminders) {
        printf("[HEADLESS] Running maintenance reminder evaluation...\n");
        int updated = 0, notifs = 0;
        schedule_evaluate_all(&updated, &notifs);
        printf("[HEADLESS] Check complete: %d schedule(s) updated, %d alert(s) generated.\n", updated, notifs);
        db_close();
        return 0;
    }

    /* First run setup if no admin exists */
    bool is_first_run = false;
    auth_is_first_run(&is_first_run);
    if (is_first_run) {
        handle_first_run();
    }

    /* Interactive operator login */
    if (!perform_login()) {
        db_close();
        return 0;
    }

    /* Run reminder check on startup */
    int updated_cnt = 0, notif_cnt = 0;
    schedule_evaluate_all(&updated_cnt, &notif_cnt);
    if (notif_cnt > 0) {
        char alert_msg[MAX_BUFFER_MD];
        snprintf(alert_msg, sizeof(alert_msg), "ATTENTION: %d urgent/upcoming maintenance service alerts require review!", notif_cnt);
        ui_print_warning(alert_msg);
        input_pause();
    }

    /* Enter main user interface loop */
    ui_run_main_loop();

    /* Clean application shutdown */
    auth_clear_session();
    db_close();

    return 0;
}
