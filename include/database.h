#ifndef DATABASE_H
#define DATABASE_H

#include "common.h"
#include <sqlite3.h>

/* Database connection & transaction management */
StatusCode db_init(const char *db_path);
void db_close(void);
sqlite3 *db_get_handle(void);
const char *db_get_path(void);

StatusCode db_begin_transaction(void);
StatusCode db_commit_transaction(void);
StatusCode db_rollback_transaction(void);

/* Migration execution */
StatusCode db_run_migrations(const char *migrations_dir);
StatusCode db_check_integrity(char *out_result, size_t max_len);

/* Helper to execute a single SQL statement */
StatusCode db_exec_simple(const char *sql);

/* Helper to get last inserted rowid */
int64_t db_last_insert_id(void);

#endif /* DATABASE_H */
