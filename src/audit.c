#include "audit.h"
#include "db.h"
#include "util.h"

void audit_log_record(int user_id, const char *user_email, const char *action,
                      const char *entity_type, int entity_id,
                      const char *details, const char *ip) {
    sqlite3 *db = db_get_handle();
    if (!db) return;

    char now_iso[32];
    util_now_iso8601(now_iso, sizeof(now_iso));

    const char *sql =
        "INSERT INTO audit_logs (user_id, user_email, action, entity_type, entity_id, details, ip_address, created_at)\n"
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (user_id > 0) sqlite3_bind_int(stmt, 1, user_id);
        else sqlite3_bind_null(stmt, 1);

        sqlite3_bind_text(stmt, 2, user_email ? user_email : "system", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, action ? action : "ACTION", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, entity_type ? entity_type : "unknown", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 5, entity_id);
        sqlite3_bind_text(stmt, 6, details ? details : "", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, ip ? ip : "127.0.0.1", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 8, now_iso, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void handle_audit_list(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user) {
    if (strcmp(user->role, "admin") != 0 && strcmp(user->role, "manager") != 0) {
        util_json_error(c, 403, "FORBIDDEN", "Admin or manager access required");
        return;
    }

    sqlite3 *db = db_get_handle();
    if (!db) {
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    char page_str[32] = {0};
    char limit_str[32] = {0};
    char action_str[64] = {0};

    util_get_query_param(hm, "page", page_str, sizeof(page_str));
    util_get_query_param(hm, "limit", limit_str, sizeof(limit_str));
    util_get_query_param(hm, "action", action_str, sizeof(action_str));

    int page = atoi(page_str);
    int limit = atoi(limit_str);
    if (page < 1) page = 1;
    if (limit < 1 || limit > 100) limit = 20;
    int offset = (page - 1) * limit;

    bool has_action = (action_str[0] != '\0');

    /* Count total */
    int total = 0;
    const char *count_sql = has_action
        ? "SELECT COUNT(*) FROM audit_logs WHERE action = ?;"
        : "SELECT COUNT(*) FROM audit_logs;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, count_sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (has_action) sqlite3_bind_text(stmt, 1, action_str, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            total = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    /* Query records */
    const char *data_sql = has_action
        ? "SELECT id, user_id, user_email, action, entity_type, entity_id, details, ip_address, created_at "
          "FROM audit_logs WHERE action = ? ORDER BY id DESC LIMIT ? OFFSET ?;"
        : "SELECT id, user_id, user_email, action, entity_type, entity_id, details, ip_address, created_at "
          "FROM audit_logs ORDER BY id DESC LIMIT ? OFFSET ?;";

    cJSON *items = cJSON_CreateArray();
    if (sqlite3_prepare_v2(db, data_sql, -1, &stmt, NULL) == SQLITE_OK) {
        int idx = 1;
        if (has_action) {
            sqlite3_bind_text(stmt, idx++, action_str, -1, SQLITE_STATIC);
        }
        sqlite3_bind_int(stmt, idx++, limit);
        sqlite3_bind_int(stmt, idx++, offset);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
            if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
                cJSON_AddNumberToObject(item, "user_id", sqlite3_column_int(stmt, 1));
            } else {
                cJSON_AddNullToObject(item, "user_id");
            }
            cJSON_AddStringToObject(item, "user_email", (const char *)sqlite3_column_text(stmt, 2));
            cJSON_AddStringToObject(item, "action", (const char *)sqlite3_column_text(stmt, 3));
            cJSON_AddStringToObject(item, "entity_type", (const char *)sqlite3_column_text(stmt, 4));
            cJSON_AddNumberToObject(item, "entity_id", sqlite3_column_int(stmt, 5));
            cJSON_AddStringToObject(item, "details", (const char *)sqlite3_column_text(stmt, 6));
            cJSON_AddStringToObject(item, "ip_address", (const char *)sqlite3_column_text(stmt, 7));
            cJSON_AddStringToObject(item, "created_at", (const char *)sqlite3_column_text(stmt, 8));
            cJSON_AddItemToArray(items, item);
        }
        sqlite3_finalize(stmt);
    }

    cJSON *res = cJSON_CreateObject();
    cJSON_AddItemToObject(res, "items", items);

    cJSON *pag = cJSON_CreateObject();
    cJSON_AddNumberToObject(pag, "page", page);
    cJSON_AddNumberToObject(pag, "limit", limit);
    cJSON_AddNumberToObject(pag, "total", total);
    int pages = (total + limit - 1) / limit;
    cJSON_AddNumberToObject(pag, "pages", pages > 0 ? pages : 1);
    cJSON_AddItemToObject(res, "pagination", pag);

    util_json_response(c, 200, res);
}
