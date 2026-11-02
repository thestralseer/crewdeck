#include "departments.h"
#include "db.h"
#include "util.h"
#include "audit.h"

void handle_departments_list(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user) {
    (void)hm;
    (void)user;
    sqlite3 *db = db_get_handle();
    if (!db) {
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    const char *sql =
        "SELECT d.id, d.name, d.description, d.head_id, d.created_at, "
        "       COALESCE(e.name, 'Unassigned') as head_name, "
        "       COALESCE(e.avatar_color, '#64748B') as head_color, "
        "       (SELECT COUNT(*) FROM employees emp WHERE emp.department_id = d.id AND emp.status != 'exited') as headcount, "
        "       (SELECT COALESCE(SUM(emp.salary), 0) FROM employees emp WHERE emp.department_id = d.id AND emp.status != 'exited') as total_payroll\n"
        "FROM departments d\n"
        "LEFT JOIN employees e ON d.head_id = e.id\n"
        "ORDER BY d.id ASC;";

    sqlite3_stmt *stmt = NULL;
    cJSON *items = cJSON_CreateArray();

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(item, "name", (const char *)sqlite3_column_text(stmt, 1));
            const char *desc = (const char *)sqlite3_column_text(stmt, 2);
            cJSON_AddStringToObject(item, "description", desc ? desc : "");
            if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
                cJSON_AddNumberToObject(item, "head_id", sqlite3_column_int(stmt, 3));
            } else {
                cJSON_AddNullToObject(item, "head_id");
            }
            cJSON_AddStringToObject(item, "created_at", (const char *)sqlite3_column_text(stmt, 4));
            cJSON_AddStringToObject(item, "head_name", (const char *)sqlite3_column_text(stmt, 5));
            cJSON_AddStringToObject(item, "head_color", (const char *)sqlite3_column_text(stmt, 6));
            cJSON_AddNumberToObject(item, "headcount", sqlite3_column_int(stmt, 7));
            cJSON_AddNumberToObject(item, "total_payroll", sqlite3_column_double(stmt, 8));
            cJSON_AddItemToArray(items, item);
        }
        sqlite3_finalize(stmt);
    }

    cJSON *res = cJSON_CreateObject();
    cJSON_AddItemToObject(res, "departments", items);
    util_json_response(c, 200, res);
}

void handle_department_create(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user) {
    if (strcmp(user->role, "admin") != 0) {
        util_json_error(c, 403, "FORBIDDEN", "Only administrators can create departments");
        return;
    }

    cJSON *body = util_parse_json(hm);
    if (!body) {
        util_json_error(c, 400, "INVALID_JSON", "Request body must be valid JSON");
        return;
    }

    cJSON *name_item = cJSON_GetObjectItemCaseSensitive(body, "name");
    cJSON *desc_item = cJSON_GetObjectItemCaseSensitive(body, "description");
    cJSON *head_item = cJSON_GetObjectItemCaseSensitive(body, "head_id");

    if (!cJSON_IsString(name_item) || !name_item->valuestring || strlen(name_item->valuestring) == 0) {
        cJSON_Delete(body);
        util_json_error(c, 400, "VALIDATION_ERROR", "Department name is required");
        return;
    }

    const char *name = name_item->valuestring;
    const char *desc = (cJSON_IsString(desc_item) && desc_item->valuestring) ? desc_item->valuestring : "";
    int head_id = (cJSON_IsNumber(head_item) && head_item->valueint > 0) ? head_item->valueint : 0;

    sqlite3 *db = db_get_handle();
    if (!db) {
        cJSON_Delete(body);
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    char now_iso[32];
    util_now_iso8601(now_iso, sizeof(now_iso));

    const char *sql = "INSERT INTO departments (name, description, head_id, created_at) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt = NULL;
    int new_id = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, desc, -1, SQLITE_STATIC);
        if (head_id > 0) sqlite3_bind_int(stmt, 3, head_id);
        else sqlite3_bind_null(stmt, 3);
        sqlite3_bind_text(stmt, 4, now_iso, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            new_id = (int)sqlite3_last_insert_rowid(db);
        }
        sqlite3_finalize(stmt);
    }
    cJSON_Delete(body);

    if (new_id <= 0) {
        util_json_error(c, 400, "CREATE_FAILED", "Failed to create department. Name may already exist.");
        return;
    }

    char ip[64];
    util_get_client_ip(c, hm, ip, sizeof(ip));
    char log_details[256];
    snprintf(log_details, sizeof(log_details), "Created department '%s' (ID: %d)", name, new_id);
    audit_log_record(user->id, user->email, "CREATE_DEPARTMENT", "department", new_id, log_details, ip);

    cJSON *res = cJSON_CreateObject();
    cJSON_AddNumberToObject(res, "id", new_id);
    cJSON_AddStringToObject(res, "name", name);
    cJSON_AddStringToObject(res, "message", "Department created successfully");
    util_json_response(c, 201, res);
}

void handle_department_get(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int id) {
    (void)hm;
    (void)user;
    sqlite3 *db = db_get_handle();
    if (!db) {
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    const char *sql =
        "SELECT d.id, d.name, d.description, d.head_id, d.created_at, "
        "       COALESCE(e.name, 'Unassigned') as head_name, "
        "       COALESCE(e.avatar_color, '#64748B') as head_color, "
        "       (SELECT COUNT(*) FROM employees emp WHERE emp.department_id = d.id AND emp.status != 'exited') as headcount\n"
        "FROM departments d\n"
        "LEFT JOIN employees e ON d.head_id = e.id\n"
        "WHERE d.id = ?;";

    sqlite3_stmt *stmt = NULL;
    cJSON *res = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            res = cJSON_CreateObject();
            cJSON_AddNumberToObject(res, "id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(res, "name", (const char *)sqlite3_column_text(stmt, 1));
            const char *desc = (const char *)sqlite3_column_text(stmt, 2);
            cJSON_AddStringToObject(res, "description", desc ? desc : "");
            if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
                cJSON_AddNumberToObject(res, "head_id", sqlite3_column_int(stmt, 3));
            } else {
                cJSON_AddNullToObject(res, "head_id");
            }
            cJSON_AddStringToObject(res, "created_at", (const char *)sqlite3_column_text(stmt, 4));
            cJSON_AddStringToObject(res, "head_name", (const char *)sqlite3_column_text(stmt, 5));
            cJSON_AddStringToObject(res, "head_color", (const char *)sqlite3_column_text(stmt, 6));
            cJSON_AddNumberToObject(res, "headcount", sqlite3_column_int(stmt, 7));
        }
        sqlite3_finalize(stmt);
    }

    if (!res) {
        util_json_error(c, 404, "NOT_FOUND", "Department not found");
        return;
    }

    util_json_response(c, 200, res);
}

void handle_department_update(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int id) {
    if (strcmp(user->role, "admin") != 0 && strcmp(user->role, "manager") != 0) {
        util_json_error(c, 403, "FORBIDDEN", "Admin or manager access required");
        return;
    }

    cJSON *body = util_parse_json(hm);
    if (!body) {
        util_json_error(c, 400, "INVALID_JSON", "Request body must be valid JSON");
        return;
    }

    cJSON *name_item = cJSON_GetObjectItemCaseSensitive(body, "name");
    cJSON *desc_item = cJSON_GetObjectItemCaseSensitive(body, "description");
    cJSON *head_item = cJSON_GetObjectItemCaseSensitive(body, "head_id");

    if (!cJSON_IsString(name_item) || !name_item->valuestring || strlen(name_item->valuestring) == 0) {
        cJSON_Delete(body);
        util_json_error(c, 400, "VALIDATION_ERROR", "Department name is required");
        return;
    }

    const char *name = name_item->valuestring;
    const char *desc = (cJSON_IsString(desc_item) && desc_item->valuestring) ? desc_item->valuestring : "";
    int head_id = (cJSON_IsNumber(head_item) && head_item->valueint > 0) ? head_item->valueint : 0;

    sqlite3 *db = db_get_handle();
    if (!db) {
        cJSON_Delete(body);
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    const char *sql = "UPDATE departments SET name = ?, description = ?, head_id = ? WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    bool ok = false;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, desc, -1, SQLITE_STATIC);
        if (head_id > 0) sqlite3_bind_int(stmt, 3, head_id);
        else sqlite3_bind_null(stmt, 3);
        sqlite3_bind_int(stmt, 4, id);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = (sqlite3_changes(db) > 0);
        }
        sqlite3_finalize(stmt);
    }
    cJSON_Delete(body);

    if (!ok) {
        util_json_error(c, 400, "UPDATE_FAILED", "Failed to update department");
        return;
    }

    char ip[64];
    util_get_client_ip(c, hm, ip, sizeof(ip));
    char log_details[256];
    snprintf(log_details, sizeof(log_details), "Updated department ID %d ('%s')", id, name);
    audit_log_record(user->id, user->email, "UPDATE_DEPARTMENT", "department", id, log_details, ip);

    cJSON *res = cJSON_CreateObject();
    cJSON_AddNumberToObject(res, "id", id);
    cJSON_AddStringToObject(res, "name", name);
    cJSON_AddStringToObject(res, "message", "Department updated successfully");
    util_json_response(c, 200, res);
}

void handle_department_delete(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int id) {
    if (strcmp(user->role, "admin") != 0) {
        util_json_error(c, 403, "FORBIDDEN", "Only administrators can delete departments");
        return;
    }

    sqlite3 *db = db_get_handle();
    if (!db) {
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    /* Unassign employees first */
    sqlite3_stmt *u_stmt = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE employees SET department_id = NULL WHERE department_id = ?;", -1, &u_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(u_stmt, 1, id);
        sqlite3_step(u_stmt);
        sqlite3_finalize(u_stmt);
    }

    sqlite3_stmt *stmt = NULL;
    bool ok = false;
    if (sqlite3_prepare_v2(db, "DELETE FROM departments WHERE id = ?;", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = (sqlite3_changes(db) > 0);
        }
        sqlite3_finalize(stmt);
    }

    if (!ok) {
        util_json_error(c, 404, "NOT_FOUND", "Department not found or already deleted");
        return;
    }

    char ip[64];
    util_get_client_ip(c, hm, ip, sizeof(ip));
    char log_details[256];
    snprintf(log_details, sizeof(log_details), "Deleted department ID: %d", id);
    audit_log_record(user->id, user->email, "DELETE_DEPARTMENT", "department", id, log_details, ip);

    cJSON *res = cJSON_CreateObject();
    cJSON_AddNumberToObject(res, "id", id);
    cJSON_AddStringToObject(res, "message", "Department deleted successfully");
    util_json_response(c, 200, res);
}
