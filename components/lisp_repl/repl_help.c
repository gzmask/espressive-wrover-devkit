#include "repl_help.h"
#include <string.h>

static void emit(repl_help_output_t output, void *user, const char *text)
{
    output(user, text, strlen(text));
}

void repl_help_write(repl_help_output_t output, void *user, bool app, bool url, bool tcp, bool sd)
{
    if (output == NULL)
        return;
    emit(output, user,
         "LispBM: definitions in RAM\n"
         ":help commands; :clear LCD only\n"
         "(lcd-off) (lcd-on) (lcd-status)\n");
    emit(output, user, sd ? ":help-lcd/:help-sd: hardware rules\n" : ":help-lcd: LCD status and SD rules\n");
    if (sd)
        emit(output, user,
             "(sd-mount) (sd-read ...)\n"
             "(sd-write ...) (sd-result)\n");
    emit(output, user,
         ":reset: ALL contexts + definitions gone\n"
         "REPL Ctrl-C resets RAM; Ctrl-U line\n");
    if (tcp)
        emit(output, user, "TCP ^C clears input; RAM stays\n");
    if (url)
        emit(output, user,
             "(app-load-url \"https://host/a.lisp\")\n"
             ":load-status/:load-cancel; not auto-run\n");
    if (app) {
        emit(output, user, "(app-run demo): run defined app\n");
        if (tcp)
            emit(output, user, "App TCP :stop; :key X sends a key\n");
        emit(output, user,
             "Stop: wait status; keeps definitions\n"
             "App blocks REPL until stopped\n");
    }
    if (tcp)
        emit(output, user, "TCP2323: no auth; trusted LAN only\n");
    emit(output, user,
         "Enter eval/multiline; BS/DEL edit\n"
         "Restart clears RAM; no save\n");
}

void repl_help_lcd_write(repl_help_output_t output, void *user, bool sd)
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
         "Stop app/URL load before LCD control\n");
    emit(output, user, sd ? "Off keeps RAM; SD API optional\n" : "Off keeps RAM; SD not enabled\n");
}

void repl_help_sd_write(repl_help_output_t output, void *user)
{
    if (output == NULL)
        return;
    emit(output, user,
         "SD API: persistent ASCII text files\n"
         "(sd-mount): request; poll sd-status\n"
         "(sd-status): state; (sd-last-error)\n"
         "(sd-diagnostics): phase/FatFs/stack\n"
         "(app-load-sd \"app.lisp\"): load to RAM\n"
         "(sd-unmount): request; wait unmounted\n"
         "(sd-read \"name.txt\"): request read\n"
         "(sd-write \"name.txt\" \"text\"): replace\n"
         "(sd-result): pending, text, t, error\n"
         "Load: :load-status / :load-cancel\n"
         "Flat safe filename; no folders/path\n"
         "ASCII write<=512; read<=240\n"
         "sd-write truncates; existing data lost\n"
         "Mount failure never auto-formats card\n"
         "Insert/mount only AFTER LCD off\n"
         "Never hotplug SD while LCD on/starting\n"
         "Unmount and remove card before lcd-on\n"
         "SDSPI uses JTAG GPIOs while mounted\n"
         "Fault keeps pins; no automatic retry\n");
}
