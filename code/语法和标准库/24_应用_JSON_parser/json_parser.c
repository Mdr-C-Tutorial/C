#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_kind;

typedef struct json_value json_value;

typedef struct {
    char *key;
    json_value *value;
} json_member;

struct json_value {
    json_kind kind;
    union {
        bool b;
        double num;
        char *str;
        struct {
            json_value **items;
            size_t len;
        } arr;
        struct {
            json_member *items;
            size_t len;
        } obj;
    } as;
};

typedef struct {
    const char *cur;
    const char *end;
    const char *msg;
} json_parser;

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} str_builder;

static void skip_ws(json_parser *p) {
    while (p->cur < p->end) {
        char c = *p->cur;
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
            break;
        }
        p->cur++;
    }
}

static void set_error(json_parser *p, const char *msg) {
    if (p->msg == NULL) {
        p->msg = msg;
    }
}

static json_value *json_new(json_kind kind) {
    json_value *v = (json_value *)calloc(1, sizeof(*v));
    if (v != NULL) {
        v->kind = kind;
    }
    return v;
}

static int sb_reserve(str_builder *sb, size_t need) {
    if (need <= sb->cap) {
        return 0;
    }
    size_t next = sb->cap == 0 ? 32u : sb->cap;
    while (next < need) {
        next *= 2u;
    }
    char *mem = (char *)realloc(sb->buf, next);
    if (mem == NULL) {
        return -1;
    }
    sb->buf = mem;
    sb->cap = next;
    return 0;
}

static int sb_push_char(str_builder *sb, char c) {
    if (sb_reserve(sb, sb->len + 2u) != 0) {
        return -1;
    }
    sb->buf[sb->len++] = c;
    sb->buf[sb->len] = '\0';
    return 0;
}

static int sb_push_utf8(str_builder *sb, uint32_t cp) {
    if (cp <= 0x7Fu) {
        return sb_push_char(sb, (char)cp);
    }
    if (cp <= 0x7FFu) {
        if (sb_reserve(sb, sb->len + 3u) != 0) {
            return -1;
        }
        sb->buf[sb->len++] = (char)(0xC0u | (cp >> 6));
        sb->buf[sb->len++] = (char)(0x80u | (cp & 0x3Fu));
        sb->buf[sb->len] = '\0';
        return 0;
    }
    if (cp <= 0xFFFFu) {
        if (sb_reserve(sb, sb->len + 4u) != 0) {
            return -1;
        }
        sb->buf[sb->len++] = (char)(0xE0u | (cp >> 12));
        sb->buf[sb->len++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        sb->buf[sb->len++] = (char)(0x80u | (cp & 0x3Fu));
        sb->buf[sb->len] = '\0';
        return 0;
    }
    if (cp <= 0x10FFFFu) {
        if (sb_reserve(sb, sb->len + 5u) != 0) {
            return -1;
        }
        sb->buf[sb->len++] = (char)(0xF0u | (cp >> 18));
        sb->buf[sb->len++] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        sb->buf[sb->len++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        sb->buf[sb->len++] = (char)(0x80u | (cp & 0x3Fu));
        sb->buf[sb->len] = '\0';
        return 0;
    }
    return -1;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

static int parse_hex4(json_parser *p, uint32_t *out) {
    if ((size_t)(p->end - p->cur) < 4u) {
        return -1;
    }
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
        int h = hex_value(p->cur[i]);
        if (h < 0) {
            return -1;
        }
        v = (v << 4) | (uint32_t)h;
    }
    p->cur += 4;
    *out = v;
    return 0;
}

static void json_free(json_value *v);
static json_value *parse_value(json_parser *p);

static json_value *parse_string(json_parser *p) {
    if (p->cur >= p->end || *p->cur != '"') {
        set_error(p, "expect string");
        return NULL;
    }
    p->cur++;

    str_builder sb = {0};
    while (p->cur < p->end) {
        unsigned char c = (unsigned char)*p->cur++;
        if (c == '"') {
            json_value *v = json_new(JSON_STRING);
            if (v == NULL) {
                free(sb.buf);
                set_error(p, "out of memory");
                return NULL;
            }
            if (sb.buf == NULL) {
                sb.buf = (char *)malloc(1u);
                if (sb.buf == NULL) {
                    free(v);
                    set_error(p, "out of memory");
                    return NULL;
                }
                sb.buf[0] = '\0';
            }
            v->as.str = sb.buf;
            return v;
        }

        if (c < 0x20u) {
            free(sb.buf);
            set_error(p, "control character in string");
            return NULL;
        }

        if (c != '\\') {
            if (sb_push_char(&sb, (char)c) != 0) {
                free(sb.buf);
                set_error(p, "out of memory");
                return NULL;
            }
            continue;
        }

        if (p->cur >= p->end) {
            free(sb.buf);
            set_error(p, "bad escape");
            return NULL;
        }
        char esc = *p->cur++;
        switch (esc) {
            case '"':
                if (sb_push_char(&sb, '"') != 0) goto oom;
                break;
            case '\\':
                if (sb_push_char(&sb, '\\') != 0) goto oom;
                break;
            case '/':
                if (sb_push_char(&sb, '/') != 0) goto oom;
                break;
            case 'b':
                if (sb_push_char(&sb, '\b') != 0) goto oom;
                break;
            case 'f':
                if (sb_push_char(&sb, '\f') != 0) goto oom;
                break;
            case 'n':
                if (sb_push_char(&sb, '\n') != 0) goto oom;
                break;
            case 'r':
                if (sb_push_char(&sb, '\r') != 0) goto oom;
                break;
            case 't':
                if (sb_push_char(&sb, '\t') != 0) goto oom;
                break;
            case 'u': {
                uint32_t cp1 = 0;
                if (parse_hex4(p, &cp1) != 0) {
                    free(sb.buf);
                    set_error(p, "bad unicode escape");
                    return NULL;
                }
                if (cp1 >= 0xD800u && cp1 <= 0xDBFFu) {
                    if ((size_t)(p->end - p->cur) < 6u || p->cur[0] != '\\' || p->cur[1] != 'u') {
                        free(sb.buf);
                        set_error(p, "bad surrogate pair");
                        return NULL;
                    }
                    p->cur += 2;
                    uint32_t cp2 = 0;
                    if (parse_hex4(p, &cp2) != 0 || cp2 < 0xDC00u || cp2 > 0xDFFFu) {
                        free(sb.buf);
                        set_error(p, "bad surrogate pair");
                        return NULL;
                    }
                    uint32_t code = 0x10000u + (((cp1 - 0xD800u) << 10) | (cp2 - 0xDC00u));
                    if (sb_push_utf8(&sb, code) != 0) {
                        goto oom;
                    }
                } else if (cp1 >= 0xDC00u && cp1 <= 0xDFFFu) {
                    free(sb.buf);
                    set_error(p, "bad surrogate pair");
                    return NULL;
                } else {
                    if (sb_push_utf8(&sb, cp1) != 0) {
                        goto oom;
                    }
                }
                break;
            }
            default:
                free(sb.buf);
                set_error(p, "bad escape");
                return NULL;
        }
    }

    free(sb.buf);
    set_error(p, "unterminated string");
    return NULL;

oom:
    free(sb.buf);
    set_error(p, "out of memory");
    return NULL;
}

static json_value *parse_number(json_parser *p) {
    const char *s = p->cur;
    const char *q = p->cur;

    if (q < p->end && *q == '-') {
        q++;
    }
    if (q >= p->end) {
        set_error(p, "bad number");
        return NULL;
    }

    if (*q == '0') {
        q++;
    } else {
        if (!isdigit((unsigned char)*q)) {
            set_error(p, "bad number");
            return NULL;
        }
        while (q < p->end && isdigit((unsigned char)*q)) {
            q++;
        }
    }

    if (q < p->end && *q == '.') {
        q++;
        if (q >= p->end || !isdigit((unsigned char)*q)) {
            set_error(p, "bad number");
            return NULL;
        }
        while (q < p->end && isdigit((unsigned char)*q)) {
            q++;
        }
    }

    if (q < p->end && (*q == 'e' || *q == 'E')) {
        q++;
        if (q < p->end && (*q == '+' || *q == '-')) {
            q++;
        }
        if (q >= p->end || !isdigit((unsigned char)*q)) {
            set_error(p, "bad number");
            return NULL;
        }
        while (q < p->end && isdigit((unsigned char)*q)) {
            q++;
        }
    }

    size_t n = (size_t)(q - s);
    char *tmp = (char *)malloc(n + 1u);
    if (tmp == NULL) {
        set_error(p, "out of memory");
        return NULL;
    }
    memcpy(tmp, s, n);
    tmp[n] = '\0';

    errno = 0;
    char *ep = NULL;
    double num = strtod(tmp, &ep);
    if (ep == NULL || (size_t)(ep - tmp) != n || errno == ERANGE) {
        free(tmp);
        set_error(p, "bad number");
        return NULL;
    }
    free(tmp);

    json_value *v = json_new(JSON_NUMBER);
    if (v == NULL) {
        set_error(p, "out of memory");
        return NULL;
    }
    v->as.num = num;
    p->cur = q;
    return v;
}

static int match_keyword(json_parser *p, const char *kw) {
    size_t n = strlen(kw);
    if ((size_t)(p->end - p->cur) < n) {
        return 0;
    }
    if (strncmp(p->cur, kw, n) != 0) {
        return 0;
    }
    p->cur += n;
    return 1;
}

static json_value *parse_array(json_parser *p) {
    if (p->cur >= p->end || *p->cur != '[') {
        set_error(p, "expect '['");
        return NULL;
    }
    p->cur++;

    json_value *arr = json_new(JSON_ARRAY);
    if (arr == NULL) {
        set_error(p, "out of memory");
        return NULL;
    }

    skip_ws(p);
    if (p->cur < p->end && *p->cur == ']') {
        p->cur++;
        return arr;
    }

    while (p->cur < p->end) {
        skip_ws(p);
        json_value *item = parse_value(p);
        if (item == NULL) {
            json_free(arr);
            return NULL;
        }
        size_t next_len = arr->as.arr.len + 1u;
        json_value **mem = (json_value **)realloc(arr->as.arr.items, next_len * sizeof(*mem));
        if (mem == NULL) {
            json_free(item);
            json_free(arr);
            set_error(p, "out of memory");
            return NULL;
        }
        arr->as.arr.items = mem;
        arr->as.arr.items[arr->as.arr.len++] = item;

        skip_ws(p);
        if (p->cur < p->end && *p->cur == ',') {
            p->cur++;
            continue;
        }
        if (p->cur < p->end && *p->cur == ']') {
            p->cur++;
            return arr;
        }
        break;
    }

    json_free(arr);
    set_error(p, "expect ',' or ']'");
    return NULL;
}

static json_value *parse_object(json_parser *p) {
    if (p->cur >= p->end || *p->cur != '{') {
        set_error(p, "expect '{'");
        return NULL;
    }
    p->cur++;

    json_value *obj = json_new(JSON_OBJECT);
    if (obj == NULL) {
        set_error(p, "out of memory");
        return NULL;
    }

    skip_ws(p);
    if (p->cur < p->end && *p->cur == '}') {
        p->cur++;
        return obj;
    }

    while (p->cur < p->end) {
        skip_ws(p);
        json_value *k = parse_string(p);
        if (k == NULL) {
            json_free(obj);
            return NULL;
        }

        skip_ws(p);
        if (p->cur >= p->end || *p->cur != ':') {
            json_free(k);
            json_free(obj);
            set_error(p, "expect ':'");
            return NULL;
        }
        p->cur++;

        skip_ws(p);
        json_value *v = parse_value(p);
        if (v == NULL) {
            json_free(k);
            json_free(obj);
            return NULL;
        }

        size_t next_len = obj->as.obj.len + 1u;
        json_member *mem = (json_member *)realloc(obj->as.obj.items, next_len * sizeof(*mem));
        if (mem == NULL) {
            json_free(k);
            json_free(v);
            json_free(obj);
            set_error(p, "out of memory");
            return NULL;
        }
        obj->as.obj.items = mem;
        obj->as.obj.items[obj->as.obj.len].key = k->as.str;
        obj->as.obj.items[obj->as.obj.len].value = v;
        obj->as.obj.len++;
        free(k);

        skip_ws(p);
        if (p->cur < p->end && *p->cur == ',') {
            p->cur++;
            continue;
        }
        if (p->cur < p->end && *p->cur == '}') {
            p->cur++;
            return obj;
        }
        break;
    }

    json_free(obj);
    set_error(p, "expect ',' or '}'");
    return NULL;
}

static json_value *parse_value(json_parser *p) {
    skip_ws(p);
    if (p->cur >= p->end) {
        set_error(p, "unexpected end");
        return NULL;
    }

    char c = *p->cur;
    if (c == '"') {
        return parse_string(p);
    }
    if (c == '{') {
        return parse_object(p);
    }
    if (c == '[') {
        return parse_array(p);
    }
    if (c == 't') {
        if (!match_keyword(p, "true")) {
            set_error(p, "bad keyword");
            return NULL;
        }
        json_value *v = json_new(JSON_BOOL);
        if (v == NULL) {
            set_error(p, "out of memory");
            return NULL;
        }
        v->as.b = true;
        return v;
    }
    if (c == 'f') {
        if (!match_keyword(p, "false")) {
            set_error(p, "bad keyword");
            return NULL;
        }
        json_value *v = json_new(JSON_BOOL);
        if (v == NULL) {
            set_error(p, "out of memory");
            return NULL;
        }
        v->as.b = false;
        return v;
    }
    if (c == 'n') {
        if (!match_keyword(p, "null")) {
            set_error(p, "bad keyword");
            return NULL;
        }
        json_value *v = json_new(JSON_NULL);
        if (v == NULL) {
            set_error(p, "out of memory");
        }
        return v;
    }
    if (c == '-' || isdigit((unsigned char)c)) {
        return parse_number(p);
    }

    set_error(p, "unexpected token");
    return NULL;
}

static json_value *json_parse_n(const char *text, size_t len, const char **err_pos, const char **err_msg) {
    json_parser p = {0};
    p.cur = text;
    p.end = text + len;
    p.msg = NULL;

    skip_ws(&p);
    json_value *root = parse_value(&p);
    if (root == NULL) {
        if (err_pos != NULL) {
            *err_pos = p.cur;
        }
        if (err_msg != NULL) {
            *err_msg = p.msg == NULL ? "parse failed" : p.msg;
        }
        return NULL;
    }

    skip_ws(&p);
    if (p.cur != p.end) {
        json_free(root);
        if (err_pos != NULL) {
            *err_pos = p.cur;
        }
        if (err_msg != NULL) {
            *err_msg = "trailing characters";
        }
        return NULL;
    }

    if (err_pos != NULL) {
        *err_pos = p.cur;
    }
    if (err_msg != NULL) {
        *err_msg = NULL;
    }
    return root;
}

static json_value *json_parse(const char *text, const char **err_pos, const char **err_msg) {
    return json_parse_n(text, strlen(text), err_pos, err_msg);
}

static void json_free(json_value *v) {
    if (v == NULL) {
        return;
    }
    switch (v->kind) {
        case JSON_STRING:
            free(v->as.str);
            break;
        case JSON_ARRAY:
            for (size_t i = 0; i < v->as.arr.len; ++i) {
                json_free(v->as.arr.items[i]);
            }
            free(v->as.arr.items);
            break;
        case JSON_OBJECT:
            for (size_t i = 0; i < v->as.obj.len; ++i) {
                free(v->as.obj.items[i].key);
                json_free(v->as.obj.items[i].value);
            }
            free(v->as.obj.items);
            break;
        case JSON_NULL:
        case JSON_BOOL:
        case JSON_NUMBER:
            break;
    }
    free(v);
}

static const json_value *json_object_get(const json_value *obj, const char *key) {
    if (obj == NULL || obj->kind != JSON_OBJECT) {
        return NULL;
    }
    for (size_t i = 0; i < obj->as.obj.len; ++i) {
        if (strcmp(obj->as.obj.items[i].key, key) == 0) {
            return obj->as.obj.items[i].value;
        }
    }
    return NULL;
}

static const json_value *json_array_get(const json_value *arr, size_t index) {
    if (arr == NULL || arr->kind != JSON_ARRAY || index >= arr->as.arr.len) {
        return NULL;
    }
    return arr->as.arr.items[index];
}

int main(void) {
    const char *text =
        "{"
        "\"user\":\"Ada\","
        "\"age\":18,"
        "\"active\":true,"
        "\"scores\":[99.5,88,77],"
        "\"city\":\"Shenzhen\""
        "}";

    const char *err_pos = NULL;
    const char *err_msg = NULL;
    json_value *root = json_parse(text, &err_pos, &err_msg);
    if (root == NULL) {
        long col = err_pos == NULL ? -1L : (long)(err_pos - text);
        printf("parse error at %ld: %s\n", col, err_msg == NULL ? "unknown" : err_msg);
        return 1;
    }

    const json_value *user = json_object_get(root, "user");
    const json_value *age = json_object_get(root, "age");
    const json_value *active = json_object_get(root, "active");
    const json_value *scores = json_object_get(root, "scores");
    const json_value *first = json_array_get(scores, 0);

    printf("user=%s\n", (user != NULL && user->kind == JSON_STRING) ? user->as.str : "<bad>");
    printf("age=%.0f\n", (age != NULL && age->kind == JSON_NUMBER) ? age->as.num : -1.0);
    printf("active=%s\n", (active != NULL && active->kind == JSON_BOOL && active->as.b) ? "true" : "false");
    printf("first_score=%.1f\n", (first != NULL && first->kind == JSON_NUMBER) ? first->as.num : -1.0);

    json_free(root);
    return 0;
}
