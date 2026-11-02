#include "util.h"

void util_json_error(struct mg_connection *c, int status_code, const char *code, const char *message) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        mg_http_reply(c, 500, "Content-Type: application/json\r\n", "{\"error\":{\"code\":\"INTERNAL_ERROR\",\"message\":\"Memory error\"}}");
        return;
    }
    cJSON *err = cJSON_CreateObject();
    if (err) {
        cJSON_AddStringToObject(err, "code", code ? code : "ERROR");
        cJSON_AddStringToObject(err, "message", message ? message : "Unknown error occurred");
        cJSON_AddItemToObject(root, "error", err);
    }
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        mg_http_reply(c, status_code, "Content-Type: application/json\r\nCache-Control: no-store\r\n", "%s", json_str);
        free(json_str);
    } else {
        mg_http_reply(c, 500, "Content-Type: application/json\r\n", "{\"error\":{\"code\":\"INTERNAL_ERROR\",\"message\":\"Serialization error\"}}");
    }
    cJSON_Delete(root);
}

void util_json_response(struct mg_connection *c, int status_code, cJSON *json) {
    if (!json) {
        util_json_error(c, 500, "INTERNAL_ERROR", "Null JSON object");
        return;
    }
    char *json_str = cJSON_PrintUnformatted(json);
    if (json_str) {
        mg_http_reply(c, status_code, "Content-Type: application/json\r\nCache-Control: no-store\r\n", "%s", json_str);
        free(json_str);
    } else {
        util_json_error(c, 500, "INTERNAL_ERROR", "Failed to print JSON response");
    }
    cJSON_Delete(json);
}

void util_json_string(struct mg_connection *c, int status_code, const char *json_str) {
    if (!json_str) {
        json_str = "{}";
    }
    mg_http_reply(c, status_code, "Content-Type: application/json\r\nCache-Control: no-store\r\n", "%s", json_str);
}

cJSON *util_parse_json(struct mg_http_message *hm) {
    if (!hm || hm->body.len == 0) {
        return NULL;
    }
    char *buf = (char *)malloc(hm->body.len + 1);
    if (!buf) return NULL;
    memcpy(buf, hm->body.buf, hm->body.len);
    buf[hm->body.len] = '\0';

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    return json;
}

bool util_get_cookie(struct mg_http_message *hm, const char *name, char *dest, size_t dest_len) {
    if (!hm || !name || !dest || dest_len == 0) return false;
    dest[0] = '\0';
    struct mg_str *cookie_hdr = mg_http_get_header(hm, "Cookie");
    if (!cookie_hdr || cookie_hdr->len == 0) return false;

    struct mg_str val = mg_http_get_header_var(*cookie_hdr, mg_str(name));
    if (val.len == 0) return false;

    size_t copy_len = (val.len < dest_len - 1) ? val.len : dest_len - 1;
    memcpy(dest, val.buf, copy_len);
    dest[copy_len] = '\0';
    return true;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

size_t util_url_decode(const char *src, size_t src_len, char *dst, size_t dst_len) {
    if (!src || !dst || dst_len == 0) return 0;
    size_t s = 0, d = 0;
    while (s < src_len && d < dst_len - 1) {
        if (src[s] == '%' && s + 2 < src_len) {
            int h1 = hex_val(src[s + 1]);
            int h2 = hex_val(src[s + 2]);
            if (h1 >= 0 && h2 >= 0) {
                dst[d++] = (char)((h1 << 4) | h2);
                s += 3;
                continue;
            }
        }
        if (src[s] == '+') {
            dst[d++] = ' ';
            s++;
        } else {
            dst[d++] = src[s++];
        }
    }
    dst[d] = '\0';
    return d;
}

bool util_get_query_param(struct mg_http_message *hm, const char *name, char *dest, size_t dest_len) {
    if (!hm || !name || !dest || dest_len == 0) return false;
    dest[0] = '\0';
    if (hm->query.len == 0) return false;

    char raw[512];
    int rc = mg_http_get_var(&hm->query, name, raw, sizeof(raw));
    if (rc <= 0) return false;

    util_url_decode(raw, (size_t)rc, dest, dest_len);
    return true;
}

bool util_get_header(struct mg_http_message *hm, const char *name, char *dest, size_t dest_len) {
    if (!hm || !name || !dest || dest_len == 0) return false;
    dest[0] = '\0';
    struct mg_str *hdr = mg_http_get_header(hm, name);
    if (!hdr || hdr->len == 0) return false;

    size_t copy_len = (hdr->len < dest_len - 1) ? hdr->len : dest_len - 1;
    memcpy(dest, hdr->buf, copy_len);
    dest[copy_len] = '\0';
    return true;
}

void util_get_client_ip(struct mg_connection *c, struct mg_http_message *hm, char *dest, size_t dest_len) {
    if (!dest || dest_len == 0) return;
    dest[0] = '\0';

    if (hm) {
        if (util_get_header(hm, "X-Forwarded-For", dest, dest_len)) {
            char *comma = strchr(dest, ',');
            if (comma) *comma = '\0';
            return;
        }
        if (util_get_header(hm, "X-Real-IP", dest, dest_len)) {
            return;
        }
    }

    if (c) {
        mg_snprintf(dest, dest_len, "%M", mg_print_ip, &c->rem);
        char *colon = strchr(dest, ':');
        if (colon) *colon = '\0';
    }
    if (dest[0] == '\0') {
        snprintf(dest, dest_len, "127.0.0.1");
    }
}

void util_now_iso8601(char *dest, size_t dest_len) {
    if (!dest || dest_len == 0) return;
    time_t now = time(NULL);
    struct tm gm;
#ifdef _WIN32
    gmtime_s(&gm, &now);
#else
    gmtime_r(&now, &gm);
#endif
    strftime(dest, dest_len, "%Y-%m-%dT%H:%M:%SZ", &gm);
}

void util_today_date(char *dest, size_t dest_len) {
    if (!dest || dest_len == 0) return;
    time_t now = time(NULL);
    struct tm gm;
#ifdef _WIN32
    gmtime_s(&gm, &now);
#else
    gmtime_r(&now, &gm);
#endif
    strftime(dest, dest_len, "%Y-%m-%d", &gm);
}

void util_now_time(char *dest, size_t dest_len) {
    if (!dest || dest_len == 0) return;
    time_t now = time(NULL);
    struct tm gm;
#ifdef _WIN32
    gmtime_s(&gm, &now);
#else
    gmtime_r(&now, &gm);
#endif
    strftime(dest, dest_len, "%H:%M:%S", &gm);
}

void util_generate_token(char *dest, size_t dest_len) {
    if (!dest || dest_len < 65) return;
    uint8_t raw[32];
    randombytes_buf(raw, sizeof(raw));
    for (size_t i = 0; i < 32; i++) {
        snprintf(&dest[i * 2], 3, "%02x", raw[i]);
    }
    dest[64] = '\0';
}

bool util_get_path_id(const char *uri, const char *prefix, int *out_id) {
    if (!uri || !prefix || !out_id) return false;
    size_t pre_len = strlen(prefix);
    if (strncmp(uri, prefix, pre_len) != 0) return false;

    const char *id_str = uri + pre_len;
    if (*id_str == '\0') return false;

    char *endptr = NULL;
    long val = strtol(id_str, &endptr, 10);
    if (endptr == id_str || (*endptr != '\0' && *endptr != '?' && *endptr != '/')) {
        return false;
    }
    if (val <= 0 || val > 2147483647) return false;
    *out_id = (int)val;
    return true;
}

bool util_is_valid_email(const char *email) {
    if (!email || strlen(email) < 5 || strlen(email) > 120) return false;
    const char *at = strchr(email, '@');
    if (!at || at == email) return false;
    const char *dot = strrchr(at, '.');
    if (!dot || dot == at + 1 || *(dot + 1) == '\0') return false;
    return true;
}

bool util_is_valid_date(const char *date_str) {
    if (!date_str || strlen(date_str) != 10) return false;
    if (date_str[4] != '-' || date_str[7] != '-') return false;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (!isdigit((unsigned char)date_str[i])) return false;
    }
    int y = atoi(date_str);
    int m = atoi(date_str + 5);
    int d = atoi(date_str + 8);
    if (y < 1970 || y > 2100) return false;
    if (m < 1 || m > 12) return false;
    if (d < 1 || d > 31) return false;
    return true;
}
