#include "lisp_load_runtime.h"
#include "lispbm.h"
#include <stdio.h>
#include <string.h>

typedef enum { LOAD_IDLE, LOAD_QUEUED, LOAD_FETCHING, LOAD_EVALUATING } load_phase_t;
static load_phase_t phase;
static lisp_app_url_backend_t backend;
static bool attached;
static char pending_url[LISP_APP_URL_CAPACITY];
static size_t pending_length;
static const char *status = "idle";
static uint64_t started_ms;

bool lisp_runtime_load_attach(const lisp_app_url_backend_t *transport)
{
    if (phase != LOAD_IDLE || !transport || !transport->request || !transport->poll || !transport->cancel ||
        !transport->release)
        return false;
    backend = *transport;
    attached = true;
    return true;
}
bool lisp_runtime_load_active(void)
{
    return phase != LOAD_IDLE;
}
const char *lisp_runtime_load_status(void)
{
    return status;
}
void lisp_load_runtime_reset(void)
{
    if (phase != LOAD_IDLE) {
        if (attached)
            backend.cancel(backend.user);
        phase = LOAD_IDLE;
        status = "cancelled";
    } else
        status = "idle";
}
void lisp_load_runtime_failed(void)
{
    lisp_load_runtime_reset();
    status = "failed";
}
void lisp_runtime_load_cancel(void)
{
    if (phase == LOAD_EVALUATING || (phase == LOAD_QUEUED && lisp_runtime_busy())) {
        /* Stopping already-running top-level code cannot roll back definitions.
         * Reset all RAM contexts/definitions, exactly like evaluation timeout. */
        (void)lisp_runtime_reset();
    } else if (phase != LOAD_IDLE)
        lisp_load_runtime_reset();
}
bool lisp_runtime_load_control(const char *source, size_t length, char *reply, size_t capacity)
{
    if (!source || !reply || capacity < 128)
        return false;
    if (length == 12 && !memcmp(source, ":load-status", 12)) {
        (void)snprintf(reply, capacity, "URL load: %s\n", status);
        return true;
    }
    if (length == 12 && !memcmp(source, ":load-cancel", 12)) {
        bool active = lisp_runtime_load_active();
        bool evaluating = phase == LOAD_EVALUATING || (phase == LOAD_QUEUED && lisp_runtime_busy());
        lisp_runtime_load_cancel();
        (void)snprintf(reply, capacity, "%s",
                       !active                 ? "No URL load active\n"
                       : !lisp_runtime_ready() ? "ERROR: runtime reset failed\n"
                       : evaluating            ? "URL load cancelled; ALL RAM definitions cleared\n"
                                               : "URL load cancelled; existing definitions preserved\n");
        return true;
    }
    return false;
}
static lbm_value load_url(lbm_value *arguments, lbm_uint count)
{
    char *url = NULL;
    size_t size = 0;
    if (count != 1 || !lbm_dec_str_size(arguments[0], &url, &size) || !url || size < 2 ||
        size > LISP_APP_URL_CAPACITY || url[size - 1] != 0 || !lisp_app_url_valid(url, size - 1))
        return ENC_SYM_TERROR;
    if (!attached || lisp_runtime_app_active() || lisp_runtime_load_active())
        return ENC_SYM_EERROR;
    memcpy(pending_url, url, size);
    pending_length = size - 1;
    phase = LOAD_QUEUED;
    status = "queued";
    return ENC_SYM_TRUE;
}
bool lisp_load_runtime_register(void)
{
    return lbm_add_extension("app-load-url", load_url);
}
void lisp_load_runtime_finished(bool success)
{
    if (phase == LOAD_QUEUED) {
        if (success && backend.request(backend.user, pending_url, pending_length)) {
            phase = LOAD_FETCHING;
            started_ms = 0;
            status = "downloading";
        } else {
            phase = LOAD_IDLE;
            status = "failed";
        }
        memset(pending_url, 0, sizeof(pending_url));
    } else if (phase == LOAD_EVALUATING) {
        phase = LOAD_IDLE;
        status = success ? "loaded" : "failed";
    }
}
void lisp_load_runtime_step(uint64_t now_ms)
{
    if (phase != LOAD_FETCHING)
        return;
    if (!started_ms)
        started_ms = now_ms + 1;
    if (now_ms + 1 - started_ms >= LISP_APP_URL_TIMEOUT_MS + 5000) {
        backend.cancel(backend.user);
        phase = LOAD_IDLE;
        status = "failed";
        return;
    }
    const char *source = NULL;
    size_t length = 0;
    lisp_app_url_phase_t result = backend.poll(backend.user, &source, &length);
    if (result == LISP_APP_URL_READY) {
        phase = LOAD_EVALUATING;
        status = "loading";
        bool submitted = lisp_runtime_submit_loaded(source, length, now_ms);
        backend.release(backend.user); /* Runtime owns an independent copy now. */
        if (!submitted) {
            phase = LOAD_IDLE;
            status = "failed";
        }
    } else if (result == LISP_APP_URL_FAILED || result == LISP_APP_URL_IDLE) {
        phase = LOAD_IDLE;
        status = "failed";
    }
}
