#include "lisp_runtime.h"
#include "lisp_test_storage.h"
#include "repl_help.h"
#include "repl_remote_logic.h"
#include "repl_tcp_session.h"
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
static repl_remote_reply_t reply;
static void output(void *user, const char *text, size_t length)
{
    (void)user;
    repl_remote_reply_append(&reply, text, length);
}
/* Real interpreter + production socket session, not a FreeRTOS mailbox model. */
static repl_remote_status_t evaluate(const char *source, size_t length, char *text, size_t capacity)
{
    assert(repl_remote_source_valid(source, length));
    assert(capacity >= sizeof(reply.text));
    repl_remote_reply_init(&reply);
    if (!strcmp(source, ":help")) {
        repl_help_write(output, NULL, true, true, true);
    } else if (!strcmp(source, ":help-lcd")) {
        repl_help_lcd_write(output, NULL);
    } else if (!strcmp(source, ":reset")) {
        assert(lisp_runtime_reset());
        repl_remote_reply_append(&reply, "RAM definitions cleared\n", 24);
    } else {
        assert(lisp_runtime_submit(source, length, 0));
        for (uint64_t now = 0; lisp_runtime_busy() && now < 6000; ++now)
            lisp_runtime_step(now);
        assert(!lisp_runtime_busy());
    }
    repl_remote_reply_finish(&reply);
    memcpy(text, reply.text, reply.used + 1);
    return REPL_REMOTE_OK;
}
static void *server(void *argument)
{
    int socket = *(int *)argument;
    void *storage = lisp_test_allocate(lisp_runtime_storage_size());
    assert(storage && lisp_runtime_init(storage, lisp_runtime_storage_size(), output, NULL));
    repl_tcp_session(socket, evaluate);
    close(socket);
    lisp_test_free(storage, lisp_runtime_storage_size());
    return NULL;
}
static void response(int socket, const char *expected)
{
    char text[4096] = {0};
    size_t used = 0;
    while (strstr(text, "lbm> ") == NULL) {
        ssize_t count = recv(socket, text + used, sizeof(text) - used - 1, 0);
        assert(count > 0);
        used += (size_t)count;
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
int main(void)
{
    signal(SIGPIPE, SIG_IGN);
    int pair[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    struct timeval timeout = {.tv_sec = 5, .tv_usec = 0};
    assert(setsockopt(pair[0], SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, server, &pair[1]) == 0);
    response(pair[0], "no TLS/auth");
    command(pair[0], "(+ 1 2)\n", "=> 3");
    command(pair[0], "(define twice (lambda (x) (* x 2)))\n", "=>");
    command(pair[0], "(twice 21)\n", "=> 42");
    command(pair[0], ":help\n", ":help-lcd");
    command(pair[0], ":help-lcd\n", "wait off BEFORE inserting");
    command(pair[0], "(twice 21)\n", "=> 42");
    command(pair[0], "(print \"hello\")\n", "hello");
    command(pair[0], "(no-such-function)\n", "ERROR:");
    command(pair[0], "(+ 2 3)\n", "=> 5");
    command(pair[0], ":reset\n", "RAM definitions cleared");
    command(pair[0], "(twice 21)\n", "ERROR:");
    command(pair[0], "(+ 1 2)\n", "=> 3");
    shutdown(pair[0], SHUT_RDWR);
    close(pair[0]);
    assert(pthread_join(thread, NULL) == 0);
    puts("PASS: production plain socket session -> real LispBM, no cert/token,3/42/hello/errors/reset; no "
         "device calls");
    return 0;
}
