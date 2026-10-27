#ifndef BACKUP_H
#define BACKUP_H

#include "common.h"

StatusCode backup_create(const char *backup_dir, char *out_filepath, size_t max_len);
StatusCode backup_restore(const char *backup_filepath);
StatusCode backup_list(const char *backup_dir);

#endif /* BACKUP_H */
