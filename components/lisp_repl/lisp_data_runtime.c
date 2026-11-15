#include "lisp_data_runtime.h"
#include "lisp_runtime.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lisp_data_workspace_t *work;
static lisp_http_backend_t http;
static bool attached, app_owned;
static uint32_t active_id;
static uint64_t clock_ms;
static int64_t (*unix_clock)(void *);
static void *clock_user;
static lbm_value symbol(const char *name)
{
    lbm_uint id;
    return lbm_add_symbol_const(name, &id) ? lbm_enc_sym(id) : ENC_SYM_MERROR;
}
static lbm_value pair(lbm_value a, lbm_value b)
{
    if (lbm_is_error(a) || lbm_is_error(b))
        return ENC_SYM_MERROR;
    return lbm_heap_allocate_list_init(2, a, b);
}
static bool string(lbm_value value, char **text, size_t *length)
{
    size_t size;
    if (!lbm_dec_str_size(value, text, &size) || !*text || !size || (*text)[size - 1] ||
        memchr(*text, 0, size - 1))
        return false;
    *length = size - 1;
    return true;
}
static lbm_value copy_string(const char *text, size_t length)
{
    lbm_value result;
    if (length > LISP_HTTP_BODY_MAX || !lbm_create_array(&result, (lbm_uint)(length + 1)))
        return ENC_SYM_MERROR;
    char *out = lbm_dec_str(result);
    if (!out)
        return ENC_SYM_MERROR;
    memcpy(out, text, length);
    out[length] = 0;
    return result;
}
bool lisp_runtime_http_attach(const lisp_http_backend_t *backend)
{
    if (active_id || !backend || !backend->request || !backend->poll || !backend->cancel || !backend->release)
        return false;
    http = *backend;
    attached = true;
    return true;
}
bool lisp_runtime_time_attach(int64_t (*callback)(void *), void *user)
{
    if (!callback)
        return false;
    unix_clock = callback;
    clock_user = user;
    return true;
}
void lisp_data_runtime_reset(void)
{
    if (active_id && attached)
        (void)http.release(http.user, active_id);
    active_id = 0;
    app_owned = false;
}
void lisp_data_runtime_app_stop(void)
{
    if (app_owned)
        lisp_data_runtime_reset();
}
void lisp_data_runtime_step(uint64_t now_ms)
{
    clock_ms = now_ms;
}
static lbm_value http_get(lbm_value *args, lbm_uint n)
{
    char *url;
    size_t size;
    if (n != 1 || !string(args[0], &url, &size) || !lisp_app_url_valid(url, size))
        return ENC_SYM_TERROR;
    if (!attached)
        return pair(symbol("error"), symbol("unavailable"));
    if (active_id)
        return pair(symbol("error"), symbol("busy"));
    /* Allocate BEFORE dispatch. Extension OOM retries must never duplicate IO. */
    lbm_value result = pair(symbol("ok"), ENC_SYM_NIL);
    if (lbm_is_error(result))
        return result;
    uint32_t id = 0;
    lisp_http_error_t error = http.request(http.user, url, size, &id);
    if (error != LISP_HTTP_OK) {
        (void)lbm_set_car(result, symbol("error"));
        (void)lbm_set_car(lbm_cdr(result), symbol(lisp_http_error_name(error)));
    } else {
        active_id = id;
        app_owned = lisp_runtime_app_active();
        (void)lbm_set_car(lbm_cdr(result), lbm_enc_i((lbm_int)id));
    }
    return result;
}
static bool request_id(lbm_value v, uint32_t *id)
{
    lbm_type t = lbm_type_of_functional(v);
    if (t != LBM_TYPE_I && t != LBM_TYPE_U && t != LBM_TYPE_I32 && t != LBM_TYPE_U32 && t != LBM_TYPE_I64 &&
        t != LBM_TYPE_U64)
        return false;
    uint64_t number = lbm_dec_as_u64(v);
    if (!number || number > LISP_HTTP_ID_MAX)
        return false;
    *id = (uint32_t)number;
    return true;
}
static lbm_value http_poll(lbm_value *args, lbm_uint n)
{
    uint32_t id;
    if (n != 1 || !request_id(args[0], &id))
        return ENC_SYM_TERROR;
    lisp_http_result_t r = {.error = LISP_HTTP_STALE};
    lisp_http_phase_t phase = attached && id == active_id ? http.poll(http.user, id, &r) : LISP_HTTP_IDLE;
    if (phase == LISP_HTTP_FETCHING)
        return lbm_heap_allocate_list_init(1, symbol("pending"));
    if (phase != LISP_HTTP_READY || r.error != LISP_HTTP_OK)
        return lbm_heap_allocate_list_init(3, symbol("error"), symbol(lisp_http_error_name(r.error)),
                                           lbm_enc_i(r.status));
    lbm_value body = copy_string(r.body, r.length);
    if (lbm_is_error(body))
        return body;
    /* READY bytes remain leased through allocations/GC retry and repeated poll. */
    return lbm_heap_allocate_list_init(3, symbol("ok"), lbm_enc_i(r.status), body);
}
static lbm_value http_cancel(lbm_value *args, lbm_uint n)
{
    uint32_t id;
    if (n != 1 || !request_id(args[0], &id))
        return ENC_SYM_TERROR;
    return attached && id == active_id && http.cancel(http.user, id) ? ENC_SYM_TRUE : ENC_SYM_NIL;
}
static lbm_value http_release(lbm_value *args, lbm_uint n)
{
    uint32_t id;
    if (n != 1 || !request_id(args[0], &id))
        return ENC_SYM_TERROR;
    if (!attached || id != active_id || !http.release(http.user, id))
        return ENC_SYM_NIL;
    active_id = 0;
    app_owned = false;
    return ENC_SYM_TRUE;
}
static lbm_value monotonic_ms(lbm_value *args, lbm_uint n)
{
    (void)args;
    return n ? ENC_SYM_TERROR : lbm_enc_u64(clock_ms);
}
static int64_t utc_seconds(void)
{
    int64_t now = unix_clock ? unix_clock(clock_user) : 0;
    return now >= INT64_C(1735689600) ? now : 0;
}
static lbm_value time_unix(lbm_value *args, lbm_uint n)
{
    (void)args;
    if (n)
        return ENC_SYM_TERROR;
    int64_t now = utc_seconds();
    return now ? lbm_enc_i64(now) : ENC_SYM_NIL;
}
static lbm_value time_status(lbm_value *args, lbm_uint n)
{
    (void)args;
    return n ? ENC_SYM_TERROR : symbol(utc_seconds() ? "set" : "unset");
}
static lbm_value str_from_n(lbm_value *args, lbm_uint n)
{
    if ((n != 1 && n != 2) || !lbm_is_number(args[0]))
        return ENC_SYM_TERROR;
    char text[96];
    int length;
    lbm_type type = lbm_type_of_functional(args[0]);
    bool real = type == LBM_TYPE_FLOAT || type == LBM_TYPE_DOUBLE;
    if (n == 2) {
        lbm_type p = lbm_type_of_functional(args[1]);
        if ((p != LBM_TYPE_I && p != LBM_TYPE_U && p != LBM_TYPE_I32 && p != LBM_TYPE_U32) ||
            lbm_dec_as_i64(args[1]) < 0 || lbm_dec_as_u64(args[1]) > 6)
            return ENC_SYM_TERROR;
    }
    if (real || n == 2) {
        double value = lbm_dec_as_double(args[0]);
        if (!isfinite(value))
            return ENC_SYM_TERROR;
        length = n == 2 ? snprintf(text, sizeof(text), "%.*f", (int)lbm_dec_as_i32(args[1]), value)
                        : snprintf(text, sizeof(text), "%.6g", value);
    } else if (type == LBM_TYPE_U || type == LBM_TYPE_U32 || type == LBM_TYPE_U64)
        length = snprintf(text, sizeof(text), "%" PRIu64, lbm_dec_as_u64(args[0]));
    else
        length = snprintf(text, sizeof(text), "%" PRId64, lbm_dec_as_i64(args[0]));
    if (length < 0 || (size_t)length >= sizeof(text))
        return ENC_SYM_TERROR;
    return copy_string(text, (size_t)length);
}
static lbm_value str_join(lbm_value *args, lbm_uint n)
{
    if (n != 1 && n != 2)
        return ENC_SYM_TERROR;
    char *separator = "";
    size_t sep_size = 0;
    if (n == 2 && !string(args[1], &separator, &sep_size))
        return ENC_SYM_TERROR;
    if (sep_size > LISP_JSON_STRING_MAX)
        return ENC_SYM_TERROR;
    lbm_value list = args[0];
    size_t total = 0;
    unsigned count = 0;
    while (lbm_is_cons(list)) {
        char *part;
        size_t size;
        if (++count > 64 || !string(lbm_car(list), &part, &size) || size > LISP_JSON_STRING_MAX)
            return ENC_SYM_TERROR;
        total += size + (count > 1 ? sep_size : 0);
        if (total > LISP_JSON_STRING_MAX)
            return ENC_SYM_TERROR;
        list = lbm_cdr(list);
    }
    if (list != ENC_SYM_NIL)
        return ENC_SYM_TERROR;
    size_t offset = 0;
    list = args[0];
    for (unsigned i = 0; i < count; i++, list = lbm_cdr(list)) {
        char *part;
        size_t size;
        (void)string(lbm_car(list), &part, &size);
        if (i) {
            memcpy(work->text + offset, separator, sep_size);
            offset += sep_size;
        }
        memcpy(work->text + offset, part, size);
        offset += size;
    }
    return copy_string(work->text, total);
}
static lbm_value json_value(const char *source, uint16_t ix)
{
    const lisp_json_token_t *t = &work->json.tokens[ix];
    switch (t->type) {
    case LISP_JSON_STRING: {
        size_t n;
        if (!lisp_json_string(source, t, work->text, sizeof(work->text), &n))
            return ENC_SYM_TERROR;
        return copy_string(work->text, n);
    }
    case LISP_JSON_INTEGER:
    case LISP_JSON_NUMBER:
        memcpy(work->text, source + t->start, t->length);
        work->text[t->length] = 0;
        return t->type == LISP_JSON_INTEGER ? lbm_enc_i64(strtoll(work->text, NULL, 10))
                                            : lbm_enc_double(strtod(work->text, NULL));
    case LISP_JSON_TRUE:
        return symbol("json-true");
    case LISP_JSON_FALSE:
        return symbol("json-false");
    case LISP_JSON_NULL:
        return symbol("json-null");
    case LISP_JSON_OBJECT:
    case LISP_JSON_ARRAY: {
        lbm_value list = ENC_SYM_NIL;
        uint16_t child = t->child;
        while (child != LISP_JSON_NONE) {
            lbm_value entry = work->values[child];
            child = work->json.tokens[child].next;
            if (t->type == LISP_JSON_OBJECT) {
                if (child == LISP_JSON_NONE)
                    return ENC_SYM_TERROR;
                entry = lbm_cons(entry, work->values[child]);
                child = work->json.tokens[child].next;
                if (lbm_is_error(entry))
                    return entry;
            }
            list = lbm_cons(entry, list);
            if (lbm_is_error(list))
                return list;
        }
        /* Objects stay reversed: last duplicate decoded key wins. */
        if (t->type == LISP_JSON_ARRAY)
            list = lbm_list_destructive_reverse(list);
        return lbm_cons(symbol(t->type == LISP_JSON_OBJECT ? "json-object" : "json-array"), list);
    }
    }
    return ENC_SYM_TERROR;
}
static lbm_value json_parse(lbm_value *args, lbm_uint n)
{
    char *source;
    size_t size;
    if (n != 1 || !string(args[0], &source, &size))
        return ENC_SYM_TERROR;
    const char *error = lisp_json_parse(&work->json, source, size);
    if (error)
        return pair(symbol("error"), symbol(error));
    /* These upstream allocators do NOT collect. No C-held value survives a GC:
     * return MERROR for evaluator GC/retry rather than collecting inside here. */
    for (unsigned i = work->json.count; i-- > 0;) {
        work->values[i] = json_value(source, (uint16_t)i);
        if (lbm_is_error(work->values[i]))
            return ENC_SYM_MERROR;
    }
    return pair(symbol("ok"), work->values[0]);
}
static lbm_value json_get(lbm_value *args, lbm_uint n)
{
    if (n != 2 || !lbm_is_cons(args[0]))
        return ENC_SYM_TERROR;
    lbm_value tag = lbm_car(args[0]), list = lbm_cdr(args[0]);
    bool object = tag == symbol("json-object");
    if (!object && tag != symbol("json-array"))
        return ENC_SYM_TERROR;
    char *key = NULL;
    size_t key_size = 0;
    uint64_t index = 0;
    if (object) {
        if (!string(args[1], &key, &key_size) || key_size > LISP_JSON_STRING_MAX)
            return ENC_SYM_TERROR;
    } else {
        lbm_type type = lbm_type_of_functional(args[1]);
        if (type != LBM_TYPE_I && type != LBM_TYPE_U && type != LBM_TYPE_I32 && type != LBM_TYPE_U32 &&
            type != LBM_TYPE_I64 && type != LBM_TYPE_U64)
            return ENC_SYM_TERROR;
        index = lbm_dec_as_u64(args[1]);
    }
    for (unsigned i = 0; i < LISP_JSON_NODES && lbm_is_cons(list); i++, list = lbm_cdr(list)) {
        lbm_value entry = lbm_car(list);
        if (!object) {
            if (index == i)
                return entry;
        } else {
            char *s;
            size_t size;
            if (!lbm_is_cons(entry) || !string(lbm_car(entry), &s, &size))
                return ENC_SYM_TERROR;
            if (size == key_size && !memcmp(s, key, size))
                return lbm_cdr(entry);
        }
    }
    return symbol("json-missing");
}
bool lisp_data_runtime_register(lisp_data_workspace_t *workspace)
{
    if (!workspace)
        return false;
    work = workspace;
    static const char *const names[] = {"ok",           "error",        "pending",   "json-object",
                                        "json-array",   "json-null",    "json-true", "json-false",
                                        "json-missing", "invalid-json", "too-large", "depth-limit",
                                        "node-limit",   "set",          "unset"};
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (lbm_is_error(symbol(names[i])))
            return false;
    for (unsigned i = 0; i <= LISP_HTTP_EXHAUSTED; i++)
        if (lbm_is_error(symbol(lisp_http_error_name((lisp_http_error_t)i))))
            return false;
    return lbm_add_extension("http-get", http_get) && lbm_add_extension("http-poll", http_poll) &&
           lbm_add_extension("http-cancel", http_cancel) && lbm_add_extension("http-release", http_release) &&
           lbm_add_extension("json-parse", json_parse) && lbm_add_extension("json-get", json_get) &&
           lbm_add_extension("monotonic-ms", monotonic_ms) && lbm_add_extension("time-unix", time_unix) &&
           lbm_add_extension("time-status", time_status) && lbm_add_extension("str-from-n", str_from_n) &&
           lbm_add_extension("str-join", str_join);
}
