#include "payroll.h"
#include "db.h"
#include "util.h"
#include "audit.h"

void handle_payroll_summary(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user) {
    (void)hm;
    if (strcmp(user->role, "admin") != 0 && strcmp(user->role, "manager") != 0) {
        util_json_error(c, 403, "FORBIDDEN", "Admin or manager access required for payroll information");
        return;
    }

    sqlite3 *db = db_get_handle();
    if (!db) {
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    /* Overall metrics */
    const char *overall_sql =
        "SELECT COUNT(*), COALESCE(SUM(salary), 0), COALESCE(AVG(salary), 0), "
        "       COALESCE(MIN(salary), 0), COALESCE(MAX(salary), 0)\n"
        "FROM employees WHERE status != 'exited';";

    sqlite3_stmt *stmt = NULL;
    int total_headcount = 0;
    double total_annual = 0.0;
    double avg_annual = 0.0;
    double min_annual = 0.0;
    double max_annual = 0.0;

    if (sqlite3_prepare_v2(db, overall_sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            total_headcount = sqlite3_column_int(stmt, 0);
            total_annual = sqlite3_column_double(stmt, 1);
            avg_annual = sqlite3_column_double(stmt, 2);
            min_annual = sqlite3_column_double(stmt, 3);
            max_annual = sqlite3_column_double(stmt, 4);
        }
        sqlite3_finalize(stmt);
    }

    /* Department breakdown */
    const char *dept_sql =
        "SELECT d.id, d.name, "
        "       COUNT(e.id) as headcount, "
        "       COALESCE(SUM(e.salary), 0) as total_salary, "
        "       COALESCE(AVG(e.salary), 0) as avg_salary\n"
        "FROM departments d\n"
        "LEFT JOIN employees e ON d.id = e.department_id AND e.status != 'exited'\n"
        "GROUP BY d.id, d.name\n"
        "ORDER BY total_salary DESC;";

    cJSON *depts_arr = cJSON_CreateArray();
    if (sqlite3_prepare_v2(db, dept_sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cJSON *d = cJSON_CreateObject();
            cJSON_AddNumberToObject(d, "department_id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(d, "department_name", (const char *)sqlite3_column_text(stmt, 1));
            int hc = sqlite3_column_int(stmt, 2);
            double tot_sal = sqlite3_column_double(stmt, 3);
            double avg_sal = sqlite3_column_double(stmt, 4);

            cJSON_AddNumberToObject(d, "headcount", hc);
            cJSON_AddNumberToObject(d, "annual_payroll", tot_sal);
            cJSON_AddNumberToObject(d, "monthly_payroll", tot_sal / 12.0);
            cJSON_AddNumberToObject(d, "average_salary", avg_sal);
            cJSON_AddItemToArray(depts_arr, d);
        }
        sqlite3_finalize(stmt);
    }

    cJSON *res = cJSON_CreateObject();
    cJSON_AddNumberToObject(res, "total_headcount", total_headcount);
    cJSON_AddNumberToObject(res, "total_annual_payroll", total_annual);
    cJSON_AddNumberToObject(res, "total_monthly_payroll", total_annual / 12.0);
    cJSON_AddNumberToObject(res, "average_annual_salary", avg_annual);
    cJSON_AddNumberToObject(res, "average_monthly_salary", avg_annual / 12.0);
    cJSON_AddNumberToObject(res, "min_salary", min_annual);
    cJSON_AddNumberToObject(res, "max_salary", max_annual);
    cJSON_AddItemToObject(res, "departments", depts_arr);

    util_json_response(c, 200, res);
}

void handle_payroll_export(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user) {
    if (strcmp(user->role, "admin") != 0 && strcmp(user->role, "manager") != 0) {
        util_json_error(c, 403, "FORBIDDEN", "Admin or manager access required to export payroll");
        return;
    }

    sqlite3 *db = db_get_handle();
    if (!db) {
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    char ip[64];
    util_get_client_ip(c, hm, ip, sizeof(ip));
    audit_log_record(user->id, user->email, "EXPORT_PAYROLL", "payroll", 0, "Exported full payroll CSV dataset", ip);

    const char *sql =
        "SELECT e.id, e.name, e.email, COALESCE(d.name, 'Unassigned'), e.role_title, "
        "       e.join_date, e.status, e.salary\n"
        "FROM employees e\n"
        "LEFT JOIN departments d ON e.department_id = d.id\n"
        "ORDER BY e.id ASC;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        util_json_error(c, 500, "DB_ERROR", "Failed to query payroll data");
        return;
    }

    /* Allocate dynamic buffer for CSV */
    size_t cap = 65536;
    size_t len = 0;
    char *csv = (char *)malloc(cap);
    if (!csv) {
        sqlite3_finalize(stmt);
        util_json_error(c, 500, "MEMORY_ERROR", "Out of memory");
        return;
    }

    len += snprintf(csv + len, cap - len,
                    "Employee ID,Name,Email,Department,Role,Join Date,Status,Annual Salary (INR),Monthly Salary (INR)\r\n");

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        const char *email = (const char *)sqlite3_column_text(stmt, 2);
        const char *dept = (const char *)sqlite3_column_text(stmt, 3);
        const char *role = (const char *)sqlite3_column_text(stmt, 4);
        const char *join_date = (const char *)sqlite3_column_text(stmt, 5);
        const char *status = (const char *)sqlite3_column_text(stmt, 6);
        double salary = sqlite3_column_double(stmt, 7);
        double monthly = salary / 12.0;

        char row[512];
        int rlen = snprintf(row, sizeof(row),
                            "%d,\"%s\",\"%s\",\"%s\",\"%s\",%s,%s,%.2f,%.2f\r\n",
                            id, name ? name : "", email ? email : "",
                            dept ? dept : "", role ? role : "",
                            join_date ? join_date : "", status ? status : "",
                            salary, monthly);

        if (len + rlen + 64 >= cap) {
            cap *= 2;
            char *new_buf = (char *)realloc(csv, cap);
            if (!new_buf) break;
            csv = new_buf;
        }

        memcpy(csv + len, row, rlen);
        len += rlen;
        csv[len] = '\0';
    }
    sqlite3_finalize(stmt);

    char headers[512];
    snprintf(headers, sizeof(headers),
             "Content-Type: text/csv; charset=utf-8\r\n"
             "Content-Disposition: attachment; filename=\"crewdeck_payroll_summary.csv\"\r\n"
             "Content-Length: %lu\r\n"
             "Cache-Control: no-store\r\n",
             (unsigned long)len);

    mg_http_reply(c, 200, headers, "%s", csv);
    free(csv);
}
