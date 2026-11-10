#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE /* MAP_ANON for the existing low-address word32 test allocator. */
#endif
#include "lisp_runtime.h"
#include "lisp_test_storage.h"
#include "repl_remote_logic.h"
#include "repl_tcp_session.h"
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* Actual production TCP session and interpreter, with a host-only single-owner
 * mailbox model. Not the ESP32/FreeRTOS mailbox or an lwIP hardware test. */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t result_ready = PTHREAD_COND_INITIALIZER;
static bool request_pending, result_pending;
static char request[1024];
static repl_remote_reply_t reply;
static bool capturing;
static atomic_bool owner_ready, app_active, halt, load_active, finish_download;
static lisp_app_url_slot_t url_slot;
static char url_source[LISP_APP_URL_SOURCE_CAPACITY];
static bool url_request(void *user, const char *url, size_t length)
{
    (void)user;
    return lisp_app_url_slot_request(&url_slot, url, length);
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
static atomic_uint app_frames;
static uint16_t pixels[2][LISP_APP_PIXELS];
static lisp_app_canvas_t canvas;
static bool begin(void *u)
{
    return lisp_app_canvas_begin(u);
}
static uint16_t *draw(void *u)
{
    return lisp_app_canvas_draw(u);
}
static bool publish(void *u)
{
    return lisp_app_canvas_publish(u);
}
static void activate(void *u)
{
    lisp_app_canvas_activate(u);
}
static void restore(void *u)
{
    lisp_app_canvas_restore(u);
}
static lisp_app_display_state_t state(void *u)
{
    return lisp_app_canvas_state(u);
}
static void pause_tick(void)
{
    struct timespec delay = {0, 500000};
    nanosleep(&delay, NULL);
}
static void output(void *u, const char *text, size_t length)
{
    (void)u;
    if (capturing)
        repl_remote_reply_append(&reply, text, length);
}
static void append(const char *text)
{
    repl_remote_reply_append(&reply, text, strlen(text));
}
static void *owner(void *unused)
{
    (void)unused;
    size_t bytes = lisp_runtime_storage_size();
    void *storage = lisp_test_allocate(bytes);
    assert(storage && lisp_runtime_init(storage, bytes, output, NULL));
    assert(lisp_app_canvas_init(&canvas, pixels[0], pixels[1], LISP_APP_PIXELS));
    lisp_app_display_t backend = {&canvas, begin, draw, publish, activate, restore, state};
    assert(lisp_runtime_app_attach(&backend));
    assert(lisp_app_url_slot_init(&url_slot, url_source, sizeof(url_source)));
    lisp_app_url_backend_t url_backend = {NULL, url_request, url_poll, url_cancel, url_release};
    assert(lisp_runtime_load_attach(&url_backend));
    atomic_store(&owner_ready, true);
    bool inflight = false;
    for (uint64_t now = 0; !atomic_load(&halt); now += 5) {
        char source[1024];
        pthread_mutex_lock(&lock);
        bool received = request_pending;
        if (received) {
            memcpy(source, request, sizeof(source));
            request_pending = false;
        }
        pthread_mutex_unlock(&lock);
        if (received) {
            assert(!inflight);
            repl_remote_reply_init(&reply);
            inflight = true;
            lisp_app_command_t control = lisp_runtime_app_control(source, strlen(source));
            char load_reply[128];
            if (lisp_runtime_load_control(source, strlen(source), load_reply, sizeof(load_reply)))
                append(load_reply);
            else if (lisp_runtime_load_active())
                append("ERROR: URL load active; use :load-status or :load-cancel\n");
            else if (control == LISP_APP_COMMAND_OK)
                append("Application control accepted\n");
            else if (control == LISP_APP_COMMAND_INVALID)
                append("ERROR: invalid application control\n");
            else if (lisp_runtime_app_active())
                append("ERROR: application owns screen; use :stop or :key X\n");
            else {
                capturing = true;
                assert(lisp_runtime_submit(source, strlen(source), now));
            }
        }
        if (atomic_exchange(&finish_download, false)) {
            assert(url_slot.phase == LISP_APP_URL_FETCHING);
            memset(url_source, ' ', 2048);
            strcpy(url_source + 2048, "(define url-twice (lambda (x) (* x 2)))");
            lisp_app_url_slot_finish(&url_slot, true, strlen(url_source));
        }
        lisp_runtime_step(now);
        atomic_store(&load_active, lisp_runtime_load_active());
        if (now % 200 == 0) {
            lisp_app_frame_t frame;
            assert(lisp_app_canvas_acquire(&canvas, &frame));
            if (frame.pixels)
                atomic_fetch_add(&app_frames, 1);
            lisp_app_canvas_complete(&canvas, &frame, true);
        }
        atomic_store(&app_active, lisp_runtime_app_active());
        if (inflight && !lisp_runtime_busy()) {
            capturing = false;
            repl_remote_reply_finish(&reply);
            pthread_mutex_lock(&lock);
            result_pending = true;
            pthread_cond_signal(&result_ready);
            pthread_mutex_unlock(&lock);
            inflight = false;
        }
        pause_tick();
    }
    assert(!lisp_runtime_app_active() && !lisp_runtime_load_active());
    lisp_test_free(storage, bytes);
    return NULL;
}
static repl_remote_status_t evaluate(const char *source, size_t length, char *text, size_t capacity)
{
    assert(length < sizeof(request) && capacity >= sizeof(reply.text));
    struct timespec timeout;
    assert(clock_gettime(CLOCK_REALTIME, &timeout) == 0);
    timeout.tv_sec += 5;
    pthread_mutex_lock(&lock);
    assert(!request_pending && !result_pending);
    memcpy(request, source, length);
    request[length] = 0;
    request_pending = true;
    while (!result_pending)
        assert(pthread_cond_timedwait(&result_ready, &lock, &timeout) == 0);
    memcpy(text, reply.text, reply.used + 1);
    result_pending = false;
    pthread_mutex_unlock(&lock);
    return REPL_REMOTE_OK;
}
static void *session(void *argument)
{
    int socket = *(int *)argument;
    repl_tcp_session(socket, evaluate);
    close(socket);
    return NULL;
}
static void response(int socket, const char *expected)
{
    char text[4096] = {0};
    size_t used = 0;
    while (!strstr(text, "lbm> ")) {
        ssize_t n = recv(socket, text + used, sizeof(text) - used - 1, 0);
        assert(n > 0);
        used += (size_t)n;
        assert(used < sizeof(text) - 1);
        text[used] = 0;
    }
    assert(strstr(text, expected));
}
static void command(int socket, const char *source, const char *expected)
{
    assert(send(socket, source, strlen(source), 0) == (ssize_t)strlen(source));
    response(socket, expected);
}
static void wait_idle(void)
{
    for (unsigned i = 0; i < 4000 && atomic_load(&app_active); i++)
        pause_tick();
    assert(!atomic_load(&app_active));
}
static void start_session(int pair[2], pthread_t *thread)
{
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    struct timeval timeout = {5, 0};
    assert(setsockopt(pair[0], SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);
    assert(pthread_create(thread, NULL, session, &pair[1]) == 0);
    response(pair[0], "no TLS/auth");
}
static void end_session(int pair[2], pthread_t thread)
{
    shutdown(pair[0], SHUT_RDWR);
    close(pair[0]);
    assert(pthread_join(thread, NULL) == 0);
}
int main(void)
{
    signal(SIGPIPE, SIG_IGN);
    pthread_t owner_thread, session_thread;
    assert(pthread_create(&owner_thread, NULL, owner, NULL) == 0);
    while (!atomic_load(&owner_ready))
        pause_tick();
    int pair[2];
    start_session(pair, &session_thread);
    command(pair[0], "(define twice (lambda (x) (* x 2)))\n", "=>");
    const char *launch =
        "(app-run (list (lambda () (gfx-clear 31)) (lambda (e) (if (= e 113) 'exit t)) (lambda () t)))\n";
    command(pair[0], launch, "=> t"); /* Must reply without waiting for app exit. */
    assert(atomic_load(&app_active));
    command(pair[0], "(define wrong 123)\n", "application owns screen");
    unsigned frames = atomic_load(&app_frames);
    struct timespec delay = {0, 120000000};
    nanosleep(&delay, NULL);
    assert(atomic_load(&app_frames) > frames && atomic_load(&app_active));
    command(pair[0], ":key q\n", "control accepted");
    wait_idle();
    command(pair[0], "(twice 21)\n", "=> 42");
    command(pair[0], "wrong\n", "ERROR:");
    command(pair[0], launch, "=> t");
    end_session(pair, session_thread);
    frames = atomic_load(&app_frames);
    nanosleep(&delay, NULL);
    assert(atomic_load(&app_active) && atomic_load(&app_frames) > frames); /* EOF doesn't stop it. */
    start_session(pair, &session_thread);
    command(pair[0], ":stop\n", "control accepted");
    wait_idle();
    command(pair[0], "(twice 21)\n", "=> 42");
    command(pair[0],
            "(app-run (list (lambda () (app-load-url \"https://example.com/a\")) "
            "(lambda (e) t) (lambda () t)))\n",
            "=> t");
    wait_idle();
    command(pair[0], ":load-status\n", "idle"); /* Callbacks cannot start a network load. */
    const char *load = "(app-load-url \"https://example.com/app.lisp\")\n";
    command(pair[0], load, "=> t");
    assert(atomic_load(&load_active));
    command(pair[0], ":load-status\n", "downloading");
    command(pair[0], "(define forbidden 9)\n", "URL load active");
    end_session(pair, session_thread); /* EOF does not cancel the board's request. */
    atomic_store(&finish_download, true);
    for (unsigned i = 0; i < 4000 && atomic_load(&load_active); i++)
        pause_tick();
    assert(!atomic_load(&load_active));
    start_session(pair, &session_thread);
    command(pair[0], ":load-status\n", "loaded");
    command(pair[0], "(url-twice 21)\n", "=> 42");
    command(pair[0], "forbidden\n", "ERROR:");
    command(pair[0], load, "=> t");
    command(pair[0], ":load-cancel\n", "preserved");
    atomic_store(&finish_download, true); /* Late completion discarded, not evaluated. */
    struct timespec settle = {0, 10000000};
    nanosleep(&settle, NULL);
    command(pair[0], "(twice 21)\n", "=> 42");
    end_session(pair, session_thread);
    atomic_store(&halt, true);
    assert(pthread_join(owner_thread, NULL) == 0);
    puts("PASS: production session + LispBM + host mailbox; app + URL ACK/status/cancel/EOF/reconnect");
    return 0;
}
