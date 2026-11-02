#include "../include/common.h"
#include "../include/config.h"
#include "../include/util.h"
#include "../include/auth.h"
#include "../include/db.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (cond) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        printf("  [FAIL] %s (Line %d)\n", msg, __LINE__); \
    } \
} while(0)

static void test_util_layer(void) {
    printf("\n--- Testing Util Layer ---\n");

    /* Email validation */
    TEST_ASSERT(util_is_valid_email("admin@crewdeck.io") == true, "Valid standard email");
    TEST_ASSERT(util_is_valid_email("priya.patel@company.co.in") == true, "Valid subdomain email");
    TEST_ASSERT(util_is_valid_email("invalid-email") == false, "Invalid email missing @");
    TEST_ASSERT(util_is_valid_email("@domain.com") == false, "Invalid email missing user");
    TEST_ASSERT(util_is_valid_email("user@") == false, "Invalid email missing domain");

    /* Date validation */
    TEST_ASSERT(util_is_valid_date("2026-11-02") == true, "Valid YYYY-MM-DD date");
    TEST_ASSERT(util_is_valid_date("2024-02-29") == true, "Valid leap year date");
    TEST_ASSERT(util_is_valid_date("2026/11/02") == false, "Invalid date format with slashes");
    TEST_ASSERT(util_is_valid_date("26-11-2026") == false, "Invalid date format DD-MM-YYYY");
    TEST_ASSERT(util_is_valid_date("2026-13-01") == false, "Invalid month > 12");

    /* Path ID parsing */
    int out_id = 0;
    TEST_ASSERT(util_get_path_id("/api/v1/employees/42", "/api/v1/employees/", &out_id) && out_id == 42, "Path ID parsed 42");
    TEST_ASSERT(util_get_path_id("/api/v1/employees/abc", "/api/v1/employees/", &out_id) == false, "Non-numeric Path ID rejected");

    /* URL decoding */
    char decoded[64];
    util_url_decode("hello%20world%21", 16, decoded, sizeof(decoded));
    TEST_ASSERT(strcmp(decoded, "hello world!") == 0, "URL decode %20 and %21");

    /* Cryptographic Token Generation */
    char tok1[128] = {0};
    char tok2[128] = {0};
    util_generate_token(tok1, sizeof(tok1));
    util_generate_token(tok2, sizeof(tok2));
    TEST_ASSERT(strlen(tok1) == 64, "Generated 64-hex token");
    TEST_ASSERT(strcmp(tok1, tok2) != 0, "Distinct random tokens generated");
}

static void test_auth_layer(void) {
    printf("\n--- Testing Auth Layer ---\n");

    /* Argon2id password hashing */
    char hash[128];
    bool hash_ok = auth_hash_password("SuperSecret123!", hash, sizeof(hash));
    TEST_ASSERT(hash_ok == true, "Argon2id password hash generated");
    TEST_ASSERT(strncmp(hash, "$argon2id$", 10) == 0, "Hash is valid Argon2id format");

    /* Verification */
    TEST_ASSERT(auth_verify_password("SuperSecret123!", hash) == true, "Argon2id password verify success");
    TEST_ASSERT(auth_verify_password("WrongPassword!", hash) == false, "Argon2id password verify wrong rejected");

    /* Rate limiting */
    auth_clear_login_attempts("192.168.1.100");
    TEST_ASSERT(auth_is_rate_limited("192.168.1.100") == false, "Initial rate limit is false");
    for (int i = 0; i < 5; i++) {
        auth_record_login_attempt("192.168.1.100");
    }
    TEST_ASSERT(auth_is_rate_limited("192.168.1.100") == true, "Rate limit reached after 5 attempts");
    auth_clear_login_attempts("192.168.1.100");
    TEST_ASSERT(auth_is_rate_limited("192.168.1.100") == false, "Rate limit cleared successfully");
}

static void test_db_layer(void) {
    printf("\n--- Testing DB Layer ---\n");

    /* Check DB handle and seeded tables */
    sqlite3 *db = db_get_handle();
    TEST_ASSERT(db != NULL, "SQLite DB handle is active");

    sqlite3_stmt *stmt = NULL;
    int emp_count = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM employees;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            emp_count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    TEST_ASSERT(emp_count >= 40, "Seeded at least 40 employees");

    int dept_count = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM departments;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            dept_count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    TEST_ASSERT(dept_count >= 5, "Seeded at least 5 departments");

    int att_count = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM attendance;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            att_count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    TEST_ASSERT(att_count > 500, "Seeded rich attendance records for heatmap");
}

int main(void) {
    config_init();
    if (sodium_init() < 0) {
        fprintf(stderr, "Failed to initialize sodium in tests\n");
        return 1;
    }

    /* Use test db path */
    snprintf(g_config.db_path, sizeof(g_config.db_path), "./data/test_crewdeck.db");
    if (!db_init(g_config.db_path)) {
        fprintf(stderr, "Failed to initialize test database\n");
        return 1;
    }

    test_util_layer();
    test_auth_layer();
    test_db_layer();

    db_close();

    printf("\n=========================================\n");
    printf(" Unit Tests Summary: %d / %d Passed (%d%%)\n",
           g_tests_passed, g_tests_run, (g_tests_passed * 100) / (g_tests_run > 0 ? g_tests_run : 1));
    printf("=========================================\n\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
