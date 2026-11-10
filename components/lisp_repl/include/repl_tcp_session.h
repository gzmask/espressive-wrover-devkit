#pragma once
#include <stddef.h>

typedef enum {
    REPL_REMOTE_OK,
    REPL_REMOTE_BUSY,
    REPL_REMOTE_TIMEOUT,
    REPL_REMOTE_FAILED
} repl_remote_status_t;
typedef repl_remote_status_t (*repl_tcp_eval_fn)(const char *source, size_t length, char *reply,
                                                 size_t capacity);
/* One plain TCP stream, no TLS/authentication. Caller owns and closes socket.
 * Partial source is discarded on disconnect. Only callback may submit to owner. */
void repl_tcp_session(int socket, repl_tcp_eval_fn evaluate);
