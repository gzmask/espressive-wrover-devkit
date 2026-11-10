#include "repl_tcp_session.h"
#include "repl_frontend.h"
#include "repl_remote_logic.h"
#include <errno.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "lwip/sockets.h"
#else
#include <sys/socket.h>
#endif

static bool send_text(int socket, const char *text)
{
    size_t length = strlen(text);
    while (length != 0) {
        ssize_t count = send(socket, text, length, MSG_NOSIGNAL);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return false;
        text += count;
        length -= (size_t)count;
    }
    return true;
}
void repl_tcp_session(int socket, repl_tcp_eval_fn evaluate)
{
    repl_frontend_t input;
    repl_frontend_init(&input);
    bool invalid = false;
    char reply[REPL_REMOTE_REPLY_CAPACITY];
    if (!send_text(socket, "LispBM TCP REPL; no TLS/auth; RAM only.\nlbm> "))
        return;
    for (;;) {
        unsigned char bytes[64];
        ssize_t count = recv(socket, bytes, sizeof(bytes), 0);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return; /* Never evaluate a partial line on disconnect. */
        for (ssize_t i = 0; i < count; ++i) {
            unsigned char byte = bytes[i];
            bool newline = byte == '\n' || byte == '\r';
            if (byte > 127 ||
                (byte < 32 && !newline && byte != 3 && byte != 8 && byte != 21 && byte != 27 && byte != '\t'))
                invalid = true;
            if (invalid && (newline || byte == 3 || byte == 21)) {
                repl_frontend_finish(&input);
                input.suppress_lf = byte == '\r';
                invalid = false;
                if (!send_text(socket, "ERROR: input invalid or too long; discarded\nlbm> "))
                    return;
                continue;
            }
            if (invalid)
                continue;
            repl_input_event_t event = repl_frontend_feed(&input, byte);
            if (event == REPL_OVERFLOW) {
                if (newline) {
                    repl_frontend_finish(&input);
                    input.suppress_lf = byte == '\r';
                    if (!send_text(socket, "ERROR: input too long; discarded\nlbm> "))
                        return;
                } else
                    invalid = true;
            } else if (event == REPL_CANCELLED) {
                if (!send_text(socket, "Input cleared; :reset clears RAM definitions\nlbm> "))
                    return;
            } else if (event == REPL_READY) {
                repl_remote_status_t status = evaluate(input.source, input.length, reply, sizeof(reply));
                if (status == REPL_REMOTE_TIMEOUT) {
                    (void)send_text(socket,
                                    "ERROR: owner timeout; command may have executed; do not retry\n");
                    return;
                }
                const char *text = status == REPL_REMOTE_OK ? reply
                                   : status == REPL_REMOTE_BUSY
                                       ? "ERROR: REPL busy; finish UART input first\n"
                                       : "ERROR: REPL unavailable\n";
                if (!send_text(socket, text) || !send_text(socket, "lbm> "))
                    return;
                repl_frontend_finish(&input);
            } else if (newline && event == REPL_CHANGED) {
                if (!send_text(socket, input.line_start ? "...> " : "lbm> "))
                    return;
            }
        }
    }
}
