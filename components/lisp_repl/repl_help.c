#include "repl_help.h"
#include <string.h>

static void emit(repl_help_output_t output, void *user, const char *text)
{
    output(user, text, strlen(text));
}

void repl_help_write(repl_help_output_t output, void *user, bool app, bool url, bool tcp)
{
    if (output == NULL)
        return;
    emit(output, user,
         "LispBM REPL: RAM only\n"
         ":help commands; :clear LCD only\n"
         ":reset: ALL contexts + definitions gone\n"
         "REPL UART Ctrl-C: reset; Ctrl-U: line\n");
    if (tcp)
        emit(output, user, "TCP Ctrl-C: clear input, not RAM\n");
    if (url)
        emit(output, user,
             "(app-load-url \"https://host/a.lisp\")\n"
             "Accepted != loaded; no auto-run\n"
             ":load-status: wait for loaded\n"
             ":load-cancel: cancel; eval clears ALL\n");
    if (app) {
        emit(output, user,
             "(app-run demo): run defined app\n"
             "App UART Ctrl-C: stop\n");
        if (tcp)
            emit(output, user, "App TCP :stop; :key X sends a key\n");
        emit(output, user,
             "Stop accepted != finished; wait\n"
             "Stop retains definitions; then :reset\n"
             "App blocks :help/:reset until stopped\n");
    }
    if (tcp)
        emit(output, user, "TCP2323: no auth; trusted LAN only\n");
    emit(output, user,
         "Enter: eval; multiline; BS/DEL: edit\n"
         "Restart clears RAM; no save\n");
}
