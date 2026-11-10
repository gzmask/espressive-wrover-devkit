#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    LISP_APP_URL_CAPACITY = 513,
    LISP_APP_URL_SOURCE_LIMIT = 32768,
    LISP_APP_URL_SOURCE_CAPACITY = LISP_APP_URL_SOURCE_LIMIT + 1,
    LISP_APP_URL_TIMEOUT_MS = 30000,
    LISP_APP_URL_IO_TIMEOUT_MS = 3000,
};
bool lisp_app_url_valid(const char *url, size_t length);
typedef struct {
    int status;
    int64_t length; /* -1: chunked/unknown length; never trust this as completion. */
    bool encoded;
} lisp_app_url_response_t;
typedef struct {
    void *user;
    uint64_t (*now_ms)(void *user);
    bool (*cancelled)(void *user);
    bool (*open)(void *user, const char *url, unsigned timeout_ms, lisp_app_url_response_t *response);
    int (*read)(void *user, char *buffer, size_t capacity, unsigned timeout_ms);
    bool (*complete)(void *user);
    void (*close)(void *user); /* Also called after a partially failed open. */
} lisp_app_url_io_t;
/* No evaluation until HTTP200, complete body and all byte/size checks succeed.
 * Transport must verify TLS and disallow redirects/auth/retries. Never logs URL. */
bool lisp_app_url_download(const lisp_app_url_io_t *io, const char *url, char *buffer, size_t capacity,
                           size_t *length, uint64_t deadline_ms);

typedef enum {
    LISP_APP_URL_IDLE,
    LISP_APP_URL_FETCHING,
    LISP_APP_URL_READY,
    LISP_APP_URL_FAILED,
    LISP_APP_URL_LEASED,
} lisp_app_url_phase_t;
/* Caller serializes metadata. Only worker writes bytes while FETCHING. Cancel
 * cannot release/reuse a worker-owned buffer. No task-stack pointers cross. */
typedef struct {
    lisp_app_url_phase_t phase;
    bool cancelled;
    char url[LISP_APP_URL_CAPACITY];
    char *source;
    size_t capacity, length;
} lisp_app_url_slot_t;
bool lisp_app_url_slot_init(lisp_app_url_slot_t *slot, char *buffer, size_t capacity);
bool lisp_app_url_slot_request(lisp_app_url_slot_t *slot, const char *url, size_t length);
void lisp_app_url_slot_cancel(lisp_app_url_slot_t *slot);
void lisp_app_url_slot_finish(lisp_app_url_slot_t *slot, bool success, size_t length);
lisp_app_url_phase_t lisp_app_url_slot_poll(lisp_app_url_slot_t *slot, const char **source, size_t *length);
void lisp_app_url_slot_release(lisp_app_url_slot_t *slot);
/* Runtime calls on original Lisp owner only; backend worker never calls LispBM.
 * poll READY leases bytes until release; runtime copies before releasing. */
typedef struct {
    void *user;
    bool (*request)(void *user, const char *url, size_t length);
    lisp_app_url_phase_t (*poll)(void *user, const char **source, size_t *length);
    void (*cancel)(void *user);
    void (*release)(void *user);
} lisp_app_url_backend_t;
