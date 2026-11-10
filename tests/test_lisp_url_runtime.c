#include "lisp_runtime.h"
#include "lisp_test_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static lisp_app_url_slot_t slot;
static char download_buffer[LISP_APP_URL_SOURCE_CAPACITY];
static char source_buffer[LISP_APP_URL_SOURCE_CAPACITY];
static char log_text[32768];
static size_t used;
static unsigned requests;
static bool began;
static bool display_begin(void *user)
{
    (void)user;
    began = true;
    return true;
}
static uint16_t *display_draw(void *user)
{
    (void)user;
    return NULL;
}
static bool display_publish(void *user)
{
    (void)user;
    return true;
}
static void display_noop(void *user)
{
    (void)user;
}
static lisp_app_display_state_t display_state(void *user)
{
    (void)user;
    return LISP_APP_DISPLAY_TERMINAL;
}
static const lisp_app_display_t display = {NULL,         display_begin, display_draw, display_publish,
                                           display_noop, display_noop,  display_state};
static uint64_t now;
static bool request(void *user, const char *url, size_t length)
{
    (void)user;
    requests++;
    return lisp_app_url_slot_request(&slot, url, length);
}
static lisp_app_url_phase_t poll(void *user, const char **source, size_t *length)
{
    (void)user;
    return lisp_app_url_slot_poll(&slot, source, length);
}
static void cancel(void *user)
{
    (void)user;
    lisp_app_url_slot_cancel(&slot);
}
static void release(void *user)
{
    (void)user;
    lisp_app_url_slot_release(&slot);
}
static const lisp_app_url_backend_t backend = {NULL, request, poll, cancel, release};
static void output(void *user, const char *text, size_t length)
{
    (void)user;
    assert(length < sizeof(log_text) - used);
    memcpy(log_text + used, text, length);
    used += length;
    log_text[used] = 0;
}
static void advance(unsigned steps)
{
    for (unsigned i = 0; i < steps; i++)
        lisp_runtime_step(now++);
}
static void evaluate(const char *source)
{
    used = 0;
    log_text[0] = 0;
    assert(lisp_runtime_submit(source, strlen(source), now));
    for (unsigned i = 0; i < 6000 && lisp_runtime_busy(); i++)
        advance(1);
    assert(!lisp_runtime_busy());
}
static void start(void)
{
    evaluate("(app-load-url \"https://example.com/app.lisp\")");
    assert(strstr(log_text, "=> t") && lisp_runtime_load_active());
    assert(!lisp_runtime_busy() && !strcmp(lisp_runtime_load_status(), "downloading"));
}
static void downloaded(const char *source)
{
    strcpy(download_buffer, source);
    lisp_app_url_slot_finish(&slot, true, strlen(source));
}
static void finish(void)
{
    for (unsigned i = 0; i < 6000 && lisp_runtime_load_active(); i++)
        advance(1);
    assert(!lisp_runtime_load_active() && !lisp_runtime_busy());
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    void *storage = lisp_test_allocate(lisp_runtime_storage_size());
    assert(storage && lisp_runtime_init(storage, lisp_runtime_storage_size(), output, NULL));
    assert(lisp_app_url_slot_init(&slot, download_buffer, sizeof(download_buffer)));
    if (!strcmp(argv[1], "disabled")) {
        evaluate("(app-load-url \"https://example.com/a\")");
        assert(strstr(log_text, "ERROR") && !requests && !lisp_runtime_load_active());
    } else {
        assert(lisp_runtime_load_attach(&backend));
        assert(lisp_runtime_app_attach(&display));
        if (!strcmp(argv[1], "load")) {
            start();
            assert(requests == 1);
            assert(!lisp_runtime_submit("3", 1, now));
            memset(source_buffer, ' ', 2048);
            strcpy(source_buffer + 2048, "(define from-url (lambda (x) (* x 2)))");
            downloaded(source_buffer);
            advance(1);
            assert(!strcmp(lisp_runtime_load_status(), "loading"));
            assert(slot.phase == LISP_APP_URL_IDLE); /* Copy made before releasing worker buffer. */
            memset(download_buffer, '?', sizeof(download_buffer));
            finish();
            assert(!strcmp(lisp_runtime_load_status(), "loaded"));
            assert(!lisp_runtime_app_active());
            evaluate("(from-url 21)");
            assert(strstr(log_text, "=> 42"));
        } else if (!strcmp(argv[1], "bounds")) {
            start();
            memset(source_buffer, ' ', LISP_APP_URL_SOURCE_LIMIT);
            memcpy(source_buffer, "(define max-source 7)", 21);
            source_buffer[LISP_APP_URL_SOURCE_LIMIT] = 0;
            downloaded(source_buffer);
            finish();
            assert(!strcmp(lisp_runtime_load_status(), "loaded"));
            evaluate("max-source");
            assert(strstr(log_text, "=> 7"));
            assert(!lisp_runtime_submit(source_buffer, strlen(source_buffer), now));
        } else if (!strcmp(argv[1], "validation")) {
            const char *invalid[] = {"(app-load-url 3)", "(app-load-url \"http://example.com/a\")",
                                     "(app-load-url \"https://user@example.com/a\")",
                                     "(app-load-url \"https://example.com/a\" 1)",
                                     "(app-load-url \"https://example.com/a\\x00x\")"};
            for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
                evaluate(invalid[i]);
                assert(strstr(log_text, "ERROR") && !requests);
            }
            evaluate("(progn (app-load-url \"https://example.com/a\") (missing))");
            assert(!requests && !lisp_runtime_load_active());
        } else if (!strcmp(argv[1], "error")) {
            start();
            downloaded("(define partial 99) (missing)");
            finish();
            assert(!strcmp(lisp_runtime_load_status(), "failed"));
            evaluate("partial");
            assert(strstr(log_text, "=> 99")); /* Not a rollback sandbox. */
            start();
            downloaded("(+ 1");
            finish();
            assert(!strcmp(lisp_runtime_load_status(), "failed"));
            evaluate("(+ 1 2)");
            assert(strstr(log_text, "=> 3"));
        } else if (!strcmp(argv[1], "cancel")) {
            evaluate("(define keep 42)");
            start();
            lisp_runtime_load_cancel();
            assert(!lisp_runtime_load_active() && slot.phase == LISP_APP_URL_FETCHING);
            evaluate("keep");
            assert(strstr(log_text, "=> 42"));
            evaluate("(app-load-url \"https://example.com/a\")");
            assert(!strcmp(lisp_runtime_load_status(), "failed"));
            downloaded("(define stale 123)");
            advance(20);
            evaluate("stale");
            assert(strstr(log_text, "ERROR"));
            start();
            downloaded("(define fresh 7)");
            finish();
            evaluate("fresh");
            assert(strstr(log_text, "=> 7"));
        } else if (!strcmp(argv[1], "timeout")) {
            start();
            advance(35010);
            assert(!lisp_runtime_load_active() && slot.cancelled);
            downloaded("(define stale 1)");
            start();
            downloaded("(define forever (lambda () (forever))) (forever)");
            finish();
            assert(strstr(log_text, "timeout"));
            evaluate("forever");
            assert(strstr(log_text, "ERROR"));
        } else if (!strcmp(argv[1], "nested")) {
            start();
            downloaded("(app-load-url \"https://example.com/nested\")");
            finish();
            assert(requests == 1 && !strcmp(lisp_runtime_load_status(), "failed"));
            start();
            downloaded("(app-run (list (lambda () t) (lambda (e) t) (lambda () t)))");
            finish();
            assert(!lisp_runtime_app_active() && !began && !strcmp(lisp_runtime_load_status(), "failed"));
        } else if (!strcmp(argv[1], "controls")) {
            char reply[128];
            assert(!lisp_runtime_load_control(":load-statusX", 13, reply, sizeof(reply)));
            assert(!lisp_runtime_load_control(NULL, 1, reply, sizeof(reply)));
            assert(!lisp_runtime_load_control(":load-status", 12, reply, 10));
            start();
            assert(lisp_runtime_load_control(":load-status", 12, reply, sizeof(reply)));
            assert(strstr(reply, "downloading"));
            assert(lisp_runtime_load_control(":load-cancel", 12, reply, sizeof(reply)));
            assert(strstr(reply, "preserved") && !lisp_runtime_load_active());
            downloaded("(define late 4)");
            start();
            downloaded("(define forever (lambda () (forever))) (forever)");
            advance(20);
            assert(lisp_runtime_load_control(":load-cancel", 12, reply, sizeof(reply)));
            assert(strstr(reply, "ALL RAM definitions cleared") && !lisp_runtime_busy());
            const char *queued = "(progn (app-load-url \"https://example.com/a\") "
                                 "(define forever (lambda () (forever))) (forever))";
            unsigned before = requests;
            assert(lisp_runtime_submit(queued, strlen(queued), now));
            advance(20);
            assert(!strcmp(lisp_runtime_load_status(), "queued") && lisp_runtime_busy());
            assert(lisp_runtime_load_control(":load-cancel", 12, reply, sizeof(reply)));
            assert(strstr(reply, "ALL RAM definitions cleared") && requests == before);
        } else if (!strcmp(argv[1], "background")) {
            start();
            downloaded("(define forever (lambda () (forever))) (spawn forever)");
            finish();
            assert(strstr(log_text, "detached contexts"));
            assert(!strcmp(lisp_runtime_load_status(), "failed"));
            evaluate("forever");
            assert(strstr(log_text, "ERROR"));
        } else if (!strcmp(argv[1], "reset")) {
            start();
            assert(lisp_runtime_reset());
            downloaded("(define stale 3)");
            advance(10);
            evaluate("stale");
            assert(strstr(log_text, "ERROR"));
            start();
            downloaded("(define forever (lambda () (forever))) (forever)");
            advance(20);
            assert(lisp_runtime_busy());
            lisp_runtime_load_cancel();
            assert(!lisp_runtime_busy() && !lisp_runtime_load_active());
            evaluate("forever");
            assert(strstr(log_text, "ERROR"));
        } else if (!strcmp(argv[1], "download-failure")) {
            evaluate("(define keep 9)");
            start();
            lisp_app_url_slot_finish(&slot, false, 0);
            finish();
            assert(!strcmp(lisp_runtime_load_status(), "failed"));
            evaluate("keep");
            assert(strstr(log_text, "=> 9"));
        } else if (!strcmp(argv[1], "repeat")) {
            for (unsigned i = 0; i < 50; i++) {
                start();
                downloaded("(define value 42)");
                finish();
            }
            evaluate("value");
            assert(strstr(log_text, "=> 42"));
        } else
            return 1;
    }
    lisp_test_free(storage, lisp_runtime_storage_size());
    puts("PASS");
    return 0;
}
