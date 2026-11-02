#include "leave.h"
#include "db.h"
#include "util.h"
#include "audit.h"

void handle_leave_list(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user) {
    sqlite3 *db = db_get_handle();
    if (!db) {
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    char status[32] = {0};
    char emp_str[32] = {0};
    char page_str[32] = {0};
    char limit_str[32] = {0};

    util_get_query_param(hm, "status", status, sizeof(status));
    util_get_query_param(hm, "employee_id", emp_str, sizeof(emp_str));
    util_get_query_param(hm, "page", page_str, sizeof(page_str));
    util_get_query_param(hm, "limit", limit_str, sizeof(limit_str));

    int page = atoi(page_str);
    int limit = atoi(limit_str);
    if (page < 1) page = 1;
    if (limit < 1 || limit > 100) limit = 15;
    int offset = (page - 1) * limit;

    int filter_emp = atoi(emp_str);

    /* If regular employee, restrict to own requests */
    if (strcmp(user->role, "employee") == 0 && user->employee_id > 0) {
        filter_emp = user->employee_id;
    }

    bool has_status = (status[0] != '\0');
    bool has_emp = (filter_emp > 0);

    /* Total count */
    char count_sql[512];
    snprintf(count_sql, sizeof(count_sql),
             "SELECT COUNT(*) FROM leave_requests lr\n"
             "WHERE 1=1 %s %s;",
             has_status ? "AND lr.status = ?" : "",
             has_emp ? "AND lr.employee_id = ?" : "");

    sqlite3_stmt *stmt = NULL;
    int total = 0;
    if (sqlite3_prepare_v2(db, count_sql, -1, &stmt, NULL) == SQLITE_OK) {
        int idx = 1;
        if (has_status) sqlite3_bind_text(stmt, idx++, status, -1, SQLITE_STATIC);
        if (has_emp) sqlite3_bind_int(stmt, idx++, filter_emp);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            total = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    /* Query list */
    char data_sql[1024];
    snprintf(data_sql, sizeof(data_sql),
             "SELECT lr.id, lr.employee_id, lr.leave_type, lr.start_date, lr.end_date, lr.days, "
             "       lr.reason, lr.status, lr.reviewed_by, lr.review_comment, lr.created_at, "
             "       e.name as employee_name, e.role_title, e.avatar_color, "
             "       COALESCE(d.name, 'Unassigned') as dept_name, "
             "       COALESCE(u.email, 'Admin') as reviewer_email\n"
             "FROM leave_requests lr\n"
             "JOIN employees e ON lr.employee_id = e.id\n"
             "LEFT JOIN departments d ON e.department_id = d.id\n"
             "LEFT JOIN users u ON lr.reviewed_by = u.id\n"
             "WHERE 1=1 %s %s\n"
             "ORDER BY lr.id DESC LIMIT ? OFFSET ?;",
             has_status ? "AND lr.status = ?" : "",
             has_emp ? "AND lr.employee_id = ?" : "");

    cJSON *items = cJSON_CreateArray();
    if (sqlite3_prepare_v2(db, data_sql, -1, &stmt, NULL) == SQLITE_OK) {
        int idx = 1;
        if (has_status) sqlite3_bind_text(stmt, idx++, status, -1, SQLITE_STATIC);
        if (has_emp) sqlite3_bind_int(stmt, idx++, filter_emp);
        sqlite3_bind_int(stmt, idx++, limit);
        sqlite3_bind_int(stmt, idx++, offset);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
            cJSON_AddNumberToObject(item, "employee_id", sqlite3_column_int(stmt, 1));
            cJSON_AddStringToObject(item, "leave_type", (const char *)sqlite3_column_text(stmt, 2));
            cJSON_AddStringToObject(item, "start_date", (const char *)sqlite3_column_text(stmt, 3));
            cJSON_AddStringToObject(item, "end_date", (const char *)sqlite3_column_text(stmt, 4));
            cJSON_AddNumberToObject(item, "days", sqlite3_column_int(stmt, 5));
            cJSON_AddStringToObject(item, "reason", (const char *)sqlite3_column_text(stmt, 6));
            cJSON_AddStringToObject(item, "status", (const char *)sqlite3_column_text(stmt, 7));

            if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
                cJSON_AddNumberToObject(item, "reviewed_by", sqlite3_column_int(stmt, 8));
            } else {
                cJSON_AddNullToObject(item, "reviewed_by");
            }
            const char *rev_comment = (const char *)sqlite3_column_text(stmt, 9);
            cJSON_AddStringToObject(item, "review_comment", rev_comment ? rev_comment : "");
            cJSON_AddStringToObject(item, "created_at", (const char *)sqlite3_column_text(stmt, 10));
            cJSON_AddStringToObject(item, "employee_name", (const char *)sqlite3_column_text(stmt, 11));
            cJSON_AddStringToObject(item, "role_title", (const char *)sqlite3_column_text(stmt, 12));
            cJSON_AddStringToObject(item, "avatar_color", (const char *)sqlite3_column_text(stmt, 13));
            cJSON_AddStringToObject(item, "department_name", (const char *)sqlite3_column_text(stmt, 14));
            cJSON_AddStringToObject(item, "reviewer_name", (const char *)sqlite3_column_text(stmt, 15));
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

void handle_leave_create(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user) {
    cJSON *body = util_parse_json(hm);
    if (!body) {
        util_json_error(c, 400, "INVALID_JSON", "Request body must be valid JSON");
        return;
    }

    cJSON *emp_item = cJSON_GetObjectItemCaseSensitive(body, "employee_id");
    cJSON *type_item = cJSON_GetObjectItemCaseSensitive(body, "leave_type");
    cJSON *start_item = cJSON_GetObjectItemCaseSensitive(body, "start_date");
    cJSON *end_item = cJSON_GetObjectItemCaseSensitive(body, "end_date");
    cJSON *days_item = cJSON_GetObjectItemCaseSensitive(body, "days");
    cJSON *reason_item = cJSON_GetObjectItemCaseSensitive(body, "reason");

    int target_emp_id = user->employee_id;
    if (emp_item && cJSON_IsNumber(emp_item) && emp_item->valueint > 0) {
        if (strcmp(user->role, "admin") == 0 || strcmp(user->role, "manager") == 0) {
            target_emp_id = emp_item->valueint;
        }
    }

    if (target_emp_id <= 0) {
        cJSON_Delete(body);
        util_json_error(c, 400, "MISSING_EMPLOYEE", "Valid employee ID is required");
        return;
    }

    if (!cJSON_IsString(type_item) || !cJSON_IsString(start_item) || !cJSON_IsString(end_item) ||
        !cJSON_IsNumber(days_item) || !cJSON_IsString(reason_item)) {
        cJSON_Delete(body);
        util_json_error(c, 400, "VALIDATION_ERROR", "All fields (leave_type, start_date, end_date, days, reason) are required");
        return;
    }

    const char *type = type_item->valuestring;
    if (strcmp(type, "annual") != 0 && strcmp(type, "sick") != 0 &&
        strcmp(type, "casual") != 0 && strcmp(type, "unpaid") != 0) {
        cJSON_Delete(body);
        util_json_error(c, 400, "INVALID_TYPE", "leave_type must be annual, sick, casual, or unpaid");
        return;
    }

    if (!util_is_valid_date(start_item->valuestring) || !util_is_valid_date(end_item->valuestring)) {
        cJSON_Delete(body);
        util_json_error(c, 400, "INVALID_DATE", "Dates must be in YYYY-MM-DD format");
        return;
    }

    int days = days_item->valueint;
    if (days < 1 || days > 90) {
        cJSON_Delete(body);
        util_json_error(c, 400, "INVALID_DAYS", "Days requested must be between 1 and 90");
        return;
    }

    const char *reason = reason_item->valuestring;

    sqlite3 *db = db_get_handle();
    if (!db) {
        cJSON_Delete(body);
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    /* Check balance if not unpaid */
    if (strcmp(type, "unpaid") != 0) {
        const char *bal_col_tot = "annual_total";
        const char *bal_col_usd = "annual_used";
        if (strcmp(type, "sick") == 0) {
            bal_col_tot = "sick_total";
            bal_col_usd = "sick_used";
        } else if (strcmp(type, "casual") == 0) {
            bal_col_tot = "casual_total";
            bal_col_usd = "casual_used";
        }

        char check_sql[256];
        snprintf(check_sql, sizeof(check_sql),
                 "SELECT (%s - %s) FROM leave_balances WHERE employee_id = ? AND year = 2026;",
                 bal_col_tot, bal_col_usd);

        sqlite3_stmt *b_stmt = NULL;
        int remaining = 0;
        bool has_row = false;
        if (sqlite3_prepare_v2(db, check_sql, -1, &b_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(b_stmt, 1, target_emp_id);
            if (sqlite3_step(b_stmt) == SQLITE_ROW) {
                remaining = sqlite3_column_int(b_stmt, 0);
                has_row = true;
            }
            sqlite3_finalize(b_stmt);
        }

        if (has_row && remaining < days) {
            cJSON_Delete(body);
            char err_msg[128];
            snprintf(err_msg, sizeof(err_msg), "Insufficient %s leave balance (%d days remaining, %d requested)", type, remaining, days);
            util_json_error(c, 400, "INSUFFICIENT_BALANCE", err_msg);
            return;
        }
    }

    char now_iso[32];
    util_now_iso8601(now_iso, sizeof(now_iso));

    const char *ins_sql =
        "INSERT INTO leave_requests (employee_id, leave_type, start_date, end_date, days, reason, status, created_at, updated_at)\n"
        "VALUES (?, ?, ?, ?, ?, ?, 'pending', ?, ?);";

    sqlite3_stmt *stmt = NULL;
    int new_id = 0;

    if (sqlite3_prepare_v2(db, ins_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, target_emp_id);
        sqlite3_bind_text(stmt, 2, type, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, start_item->valuestring, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, end_item->valuestring, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 5, days);
        sqlite3_bind_text(stmt, 6, reason, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, now_iso, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 8, now_iso, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            new_id = (int)sqlite3_last_insert_rowid(db);
        }
        sqlite3_finalize(stmt);
    }
    cJSON_Delete(body);

    if (new_id <= 0) {
        util_json_error(c, 500, "CREATE_FAILED", "Failed to submit leave request");
        return;
    }

    char ip[64];
    util_get_client_ip(c, hm, ip, sizeof(ip));
    char log_details[256];
    snprintf(log_details, sizeof(log_details), "Submitted %d-day %s leave request (ID: %d) for emp %d", days, type, new_id, target_emp_id);
    audit_log_record(user->id, user->email, "SUBMIT_LEAVE", "leave", new_id, log_details, ip);

    cJSON *res = cJSON_CreateObject();
    cJSON_AddNumberToObject(res, "id", new_id);
    cJSON_AddStringToObject(res, "status", "pending");
    cJSON_AddStringToObject(res, "message", "Leave request submitted for review");
    util_json_response(c, 201, res);
}

void handle_leave_review(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int request_id) {
    if (strcmp(user->role, "admin") != 0 && strcmp(user->role, "manager") != 0) {
        util_json_error(c, 403, "FORBIDDEN", "Only managers and administrators can review leave requests");
        return;
    }

    cJSON *body = util_parse_json(hm);
    if (!body) {
        util_json_error(c, 400, "INVALID_JSON", "Request body must be valid JSON");
        return;
    }

    cJSON *status_item = cJSON_GetObjectItemCaseSensitive(body, "status");
    cJSON *comment_item = cJSON_GetObjectItemCaseSensitive(body, "review_comment");

    if (!cJSON_IsString(status_item) ||
        (strcmp(status_item->valuestring, "approved") != 0 && strcmp(status_item->valuestring, "rejected") != 0)) {
        cJSON_Delete(body);
        util_json_error(c, 400, "VALIDATION_ERROR", "Status must be 'approved' or 'rejected'");
        return;
    }

    const char *new_status = status_item->valuestring;
    const char *comment = (cJSON_IsString(comment_item) && comment_item->valuestring) ? comment_item->valuestring : "";

    sqlite3 *db = db_get_handle();
    if (!db) {
        cJSON_Delete(body);
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    /* Retrieve current request */
    sqlite3_stmt *stmt = NULL;
    int emp_id = 0;
    char leave_type[32] = {0};
    int days = 0;
    char cur_status[32] = {0};

    if (sqlite3_prepare_v2(db, "SELECT employee_id, leave_type, days, status FROM leave_requests WHERE id = ?;", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, request_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            emp_id = sqlite3_column_int(stmt, 0);
            snprintf(leave_type, sizeof(leave_type), "%s", (const char *)sqlite3_column_text(stmt, 1));
            days = sqlite3_column_int(stmt, 2);
            snprintf(cur_status, sizeof(cur_status), "%s", (const char *)sqlite3_column_text(stmt, 3));
        }
        sqlite3_finalize(stmt);
    }

    if (emp_id <= 0) {
        cJSON_Delete(body);
        util_json_error(c, 404, "NOT_FOUND", "Leave request not found");
        return;
    }

    char now_iso[32];
    util_now_iso8601(now_iso, sizeof(now_iso));

    /* Update leave request */
    const char *upd_sql =
        "UPDATE leave_requests SET status = ?, reviewed_by = ?, review_comment = ?, updated_at = ? WHERE id = ?;";

    bool ok = false;
    if (sqlite3_prepare_v2(db, upd_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, new_status, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, user->id);
        sqlite3_bind_text(stmt, 3, comment, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, now_iso, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 5, request_id);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = true;
        }
        sqlite3_finalize(stmt);
    }

    /* If approved and previously pending, deduct from leave_balances */
    if (ok && strcmp(new_status, "approved") == 0 && strcmp(cur_status, "pending") == 0 && strcmp(leave_type, "unpaid") != 0) {
        const char *bal_col = "annual_used";
        if (strcmp(leave_type, "sick") == 0) bal_col = "sick_used";
        else if (strcmp(leave_type, "casual") == 0) bal_col = "casual_used";

        char ded_sql[256];
        snprintf(ded_sql, sizeof(ded_sql),
                 "UPDATE leave_balances SET %s = %s + ? WHERE employee_id = ? AND year = 2026;",
                 bal_col, bal_col);

        sqlite3_stmt *b_stmt = NULL;
        if (sqlite3_prepare_v2(db, ded_sql, -1, &b_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(b_stmt, 1, days);
            sqlite3_bind_int(b_stmt, 2, emp_id);
            sqlite3_step(b_stmt);
            sqlite3_finalize(b_stmt);
        }
    }

    cJSON_Delete(body);

    if (!ok) {
        util_json_error(c, 500, "REVIEW_FAILED", "Failed to update review status");
        return;
    }

    char ip[64];
    util_get_client_ip(c, hm, ip, sizeof(ip));
    char log_details[256];
    snprintf(log_details, sizeof(log_details), "%s leave request ID %d for emp %d (%d days)",
             new_status, request_id, emp_id, days);
    audit_log_record(user->id, user->email, "REVIEW_LEAVE", "leave", request_id, log_details, ip);

    cJSON *res = cJSON_CreateObject();
    cJSON_AddNumberToObject(res, "id", request_id);
    cJSON_AddStringToObject(res, "status", new_status);
    cJSON_AddStringToObject(res, "message", "Leave request review completed");
    util_json_response(c, 200, res);
}

void handle_leave_balance(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int employee_id) {
    (void)hm;
    (void)user;
    sqlite3 *db = db_get_handle();
    if (!db) {
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    const char *sql =
        "SELECT annual_total, annual_used, sick_total, sick_used, casual_total, casual_used\n"
        "FROM leave_balances WHERE employee_id = ? AND year = 2026;";

    sqlite3_stmt *stmt = NULL;
    cJSON *bal = cJSON_CreateObject();
    cJSON_AddNumberToObject(bal, "employee_id", employee_id);
    cJSON_AddNumberToObject(bal, "year", 2026);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, employee_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int a_tot = sqlite3_column_int(stmt, 0);
            int a_use = sqlite3_column_int(stmt, 1);
            int s_tot = sqlite3_column_int(stmt, 2);
            int s_use = sqlite3_column_int(stmt, 3);
            int c_tot = sqlite3_column_int(stmt, 4);
            int c_use = sqlite3_column_int(stmt, 5);

            cJSON_AddNumberToObject(bal, "annual_total", a_tot);
            cJSON_AddNumberToObject(bal, "annual_used", a_use);
            cJSON_AddNumberToObject(bal, "annual_remaining", a_tot - a_use);

            cJSON_AddNumberToObject(bal, "sick_total", s_tot);
            cJSON_AddNumberToObject(bal, "sick_used", s_use);
            cJSON_AddNumberToObject(bal, "sick_remaining", s_tot - s_use);

            cJSON_AddNumberToObject(bal, "casual_total", c_tot);
            cJSON_AddNumberToObject(bal, "casual_used", c_use);
            cJSON_AddNumberToObject(bal, "casual_remaining", c_tot - c_use);
        } else {
            /* Default initialization if no row */
            cJSON_AddNumberToObject(bal, "annual_total", 18);
            cJSON_AddNumberToObject(bal, "annual_used", 0);
            cJSON_AddNumberToObject(bal, "annual_remaining", 18);
            cJSON_AddNumberToObject(bal, "sick_total", 12);
            cJSON_AddNumberToObject(bal, "sick_used", 0);
            cJSON_AddNumberToObject(bal, "sick_remaining", 12);
            cJSON_AddNumberToObject(bal, "casual_total", 10);
            cJSON_AddNumberToObject(bal, "casual_used", 0);
            cJSON_AddNumberToObject(bal, "casual_remaining", 10);
        }
        sqlite3_finalize(stmt);
    }

    util_json_response(c, 200, bal);
}
