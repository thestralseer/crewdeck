#include "employees.h"
#include "db.h"
#include "util.h"
#include "audit.h"

static const char *DEFAULT_AVATAR_COLORS[] = {
    "#FFB020", "#2DD4BF", "#EC4899", "#3B82F6", "#10B981", "#F59E0B", "#8B5CF6", "#06B6D4", "#F97316"
};

void handle_employees_list(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user) {
    (void)user;
    sqlite3 *db = db_get_handle();
    if (!db) {
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    char search[128] = {0};
    char dept_str[32] = {0};
    char status[32] = {0};
    char sort[32] = {0};
    char order[16] = {0};
    char page_str[32] = {0};
    char limit_str[32] = {0};

    util_get_query_param(hm, "search", search, sizeof(search));
    util_get_query_param(hm, "dept", dept_str, sizeof(dept_str));
    util_get_query_param(hm, "status", status, sizeof(status));
    util_get_query_param(hm, "sort", sort, sizeof(sort));
    util_get_query_param(hm, "order", order, sizeof(order));
    util_get_query_param(hm, "page", page_str, sizeof(page_str));
    util_get_query_param(hm, "limit", limit_str, sizeof(limit_str));

    int page = atoi(page_str);
    int limit = atoi(limit_str);
    if (page < 1) page = 1;
    if (limit < 1 || limit > 100) limit = 10;
    int offset = (page - 1) * limit;

    int dept_id = atoi(dept_str);
    bool has_search = (search[0] != '\0');
    bool has_dept = (dept_id > 0);
    bool has_status = (status[0] != '\0');

    /* Build sort column and direction safely */
    const char *sort_col = "e.id";
    if (strcmp(sort, "name") == 0) sort_col = "e.name";
    else if (strcmp(sort, "salary") == 0) sort_col = "e.salary";
    else if (strcmp(sort, "join_date") == 0) sort_col = "e.join_date";
    else if (strcmp(sort, "department") == 0) sort_col = "d.name";

    const char *sort_dir = "ASC";
    if (strcasecmp(order, "desc") == 0) sort_dir = "DESC";

    /* Count query */
    char count_sql[512];
    snprintf(count_sql, sizeof(count_sql),
             "SELECT COUNT(*) FROM employees e\n"
             "LEFT JOIN departments d ON e.department_id = d.id\n"
             "WHERE 1=1 %s %s %s;",
             has_search ? "AND (e.name LIKE ? OR e.email LIKE ? OR e.role_title LIKE ?)" : "",
             has_dept ? "AND e.department_id = ?" : "",
             has_status ? "AND e.status = ?" : "");

    sqlite3_stmt *stmt = NULL;
    int total = 0;
    char search_pattern[140] = {0};
    if (has_search) {
        snprintf(search_pattern, sizeof(search_pattern), "%%%s%%", search);
    }

    if (sqlite3_prepare_v2(db, count_sql, -1, &stmt, NULL) == SQLITE_OK) {
        int bind_idx = 1;
        if (has_search) {
            sqlite3_bind_text(stmt, bind_idx++, search_pattern, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, bind_idx++, search_pattern, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, bind_idx++, search_pattern, -1, SQLITE_STATIC);
        }
        if (has_dept) {
            sqlite3_bind_int(stmt, bind_idx++, dept_id);
        }
        if (has_status) {
            sqlite3_bind_text(stmt, bind_idx++, status, -1, SQLITE_STATIC);
        }
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            total = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    /* Data query */
    char data_sql[1024];
    snprintf(data_sql, sizeof(data_sql),
             "SELECT e.id, e.name, e.email, e.phone, e.role_title, e.department_id, e.manager_id, "
             "       e.salary, e.join_date, e.status, e.avatar_color, e.created_at, e.updated_at, "
             "       COALESCE(d.name, 'Unassigned') as dept_name, "
             "       COALESCE(m.name, 'None') as manager_name\n"
             "FROM employees e\n"
             "LEFT JOIN departments d ON e.department_id = d.id\n"
             "LEFT JOIN employees m ON e.manager_id = m.id\n"
             "WHERE 1=1 %s %s %s\n"
             "ORDER BY %s %s LIMIT ? OFFSET ?;",
             has_search ? "AND (e.name LIKE ? OR e.email LIKE ? OR e.role_title LIKE ?)" : "",
             has_dept ? "AND e.department_id = ?" : "",
             has_status ? "AND e.status = ?" : "",
             sort_col, sort_dir);

    cJSON *items = cJSON_CreateArray();
    if (sqlite3_prepare_v2(db, data_sql, -1, &stmt, NULL) == SQLITE_OK) {
        int bind_idx = 1;
        if (has_search) {
            sqlite3_bind_text(stmt, bind_idx++, search_pattern, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, bind_idx++, search_pattern, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, bind_idx++, search_pattern, -1, SQLITE_STATIC);
        }
        if (has_dept) {
            sqlite3_bind_int(stmt, bind_idx++, dept_id);
        }
        if (has_status) {
            sqlite3_bind_text(stmt, bind_idx++, status, -1, SQLITE_STATIC);
        }
        sqlite3_bind_int(stmt, bind_idx++, limit);
        sqlite3_bind_int(stmt, bind_idx++, offset);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(item, "name", (const char *)sqlite3_column_text(stmt, 1));
            cJSON_AddStringToObject(item, "email", (const char *)sqlite3_column_text(stmt, 2));
            cJSON_AddStringToObject(item, "phone", (const char *)sqlite3_column_text(stmt, 3));
            cJSON_AddStringToObject(item, "role_title", (const char *)sqlite3_column_text(stmt, 4));
            if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
                cJSON_AddNumberToObject(item, "department_id", sqlite3_column_int(stmt, 5));
            } else {
                cJSON_AddNullToObject(item, "department_id");
            }
            if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
                cJSON_AddNumberToObject(item, "manager_id", sqlite3_column_int(stmt, 6));
            } else {
                cJSON_AddNullToObject(item, "manager_id");
            }
            cJSON_AddNumberToObject(item, "salary", sqlite3_column_double(stmt, 7));
            cJSON_AddStringToObject(item, "join_date", (const char *)sqlite3_column_text(stmt, 8));
            cJSON_AddStringToObject(item, "status", (const char *)sqlite3_column_text(stmt, 9));
            cJSON_AddStringToObject(item, "avatar_color", (const char *)sqlite3_column_text(stmt, 10));
            cJSON_AddStringToObject(item, "created_at", (const char *)sqlite3_column_text(stmt, 11));
            cJSON_AddStringToObject(item, "updated_at", (const char *)sqlite3_column_text(stmt, 12));
            cJSON_AddStringToObject(item, "department_name", (const char *)sqlite3_column_text(stmt, 13));
            cJSON_AddStringToObject(item, "manager_name", (const char *)sqlite3_column_text(stmt, 14));
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

void handle_employee_create(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user) {
    if (strcmp(user->role, "admin") != 0 && strcmp(user->role, "manager") != 0) {
        util_json_error(c, 403, "FORBIDDEN", "Admin or manager access required to add employees");
        return;
    }

    cJSON *body = util_parse_json(hm);
    if (!body) {
        util_json_error(c, 400, "INVALID_JSON", "Request body must be valid JSON");
        return;
    }

    cJSON *name_item = cJSON_GetObjectItemCaseSensitive(body, "name");
    cJSON *email_item = cJSON_GetObjectItemCaseSensitive(body, "email");
    cJSON *phone_item = cJSON_GetObjectItemCaseSensitive(body, "phone");
    cJSON *role_item = cJSON_GetObjectItemCaseSensitive(body, "role_title");
    cJSON *dept_item = cJSON_GetObjectItemCaseSensitive(body, "department_id");
    cJSON *mgr_item = cJSON_GetObjectItemCaseSensitive(body, "manager_id");
    cJSON *salary_item = cJSON_GetObjectItemCaseSensitive(body, "salary");
    cJSON *join_item = cJSON_GetObjectItemCaseSensitive(body, "join_date");
    cJSON *status_item = cJSON_GetObjectItemCaseSensitive(body, "status");
    cJSON *color_item = cJSON_GetObjectItemCaseSensitive(body, "avatar_color");

    if (!cJSON_IsString(name_item) || strlen(name_item->valuestring) < 2) {
        cJSON_Delete(body);
        util_json_error(c, 400, "VALIDATION_ERROR", "Valid employee name is required (minimum 2 characters)");
        return;
    }
    if (!cJSON_IsString(email_item) || !util_is_valid_email(email_item->valuestring)) {
        cJSON_Delete(body);
        util_json_error(c, 400, "VALIDATION_ERROR", "A valid email address is required");
        return;
    }
    if (!cJSON_IsString(phone_item) || strlen(phone_item->valuestring) < 5) {
        cJSON_Delete(body);
        util_json_error(c, 400, "VALIDATION_ERROR", "Valid phone number is required");
        return;
    }
    if (!cJSON_IsString(role_item) || strlen(role_item->valuestring) < 2) {
        cJSON_Delete(body);
        util_json_error(c, 400, "VALIDATION_ERROR", "Role / Job title is required");
        return;
    }

    const char *name = name_item->valuestring;
    const char *email = email_item->valuestring;
    const char *phone = phone_item->valuestring;
    const char *role_title = role_item->valuestring;
    int dept_id = (cJSON_IsNumber(dept_item) && dept_item->valueint > 0) ? dept_item->valueint : 0;
    int mgr_id = (cJSON_IsNumber(mgr_item) && mgr_item->valueint > 0) ? mgr_item->valueint : 0;
    double salary = (cJSON_IsNumber(salary_item) && salary_item->valuedouble >= 0.0) ? salary_item->valuedouble : 0.0;

    char join_date[32] = {0};
    if (cJSON_IsString(join_item) && util_is_valid_date(join_item->valuestring)) {
        snprintf(join_date, sizeof(join_date), "%s", join_item->valuestring);
    } else {
        util_today_date(join_date, sizeof(join_date));
    }

    const char *status = "active";
    if (cJSON_IsString(status_item) && status_item->valuestring) {
        if (strcmp(status_item->valuestring, "on-leave") == 0 ||
            strcmp(status_item->valuestring, "exited") == 0) {
            status = status_item->valuestring;
        }
    }

    char color[32] = {0};
    if (cJSON_IsString(color_item) && color_item->valuestring && color_item->valuestring[0] == '#') {
        snprintf(color, sizeof(color), "%s", color_item->valuestring);
    } else {
        /* Pick deterministically from name hash */
        unsigned int h = 0;
        for (const char *p = name; *p; p++) h = h * 31 + (unsigned char)*p;
        int color_idx = h % (sizeof(DEFAULT_AVATAR_COLORS) / sizeof(DEFAULT_AVATAR_COLORS[0]));
        snprintf(color, sizeof(color), "%s", DEFAULT_AVATAR_COLORS[color_idx]);
    }

    sqlite3 *db = db_get_handle();
    if (!db) {
        cJSON_Delete(body);
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    char now_iso[32];
    util_now_iso8601(now_iso, sizeof(now_iso));

    const char *ins_sql =
        "INSERT INTO employees (name, email, phone, role_title, department_id, manager_id, salary, join_date, status, avatar_color, created_at, updated_at)\n"
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    int new_id = 0;

    if (sqlite3_prepare_v2(db, ins_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, email, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, phone, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, role_title, -1, SQLITE_STATIC);
        if (dept_id > 0) sqlite3_bind_int(stmt, 5, dept_id);
        else sqlite3_bind_null(stmt, 5);
        if (mgr_id > 0) sqlite3_bind_int(stmt, 6, mgr_id);
        else sqlite3_bind_null(stmt, 6);
        sqlite3_bind_double(stmt, 7, salary);
        sqlite3_bind_text(stmt, 8, join_date, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 9, status, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 10, color, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 11, now_iso, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 12, now_iso, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            new_id = (int)sqlite3_last_insert_rowid(db);
        }
        sqlite3_finalize(stmt);
    }
    cJSON_Delete(body);

    if (new_id <= 0) {
        util_json_error(c, 400, "CREATE_FAILED", "Failed to create employee. Email may already exist.");
        return;
    }

    /* Initialize leave balance for new employee */
    sqlite3_stmt *bal_stmt = NULL;
    const char *bal_sql = "INSERT INTO leave_balances (employee_id, year, annual_total, annual_used, sick_total, sick_used, casual_total, casual_used) VALUES (?, 2026, 18, 0, 12, 0, 10, 0);";
    if (sqlite3_prepare_v2(db, bal_sql, -1, &bal_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(bal_stmt, 1, new_id);
        sqlite3_step(bal_stmt);
        sqlite3_finalize(bal_stmt);
    }

    char ip[64];
    util_get_client_ip(c, hm, ip, sizeof(ip));
    char log_details[256];
    snprintf(log_details, sizeof(log_details), "Created employee '%s' (%s, ID: %d)", name, role_title, new_id);
    audit_log_record(user->id, user->email, "CREATE_EMPLOYEE", "employee", new_id, log_details, ip);

    cJSON *res = cJSON_CreateObject();
    cJSON_AddNumberToObject(res, "id", new_id);
    cJSON_AddStringToObject(res, "name", name);
    cJSON_AddStringToObject(res, "email", email);
    cJSON_AddStringToObject(res, "message", "Employee onboarded successfully");
    util_json_response(c, 201, res);
}

void handle_employee_get(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int id) {
    (void)hm;
    (void)user;
    sqlite3 *db = db_get_handle();
    if (!db) {
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    const char *sql =
        "SELECT e.id, e.name, e.email, e.phone, e.role_title, e.department_id, e.manager_id, "
        "       e.salary, e.join_date, e.status, e.avatar_color, e.created_at, e.updated_at, "
        "       COALESCE(d.name, 'Unassigned') as dept_name, "
        "       COALESCE(m.name, 'None') as manager_name\n"
        "FROM employees e\n"
        "LEFT JOIN departments d ON e.department_id = d.id\n"
        "LEFT JOIN employees m ON e.manager_id = m.id\n"
        "WHERE e.id = ?;";

    sqlite3_stmt *stmt = NULL;
    cJSON *emp = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            emp = cJSON_CreateObject();
            cJSON_AddNumberToObject(emp, "id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(emp, "name", (const char *)sqlite3_column_text(stmt, 1));
            cJSON_AddStringToObject(emp, "email", (const char *)sqlite3_column_text(stmt, 2));
            cJSON_AddStringToObject(emp, "phone", (const char *)sqlite3_column_text(stmt, 3));
            cJSON_AddStringToObject(emp, "role_title", (const char *)sqlite3_column_text(stmt, 4));
            if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
                cJSON_AddNumberToObject(emp, "department_id", sqlite3_column_int(stmt, 5));
            } else {
                cJSON_AddNullToObject(emp, "department_id");
            }
            if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
                cJSON_AddNumberToObject(emp, "manager_id", sqlite3_column_int(stmt, 6));
            } else {
                cJSON_AddNullToObject(emp, "manager_id");
            }
            cJSON_AddNumberToObject(emp, "salary", sqlite3_column_double(stmt, 7));
            cJSON_AddStringToObject(emp, "join_date", (const char *)sqlite3_column_text(stmt, 8));
            cJSON_AddStringToObject(emp, "status", (const char *)sqlite3_column_text(stmt, 9));
            cJSON_AddStringToObject(emp, "avatar_color", (const char *)sqlite3_column_text(stmt, 10));
            cJSON_AddStringToObject(emp, "created_at", (const char *)sqlite3_column_text(stmt, 11));
            cJSON_AddStringToObject(emp, "updated_at", (const char *)sqlite3_column_text(stmt, 12));
            cJSON_AddStringToObject(emp, "department_name", (const char *)sqlite3_column_text(stmt, 13));
            cJSON_AddStringToObject(emp, "manager_name", (const char *)sqlite3_column_text(stmt, 14));
        }
        sqlite3_finalize(stmt);
    }

    if (!emp) {
        util_json_error(c, 404, "NOT_FOUND", "Employee not found");
        return;
    }

    /* Include Leave Balance */
    const char *bal_sql = "SELECT annual_total, annual_used, sick_total, sick_used, casual_total, casual_used FROM leave_balances WHERE employee_id = ? AND year = 2026;";
    sqlite3_stmt *b_stmt = NULL;
    cJSON *bal = cJSON_CreateObject();
    if (sqlite3_prepare_v2(db, bal_sql, -1, &b_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(b_stmt, 1, id);
        if (sqlite3_step(b_stmt) == SQLITE_ROW) {
            cJSON_AddNumberToObject(bal, "annual_total", sqlite3_column_int(b_stmt, 0));
            cJSON_AddNumberToObject(bal, "annual_used", sqlite3_column_int(b_stmt, 1));
            cJSON_AddNumberToObject(bal, "sick_total", sqlite3_column_int(b_stmt, 2));
            cJSON_AddNumberToObject(bal, "sick_used", sqlite3_column_int(b_stmt, 3));
            cJSON_AddNumberToObject(bal, "casual_total", sqlite3_column_int(b_stmt, 4));
            cJSON_AddNumberToObject(bal, "casual_used", sqlite3_column_int(b_stmt, 5));
        } else {
            cJSON_AddNumberToObject(bal, "annual_total", 18);
            cJSON_AddNumberToObject(bal, "annual_used", 0);
            cJSON_AddNumberToObject(bal, "sick_total", 12);
            cJSON_AddNumberToObject(bal, "sick_used", 0);
            cJSON_AddNumberToObject(bal, "casual_total", 10);
            cJSON_AddNumberToObject(bal, "casual_used", 0);
        }
        sqlite3_finalize(b_stmt);
    }
    cJSON_AddItemToObject(emp, "leave_balance", bal);

    /* Include Recent 7 Attendance Records */
    const char *att_sql = "SELECT date, check_in, check_out, status FROM attendance WHERE employee_id = ? ORDER BY date DESC LIMIT 7;";
    sqlite3_stmt *a_stmt = NULL;
    cJSON *recent_att = cJSON_CreateArray();
    if (sqlite3_prepare_v2(db, att_sql, -1, &a_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(a_stmt, 1, id);
        while (sqlite3_step(a_stmt) == SQLITE_ROW) {
            cJSON *ar = cJSON_CreateObject();
            cJSON_AddStringToObject(ar, "date", (const char *)sqlite3_column_text(a_stmt, 0));
            const char *cin = (const char *)sqlite3_column_text(a_stmt, 1);
            const char *cout = (const char *)sqlite3_column_text(a_stmt, 2);
            cJSON_AddStringToObject(ar, "check_in", cin ? cin : "--");
            cJSON_AddStringToObject(ar, "check_out", cout ? cout : "--");
            cJSON_AddStringToObject(ar, "status", (const char *)sqlite3_column_text(a_stmt, 3));
            cJSON_AddItemToArray(recent_att, ar);
        }
        sqlite3_finalize(a_stmt);
    }
    cJSON_AddItemToObject(emp, "recent_attendance", recent_att);

    util_json_response(c, 200, emp);
}

void handle_employee_update(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int id) {
    if (strcmp(user->role, "admin") != 0 && strcmp(user->role, "manager") != 0) {
        util_json_error(c, 403, "FORBIDDEN", "Admin or manager access required to edit employee details");
        return;
    }

    cJSON *body = util_parse_json(hm);
    if (!body) {
        util_json_error(c, 400, "INVALID_JSON", "Request body must be valid JSON");
        return;
    }

    sqlite3 *db = db_get_handle();
    if (!db) {
        cJSON_Delete(body);
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    /* Retrieve current record */
    sqlite3_stmt *stmt = NULL;
    bool exists = false;
    char cur_name[128] = {0};
    char cur_email[128] = {0};
    char cur_phone[64] = {0};
    char cur_role[128] = {0};
    int cur_dept = 0;
    int cur_mgr = 0;
    double cur_salary = 0.0;
    char cur_join[32] = {0};
    char cur_status[32] = {0};
    char cur_color[32] = {0};

    if (sqlite3_prepare_v2(db, "SELECT name, email, phone, role_title, department_id, manager_id, salary, join_date, status, avatar_color FROM employees WHERE id = ?;", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = true;
            snprintf(cur_name, sizeof(cur_name), "%s", (const char *)sqlite3_column_text(stmt, 0));
            snprintf(cur_email, sizeof(cur_email), "%s", (const char *)sqlite3_column_text(stmt, 1));
            snprintf(cur_phone, sizeof(cur_phone), "%s", (const char *)sqlite3_column_text(stmt, 2));
            snprintf(cur_role, sizeof(cur_role), "%s", (const char *)sqlite3_column_text(stmt, 3));
            cur_dept = (sqlite3_column_type(stmt, 4) != SQLITE_NULL) ? sqlite3_column_int(stmt, 4) : 0;
            cur_mgr = (sqlite3_column_type(stmt, 5) != SQLITE_NULL) ? sqlite3_column_int(stmt, 5) : 0;
            cur_salary = sqlite3_column_double(stmt, 6);
            snprintf(cur_join, sizeof(cur_join), "%s", (const char *)sqlite3_column_text(stmt, 7));
            snprintf(cur_status, sizeof(cur_status), "%s", (const char *)sqlite3_column_text(stmt, 8));
            snprintf(cur_color, sizeof(cur_color), "%s", (const char *)sqlite3_column_text(stmt, 9));
        }
        sqlite3_finalize(stmt);
    }

    if (!exists) {
        cJSON_Delete(body);
        util_json_error(c, 404, "NOT_FOUND", "Employee not found");
        return;
    }

    /* Merge fields */
    cJSON *item = NULL;
    if ((item = cJSON_GetObjectItemCaseSensitive(body, "name")) && cJSON_IsString(item) && strlen(item->valuestring) >= 2) {
        snprintf(cur_name, sizeof(cur_name), "%s", item->valuestring);
    }
    if ((item = cJSON_GetObjectItemCaseSensitive(body, "email")) && cJSON_IsString(item) && util_is_valid_email(item->valuestring)) {
        snprintf(cur_email, sizeof(cur_email), "%s", item->valuestring);
    }
    if ((item = cJSON_GetObjectItemCaseSensitive(body, "phone")) && cJSON_IsString(item) && strlen(item->valuestring) >= 5) {
        snprintf(cur_phone, sizeof(cur_phone), "%s", item->valuestring);
    }
    if ((item = cJSON_GetObjectItemCaseSensitive(body, "role_title")) && cJSON_IsString(item) && strlen(item->valuestring) >= 2) {
        snprintf(cur_role, sizeof(cur_role), "%s", item->valuestring);
    }
    if ((item = cJSON_GetObjectItemCaseSensitive(body, "department_id"))) {
        if (cJSON_IsNumber(item)) cur_dept = item->valueint;
        else if (cJSON_IsNull(item)) cur_dept = 0;
    }
    if ((item = cJSON_GetObjectItemCaseSensitive(body, "manager_id"))) {
        if (cJSON_IsNumber(item)) cur_mgr = item->valueint;
        else if (cJSON_IsNull(item)) cur_mgr = 0;
    }
    if ((item = cJSON_GetObjectItemCaseSensitive(body, "salary")) && cJSON_IsNumber(item) && item->valuedouble >= 0.0) {
        cur_salary = item->valuedouble;
    }
    if ((item = cJSON_GetObjectItemCaseSensitive(body, "join_date")) && cJSON_IsString(item) && util_is_valid_date(item->valuestring)) {
        snprintf(cur_join, sizeof(cur_join), "%s", item->valuestring);
    }
    if ((item = cJSON_GetObjectItemCaseSensitive(body, "status")) && cJSON_IsString(item)) {
        if (strcmp(item->valuestring, "active") == 0 ||
            strcmp(item->valuestring, "on-leave") == 0 ||
            strcmp(item->valuestring, "exited") == 0) {
            snprintf(cur_status, sizeof(cur_status), "%s", item->valuestring);
        }
    }
    if ((item = cJSON_GetObjectItemCaseSensitive(body, "avatar_color")) && cJSON_IsString(item) && item->valuestring[0] == '#') {
        snprintf(cur_color, sizeof(cur_color), "%s", item->valuestring);
    }

    cJSON_Delete(body);

    char now_iso[32];
    util_now_iso8601(now_iso, sizeof(now_iso));

    const char *upd_sql =
        "UPDATE employees SET name = ?, email = ?, phone = ?, role_title = ?, department_id = ?, manager_id = ?, "
        "salary = ?, join_date = ?, status = ?, avatar_color = ?, updated_at = ? WHERE id = ?;";

    bool ok = false;
    if (sqlite3_prepare_v2(db, upd_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, cur_name, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, cur_email, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, cur_phone, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, cur_role, -1, SQLITE_STATIC);
        if (cur_dept > 0) sqlite3_bind_int(stmt, 5, cur_dept);
        else sqlite3_bind_null(stmt, 5);
        if (cur_mgr > 0) sqlite3_bind_int(stmt, 6, cur_mgr);
        else sqlite3_bind_null(stmt, 6);
        sqlite3_bind_double(stmt, 7, cur_salary);
        sqlite3_bind_text(stmt, 8, cur_join, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 9, cur_status, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 10, cur_color, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 11, now_iso, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 12, id);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = true;
        }
        sqlite3_finalize(stmt);
    }

    if (!ok) {
        util_json_error(c, 400, "UPDATE_FAILED", "Failed to update employee details. Email may conflict.");
        return;
    }

    char ip[64];
    util_get_client_ip(c, hm, ip, sizeof(ip));
    char log_details[256];
    snprintf(log_details, sizeof(log_details), "Updated employee ID %d ('%s', %s)", id, cur_name, cur_role);
    audit_log_record(user->id, user->email, "UPDATE_EMPLOYEE", "employee", id, log_details, ip);

    cJSON *res = cJSON_CreateObject();
    cJSON_AddNumberToObject(res, "id", id);
    cJSON_AddStringToObject(res, "name", cur_name);
    cJSON_AddStringToObject(res, "message", "Employee profile updated successfully");
    util_json_response(c, 200, res);
}

void handle_employee_delete(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int id) {
    if (strcmp(user->role, "admin") != 0) {
        util_json_error(c, 403, "FORBIDDEN", "Only administrators can delete employee records");
        return;
    }

    sqlite3 *db = db_get_handle();
    if (!db) {
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    /* Check if employee exists and get name */
    char emp_name[128] = {0};
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT name FROM employees WHERE id = ?;", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            snprintf(emp_name, sizeof(emp_name), "%s", (const char *)sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }

    if (emp_name[0] == '\0') {
        util_json_error(c, 404, "NOT_FOUND", "Employee not found");
        return;
    }

    /* Unset manager_id for direct reports */
    sqlite3_stmt *u_stmt = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE employees SET manager_id = NULL WHERE manager_id = ?;", -1, &u_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(u_stmt, 1, id);
        sqlite3_step(u_stmt);
        sqlite3_finalize(u_stmt);
    }

    /* Unset head_id for departments where this employee is head */
    if (sqlite3_prepare_v2(db, "UPDATE departments SET head_id = NULL WHERE head_id = ?;", -1, &u_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(u_stmt, 1, id);
        sqlite3_step(u_stmt);
        sqlite3_finalize(u_stmt);
    }

    /* Delete employee (cascades to attendance and leave requests) */
    bool ok = false;
    if (sqlite3_prepare_v2(db, "DELETE FROM employees WHERE id = ?;", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = (sqlite3_changes(db) > 0);
        }
        sqlite3_finalize(stmt);
    }

    if (!ok) {
        util_json_error(c, 400, "DELETE_FAILED", "Failed to delete employee record");
        return;
    }

    char ip[64];
    util_get_client_ip(c, hm, ip, sizeof(ip));
    char log_details[256];
    snprintf(log_details, sizeof(log_details), "Deleted employee ID %d ('%s')", id, emp_name);
    audit_log_record(user->id, user->email, "DELETE_EMPLOYEE", "employee", id, log_details, ip);

    cJSON *res = cJSON_CreateObject();
    cJSON_AddNumberToObject(res, "id", id);
    cJSON_AddStringToObject(res, "message", "Employee record removed successfully");
    util_json_response(c, 200, res);
}
