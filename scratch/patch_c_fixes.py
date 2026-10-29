import re

# 1. Update src/main.c to copy data/fleet.db to data/demo_fleet.db if demo_fleet.db does not exist
with open('src/main.c', 'r', encoding='utf-8') as f:
    main_c = f.read()

old_demo_flag = '''        } else if (strcmp(argv[i], "--demo") == 0) {
            db_path = "data/demo_fleet.db";'''

new_demo_flag = '''        } else if (strcmp(argv[i], "--demo") == 0) {
            db_path = "data/demo_fleet.db";
            /* Ensure demo database exists by bootstrapping from fleet.db if absent */
            FILE *f_check = fopen("data/demo_fleet.db", "rb");
            if (!f_check) {
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
            } else {
                fclose(f_check);
            }'''

if old_demo_flag in main_c:
    main_c = main_c.replace(old_demo_flag, new_demo_flag, 1)
    with open('src/main.c', 'w', encoding='utf-8') as f:
        f.write(main_c)
    print('[OK] Updated src/main.c for safe demo database bootstrapping')
else:
    print('[FAIL] old_demo_flag not found in src/main.c')

# 2. Update src/http_server.c
with open('src/http_server.c', 'r', encoding='utf-8') as f:
    http_c = f.read()

# Route /api/notifications/ack-all in handle_client_request
old_route = '''        } else if (strcmp(path, "/api/notifications/ack") == 0) {
            handle_api_notifications(client, "POST", body, query, &current_user);'''

new_route = '''        } else if (strcmp(path, "/api/notifications/ack") == 0) {
            handle_api_notifications(client, "POST", body, query, &current_user);
        } else if (strcmp(path, "/api/notifications/ack-all") == 0) {
            handle_api_notifications(client, "POST", "{\"action\":\"ack_all\"}", query, &current_user);'''

if old_route in http_c:
    http_c = http_c.replace(old_route, new_route, 1)
    print('[OK] Added /api/notifications/ack-all route in http_server.c')
else:
    print('[FAIL] old_route not found in http_server.c')

with open('src/http_server.c', 'w', encoding='utf-8') as f:
    f.write(http_c)

print('[SUCCESS] http_server.c initial patch complete')
