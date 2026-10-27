#include "backup.h"
#include "database.h"
#include "audit.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define MKDIR(d) _mkdir(d)
#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#define MKDIR(d) mkdir(d, 0755)
#endif

static StatusCode copy_db_online(sqlite3 *src_db, sqlite3 *dest_db) {
    sqlite3_backup *backup = sqlite3_backup_init(dest_db, "main", src_db, "main");
    if (!backup) {
        fprintf(stderr, "[BACKUP ERROR] Failed to initialize backup: %s\n", sqlite3_errmsg(dest_db));
        return STATUS_DB_ERROR;
    }

    int rc;
    do {
        rc = sqlite3_backup_step(backup, 250);
    } while (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED);

    sqlite3_backup_finish(backup);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[BACKUP ERROR] Backup step failed with code %d\n", rc);
        return STATUS_DB_ERROR;
    }
    return STATUS_OK;
}

StatusCode backup_create(const char *backup_dir, char *out_filepath, size_t max_len) {
    sqlite3 *src_db = db_get_handle();
    if (!src_db) return STATUS_DB_ERROR;

    const char *dir = backup_dir ? backup_dir : DEFAULT_BACKUP_DIR;
    MKDIR(dir);

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char filename[MAX_BUFFER_MD];
    strftime(filename, sizeof(filename), "fleet_backup_%Y%m%d_%H%M%S.db", tm_info);

    char full_path[MAX_BUFFER_LG];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, filename);

    sqlite3 *dest_db = NULL;
    int rc = sqlite3_open(full_path, &dest_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[BACKUP ERROR] Failed to open backup file '%s': %s\n", full_path, sqlite3_errmsg(dest_db));
        if (dest_db) sqlite3_close(dest_db);
        return STATUS_DB_ERROR;
    }

    StatusCode sc = copy_db_online(src_db, dest_db);
    sqlite3_close(dest_db);

    if (sc != STATUS_OK) {
        remove(full_path);
        return sc;
    }

    if (out_filepath && max_len > 0) {
        safe_strncpy(out_filepath, full_path, max_len);
    }

    char details[MAX_BUFFER_XL];
    snprintf(details, sizeof(details), "Created database backup: %s", full_path);
    audit_log_action(0, "BACKUP_CREATE", "database", 0, details);

    return STATUS_OK;
}

StatusCode backup_restore(const char *backup_filepath) {
    if (str_is_blank(backup_filepath)) return STATUS_INVALID_INPUT;

    FILE *fp = fopen(backup_filepath, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Backup file does not exist: %s\n", backup_filepath);
        return STATUS_NOT_FOUND;
    }
    fclose(fp);

    /* Open backup file as source */
    sqlite3 *backup_db = NULL;
    int rc = sqlite3_open_v2(backup_filepath, &backup_db, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[RESTORE ERROR] Cannot open backup file '%s': %s\n", backup_filepath, sqlite3_errmsg(backup_db));
        if (backup_db) sqlite3_close(backup_db);
        return STATUS_DB_ERROR;
    }

    /* Verify backup file integrity before restoring */
    sqlite3_stmt *check_stmt = NULL;
    if (sqlite3_prepare_v2(backup_db, "PRAGMA integrity_check;", -1, &check_stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            const char *res = (const char *)sqlite3_column_text(check_stmt, 0);
            if (!res || strcmp(res, "ok") != 0) {
                fprintf(stderr, "[RESTORE ERROR] Backup file failed integrity check: %s\n", res ? res : "failed");
                sqlite3_finalize(check_stmt);
                sqlite3_close(backup_db);
                return STATUS_ERROR;
            }
        }
        sqlite3_finalize(check_stmt);
    }

    /* Step 1: Create a safety pre-restore backup of the current database */
    printf("[RESTORE] Creating safety snapshot of current database before restore...\n");
    char safety_path[MAX_BUFFER_LG];
    if (backup_create(DEFAULT_BACKUP_DIR, safety_path, sizeof(safety_path)) == STATUS_OK) {
        printf("[RESTORE] Safety snapshot saved to: %s\n", safety_path);
    }

    /* Step 2: Copy from backup_db into live db */
    sqlite3 *live_db = db_get_handle();
    StatusCode sc = copy_db_online(backup_db, live_db);
    sqlite3_close(backup_db);

    if (sc != STATUS_OK) {
        fprintf(stderr, "[RESTORE ERROR] Failed during online database restore.\n");
        return sc;
    }

    /* Step 3: Verify live database integrity */
    char check_res[64] = {0};
    if (db_check_integrity(check_res, sizeof(check_res)) == STATUS_OK) {
        if (strcmp(check_res, "ok") != 0) {
            fprintf(stderr, "[RESTORE WARNING] Live database reported integrity issue: %s\n", check_res);
            return STATUS_DB_ERROR;
        }
    }

    char details[MAX_BUFFER_LG];
    snprintf(details, sizeof(details), "Restored database from: %s", backup_filepath);
    audit_log_action(0, "BACKUP_RESTORE", "database", 0, details);

    return STATUS_OK;
}

StatusCode backup_list(const char *backup_dir) {
    const char *dir = backup_dir ? backup_dir : DEFAULT_BACKUP_DIR;
    printf("\nAvailable Backups in '%s':\n", dir);
    printf("+--------------------------------------+------------+\n");
    printf("| Backup Filename                      | Size (KB)  |\n");
    printf("+--------------------------------------+------------+\n");

    int count = 0;
#ifdef _WIN32
    char search_pattern[MAX_BUFFER_MD];
    snprintf(search_pattern, sizeof(search_pattern), "%s/*.db", dir);
    struct _finddata_t fileinfo;
    intptr_t handle = _findfirst(search_pattern, &fileinfo);
    if (handle != -1) {
        do {
            printf("| %-36s | %10ld |\n", fileinfo.name, (long)(fileinfo.size / 1024));
            count++;
        } while (_findnext(handle, &fileinfo) == 0);
        _findclose(handle);
    }
#else
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (strstr(de->d_name, ".db")) {
                char path[MAX_BUFFER_MD];
                snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
                struct stat st;
                long sz = 0;
                if (stat(path, &st) == 0) sz = st.st_size / 1024;
                printf("| %-36s | %10ld |\n", de->d_name, sz);
                count++;
            }
        }
        closedir(d);
    }
#endif

    printf("+--------------------------------------+------------+\n");
    if (count == 0) {
        printf("No backup files found.\n");
    } else {
        printf("Total backups found: %d\n", count);
    }
    return STATUS_OK;
}
