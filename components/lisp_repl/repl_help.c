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
         "(lcd-off) (lcd-on) (lcd-status)\n"
         ":help-lcd: LCD status and SD rules\n"
         ":reset: ALL contexts + definitions gone\n"
         "REPL UART Ctrl-C: reset; Ctrl-U: line\n");
    if (tcp)
        emit(output, user, "TCP Ctrl-C: clear input, not RAM\n");
    if (url)
        emit(output, user,
             "(app-load-url \"https://host/a.lisp\")\n"
             "ACK!=loaded; :load-status; no auto-run\n"
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

void repl_help_lcd_write(repl_help_output_t output, void *user)
{
    if (output == NULL)
        return;
    emit(output, user,
         "LCD lifecycle: UART/TCP control\n"
         "(lcd-off): request off; wait status\n"
         "(lcd-on): request on; remove SD first\n"
         "(lcd-status): on/off/starting/stopping\n"
         "Also: sd-blocked/fault/disabled\n"
         "t means accepted, not finished\n"
         "SD: wait off BEFORE inserting\n"
         "No SD hotplug while LCD on/starting\n"
         "Stop SD/release/remove card before on\n"
         "Stop app/URL load before LCD control\n"
         "Off keeps RAM; no SD driver yet\n");
}
