#include "lisp_data_runtime.h"
#include "lisp_runtime.h"
#include "lisp_test_storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char output[16384], buffer[LISP_HTTP_BODY_MAX + 1];
static size_t used;
static uint64_t now;
static int64_t utc;
static lisp_http_slot_t slot;
static unsigned requests;
static void emit(void *u, const char *s, size_t n)
{
    (void)u;
    assert(used + n < sizeof(output));
    memcpy(output + used, s, n);
    used += n;
    output[used] = 0;
}
static void advance(unsigned n)
{
    for (unsigned i = 0; i < n; i++)
        lisp_runtime_step(now++);
}
static void eval(const char *s)
{
    used = 0;
    output[0] = 0;
    assert(lisp_runtime_submit(s, strlen(s), now));
    for (unsigned i = 0; i < 6000 && lisp_runtime_busy(); i++)
        advance(1);
    assert(!lisp_runtime_busy());
}
static void expect(const char *s, const char *wanted)
{
    eval(s);
    if (!strstr(output, wanted))
        fprintf(stderr, "%s -> %s (wanted %s)\n", s, output, wanted);
    assert(strstr(output, wanted));
}
static lisp_http_error_t request(void *u, const char *url, size_t n, uint32_t *id)
{
    (void)u;
    requests++;
    return lisp_http_slot_request(&slot, url, n, id);
}
static lisp_http_phase_t poll(void *u, uint32_t id, lisp_http_result_t *r)
{
    (void)u;
    return lisp_http_slot_poll(&slot, id, r);
}
static bool cancel(void *u, uint32_t id)
{
    (void)u;
    return lisp_http_slot_cancel(&slot, id);
}
static bool release(void *u, uint32_t id)
{
    (void)u;
    return lisp_http_slot_release(&slot, id);
}
static int64_t unix_seconds(void *u)
{
    (void)u;
    return utc;
}
static const lisp_http_backend_t backend = {NULL, request, poll, cancel, release};
static uint16_t pixels[320 * 240];
static lisp_app_display_state_t display_status = LISP_APP_DISPLAY_TERMINAL;
static bool begin(void *u)
{
    (void)u;
    display_status = LISP_APP_DISPLAY_DRAWABLE;
    return true;
}
static uint16_t *draw(void *u)
{
    (void)u;
    return pixels;
}
static bool publish(void *u)
{
    (void)u;
    return true;
}
static void activate(void *u)
{
    (void)u;
    display_status = LISP_APP_DISPLAY_DRAWABLE;
}
static void restore(void *u)
{
    (void)u;
    display_status = LISP_APP_DISPLAY_TERMINAL;
}
static lisp_app_display_state_t state(void *u)
{
    (void)u;
    return display_status;
}
static const lisp_app_display_t display = {NULL, begin, draw, publish, activate, restore, state};
static void complete(const char *s, lisp_http_error_t error, int status)
{
    strcpy(buffer, s);
    lisp_http_slot_finish(&slot, (lisp_http_result_t){error, status, buffer, strlen(s)});
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    size_t size = lisp_runtime_storage_size();
    void *storage = lisp_test_allocate(size);
    assert(storage && lisp_runtime_init(storage, size, emit, NULL));
    assert(lbm_get_num_extensions() == 33 && lbm_get_max_extensions() == 48);
    assert(lisp_http_slot_init(&slot, buffer, sizeof(buffer)));
    assert(lisp_runtime_time_attach(unix_seconds, NULL));
    if (!strcmp(argv[1], "fixture")) {
        FILE *file = fopen(OPEN_METEO_FIXTURE, "rb");
        assert(file);
        size_t length = fread(buffer, 1, sizeof(buffer), file);
        assert(!ferror(file) && feof(file) && fclose(file) == 0 && length < sizeof(buffer));
        lbm_value fixture;
        assert(lbm_create_array(&fixture, (lbm_uint)(length + 1)));
        memcpy(lbm_dec_str(fixture), buffer, length);
        lbm_dec_str(fixture)[length] = 0;
        assert(lbm_define("fixture", fixture));
        expect("(define parsed (json-parse fixture))", "=>");
        expect("(ix parsed 0)", "=> ok");
        expect("(define data (ix parsed 1))", "=>");
        expect("(json-get (json-get data \"current\") \"temperature_2m\")", "12.5");
        expect("(json-get (json-get (json-get data \"hourly\") \"precipitation_probability\") 18)", "80");
        expect("(str-join (list (str-from-n (json-get (json-get data \"current\") \"temperature_2m\") 1) "
               "\"C\") \" \")",
               "12.5 C");
    } else if (!strcmp(argv[1], "strings")) {
        expect("(str-from-n -12.25 1)", "-12.2");
        expect("(str-from-n 12)", "\"12\"");
        expect("(str-from-n 12 2)", "\"12.00\"");
        expect("(str-from-n 9223372036854775807i64)", "9223372036854775807");
        expect("(str-from-n 18446744073709551615u64)", "18446744073709551615");
        expect("(str-join (list (str-from-n 12.5 1) \"C\") \" \")", "\"12.5 C\"");
        expect("(eq (str-join nil) \"\")", "=> t");
        expect("(str-from-n 1 7)", "ERROR:");
        expect("(str-from-n 1 \"%n\")", "ERROR:");
        expect("(str-join (list \"a\" 1))", "ERROR:");
        expect("(str-join (cons \"a\" 1))", "ERROR:");
        expect("(progn (define cycle (list \"x\")) (setcdr cycle cycle) (str-join cycle))", "ERROR:");
    } else if (!strcmp(argv[1], "json")) {
        expect("(define parsed (json-parse "
               "\"{\\\"current\\\":{\\\"temperature\\\":12.5},\\\"a\\\":[null,false,true],\\\"empty\\\":[],"
               "\\\"obj\\\":{}}\"))",
               "=>");
        expect("(define data (ix parsed 1))", "=>");
        expect("(json-get (json-get data \"current\") \"temperature\")", "12.5");
        expect("(json-get (json-get data \"a\") 0)", "json-null");
        expect("(json-get (json-get data \"a\") 1)", "json-false");
        expect("(json-get (json-get data \"a\") 2)", "json-true");
        expect("(json-get (json-get data \"a\") 3)", "json-missing");
        expect("(json-get data \"missing\")", "json-missing");
        expect("(json-get data \"empty\")", "(json-array)");
        expect("(json-get data \"obj\")", "(json-object)");
        expect("(json-get (ix (json-parse \"{\\\"x\\\":1,\\\"\\\\u0078\\\":2}\") 1) \"x\")", "=> 2");
        expect("(json-parse \"(define injected 1)\")", "(error invalid-json)");
        expect("(json-parse \"[1,]\")", "(error invalid-json)");
        expect("(json-parse \"\\\"\\\\u0000\\\"\")", "(error invalid-json)");
        expect("injected", "ERROR:");
        for (unsigned i = 0; i < 200; i++)
            expect("(json-get (ix (json-parse \"{\\\"t\\\":12.5}\") 1) \"t\")", "12.5");
    } else if (!strcmp(argv[1], "clock")) {
        expect("(time-unix)", "=> nil");
        expect("(time-status)", "=> unset");
        utc = 1794700800;
        expect("(time-unix)", "1794700800");
        expect("(time-status)", "=> set");
        now = UINT64_C(5000000000);
        expect("(> (monotonic-ms) 4294967295u64)", "=> t");
        expect("(time-unix 1)", "ERROR:");
    } else {
        expect("(http-get \"https://example.com/a\")", "(error unavailable)");
        assert(lisp_runtime_http_attach(&backend));
        expect("(define preserved 42)", "=>");
        expect("(http-get \"http://example.com/a\")", "ERROR:");
        assert(!requests);
        if (!strcmp(argv[1], "http")) {
            expect("(define h (http-get \"https://example.com/a\"))", "(ok 1)");
            expect("(http-get \"https://example.com/a\")", "(error busy)");
            expect("(http-poll 1)", "(pending)");
            complete("(define injected 99)", LISP_HTTP_OK, 200);
            expect("(define reply (http-poll 1))", "define injected 99");
            expect("(http-poll 1)", "define injected 99");
            expect("(http-release 1)", "=> t");
            memset(buffer, '?', sizeof(buffer));
            expect("reply", "define injected 99");
            expect("injected", "ERROR:");
            expect("(http-poll 1)", "(error stale 0)");
            expect("(http-get \"https://example.com/a\")", "(ok 2)");
            complete("", LISP_HTTP_STATUS, 429);
            expect("(http-poll 2)", "(error http-status 429)");
            expect("(http-release 2)", "=> t");
            expect("preserved", "=> 42");
        } else if (!strcmp(argv[1], "cancel")) {
            expect("(http-get \"https://example.com/a\")", "(ok 1)");
            expect("(http-cancel 1)", "=> t");
            expect("(http-poll 1)", "(pending)");
            complete("late", LISP_HTTP_OK, 200);
            expect("(http-poll 1)", "(error cancelled 0)");
            expect("(http-release 1)", "=> t");
            expect("(http-get \"https://example.com/a\")", "(ok 2)");
            expect("(http-release 1)", "=> nil");
            assert(!slot.cancelled);
        } else if (!strcmp(argv[1], "reset")) {
            expect("(http-get \"https://example.com/a\")", "(ok 1)");
            assert(lisp_runtime_reset());
            assert(slot.cancelled && slot.abandoned && slot.phase == LISP_HTTP_FETCHING);
            expect("(http-get \"https://example.com/a\")", "(error busy)");
            complete("late", LISP_HTTP_OK, 200);
            expect("(http-get \"https://example.com/a\")", "(ok 2)");
            assert(requests == 3);
            expect("(http-poll 1)", "(error stale 0)");
        } else if (!strcmp(argv[1], "oom")) {
            lbm_value url;
            const char *address = "https://example.com/a";
            assert(lbm_create_array(&url, (lbm_uint)(strlen(address) + 1)));
            strcpy(lbm_dec_str(url), address);
            assert(lbm_define("oom-url", url));
            lbm_uint symbol_id;
            assert(lbm_get_symbol_by_name("http-get", &symbol_id));
            extension_fptr get = lbm_get_extension(symbol_id);
            while (lbm_heap_num_free())
                (void)lbm_cons(ENC_SYM_NIL, ENC_SYM_NIL);
            assert(get(&url, 1) == ENC_SYM_MERROR && requests == 0);
            assert(lbm_perform_gc());
            assert(!lbm_is_error(get(&url, 1)) && requests == 1);
            complete("copied", LISP_HTTP_OK, 200);
            assert(lbm_get_symbol_by_name("http-poll", &symbol_id));
            extension_fptr take = lbm_get_extension(symbol_id);
            lbm_value id = lbm_enc_i(1);
            while (lbm_heap_num_free())
                (void)lbm_cons(ENC_SYM_NIL, ENC_SYM_NIL);
            assert(take(&id, 1) == ENC_SYM_MERROR && slot.phase == LISP_HTTP_READY);
            assert(lbm_perform_gc());
            assert(!lbm_is_error(take(&id, 1)) && requests == 1 && slot.phase == LISP_HTTP_READY);
        } else if (!strcmp(argv[1], "app") || !strcmp(argv[1], "app-error")) {
            assert(lisp_runtime_app_attach(&display));
            expect("(define h nil) (define got nil)", "=>");
            expect("(define weather (list (lambda () (setq h (http-get \"https://example.com/a\"))) (lambda "
                   "(e) (if (= e 113) 'exit (progn (setq got (http-poll (ix h 1))) t))) (lambda () t)))",
                   "=>");
            expect("(app-run weather)", "=> t");
            advance(40);
            assert(requests == 1 && lisp_runtime_app_active());
            bool network_error = !strcmp(argv[1], "app-error");
            complete(network_error ? "" : "{\"t\":12}", network_error ? LISP_HTTP_STATUS : LISP_HTTP_OK,
                     network_error ? 503 : 200);
            advance(500);
            assert(lisp_runtime_app_active());
            assert(lisp_runtime_app_key('q'));
            advance(50);
            assert(!lisp_runtime_app_active() && slot.phase == LISP_HTTP_IDLE);
            expect("got", network_error ? "(error http-status 503)" : "{\\\"t\\\":12}");
            expect("(app-run weather)", "=> t");
            advance(50);
            assert(requests == 2 && lisp_runtime_app_active());
            lisp_runtime_app_stop();
            advance(50);
            assert(slot.cancelled && slot.abandoned);
            complete("late", LISP_HTTP_OK, 200);
            assert(slot.phase == LISP_HTTP_IDLE);
        } else
            return 1;
    }
    lisp_data_runtime_reset();
    lisp_test_free(storage, size);
    puts("PASS: Lisp data APIs");
    return 0;
}
