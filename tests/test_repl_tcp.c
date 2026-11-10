#include "repl_tcp_session.h"
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static unsigned calls;
static char last_source[1024];
static repl_remote_status_t result;
static repl_remote_status_t evaluate(const char *source, size_t length, char *reply, size_t capacity)
{
    assert(length < sizeof(last_source) && capacity >= 16);
    memcpy(last_source, source, length);
    last_source[length] = 0;
    calls++;
    (void)snprintf(reply, capacity, "=> 3\n");
    return result;
}
static void *server(void *argument)
{
    int socket = *(int *)argument;
    repl_tcp_session(socket, evaluate);
    close(socket);
    return NULL;
}
static void send_input(int socket, const void *source, size_t length)
{
    assert(send(socket, source, length, 0) == (ssize_t)length);
}
static void until_prompt(int socket, const char *expected, const char *prompt)
{
    char text[4096] = {0};
    size_t used = 0;
    while (strstr(text, prompt) == NULL) {
        ssize_t count = recv(socket, text + used, sizeof(text) - used - 1, 0);
        assert(count > 0);
        used += (size_t)count;
        text[used] = 0;
        assert(used < sizeof(text) - 1);
    }
    assert(strstr(text, expected));
}
static void test(int socket, const char *name)
{
    until_prompt(socket, "no TLS/auth", "lbm> ");
    if (!strcmp(name, "basic")) {
        send_input(socket, "(+ 1 2)\n", 8);
        until_prompt(socket, "=> 3", "lbm> ");
        send_input(socket, "(+ 2 1)\n", 8);
        until_prompt(socket, "=> 3", "lbm> ");
    } else if (!strcmp(name, "fragmented")) {
        send_input(socket, "(+ ", 3);
        send_input(socket, "1\n", 2);
        until_prompt(socket, "...> ", "...> ");
        send_input(socket, "2)\n", 3);
        until_prompt(socket, "=> 3", "lbm> ");
    } else if (!strcmp(name, "editing")) {
        const char edited[] = "bad\025(+ 1 9\1772)\r\n";
        send_input(socket, edited, sizeof(edited) - 1);
        until_prompt(socket, "=> 3", "lbm> ");
    } else if (!strcmp(name, "overflow")) {
        char large[1025];
        memset(large, 'x', sizeof(large));
        large[1024] = '\n';
        send_input(socket, large, sizeof(large));
        until_prompt(socket, "ERROR: input", "lbm> ");
        send_input(socket, "(+ 1 2)\n", 8);
        until_prompt(socket, "=> 3", "lbm> ");
    } else if (!strcmp(name, "invalid")) {
        const char invalid[] = "(+ 1\0 2)\n";
        send_input(socket, invalid, sizeof(invalid) - 1);
        until_prompt(socket, "ERROR: input", "lbm> ");
        send_input(socket, "\377(+ 1 2)\n", 9);
        until_prompt(socket, "ERROR: input", "lbm> ");
    } else if (!strcmp(name, "eof")) {
        send_input(socket, "(define partial 1)", 18); /* no newline */
    } else if (!strcmp(name, "busy")) {
        send_input(socket, "(+ 1 2)\n", 8);
        until_prompt(socket, "ERROR: REPL busy", "lbm> ");
    } else if (!strcmp(name, "timeout")) {
        send_input(socket, "(+ 1 2)\n", 8);
        char text[256] = {0};
        ssize_t count = recv(socket, text, sizeof(text) - 1, 0);
        assert(count > 0 && strstr(text, "do not retry"));
    } else
        assert(false);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    signal(SIGPIPE, SIG_IGN);
    result = !strcmp(argv[1], "busy")      ? REPL_REMOTE_BUSY
             : !strcmp(argv[1], "timeout") ? REPL_REMOTE_TIMEOUT
                                           : REPL_REMOTE_OK;
    int pair[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    struct timeval deadline = {.tv_sec = 3, .tv_usec = 0};
    assert(setsockopt(pair[0], SOL_SOCKET, SO_RCVTIMEO, &deadline, sizeof(deadline)) == 0);
    pthread_t owner;
    assert(pthread_create(&owner, NULL, server, &pair[1]) == 0);
    test(pair[0], argv[1]);
    shutdown(pair[0], SHUT_RDWR);
    close(pair[0]);
    assert(pthread_join(owner, NULL) == 0);
    if (!strcmp(argv[1], "basic"))
        assert(calls == 2 && !strcmp(last_source, "(+ 2 1)"));
    else if (!strcmp(argv[1], "fragmented"))
        assert(calls == 1 && !strcmp(last_source, "(+ 1\n2)"));
    else if (!strcmp(argv[1], "editing") || !strcmp(argv[1], "overflow"))
        assert(calls == 1 && !strcmp(last_source, "(+ 1 2)"));
    else if (!strcmp(argv[1], "invalid") || !strcmp(argv[1], "eof"))
        assert(calls == 0);
    else
        assert(calls == 1);
    puts("PASS: plain socket session without cert/token; no device calls");
    return 0;
}
