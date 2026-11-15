#include "lisp_http.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char body[LISP_HTTP_BODY_MAX + 2], buffer[LISP_HTTP_BODY_MAX + 1];
static size_t size, position;
static uint64_t now;
static int status, read_error;
static int64_t declared;
static bool complete, encoded, cancelled, open_ok;
static unsigned closes;
static bool cancel_on_read;
static uint64_t advance_on_read;
static uint64_t clock_ms(void *u)
{
    (void)u;
    return now++;
}
static bool is_cancelled(void *u)
{
    (void)u;
    return cancelled;
}
static bool open_url(void *u, const char *url, unsigned wait, lisp_http_headers_t *h)
{
    (void)u;
    assert(url && wait && wait <= LISP_HTTP_IO_MS);
    *h = (lisp_http_headers_t){status, declared, encoded};
    return open_ok;
}
static int read_body(void *u, char *out, size_t cap, unsigned wait)
{
    (void)u;
    now += advance_on_read;
    if (cancel_on_read)
        cancelled = true;
    assert(wait && cap);
    if (read_error)
        return read_error;
    size_t n = size - position;
    if (n > cap)
        n = cap;
    if (n > 127)
        n = 127;
    memcpy(out, body + position, n);
    position += n;
    return (int)n;
}
static bool finished(void *u)
{
    (void)u;
    return complete;
}
static void close_url(void *u)
{
    (void)u;
    closes++;
}
static const lisp_http_io_t io = {NULL, clock_ms, is_cancelled, open_url, read_body, finished, close_url};
static void setup(void)
{
    memcpy(body, "{\"temperature\":12}", 18);
    size = 18;
    position = 0;
    now = 0;
    status = 200;
    declared = -1;
    complete = true;
    encoded = cancelled = false;
    open_ok = true;
    read_error = 0;
    closes = 0;
    cancel_on_read = false;
    advance_on_read = 0;
}
static lisp_http_result_t run(void)
{
    return lisp_http_download(&io, "https://example.com/weather", buffer, sizeof(buffer), 30000);
}
int main(void)
{
    setup();
    lisp_http_result_t r = run();
    assert(r.error == LISP_HTTP_OK && r.status == 200 && r.length == size && closes == 1);
    setup();
    status = 429;
    r = run();
    assert(r.error == LISP_HTTP_STATUS && r.status == 429 && !r.length);
    setup();
    status = 503;
    assert(run().error == LISP_HTTP_STATUS);
    setup();
    status = 302;
    assert(run().error == LISP_HTTP_STATUS);
    setup();
    encoded = true;
    assert(run().error == LISP_HTTP_ENCODING);
    setup();
    complete = false;
    assert(run().error == LISP_HTTP_TRUNCATED);
    setup();
    declared = 100;
    assert(run().error == LISP_HTTP_TRUNCATED);
    setup();
    declared = 1;
    assert(run().error == LISP_HTTP_TRUNCATED);
    setup();
    declared = LISP_HTTP_BODY_MAX + 1;
    assert(run().error == LISP_HTTP_TOO_LARGE);
    setup();
    memset(body, 'x', sizeof(body));
    size = LISP_HTTP_BODY_MAX;
    r = run();
    assert(r.error == LISP_HTTP_OK && r.length == LISP_HTTP_BODY_MAX && buffer[r.length] == 0);
    setup();
    memset(body, 'x', sizeof(body));
    size = LISP_HTTP_BODY_MAX + 1;
    assert(run().error == LISP_HTTP_TOO_LARGE);
    setup();
    body[2] = 0;
    assert(run().error == LISP_HTTP_TEXT);
    setup();
    size = 0;
    declared = 0;
    assert(run().error == LISP_HTTP_OK);
    setup();
    read_error = -1;
    assert(run().error == LISP_HTTP_NETWORK && closes == 1);
    setup();
    open_ok = false;
    assert(run().error == LISP_HTTP_NETWORK && closes == 1);
    setup();
    now = 30000;
    assert(run().error == LISP_HTTP_TIMEOUT && closes == 0);
    setup();
    cancelled = true;
    assert(run().error == LISP_HTTP_CANCELLED && closes == 0);
    setup();
    body[0] = (char)0xc3;
    body[1] = (char)0xa9;
    assert(run().error == LISP_HTTP_OK);
    setup();
    cancel_on_read = true;
    assert(run().error == LISP_HTTP_CANCELLED && closes == 1);
    setup();
    advance_on_read = 30000;
    assert(run().error == LISP_HTTP_TIMEOUT && closes == 1);
    lisp_http_slot_t slot;
    assert(lisp_http_slot_init(&slot, buffer, sizeof(buffer)));
    uint32_t first, second;
    assert(lisp_http_slot_request(&slot, "https://example.com/a", 21, &first) == LISP_HTTP_OK);
    assert(lisp_http_slot_request(&slot, "https://example.com/a", 21, &second) == LISP_HTTP_BUSY);
    assert(!lisp_http_slot_release(&slot, first + 1));
    assert(lisp_http_slot_cancel(&slot, first));
    assert(lisp_http_slot_poll(&slot, first, &r) == LISP_HTTP_FETCHING);
    assert(lisp_http_slot_release(&slot, first));
    assert(slot.phase == LISP_HTTP_FETCHING);
    lisp_http_slot_finish(&slot, (lisp_http_result_t){LISP_HTTP_OK, 200, buffer, 2});
    assert(slot.phase == LISP_HTTP_IDLE);
    assert(lisp_http_slot_request(&slot, "https://example.com/a", 21, &second) == LISP_HTTP_OK);
    assert(second != first && !lisp_http_slot_cancel(&slot, first));
    lisp_http_slot_finish(&slot, (lisp_http_result_t){LISP_HTTP_OK, 200, buffer, 2});
    assert(lisp_http_slot_poll(&slot, second, &r) == LISP_HTTP_READY && r.length == 2);
    assert(lisp_http_slot_poll(&slot, second, &r) == LISP_HTTP_READY);
    assert(lisp_http_slot_release(&slot, second));
    assert(lisp_http_slot_poll(&slot, second, &r) == LISP_HTTP_IDLE);
    slot.sequence = LISP_HTTP_ID_MAX;
    assert(lisp_http_slot_request(&slot, "https://example.com/a", 21, &second) == LISP_HTTP_EXHAUSTED);
    puts("PASS: bounded HTTP data and cancellation/late result/ID lifecycle");
}
