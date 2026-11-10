#include "lisp_app_url.h"
#include <string.h>

static bool alphanumeric(unsigned char byte)
{
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9');
}
bool lisp_app_url_valid(const char *url, size_t length)
{
    if (!url || length < 11 || length >= LISP_APP_URL_CAPACITY || memcmp(url, "https://", 8))
        return false;
    size_t slash = 8;
    for (size_t i = 8; i < length; i++) {
        unsigned char byte = (unsigned char)url[i];
        if (byte <= 32 || byte >= 127 || byte == '\\' || byte == '#' || byte == '@')
            return false;
        if (url[i] == '/' && slash == 8)
            slash = i;
    }
    if (slash == 8)
        return false;
    size_t host_end = slash;
    for (size_t i = 8; i < slash; i++)
        if (url[i] == ':') {
            if (slash - i != 4 || memcmp(url + i, ":443", 4))
                return false;
            host_end = i;
            break;
        }
    if (host_end <= 8 || host_end - 8 > 253)
        return false;
    size_t label = 0;
    for (size_t i = 8; i < host_end; i++) {
        unsigned char byte = (unsigned char)url[i];
        if (byte == '.') {
            if (!label || !alphanumeric((unsigned char)url[i - 1]))
                return false;
            label = 0;
        } else {
            if ((!alphanumeric(byte) && byte != '-') || (!label && byte == '-') || ++label > 63)
                return false;
        }
    }
    return label && alphanumeric((unsigned char)url[host_end - 1]);
}
static unsigned timeout(const lisp_app_url_io_t *io, uint64_t deadline)
{
    uint64_t now = io->now_ms(io->user);
    if (now >= deadline || io->cancelled(io->user))
        return 0;
    uint64_t left = deadline - now;
    return left < LISP_APP_URL_IO_TIMEOUT_MS ? (unsigned)left : LISP_APP_URL_IO_TIMEOUT_MS;
}
bool lisp_app_url_download(const lisp_app_url_io_t *io, const char *url, char *buffer, size_t capacity,
                           size_t *length, uint64_t deadline_ms)
{
    if (!length)
        return false;
    *length = 0;
    if (!io || !io->now_ms || !io->cancelled || !io->open || !io->read || !io->complete || !io->close ||
        !url || !lisp_app_url_valid(url, strlen(url)) || !buffer || capacity < LISP_APP_URL_SOURCE_CAPACITY)
        return false;
    buffer[0] = 0;
    unsigned wait = timeout(io, deadline_ms);
    if (!wait)
        return false;
    lisp_app_url_response_t response = {0};
    bool success = io->open(io->user, url, wait, &response);
    size_t used = 0;
    success = success && response.status == 200 && !response.encoded && response.length >= -1 &&
              response.length <= LISP_APP_URL_SOURCE_LIMIT;
    while (success) {
        wait = timeout(io, deadline_ms);
        if (!wait) {
            success = false;
            break;
        }
        /* One extra byte detects overflow even for unknown/chunked bodies. */
        size_t room = LISP_APP_URL_SOURCE_CAPACITY - used;
        if (room > 512)
            room = 512;
        int count = io->read(io->user, buffer + used, room, wait);
        if (count < 0 || (size_t)count > room) {
            success = false;
            break;
        }
        if (count == 0) {
            success = used != 0 && io->complete(io->user) &&
                      (response.length < 0 || (uint64_t)response.length == used) && timeout(io, deadline_ms);
            break;
        }
        for (int i = 0; i < count; i++) {
            unsigned char byte = (unsigned char)buffer[used + (size_t)i];
            if ((byte < 32 && byte != '\t' && byte != '\r' && byte != '\n') || byte > 126)
                success = false;
        }
        used += (size_t)count;
        if (used > LISP_APP_URL_SOURCE_LIMIT || (response.length >= 0 && used > (uint64_t)response.length))
            success = false;
    }
    io->close(io->user);
    buffer[success ? used : 0] = 0;
    if (success)
        *length = used;
    return success;
}
bool lisp_app_url_slot_init(lisp_app_url_slot_t *slot, char *buffer, size_t capacity)
{
    if (!slot || !buffer || capacity < LISP_APP_URL_SOURCE_CAPACITY)
        return false;
    *slot = (lisp_app_url_slot_t){.source = buffer, .capacity = capacity};
    return true;
}
bool lisp_app_url_slot_request(lisp_app_url_slot_t *slot, const char *url, size_t length)
{
    if (!slot || !slot->source || slot->phase != LISP_APP_URL_IDLE || !lisp_app_url_valid(url, length))
        return false;
    memcpy(slot->url, url, length);
    slot->url[length] = 0;
    slot->length = 0;
    slot->cancelled = false;
    slot->phase = LISP_APP_URL_FETCHING;
    return true;
}
void lisp_app_url_slot_cancel(lisp_app_url_slot_t *slot)
{
    slot->cancelled = true;
    if (slot->phase == LISP_APP_URL_READY || slot->phase == LISP_APP_URL_FAILED)
        slot->phase = LISP_APP_URL_IDLE;
}
void lisp_app_url_slot_finish(lisp_app_url_slot_t *slot, bool success, size_t length)
{
    if (slot->phase != LISP_APP_URL_FETCHING)
        return;
    slot->length = length;
    slot->phase = slot->cancelled                                            ? LISP_APP_URL_IDLE
                  : success && length && length <= LISP_APP_URL_SOURCE_LIMIT ? LISP_APP_URL_READY
                                                                             : LISP_APP_URL_FAILED;
}
lisp_app_url_phase_t lisp_app_url_slot_poll(lisp_app_url_slot_t *slot, const char **source, size_t *length)
{
    lisp_app_url_phase_t result = slot->phase;
    if (result == LISP_APP_URL_READY) {
        *source = slot->source;
        *length = slot->length;
        slot->phase = LISP_APP_URL_LEASED;
    } else if (result == LISP_APP_URL_FAILED)
        slot->phase = LISP_APP_URL_IDLE;
    return result;
}
void lisp_app_url_slot_release(lisp_app_url_slot_t *slot)
{
    if (slot->phase == LISP_APP_URL_LEASED)
        slot->phase = LISP_APP_URL_IDLE;
}
