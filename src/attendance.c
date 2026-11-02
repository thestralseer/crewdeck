#include "attendance.h"
#include "db.h"
#include "util.h"
#include "audit.h"

void handle_attendance_list(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user) {
    (void)user;
    sqlite3 *db = db_get_handle();
    if (!db) {
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    char date_str[32] = {0};
    char dept_str[32] = {0};
    char page_str[32] = {0};
    char limit_str[32] = {0};

    util_get_query_param(hm, "date", date_str, sizeof(date_str));
    util_get_query_param(hm, "dept", dept_str, sizeof(dept_str));
    util_get_query_param(hm, "page", page_str, sizeof(page_str));
    util_get_query_param(hm, "limit", limit_str, sizeof(limit_str));

    if (date_str[0] == '\0') {
        util_today_date(date_str, sizeof(date_str));
    }

    int page = atoi(page_str);
    int limit = atoi(limit_str);
    if (page < 1) page = 1;
    if (limit < 1 || limit > 100) limit = 20;
    int offset = (page - 1) * limit;

    int dept_id = atoi(dept_str);
    bool has_dept = (dept_id > 0);

    /* Total active employees */
    int total = 0;
    const char *count_sql = has_dept
        ? "SELECT COUNT(*) FROM employees WHERE status != 'exited' AND department_id = ?;"
        : "SELECT COUNT(*) FROM employees WHERE status != 'exited';";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, count_sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (has_dept) sqlite3_bind_int(stmt, 1, dept_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            total = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    /* Left join employees with attendance for the requested date */
    char data_sql[1024];
    snprintf(data_sql, sizeof(data_sql),
             "SELECT e.id, e.name, e.role_title, e.avatar_color, COALESCE(d.name, 'Unassigned') as dept_name, "
             "       COALESCE(a.status, 'absent') as att_status, "
             "       a.check_in, a.check_out, COALESCE(a.notes, '') as notes\n"
             "FROM employees e\n"
             "LEFT JOIN departments d ON e.department_id = d.id\n"
             "LEFT JOIN attendance a ON a.employee_id = e.id AND a.date = ?\n"
             "WHERE e.status != 'exited' %s\n"
             "ORDER BY e.name ASC LIMIT ? OFFSET ?;",
             has_dept ? "AND e.department_id = ?" : "");

    cJSON *items = cJSON_CreateArray();
    if (sqlite3_prepare_v2(db, data_sql, -1, &stmt, NULL) == SQLITE_OK) {
        int bind_idx = 1;
        sqlite3_bind_text(stmt, bind_idx++, date_str, -1, SQLITE_STATIC);
        if (has_dept) sqlite3_bind_int(stmt, bind_idx++, dept_id);
        sqlite3_bind_int(stmt, bind_idx++, limit);
        sqlite3_bind_int(stmt, bind_idx++, offset);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "employee_id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(item, "employee_name", (const char *)sqlite3_column_text(stmt, 1));
            cJSON_AddStringToObject(item, "role_title", (const char *)sqlite3_column_text(stmt, 2));
            cJSON_AddStringToObject(item, "avatar_color", (const char *)sqlite3_column_text(stmt, 3));
            cJSON_AddStringToObject(item, "department_name", (const char *)sqlite3_column_text(stmt, 4));
            cJSON_AddStringToObject(item, "status", (const char *)sqlite3_column_text(stmt, 5));
            const char *cin = (const char *)sqlite3_column_text(stmt, 6);
            const char *cout = (const char *)sqlite3_column_text(stmt, 7);
            cJSON_AddStringToObject(item, "check_in", cin ? cin : "--");
            cJSON_AddStringToObject(item, "check_out", cout ? cout : "--");
            cJSON_AddStringToObject(item, "notes", (const char *)sqlite3_column_text(stmt, 8));
            cJSON_AddItemToArray(items, item);
        }
        sqlite3_finalize(stmt);
    }

    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "date", date_str);
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

void handle_attendance_checkin(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user) {
    sqlite3 *db = db_get_handle();
    if (!db) {
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    cJSON *body = util_parse_json(hm);
    int target_emp_id = user->employee_id;

    if (body) {
        cJSON *emp_item = cJSON_GetObjectItemCaseSensitive(body, "employee_id");
        if (emp_item && cJSON_IsNumber(emp_item) && emp_item->valueint > 0) {
            if (strcmp(user->role, "admin") == 0 || strcmp(user->role, "manager") == 0) {
                target_emp_id = emp_item->valueint;
            }
        }
        cJSON_Delete(body);
    }

    if (target_emp_id <= 0) {
        util_json_error(c, 400, "MISSING_EMPLOYEE", "No valid employee associated with request");
        return;
    }

    char today[16];
    char now_time[16];
    util_today_date(today, sizeof(today));
    util_now_time(now_time, sizeof(now_time));

    /* If checking in after 09:30:00, marked as late */
    const char *status = "present";
    if (strcmp(now_time, "09:30:00") > 0) {
        status = "late";
    }

    const char *sql =
        "INSERT INTO attendance (employee_id, date, check_in, status, notes)\n"
        "VALUES (?, ?, ?, ?, 'Web Portal Check-in')\n"
        "ON CONFLICT(employee_id, date) DO UPDATE SET check_in = excluded.check_in, status = excluded.status;";

    sqlite3_stmt *stmt = NULL;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, target_emp_id);
        sqlite3_bind_text(stmt, 2, today, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, now_time, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, status, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = true;
        }
        sqlite3_finalize(stmt);
    }

    if (!ok) {
        util_json_error(c, 500, "CHECKIN_FAILED", "Failed to record check-in");
        return;
    }

    char ip[64];
    util_get_client_ip(c, hm, ip, sizeof(ip));
    char log_details[256];
    snprintf(log_details, sizeof(log_details), "Checked in employee ID %d at %s (%s)", target_emp_id, now_time, status);
    audit_log_record(user->id, user->email, "CHECK_IN", "attendance", target_emp_id, log_details, ip);

    cJSON *res = cJSON_CreateObject();
    cJSON_AddNumberToObject(res, "employee_id", target_emp_id);
    cJSON_AddStringToObject(res, "date", today);
    cJSON_AddStringToObject(res, "check_in", now_time);
    cJSON_AddStringToObject(res, "status", status);
    cJSON_AddStringToObject(res, "message", "Check-in recorded successfully");
    util_json_response(c, 200, res);
}

void handle_attendance_checkout(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user) {
    sqlite3 *db = db_get_handle();
    if (!db) {
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    cJSON *body = util_parse_json(hm);
    int target_emp_id = user->employee_id;

    if (body) {
        cJSON *emp_item = cJSON_GetObjectItemCaseSensitive(body, "employee_id");
        if (emp_item && cJSON_IsNumber(emp_item) && emp_item->valueint > 0) {
            if (strcmp(user->role, "admin") == 0 || strcmp(user->role, "manager") == 0) {
                target_emp_id = emp_item->valueint;
            }
        }
        cJSON_Delete(body);
    }

    if (target_emp_id <= 0) {
        util_json_error(c, 400, "MISSING_EMPLOYEE", "No valid employee associated with request");
        return;
    }

    char today[16];
    char now_time[16];
    util_today_date(today, sizeof(today));
    util_now_time(now_time, sizeof(now_time));

    const char *sql =
        "INSERT INTO attendance (employee_id, date, check_out, status, notes)\n"
        "VALUES (?, ?, ?, 'present', 'Web Portal Check-out')\n"
        "ON CONFLICT(employee_id, date) DO UPDATE SET check_out = excluded.check_out;";

    sqlite3_stmt *stmt = NULL;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, target_emp_id);
        sqlite3_bind_text(stmt, 2, today, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, now_time, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = true;
        }
        sqlite3_finalize(stmt);
    }

    if (!ok) {
        util_json_error(c, 500, "CHECKOUT_FAILED", "Failed to record check-out");
        return;
    }

    char ip[64];
    util_get_client_ip(c, hm, ip, sizeof(ip));
    char log_details[256];
    snprintf(log_details, sizeof(log_details), "Checked out employee ID %d at %s", target_emp_id, now_time);
    audit_log_record(user->id, user->email, "CHECK_OUT", "attendance", target_emp_id, log_details, ip);

    cJSON *res = cJSON_CreateObject();
    cJSON_AddNumberToObject(res, "employee_id", target_emp_id);
    cJSON_AddStringToObject(res, "date", today);
    cJSON_AddStringToObject(res, "check_out", now_time);
    cJSON_AddStringToObject(res, "message", "Check-out recorded successfully");
    util_json_response(c, 200, res);
}

void handle_attendance_heatmap(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int employee_id) {
    (void)user;
    sqlite3 *db = db_get_handle();
    if (!db) {
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    char year_str[16] = {0};
    util_get_query_param(hm, "year", year_str, sizeof(year_str));
    int year = atoi(year_str);
    if (year < 2020 || year > 2030) year = 2026;

    char start_date[32];
    char end_date[32];
    snprintf(start_date, sizeof(start_date), "%04d-01-01", year);
    snprintf(end_date, sizeof(end_date), "%04d-12-31", year);

    const char *sql =
        "SELECT date, check_in, check_out, status FROM attendance\n"
        "WHERE employee_id = ? AND date BETWEEN ? AND ?\n"
        "ORDER BY date ASC;";

    sqlite3_stmt *stmt = NULL;
    cJSON *records = cJSON_CreateArray();

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, employee_id);
        sqlite3_bind_text(stmt, 2, start_date, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, end_date, -1, SQLITE_STATIC);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cJSON *item = cJSON_CreateObject();
            const char *dt = (const char *)sqlite3_column_text(stmt, 0);
            const char *cin = (const char *)sqlite3_column_text(stmt, 1);
            const char *cout = (const char *)sqlite3_column_text(stmt, 2);
            const char *st = (const char *)sqlite3_column_text(stmt, 3);

            int level = 0;
            if (st) {
                if (strcmp(st, "present") == 0) level = 4;
                else if (strcmp(st, "late") == 0) level = 3;
                else if (strcmp(st, "half-day") == 0) level = 2;
                else if (strcmp(st, "on-leave") == 0) level = 1;
                else level = 0;
            }

            cJSON_AddStringToObject(item, "date", dt ? dt : "");
            cJSON_AddStringToObject(item, "check_in", cin ? cin : "");
            cJSON_AddStringToObject(item, "check_out", cout ? cout : "");
            cJSON_AddStringToObject(item, "status", st ? st : "absent");
            cJSON_AddNumberToObject(item, "level", level);
            cJSON_AddItemToArray(records, item);
        }
        sqlite3_finalize(stmt);
    }

    cJSON *res = cJSON_CreateObject();
    cJSON_AddNumberToObject(res, "employee_id", employee_id);
    cJSON_AddNumberToObject(res, "year", year);
    cJSON_AddItemToObject(res, "records", records);
    util_json_response(c, 200, res);
}
