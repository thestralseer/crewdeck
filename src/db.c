#include "db.h"
#include "config.h"
#include "util.h"
#include "auth.h"

static sqlite3 *g_db = NULL;

sqlite3 *db_get_handle(void) {
    return g_db;
}

bool db_exec_simple(const char *sql) {
    if (!g_db || !sql) return false;
    char *err_msg = NULL;
    int rc = sqlite3_exec(g_db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB ERROR] %s: %s\n", sql, err_msg ? err_msg : "Unknown");
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

static const char *SCHEMA_SQL =
    "PRAGMA journal_mode = WAL;\n"
    "PRAGMA busy_timeout = 5000;\n"
    "PRAGMA foreign_keys = ON;\n"
    "\n"
    "CREATE TABLE IF NOT EXISTS users (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    email TEXT UNIQUE NOT NULL,\n"
    "    password_hash TEXT NOT NULL,\n"
    "    role TEXT NOT NULL CHECK(role IN ('admin', 'manager', 'employee')),\n"
    "    employee_id INTEGER,\n"
    "    created_at TEXT NOT NULL\n"
    ");\n"
    "\n"
    "CREATE TABLE IF NOT EXISTS sessions (\n"
    "    token TEXT PRIMARY KEY,\n"
    "    user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,\n"
    "    expires_at INTEGER NOT NULL,\n"
    "    created_at TEXT NOT NULL\n"
    ");\n"
    "\n"
    "CREATE TABLE IF NOT EXISTS login_attempts (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    ip TEXT NOT NULL,\n"
    "    attempt_time INTEGER NOT NULL\n"
    ");\n"
    "\n"
    "CREATE TABLE IF NOT EXISTS departments (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    name TEXT UNIQUE NOT NULL,\n"
    "    description TEXT,\n"
    "    head_id INTEGER,\n"
    "    created_at TEXT NOT NULL\n"
    ");\n"
    "\n"
    "CREATE TABLE IF NOT EXISTS employees (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    name TEXT NOT NULL,\n"
    "    email TEXT UNIQUE NOT NULL,\n"
    "    phone TEXT NOT NULL,\n"
    "    role_title TEXT NOT NULL,\n"
    "    department_id INTEGER REFERENCES departments(id) ON DELETE SET NULL,\n"
    "    manager_id INTEGER REFERENCES employees(id) ON DELETE SET NULL,\n"
    "    salary REAL NOT NULL DEFAULT 0.0,\n"
    "    join_date TEXT NOT NULL,\n"
    "    status TEXT NOT NULL DEFAULT 'active' CHECK(status IN ('active', 'on-leave', 'exited')),\n"
    "    avatar_color TEXT NOT NULL DEFAULT '#FFB020',\n"
    "    created_at TEXT NOT NULL,\n"
    "    updated_at TEXT NOT NULL\n"
    ");\n"
    "\n"
    "CREATE TABLE IF NOT EXISTS attendance (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    employee_id INTEGER NOT NULL REFERENCES employees(id) ON DELETE CASCADE,\n"
    "    date TEXT NOT NULL,\n"
    "    check_in TEXT,\n"
    "    check_out TEXT,\n"
    "    status TEXT NOT NULL DEFAULT 'present' CHECK(status IN ('present', 'late', 'half-day', 'absent', 'on-leave')),\n"
    "    notes TEXT,\n"
    "    UNIQUE(employee_id, date)\n"
    ");\n"
    "\n"
    "CREATE TABLE IF NOT EXISTS leave_requests (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    employee_id INTEGER NOT NULL REFERENCES employees(id) ON DELETE CASCADE,\n"
    "    leave_type TEXT NOT NULL CHECK(leave_type IN ('annual', 'sick', 'casual', 'unpaid')),\n"
    "    start_date TEXT NOT NULL,\n"
    "    end_date TEXT NOT NULL,\n"
    "    days INTEGER NOT NULL,\n"
    "    reason TEXT NOT NULL,\n"
    "    status TEXT NOT NULL DEFAULT 'pending' CHECK(status IN ('pending', 'approved', 'rejected')),\n"
    "    reviewed_by INTEGER REFERENCES users(id) ON DELETE SET NULL,\n"
    "    review_comment TEXT,\n"
    "    created_at TEXT NOT NULL,\n"
    "    updated_at TEXT NOT NULL\n"
    ");\n"
    "\n"
    "CREATE TABLE IF NOT EXISTS leave_balances (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    employee_id INTEGER NOT NULL REFERENCES employees(id) ON DELETE CASCADE,\n"
    "    year INTEGER NOT NULL,\n"
    "    annual_total INTEGER NOT NULL DEFAULT 18,\n"
    "    annual_used INTEGER NOT NULL DEFAULT 0,\n"
    "    sick_total INTEGER NOT NULL DEFAULT 12,\n"
    "    sick_used INTEGER NOT NULL DEFAULT 0,\n"
    "    casual_total INTEGER NOT NULL DEFAULT 10,\n"
    "    casual_used INTEGER NOT NULL DEFAULT 0,\n"
    "    UNIQUE(employee_id, year)\n"
    ");\n"
    "\n"
    "CREATE TABLE IF NOT EXISTS audit_logs (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    user_id INTEGER REFERENCES users(id) ON DELETE SET NULL,\n"
    "    user_email TEXT,\n"
    "    action TEXT NOT NULL,\n"
    "    entity_type TEXT NOT NULL,\n"
    "    entity_id INTEGER,\n"
    "    details TEXT NOT NULL,\n"
    "    ip_address TEXT,\n"
    "    created_at TEXT NOT NULL\n"
    ");\n"
    "\n"
    "CREATE INDEX IF NOT EXISTS idx_emp_dept ON employees(department_id);\n"
    "CREATE INDEX IF NOT EXISTS idx_emp_status ON employees(status);\n"
    "CREATE INDEX IF NOT EXISTS idx_att_date ON attendance(date);\n"
    "CREATE INDEX IF NOT EXISTS idx_att_emp_date ON attendance(employee_id, date);\n"
    "CREATE INDEX IF NOT EXISTS idx_leave_emp ON leave_requests(employee_id);\n"
    "CREATE INDEX IF NOT EXISTS idx_audit_time ON audit_logs(created_at);\n"
    "CREATE INDEX IF NOT EXISTS idx_sessions_tok ON sessions(token);\n";

bool db_init(const char *db_path) {
    if (g_db) return true;
    int rc = sqlite3_open_v2(db_path, &g_db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                             NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB ERROR] Failed to open SQLite database %s: %s\n", db_path, sqlite3_errmsg(g_db));
        return false;
    }

    if (!db_exec_simple(SCHEMA_SQL)) {
        fprintf(stderr, "[DB ERROR] Failed to initialize database schema\n");
        return false;
    }

    db_seed_if_empty();
    return true;
}

void db_close(void) {
    if (g_db) {
        sqlite3_close_v2(g_db);
        g_db = NULL;
    }
}

typedef struct {
    const char *name;
    const char *email;
    const char *phone;
    const char *role_title;
    int dept_idx; /* 1-based */
    int manager_id;
    double salary;
    const char *join_date;
    const char *status;
    const char *color;
} seed_emp_t;

bool db_seed_if_empty(void) {
    if (!g_db) return false;

    /* Check if employees table already has data */
    sqlite3_stmt *stmt = NULL;
    int count = 0;
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM employees;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    if (count > 0) {
        return true; /* Already seeded */
    }

    printf("[SEED] Seeding CREW/DECK initial dataset with 40 Indian context employees...\n");

    /* Begin transaction */
    db_exec_simple("BEGIN TRANSACTION;");

    char now_iso[32];
    util_now_iso8601(now_iso, sizeof(now_iso));

    /* 1. Seed Departments */
    struct {
        const char *name;
        const char *desc;
    } depts[] = {
        {"Engineering", "Core platform architecture, backend systems, web apps and infrastructure"},
        {"Product & Design", "Product management, user experience research, UI system and design operations"},
        {"People Operations", "Talent acquisition, organizational development, HR and culture"},
        {"Sales & Marketing", "Enterprise client acquisition, partner relations, growth and brand strategy"},
        {"Finance & Operations", "Financial planning, accounting, compliance, legal and workplace operations"}
    };

    const char *ins_dept_sql = "INSERT INTO departments (name, description, created_at) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(g_db, ins_dept_sql, -1, &stmt, NULL) == SQLITE_OK) {
        for (int i = 0; i < 5; i++) {
            sqlite3_bind_text(stmt, 1, depts[i].name, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, depts[i].desc, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, now_iso, -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
    }

    /* 2. Seed 40 Employees */
    seed_emp_t employees[] = {
        {"Rajesh Verma", "rajesh.verma@crewdeck.io", "+91 98201 11001", "VP of Engineering", 1, 0, 3600000.0, "2021-03-15", "active", "#FFB020"},
        {"Priya Patel", "priya.patel@crewdeck.io", "+91 98202 22002", "Director of Product", 2, 0, 3400000.0, "2021-06-01", "active", "#2DD4BF"},
        {"Ananya Iyer", "ananya.iyer@crewdeck.io", "+91 98203 33003", "Head of People Operations", 3, 0, 2900000.0, "2021-09-10", "active", "#EC4899"},
        {"Vikram Malhotra", "vikram.malhotra@crewdeck.io", "+91 98204 44004", "Head of Sales & Revenue", 4, 0, 3300000.0, "2022-01-12", "active", "#3B82F6"},
        {"Sunita Menon", "sunita.menon@crewdeck.io", "+91 98205 55005", "Chief Financial Officer", 5, 0, 3800000.0, "2020-11-05", "active", "#F59E0B"},
        {"Aarav Sharma", "aarav.sharma@crewdeck.io", "+91 98206 66006", "Principal Systems Architect", 1, 1, 3100000.0, "2022-02-18", "active", "#10B981"},
        {"Rohan Deshmukh", "rohan.deshmukh@crewdeck.io", "+91 98207 77007", "Staff Software Engineer", 1, 1, 2750000.0, "2022-04-10", "active", "#8B5CF6"},
        {"Sneha Kulkarni", "sneha.kulkarni@crewdeck.io", "+91 98208 88008", "Senior Backend Engineer", 1, 7, 2100000.0, "2022-07-22", "active", "#06B6D4"},
        {"Arjun Reddy", "arjun.reddy@crewdeck.io", "+91 98209 99009", "Senior Frontend Engineer", 1, 7, 2050000.0, "2022-08-15", "active", "#F97316"},
        {"Divya Nair", "divya.nair@crewdeck.io", "+91 98210 10101", "Lead UX Designer", 2, 2, 2300000.0, "2022-05-09", "active", "#FFB020"},
        {"Kabir Sen", "kabir.sen@crewdeck.io", "+91 98211 20202", "Senior Product Manager", 2, 2, 2500000.0, "2022-09-01", "active", "#2DD4BF"},
        {"Neha Gupta", "neha.gupta@crewdeck.io", "+91 98212 30303", "Senior Product Designer", 2, 10, 1950000.0, "2023-01-16", "active", "#EC4899"},
        {"Meera Joshi", "meera.joshi@crewdeck.io", "+91 98213 40404", "HR Business Partner", 3, 3, 1750000.0, "2022-10-10", "active", "#3B82F6"},
        {"Siddharth Nair", "siddharth.nair@crewdeck.io", "+91 98214 50505", "Talent Acquisition Lead", 3, 3, 1850000.0, "2022-11-20", "active", "#10B981"},
        {"Pooja Agarwal", "pooja.agarwal@crewdeck.io", "+91 98215 60606", "Senior Enterprise Account Exec", 4, 4, 2400000.0, "2022-03-01", "active", "#F59E0B"},
        {"Karthik Raman", "karthik.raman@crewdeck.io", "+91 98216 70707", "Senior DevOps Engineer", 1, 6, 2200000.0, "2023-02-01", "active", "#8B5CF6"},
        {"Tanvi Shah", "tanvi.shah@crewdeck.io", "+91 98217 80808", "QA Automation Lead", 1, 1, 1900000.0, "2022-12-05", "active", "#06B6D4"},
        {"Aditya Joshi", "aditya.joshi@crewdeck.io", "+91 98218 90909", "Fullstack Engineer", 1, 8, 1600000.0, "2023-03-15", "active", "#F97316"},
        {"Kavita Rao", "kavita.rao@crewdeck.io", "+91 98219 01010", "Product Operations Specialist", 2, 11, 1500000.0, "2023-04-10", "active", "#FFB020"},
        {"Manish Pandey", "manish.pandey@crewdeck.io", "+91 98220 12121", "Finance Manager", 5, 5, 2300000.0, "2022-06-18", "active", "#2DD4BF"},
        {"Shreya Bhat", "shreya.bhat@crewdeck.io", "+91 98221 23232", "Legal Counsel", 5, 5, 2400000.0, "2023-01-05", "active", "#EC4899"},
        {"Rahul Mehta", "rahul.mehta@crewdeck.io", "+91 98222 34343", "Senior Growth Marketer", 4, 4, 2000000.0, "2023-02-15", "active", "#3B82F6"},
        {"Swati Saxena", "swati.saxena@crewdeck.io", "+91 98223 45454", "Customer Success Lead", 4, 4, 1800000.0, "2023-05-20", "active", "#10B981"},
        {"Amit Trivedi", "amit.trivedi@crewdeck.io", "+91 98224 56565", "Security Engineer", 1, 6, 2150000.0, "2023-06-12", "active", "#F59E0B"},
        {"Deepa Nambiar", "deepa.nambiar@crewdeck.io", "+91 98225 67676", "Backend Engineer", 1, 8, 1550000.0, "2023-07-03", "active", "#8B5CF6"},
        {"Harsh Vardhan", "harsh.vardhan@crewdeck.io", "+91 98226 78787", "Data Engineer", 1, 7, 1800000.0, "2023-08-14", "active", "#06B6D4"},
        {"Ritu Singhania", "ritu.singhania@crewdeck.io", "+91 98227 89898", "Content & Brand Strategist", 4, 22, 1450000.0, "2023-09-01", "active", "#F97316"},
        {"Gaurav Chopra", "gaurav.chopra@crewdeck.io", "+91 98228 90909", "Account Executive", 4, 15, 1600000.0, "2023-09-18", "active", "#FFB020"},
        {"Nisha Pillai", "nisha.pillai@crewdeck.io", "+91 98229 01234", "Employee Experience Specialist", 3, 13, 1300000.0, "2023-10-10", "on-leave", "#2DD4BF"},
        {"Prateek Jain", "prateek.jain@crewdeck.io", "+91 98230 12345", "Senior QA Engineer", 1, 17, 1650000.0, "2023-10-25", "active", "#EC4899"},
        {"Alok Banerjee", "alok.banerjee@crewdeck.io", "+91 98231 23456", "Senior Financial Analyst", 5, 20, 1700000.0, "2023-11-15", "active", "#3B82F6"},
        {"Tarun Bajaj", "tarun.bajaj@crewdeck.io", "+91 98232 34567", "Frontend Engineer", 1, 9, 1400000.0, "2023-12-01", "active", "#10B981"},
        {"Rekha Sundaram", "rekha.sundaram@crewdeck.io", "+91 98233 45678", "Visual & Motion Designer", 2, 12, 1450000.0, "2024-01-08", "active", "#F59E0B"},
        {"Varun Kapoor", "varun.kapoor@crewdeck.io", "+91 98234 56789", "Junior Systems Engineer", 1, 25, 1100000.0, "2024-02-15", "active", "#8B5CF6"},
        {"Shruti Mathur", "shruti.mathur@crewdeck.io", "+91 98235 67890", "Technical Recruiter", 3, 14, 1350000.0, "2024-03-01", "active", "#06B6D4"},
        {"Kunal Mukherjee", "kunal.mukherjee@crewdeck.io", "+91 98236 78901", "IT Support Specialist", 5, 5, 1050000.0, "2024-03-20", "active", "#F97316"},
        {"Shalini Das", "shalini.das@crewdeck.io", "+91 98237 89012", "Sales Development Representative", 4, 28, 950000.0, "2024-04-10", "active", "#FFB020"},
        {"Nikhil Tiwari", "nikhil.tiwari@crewdeck.io", "+91 98238 90123", "Site Reliability Engineer", 1, 16, 1750000.0, "2024-05-02", "on-leave", "#2DD4BF"},
        {"Preeti Ganguly", "preeti.ganguly@crewdeck.io", "+91 98239 01234", "Associate Product Manager", 2, 11, 1300000.0, "2024-06-01", "active", "#EC4899"},
        {"Mohit Saxena", "mohit.saxena@crewdeck.io", "+91 98240 12345", "Performance Marketing Analyst", 4, 22, 1250000.0, "2024-07-15", "exited", "#3B82F6"}
    };

    const char *ins_emp_sql =
        "INSERT INTO employees (name, email, phone, role_title, department_id, manager_id, salary, join_date, status, avatar_color, created_at, updated_at)\n"
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(g_db, ins_emp_sql, -1, &stmt, NULL) == SQLITE_OK) {
        for (int i = 0; i < 40; i++) {
            sqlite3_bind_text(stmt, 1, employees[i].name, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, employees[i].email, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, employees[i].phone, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, employees[i].role_title, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 5, employees[i].dept_idx);
            if (employees[i].manager_id > 0) {
                sqlite3_bind_int(stmt, 6, employees[i].manager_id);
            } else {
                sqlite3_bind_null(stmt, 6);
            }
            sqlite3_bind_double(stmt, 7, employees[i].salary);
            sqlite3_bind_text(stmt, 8, employees[i].join_date, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 9, employees[i].status, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 10, employees[i].color, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 11, now_iso, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 12, now_iso, -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
    }

    /* Update Department heads (id 1->1, 2->2, 3->3, 4->4, 5->5) */
    db_exec_simple("UPDATE departments SET head_id = id;");

    /* 3. Seed Users with Argon2id passwords */
    char admin_hash[128];
    char mgr_hash[128];
    char emp_hash[128];
    auth_hash_password(g_config.admin_password, admin_hash, sizeof(admin_hash));
    auth_hash_password("manager123", mgr_hash, sizeof(mgr_hash));
    auth_hash_password("employee123", emp_hash, sizeof(emp_hash));

    const char *ins_user_sql = "INSERT INTO users (email, password_hash, role, employee_id, created_at) VALUES (?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(g_db, ins_user_sql, -1, &stmt, NULL) == SQLITE_OK) {
        /* Admin */
        sqlite3_bind_text(stmt, 1, g_config.admin_email, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, admin_hash, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, "admin", -1, SQLITE_STATIC);
        sqlite3_bind_null(stmt, 4);
        sqlite3_bind_text(stmt, 5, now_iso, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);

        /* Manager (Rajesh Verma, emp_id = 1) */
        sqlite3_bind_text(stmt, 1, "rajesh.verma@crewdeck.io", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, mgr_hash, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, "manager", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 4, 1);
        sqlite3_bind_text(stmt, 5, now_iso, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);

        /* Employee (Aarav Sharma, emp_id = 6) */
        sqlite3_bind_text(stmt, 1, "aarav.sharma@crewdeck.io", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, emp_hash, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, "employee", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 4, 6);
        sqlite3_bind_text(stmt, 5, now_iso, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    /* 4. Seed Leave Balances for all 40 employees */
    const char *ins_bal_sql =
        "INSERT INTO leave_balances (employee_id, year, annual_total, annual_used, sick_total, sick_used, casual_total, casual_used)\n"
        "VALUES (?, 2026, 18, ?, 12, ?, 10, ?);";
    if (sqlite3_prepare_v2(g_db, ins_bal_sql, -1, &stmt, NULL) == SQLITE_OK) {
        for (int i = 1; i <= 40; i++) {
            int a_used = (i * 3) % 7;
            int s_used = (i * 2) % 4;
            int c_used = i % 3;
            sqlite3_bind_int(stmt, 1, i);
            sqlite3_bind_int(stmt, 2, a_used);
            sqlite3_bind_int(stmt, 3, s_used);
            sqlite3_bind_int(stmt, 4, c_used);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
    }

    /* 5. Seed Attendance Heatmap Records for past 90 days */
    time_t cur_time = time(NULL);
    const char *ins_att_sql =
        "INSERT OR IGNORE INTO attendance (employee_id, date, check_in, check_out, status, notes)\n"
        "VALUES (?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(g_db, ins_att_sql, -1, &stmt, NULL) == SQLITE_OK) {
        for (int day_offset = 0; day_offset < 90; day_offset++) {
            time_t t = cur_time - (day_offset * 86400);
            struct tm day_tm;
#ifdef _WIN32
            gmtime_s(&day_tm, &t);
#else
            gmtime_r(&t, &day_tm);
#endif
            /* Skip weekends: Sunday is 0, Saturday is 6 */
            if (day_tm.tm_wday == 0 || day_tm.tm_wday == 6) continue;

            char date_buf[16];
            strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &day_tm);

            /* Seed attendance for active employees */
            for (int e = 1; e <= 40; e++) {
                if (e == 40 && day_offset < 60) continue; /* Exited employee */

                const char *att_status = "present";
                const char *check_in = "09:15:00";
                const char *check_out = "18:25:00";
                const char *notes = "Standard shift";

                int hash_val = (e * 37 + day_offset * 13) % 100;
                if (hash_val < 4) {
                    att_status = "absent";
                    check_in = NULL;
                    check_out = NULL;
                    notes = "Unannounced absence";
                } else if (hash_val < 9) {
                    att_status = "on-leave";
                    check_in = NULL;
                    check_out = NULL;
                    notes = "Approved leave";
                } else if (hash_val < 18) {
                    att_status = "late";
                    check_in = "10:14:22";
                    check_out = "18:45:00";
                    notes = "Traffic delay";
                } else if (hash_val < 22) {
                    att_status = "half-day";
                    check_in = "09:30:00";
                    check_out = "13:45:00";
                    notes = "Doctor appointment";
                }

                sqlite3_bind_int(stmt, 1, e);
                sqlite3_bind_text(stmt, 2, date_buf, -1, SQLITE_STATIC);
                if (check_in) {
                    sqlite3_bind_text(stmt, 3, check_in, -1, SQLITE_STATIC);
                } else {
                    sqlite3_bind_null(stmt, 3);
                }
                if (check_out) {
                    sqlite3_bind_text(stmt, 4, check_out, -1, SQLITE_STATIC);
                } else {
                    sqlite3_bind_null(stmt, 4);
                }
                sqlite3_bind_text(stmt, 5, att_status, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 6, notes, -1, SQLITE_STATIC);
                sqlite3_step(stmt);
                sqlite3_reset(stmt);
            }
        }
        sqlite3_finalize(stmt);
    }

    /* 6. Seed Sample Leave Requests */
    const char *ins_leave_sql =
        "INSERT INTO leave_requests (employee_id, leave_type, start_date, end_date, days, reason, status, reviewed_by, review_comment, created_at, updated_at)\n"
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(g_db, ins_leave_sql, -1, &stmt, NULL) == SQLITE_OK) {
        /* Pending request */
        sqlite3_bind_int(stmt, 1, 6); /* Aarav Sharma */
        sqlite3_bind_text(stmt, 2, "annual", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, "2026-11-15", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, "2026-11-18", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 5, 4);
        sqlite3_bind_text(stmt, 6, "Family vacation to Kerala", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, "pending", -1, SQLITE_STATIC);
        sqlite3_bind_null(stmt, 8);
        sqlite3_bind_null(stmt, 9);
        sqlite3_bind_text(stmt, 10, now_iso, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 11, now_iso, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);

        /* Approved request */
        sqlite3_bind_int(stmt, 1, 29); /* Nisha Pillai */
        sqlite3_bind_text(stmt, 2, "sick", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, "2026-11-01", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, "2026-11-04", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 5, 3);
        sqlite3_bind_text(stmt, 6, "Viral fever recovery", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, "approved", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 8, 1);
        sqlite3_bind_text(stmt, 9, "Approved. Get well soon!", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 10, now_iso, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 11, now_iso, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);

        /* Rejected request */
        sqlite3_bind_int(stmt, 1, 8); /* Sneha Kulkarni */
        sqlite3_bind_text(stmt, 2, "casual", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, "2026-10-20", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, "2026-10-21", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 5, 2);
        sqlite3_bind_text(stmt, 6, "Personal emergency", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, "rejected", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 8, 1);
        sqlite3_bind_text(stmt, 9, "Critical production release on these dates", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 10, now_iso, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 11, now_iso, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    /* 7. Seed Initial Audit Log */
    const char *ins_audit_sql =
        "INSERT INTO audit_logs (user_id, user_email, action, entity_type, entity_id, details, ip_address, created_at)\n"
        "VALUES (1, ?, 'SYSTEM_SEED', 'system', 0, 'Initial deployment seeded with 40 Indian context employees and org structure', '127.0.0.1', ?);";
    if (sqlite3_prepare_v2(g_db, ins_audit_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, g_config.admin_email, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, now_iso, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    /* Commit transaction */
    db_exec_simple("COMMIT;");
    printf("[SEED] Successfully seeded database with all departments, employees, attendance, balances, and admin user.\n");
    return true;
}
