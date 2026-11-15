#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum {
    LISP_HTTP_BODY_MAX = 8192,
    LISP_HTTP_TIMEOUT_MS = 30000,
    LISP_HTTP_IO_MS = 3000,
    LISP_HTTP_ID_MAX = 0xffffff
};
typedef enum {
    LISP_HTTP_OK,
    LISP_HTTP_BUSY,
    LISP_HTTP_UNAVAILABLE,
    LISP_HTTP_OFFLINE,
    LISP_HTTP_NETWORK,
    LISP_HTTP_TIMEOUT,
    LISP_HTTP_CANCELLED,
    LISP_HTTP_STATUS,
    LISP_HTTP_TOO_LARGE,
    LISP_HTTP_TRUNCATED,
    LISP_HTTP_ENCODING,
    LISP_HTTP_TEXT,
    LISP_HTTP_CLOCK,
    LISP_HTTP_STALE,
    LISP_HTTP_EXHAUSTED
} lisp_http_error_t;
const char *lisp_http_error_name(lisp_http_error_t error);
typedef struct {
    int status;
    int64_t length;
    bool encoded;
} lisp_http_headers_t;
typedef struct {
    void *user;
    uint64_t (*now_ms)(void *);
    bool (*cancelled)(void *);
    bool (*open)(void *, const char *, unsigned, lisp_http_headers_t *);
    int (*read)(void *, char *, size_t, unsigned);
    bool (*complete)(void *);
    void (*close)(void *);
} lisp_http_io_t;
typedef struct {
    lisp_http_error_t error;
    int status;
    const char *body;
    size_t length;
} lisp_http_result_t;
lisp_http_result_t lisp_http_download(const lisp_http_io_t *io, const char *url, char *buffer,
                                      size_t capacity, uint64_t deadline);
typedef enum { LISP_HTTP_IDLE, LISP_HTTP_FETCHING, LISP_HTTP_READY } lisp_http_phase_t;
/* Caller serializes metadata; worker owns URL/buffer while FETCHING.
 * Release/cancel never reuse an in-flight buffer. IDs never wrap. */
typedef struct {
    lisp_http_phase_t phase;
    bool cancelled, abandoned;
    uint32_t id, sequence;
    char url[513];
    char *buffer;
    size_t capacity;
    lisp_http_result_t result;
} lisp_http_slot_t;
bool lisp_http_slot_init(lisp_http_slot_t *slot, char *buffer, size_t capacity);
lisp_http_error_t lisp_http_slot_request(lisp_http_slot_t *slot, const char *url, size_t length,
                                         uint32_t *id);
lisp_http_phase_t lisp_http_slot_poll(lisp_http_slot_t *slot, uint32_t id, lisp_http_result_t *result);
bool lisp_http_slot_cancel(lisp_http_slot_t *slot, uint32_t id);
bool lisp_http_slot_release(lisp_http_slot_t *slot, uint32_t id);
void lisp_http_slot_finish(lisp_http_slot_t *slot, lisp_http_result_t result);
typedef struct {
    void *user;
    lisp_http_error_t (*request)(void *, const char *, size_t, uint32_t *);
    lisp_http_phase_t (*poll)(void *, uint32_t, lisp_http_result_t *);
    bool (*cancel)(void *, uint32_t);
    bool (*release)(void *, uint32_t);
} lisp_http_backend_t;
