#include "router.h"
#include "config.h"
#include "util.h"
#include "auth.h"
#include "employees.h"
#include "departments.h"
#include "attendance.h"
#include "leave.h"
#include "payroll.h"
#include "dashboard.h"
#include "audit.h"
#include "db.h"

void router_init(void) {
    /* No-op placeholder for future route precomputation */
}

static void handle_healthz(struct mg_connection *c) {
    time_t uptime = time(NULL) - g_config.start_time;
    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "status", "ok");
    cJSON_AddStringToObject(res, "service", "CREW/DECK");
    cJSON_AddStringToObject(res, "version", APP_VERSION);
    cJSON_AddNumberToObject(res, "uptime_seconds", uptime);
    cJSON_AddBoolToObject(res, "database_connected", (db_get_handle() != NULL));
    util_json_response(c, 200, res);
}

static void handle_options(struct mg_connection *c) {
    const char *headers =
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n"
        "Access-Control-Allow-Credentials: true\r\n"
        "Access-Control-Max-Age: 86400\r\n"
        "Content-Length: 0\r\n";
    mg_http_reply(c, 204, headers, "");
}

void router_dispatch(struct mg_connection *c, struct mg_http_message *hm) {
    /* Extract null-terminated method and URI */
    char method[16] = {0};
    char uri[256] = {0};

    size_t m_len = (hm->method.len < sizeof(method) - 1) ? hm->method.len : sizeof(method) - 1;
    memcpy(method, hm->method.buf, m_len);
    method[m_len] = '\0';

    size_t u_len = (hm->uri.len < sizeof(uri) - 1) ? hm->uri.len : sizeof(uri) - 1;
    memcpy(uri, hm->uri.buf, u_len);
    method[m_len] = '\0';
    uri[u_len] = '\0';

    /* Request logging */
    printf("[%s] %s\n", method, uri);

    /* 1. CORS Preflight */
    if (strcmp(method, "OPTIONS") == 0) {
        handle_options(c);
        return;
    }

    /* 2. Health check */
    if (strcmp(uri, "/healthz") == 0) {
        handle_healthz(c);
        return;
    }

    /* 3. Auth Routes (Public) */
    if (strcmp(uri, "/api/v1/auth/login") == 0) {
        if (strcmp(method, "POST") == 0) {
            handle_auth_login(c, hm);
        } else {
            util_json_error(c, 405, "METHOD_NOT_ALLOWED", "POST required for login");
        }
        return;
    }

    if (strcmp(uri, "/api/v1/auth/logout") == 0) {
        if (strcmp(method, "POST") == 0) {
            handle_auth_logout(c, hm);
        } else {
            util_json_error(c, 405, "METHOD_NOT_ALLOWED", "POST required for logout");
        }
        return;
    }

    /* 4. Protected API Routes */
    if (strncmp(uri, "/api/", 5) == 0) {
        auth_user_t user;
        if (!auth_validate_session(hm, &user)) {
            util_json_error(c, 401, "UNAUTHORIZED", "Active session required. Please log in.");
            return;
        }

        /* Current user me */
        if (strcmp(uri, "/api/v1/auth/me") == 0 && strcmp(method, "GET") == 0) {
            handle_auth_me(c, hm);
            return;
        }

        /* Dashboard */
        if (strcmp(uri, "/api/v1/dashboard/metrics") == 0 && strcmp(method, "GET") == 0) {
            handle_dashboard_metrics(c, hm, &user);
            return;
        }

        /* Org Constellation */
        if (strcmp(uri, "/api/v1/org/constellation") == 0 && strcmp(method, "GET") == 0) {
            handle_org_constellation(c, hm, &user);
            return;
        }

        /* Employees Collection */
        if (strcmp(uri, "/api/v1/employees") == 0) {
            if (strcmp(method, "GET") == 0) {
                handle_employees_list(c, hm, &user);
            } else if (strcmp(method, "POST") == 0) {
                handle_employee_create(c, hm, &user);
            } else {
                util_json_error(c, 405, "METHOD_NOT_ALLOWED", "Method not allowed on /api/v1/employees");
            }
            return;
        }

        /* Employee Single Entity */
        int emp_id = 0;
        if (util_get_path_id(uri, "/api/v1/employees/", &emp_id)) {
            if (strcmp(method, "GET") == 0) {
                handle_employee_get(c, hm, &user, emp_id);
            } else if (strcmp(method, "PUT") == 0) {
                handle_employee_update(c, hm, &user, emp_id);
            } else if (strcmp(method, "DELETE") == 0) {
                handle_employee_delete(c, hm, &user, emp_id);
            } else {
                util_json_error(c, 405, "METHOD_NOT_ALLOWED", "Method not allowed on employee entity");
            }
            return;
        }

        /* Departments Collection */
        if (strcmp(uri, "/api/v1/departments") == 0) {
            if (strcmp(method, "GET") == 0) {
                handle_departments_list(c, hm, &user);
            } else if (strcmp(method, "POST") == 0) {
                handle_department_create(c, hm, &user);
            } else {
                util_json_error(c, 405, "METHOD_NOT_ALLOWED", "Method not allowed on /api/v1/departments");
            }
            return;
        }

        /* Department Single Entity */
        int dept_id = 0;
        if (util_get_path_id(uri, "/api/v1/departments/", &dept_id)) {
            if (strcmp(method, "GET") == 0) {
                handle_department_get(c, hm, &user, dept_id);
            } else if (strcmp(method, "PUT") == 0) {
                handle_department_update(c, hm, &user, dept_id);
            } else if (strcmp(method, "DELETE") == 0) {
                handle_department_delete(c, hm, &user, dept_id);
            } else {
                util_json_error(c, 405, "METHOD_NOT_ALLOWED", "Method not allowed on department entity");
            }
            return;
        }

        /* Attendance Collection & Actions */
        if (strcmp(uri, "/api/v1/attendance") == 0 && strcmp(method, "GET") == 0) {
            handle_attendance_list(c, hm, &user);
            return;
        }

        if (strcmp(uri, "/api/v1/attendance/check-in") == 0 && strcmp(method, "POST") == 0) {
            handle_attendance_checkin(c, hm, &user);
            return;
        }

        if (strcmp(uri, "/api/v1/attendance/check-out") == 0 && strcmp(method, "POST") == 0) {
            handle_attendance_checkout(c, hm, &user);
            return;
        }

        int heat_emp_id = 0;
        if (util_get_path_id(uri, "/api/v1/attendance/heatmap/", &heat_emp_id) && strcmp(method, "GET") == 0) {
            handle_attendance_heatmap(c, hm, &user, heat_emp_id);
            return;
        }

        /* Leave Requests */
        if (strcmp(uri, "/api/v1/leave/requests") == 0) {
            if (strcmp(method, "GET") == 0) {
                handle_leave_list(c, hm, &user);
            } else if (strcmp(method, "POST") == 0) {
                handle_leave_create(c, hm, &user);
            } else {
                util_json_error(c, 405, "METHOD_NOT_ALLOWED", "Method not allowed on /api/v1/leave/requests");
            }
            return;
        }

        /* Leave Request Review: /api/v1/leave/requests/:id/review */
        if (strncmp(uri, "/api/v1/leave/requests/", 23) == 0) {
            const char *sub = strstr(uri + 23, "/review");
            if (sub && strcmp(method, "PUT") == 0) {
                int req_id = atoi(uri + 23);
                if (req_id > 0) {
                    handle_leave_review(c, hm, &user, req_id);
                    return;
                }
            }
        }

        /* Leave Balance for Employee */
        int bal_emp_id = 0;
        if (util_get_path_id(uri, "/api/v1/leave/balances/", &bal_emp_id) && strcmp(method, "GET") == 0) {
            handle_leave_balance(c, hm, &user, bal_emp_id);
            return;
        }

        /* Payroll */
        if (strcmp(uri, "/api/v1/payroll/summary") == 0 && strcmp(method, "GET") == 0) {
            handle_payroll_summary(c, hm, &user);
            return;
        }

        if (strcmp(uri, "/api/v1/payroll/export") == 0 && strcmp(method, "GET") == 0) {
            handle_payroll_export(c, hm, &user);
            return;
        }

        /* Audit Logs */
        if (strcmp(uri, "/api/v1/audit") == 0 && strcmp(method, "GET") == 0) {
            handle_audit_list(c, hm, &user);
            return;
        }

        /* Unknown API endpoint */
        util_json_error(c, 404, "ROUTE_NOT_FOUND", "Endpoint not found");
        return;
    }

    /* 5. Static File Serving from public/ directory */
    struct mg_http_serve_opts opts = {
        .root_dir = g_config.public_dir,
        .extra_headers =
            "X-Content-Type-Options: nosniff\r\n"
            "X-Frame-Options: DENY\r\n"
            "Referrer-Policy: strict-origin-when-cross-origin\r\n"
    };

    /* Check if serving specific page or SPA fallback */
    if (strcmp(uri, "/") == 0 || strcmp(uri, "/index.html") == 0 ||
        strcmp(uri, "/login") == 0 || strcmp(uri, "/login.html") == 0 ||
        strcmp(uri, "/employees") == 0 || strcmp(uri, "/departments") == 0 ||
        strcmp(uri, "/attendance") == 0 || strcmp(uri, "/leave") == 0 ||
        strcmp(uri, "/payroll") == 0 || strcmp(uri, "/audit") == 0 ||
        strcmp(uri, "/org") == 0) {

        if (strcmp(uri, "/login") == 0 || strcmp(uri, "/login.html") == 0) {
            opts.page404 = "login.html";
            mg_http_serve_file(c, hm, "public/login.html", &opts);
            return;
        }
        mg_http_serve_file(c, hm, "public/index.html", &opts);
        return;
    }

    mg_http_serve_dir(c, hm, &opts);
}
