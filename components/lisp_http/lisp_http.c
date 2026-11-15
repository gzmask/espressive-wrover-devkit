#include "lisp_http.h"
#include "lisp_app_url.h"
#include <string.h>
const char *lisp_http_error_name(lisp_http_error_t e)
{
    static const char *const names[] = {"ok",       "busy",         "unavailable", "offline",   "network",
                                        "timeout",  "cancelled",    "http-status", "too-large", "truncated",
                                        "encoding", "invalid-text", "clock",       "stale",     "exhausted"};
    return (unsigned)e < sizeof(names) / sizeof(names[0]) ? names[e] : "network";
}
static lisp_http_error_t check(const lisp_http_io_t *io, uint64_t deadline, unsigned *wait)
{
    if (io->cancelled(io->user))
        return LISP_HTTP_CANCELLED;
    uint64_t now = io->now_ms(io->user);
    if (now >= deadline)
        return LISP_HTTP_TIMEOUT;
    uint64_t remaining = deadline - now;
    *wait = remaining < LISP_HTTP_IO_MS ? (unsigned)remaining : LISP_HTTP_IO_MS;
    return LISP_HTTP_OK;
}
lisp_http_result_t lisp_http_download(const lisp_http_io_t *io, const char *url, char *buffer,
                                      size_t capacity, uint64_t deadline)
{
    lisp_http_result_t result = {.error = LISP_HTTP_NETWORK};
    if (!io || !io->now_ms || !io->cancelled || !io->open || !io->read || !io->complete || !io->close ||
        !buffer || capacity < LISP_HTTP_BODY_MAX + 1 || !url || !lisp_app_url_valid(url, strlen(url)))
        return result;
    buffer[0] = 0;
    unsigned wait;
    result.error = check(io, deadline, &wait);
    if (result.error != LISP_HTTP_OK)
        return result;
    lisp_http_headers_t h = {0};
    bool opened = io->open(io->user, url, wait, &h);
    result.status = h.status;
    result.error = opened ? LISP_HTTP_OK : LISP_HTTP_NETWORK;
    if (opened) {
        if (h.status < 200 || h.status >= 300)
            result.error = LISP_HTTP_STATUS;
        else if (h.encoded)
            result.error = LISP_HTTP_ENCODING;
        else if (h.length > LISP_HTTP_BODY_MAX)
            result.error = LISP_HTTP_TOO_LARGE;
        else if (h.length < -1)
            result.error = LISP_HTTP_NETWORK;
    }
    size_t used = 0;
    while (result.error == LISP_HTTP_OK) {
        result.error = check(io, deadline, &wait);
        if (result.error != LISP_HTTP_OK)
            break;
        size_t room = LISP_HTTP_BODY_MAX + 1 - used;
        if (room > 512)
            room = 512;
        int n = io->read(io->user, buffer + used, room, wait);
        if (n < 0 || (size_t)n > room) {
            result.error = LISP_HTTP_NETWORK;
            break;
        }
        if (!n) {
            if (!io->complete(io->user) || (h.length >= 0 && (uint64_t)h.length != used))
                result.error = LISP_HTTP_TRUNCATED;
            break;
        }
        if (memchr(buffer + used, 0, (size_t)n)) {
            result.error = LISP_HTTP_TEXT;
            break;
        }
        used += (size_t)n;
        if (used > LISP_HTTP_BODY_MAX)
            result.error = LISP_HTTP_TOO_LARGE;
        else if (h.length >= 0 && used > (uint64_t)h.length)
            result.error = LISP_HTTP_TRUNCATED;
    }
    io->close(io->user); /* Also cleans partially opened clients. */
    lisp_http_error_t late = check(io, deadline, &wait);
    if (late != LISP_HTTP_OK)
        result.error = late;
    if (result.error == LISP_HTTP_OK) {
        buffer[used] = 0;
        result.body = buffer;
        result.length = used;
    } else
        buffer[0] = 0;
    return result;
}
bool lisp_http_slot_init(lisp_http_slot_t *s, char *buffer, size_t capacity)
{
    if (!s || !buffer || capacity < LISP_HTTP_BODY_MAX + 1)
        return false;
    *s = (lisp_http_slot_t){.buffer = buffer, .capacity = capacity};
    return true;
}
lisp_http_error_t lisp_http_slot_request(lisp_http_slot_t *s, const char *url, size_t length, uint32_t *id)
{
    if (!s || !id || !url || !lisp_app_url_valid(url, length))
        return LISP_HTTP_TEXT;
    if (s->phase != LISP_HTTP_IDLE)
        return LISP_HTTP_BUSY;
    if (s->sequence == LISP_HTTP_ID_MAX)
        return LISP_HTTP_EXHAUSTED;
    memcpy(s->url, url, length);
    s->url[length] = 0;
    s->id = ++s->sequence;
    s->cancelled = s->abandoned = false;
    s->result = (lisp_http_result_t){0};
    s->phase = LISP_HTTP_FETCHING;
    *id = s->id;
    return LISP_HTTP_OK;
}
lisp_http_phase_t lisp_http_slot_poll(lisp_http_slot_t *s, uint32_t id, lisp_http_result_t *result)
{
    if (!s || !id || id != s->id || s->abandoned || s->phase == LISP_HTTP_IDLE) {
        if (result)
            *result = (lisp_http_result_t){.error = LISP_HTTP_STALE};
        return LISP_HTTP_IDLE;
    }
    if (result && s->phase == LISP_HTTP_READY)
        *result = s->result;
    return s->phase;
}
bool lisp_http_slot_cancel(lisp_http_slot_t *s, uint32_t id)
{
    if (!s || !id || id != s->id || s->abandoned || s->phase == LISP_HTTP_IDLE)
        return false;
    s->cancelled = true;
    if (s->phase == LISP_HTTP_READY)
        s->result = (lisp_http_result_t){.error = LISP_HTTP_CANCELLED};
    return true;
}
bool lisp_http_slot_release(lisp_http_slot_t *s, uint32_t id)
{
    if (!lisp_http_slot_cancel(s, id))
        return false;
    if (s->phase == LISP_HTTP_FETCHING)
        s->abandoned = true;
    else {
        s->phase = LISP_HTTP_IDLE;
        s->buffer[0] = 0;
        memset(s->url, 0, sizeof(s->url));
        s->result = (lisp_http_result_t){0};
    }
    return true;
}
void lisp_http_slot_finish(lisp_http_slot_t *s, lisp_http_result_t result)
{
    if (!s || s->phase != LISP_HTTP_FETCHING)
        return;
    memset(s->url, 0, sizeof(s->url));
    if (s->abandoned) {
        s->buffer[0] = 0;
        s->phase = LISP_HTTP_IDLE;
        return;
    }
    if (s->cancelled)
        result = (lisp_http_result_t){.error = LISP_HTTP_CANCELLED};
    if (result.error == LISP_HTTP_OK && (result.length > LISP_HTTP_BODY_MAX || result.body != s->buffer))
        result = (lisp_http_result_t){.error = LISP_HTTP_NETWORK};
    s->result = result;
    s->phase = LISP_HTTP_READY;
}
