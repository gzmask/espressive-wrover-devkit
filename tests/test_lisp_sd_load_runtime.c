#include "board_sdcard_source.h"
#include "lisp_runtime.h"
#include "lisp_test_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static board_sdcard_source_slot_t slot;
static char buffer[BOARD_SDCARD_SOURCE_LIMIT + 1];
static lisp_app_url_slot_t url_slot;
static char url_buffer[LISP_APP_URL_SOURCE_CAPACITY];
static char output_text[16384];
static size_t used;
static unsigned requests, url_requests;
static uint64_t now;
static void output(void *user, const char *text, size_t length)
{
    (void)user;
    assert(length < sizeof(output_text) - used);
    memcpy(output_text + used, text, length);
    used += length;
    output_text[used] = 0;
}
static bool request(void *user, const char *name, size_t length)
{
    (void)user;
    assert(length == 8 && !memcmp(name, "APP.LISP", 8));
    requests++;
    return board_sdcard_source_slot_begin(&slot);
}
static lisp_app_url_phase_t poll(void *user, const char **source, size_t *length)
{
    (void)user;
    switch (board_sdcard_source_slot_poll(&slot, source, length)) {
    case BOARD_SOURCE_FETCHING:
        return LISP_APP_URL_FETCHING;
    case BOARD_SOURCE_READY:
        return LISP_APP_URL_READY;
    case BOARD_SOURCE_FAILED:
        return LISP_APP_URL_FAILED;
    case BOARD_SOURCE_LEASED:
        return LISP_APP_URL_LEASED;
    default:
        return LISP_APP_URL_IDLE;
    }
}
static void cancel(void *user)
{
    (void)user;
    board_sdcard_source_slot_cancel(&slot);
}
static void release(void *user)
{
    (void)user;
    board_sdcard_source_slot_release(&slot);
}
static bool url_request(void *user, const char *source, size_t length)
{
    (void)user;
    url_requests++;
    return lisp_app_url_slot_request(&url_slot, source, length);
}
static lisp_app_url_phase_t url_poll(void *user, const char **source, size_t *length)
{
    (void)user;
    return lisp_app_url_slot_poll(&url_slot, source, length);
}
static void url_cancel(void *user)
{
    (void)user;
    lisp_app_url_slot_cancel(&url_slot);
}
static void url_release(void *user)
{
    (void)user;
    lisp_app_url_slot_release(&url_slot);
}
static void advance(unsigned count)
{
    while (count--)
        lisp_runtime_step(now++);
}
static void evaluate(const char *source)
{
    used = 0;
    output_text[0] = 0;
    assert(lisp_runtime_submit(source, strlen(source), now));
    for (unsigned i = 0; i < 6000 && lisp_runtime_busy(); i++)
        advance(1);
    assert(!lisp_runtime_busy());
}
static void finish(void)
{
    for (unsigned i = 0; i < 6000 && lisp_runtime_load_active(); i++)
        advance(1);
    assert(!lisp_runtime_load_active());
}
int main(void)
{
    void *storage = lisp_test_allocate(lisp_runtime_storage_size());
    assert(storage && lisp_runtime_init(storage, lisp_runtime_storage_size(), output, NULL));
    assert(board_sdcard_source_slot_init(&slot, buffer, sizeof(buffer)));
    assert(lisp_app_url_slot_init(&url_slot, url_buffer, sizeof(url_buffer)));
    const lisp_app_url_backend_t sd = {NULL, request, poll, cancel, release};
    const lisp_app_url_backend_t url = {NULL, url_request, url_poll, url_cancel, url_release};
    assert(lisp_runtime_load_attach(&url));
    assert(lisp_runtime_load_attach_sd(&sd));
    evaluate("(define keep 42)");
    evaluate("(app-load-sd \"../bad\")");
    assert(strstr(output_text, "ERROR") && requests == 0);
    evaluate("(app-load-sd \"APP.LISP\")");
    assert(strstr(output_text, "=> t") && !strcmp(lisp_runtime_load_status(), "reading"));
    assert(!lisp_runtime_submit("(+ 1 2)", 7, now));
    memset(buffer, ' ', BOARD_SDCARD_SOURCE_LIMIT);
    memcpy(buffer, "(define from-sd 123)", 20);
    buffer[BOARD_SDCARD_SOURCE_LIMIT] = 0;
    board_sdcard_source_slot_finish(&slot, true, BOARD_SDCARD_SOURCE_LIMIT);
    advance(1);
    assert(slot.phase == BOARD_SOURCE_IDLE); /* runtime copied before releasing */
    memset(buffer, '?', sizeof(buffer));
    finish();
    assert(!strcmp(lisp_runtime_load_status(), "loaded"));
    evaluate("from-sd");
    assert(strstr(output_text, "=> 123"));
    evaluate("(app-load-sd \"APP.LISP\")");
    assert(slot.phase == BOARD_SOURCE_FETCHING);
    lisp_runtime_load_cancel();
    assert(slot.cancelled && slot.phase == BOARD_SOURCE_FETCHING);
    evaluate("keep");
    assert(strstr(output_text, "=> 42"));
    evaluate("(app-load-sd \"APP.LISP\")");
    assert(!strcmp(lisp_runtime_load_status(), "failed")); /* old worker owns buffer */
    board_sdcard_source_slot_finish(&slot, true, 20);
    assert(slot.phase == BOARD_SOURCE_IDLE);
    evaluate("(app-load-sd \"APP.LISP\")");
    board_sdcard_source_slot_finish(&slot, false, 0);
    finish();
    assert(!strcmp(lisp_runtime_load_status(), "failed"));
    evaluate("keep");
    assert(strstr(output_text, "=> 42"));
    evaluate("(app-load-sd \"APP.LISP\")");
    const char *nested = "(app-load-sd \"APP.LISP\")";
    memcpy(buffer, nested, strlen(nested) + 1);
    unsigned before = requests;
    board_sdcard_source_slot_finish(&slot, true, strlen(nested));
    finish();
    assert(!strcmp(lisp_runtime_load_status(), "failed") && requests == before);
    evaluate("(progn (app-load-sd \"APP.LISP\") (missing))");
    assert(!lisp_runtime_load_active() && requests == before);
    evaluate("(app-load-url \"https://example.com/app.lisp\")");
    assert(!strcmp(lisp_runtime_load_status(), "downloading") && url_requests == 1);
    const char *sd_while_url = "(app-load-sd \"APP.LISP\")";
    assert(!lisp_runtime_submit(sd_while_url, strlen(sd_while_url), now));
    assert(requests == before);
    strcpy(url_buffer, "(define from-url 55)");
    lisp_app_url_slot_finish(&url_slot, true, strlen(url_buffer));
    finish();
    evaluate("from-url");
    assert(strstr(output_text, "=> 55"));
    evaluate("(app-load-sd \"APP.LISP\")");
    const char *nested_url = "(app-load-url \"https://example.com/nested\")";
    size_t nested_url_length = strlen(nested_url);
    memcpy(buffer, nested_url, nested_url_length + 1);
    before = requests;
    board_sdcard_source_slot_finish(&slot, true, nested_url_length);
    finish();
    assert(requests == before && url_requests == 1 && !strcmp(lisp_runtime_load_status(), "failed"));
    evaluate("(app-load-sd \"APP.LISP\")");
    const char *forever = "(define forever (lambda () (forever))) (forever)";
    memcpy(buffer, forever, strlen(forever) + 1);
    board_sdcard_source_slot_finish(&slot, true, strlen(forever));
    advance(1);
    assert(lisp_runtime_busy() && lisp_runtime_load_active() && slot.phase == BOARD_SOURCE_IDLE);
    lisp_runtime_load_cancel();
    assert(lisp_runtime_ready() && !lisp_runtime_busy() && !lisp_runtime_load_active());
    assert(slot.phase == BOARD_SOURCE_IDLE); /* No SD buffer lease survives evaluation reset. */
    lisp_test_free(storage, lisp_runtime_storage_size());
    puts("PASS SD loader owner, 32KiB copy/lease, cancel/late worker and failure; mock backend");
    return 0;
}
