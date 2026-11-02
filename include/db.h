#ifndef DB_H
#define DB_H

#include "common.h"

bool db_init(const char *db_path);
void db_close(void);
sqlite3 *db_get_handle(void);
bool db_exec_simple(const char *sql);
bool db_seed_if_empty(void);

#endif /* DB_H */
