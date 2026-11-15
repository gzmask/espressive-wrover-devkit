#include "lcd_terminal.h"
#include "repl_help.h"
#include "repl_remote_logic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void collect(void *user, const char *text, size_t length)
{
    repl_remote_reply_append(user, text, length);
}
static void check_lines(const char *text)
{
    size_t line = 0, lines = 0;
    for (const char *p = text; *p; p++) {
        if (*p == '\n') {
            assert(line < LCD_TERMINAL_COLUMNS);
            line = 0;
            lines++;
        } else {
            assert((unsigned char)*p >= 32 && (unsigned char)*p <= 126);
            line++;
        }
    }
    assert(line == 0 && lines <= LCD_TERMINAL_OUTPUT_ROWS);
}
static void check(bool app, bool url, bool tcp, bool sd)
{
    repl_remote_reply_t reply;
    repl_remote_reply_init(&reply);
    repl_help_write(collect, &reply, app, url, tcp, sd);
    repl_remote_reply_finish(&reply);
    const char *text = reply.text;
    assert(!reply.truncated);
    assert(strstr(text, ":help") && strstr(text, ":clear") && strstr(text, ":reset"));
    assert(strstr(text, ":help-data"));
    assert(strstr(text, "(lcd-off)") && strstr(text, "(lcd-on)") && strstr(text, "(lcd-status)"));
    assert((strstr(text, ":help-sd") != NULL) == sd);
    assert((strstr(text, "(sd-mount)") != NULL) == sd);
    assert((strstr(text, "(sd-read ...)") != NULL) == sd);
    assert((strstr(text, "(sd-write ...)") != NULL) == sd);
    assert((strstr(text, "(sd-result)") != NULL) == sd);
    assert((strstr(text, "app-load-url") != NULL) == url);
    assert((strstr(text, ":load-status") != NULL) == url);
    assert((strstr(text, ":load-cancel") != NULL) == url);
    assert((strstr(text, "app-run demo") != NULL) == app);
    assert((strstr(text, "TCP :stop") != NULL) == (app && tcp));
    assert((strstr(text, ":key") != NULL) == (app && tcp));
    assert((strstr(text, "TCP ^C") != NULL) == tcp);
    assert((strstr(text, "trusted LAN") != NULL) == tcp);
    if (url)
        assert(strstr(text, ":load-status/:load-cancel") && strstr(text, "not auto-run"));
    if (app)
        assert(strstr(text, "wait status") && strstr(text, "keeps definitions"));
    check_lines(text);
}
static void check_lcd(void)
{
    for (int sd = 0; sd <= 1; sd++) {
        repl_remote_reply_t reply;
        repl_remote_reply_init(&reply);
        repl_help_lcd_write(collect, &reply, sd != 0);
        repl_remote_reply_finish(&reply);
        assert(!reply.truncated);
        const char *text = reply.text;
        assert(strstr(text, "(lcd-off)") && strstr(text, "(lcd-on)") && strstr(text, "(lcd-status)"));
        assert(strstr(text, "accepted, not finished"));
        assert(strstr(text, "wait off BEFORE inserting"));
        assert(strstr(text, "No SD hotplug") && strstr(text, "remove card before on"));
        assert(strstr(text, "Stop app/URL load") && strstr(text, "Off keeps RAM"));
        if (sd)
            assert(strstr(text, "SD API optional"));
        else
            assert(strstr(text, "SD not enabled"));
        check_lines(text);
    }
    repl_help_lcd_write(NULL, NULL, false);
}
static void check_sd(void)
{
    repl_remote_reply_t reply;
    repl_remote_reply_init(&reply);
    repl_help_sd_write(collect, &reply);
    repl_remote_reply_finish(&reply);
    assert(!reply.truncated);
    const char *text = reply.text;
    assert(strstr(text, "(sd-mount)") && strstr(text, "(sd-unmount)") && strstr(text, "(sd-status)"));
    assert(strstr(text, "(sd-read") && strstr(text, "(sd-write") && strstr(text, "(sd-result)"));
    assert(strstr(text, "(sd-last-error)") && strstr(text, "(sd-diagnostics)"));
    assert(strstr(text, "(app-load-sd \"app.lisp\")"));
    assert(strstr(text, ":load-status / :load-cancel"));
    assert(strstr(text, "no automatic retry"));
    assert(strstr(text, "never auto-formats"));
    assert(strstr(text, "existing data lost"));
    assert(strstr(text, "AFTER LCD off"));
    assert(strstr(text, "Never hotplug SD") && strstr(text, "JTAG GPIOs"));
    check_lines(text);
    repl_help_sd_write(NULL, NULL);
}
int main(void)
{
    for (int app = 0; app <= 1; app++)
        for (int url = 0; url <= app; url++)
            for (int tcp = 0; tcp <= 1; tcp++)
                for (int sd = 0; sd <= 1; sd++)
                    check(app != 0, url != 0, tcp != 0, sd != 0);
    repl_help_write(NULL, NULL, true, true, true, true);
    check_lcd();
    check_sd();
    for (unsigned http = 0; http < 2; http++) {
        repl_remote_reply_t reply;
        repl_remote_reply_init(&reply);
        repl_help_data_write(collect, &reply, http != 0);
        repl_remote_reply_finish(&reply);
        assert(!reply.truncated);
        assert(strstr(reply.text, "http-get") && strstr(reply.text, "http-release"));
        assert(strstr(reply.text, "json-parse") && strstr(reply.text, "json-missing"));
        assert(strstr(reply.text, "monotonic-ms") && strstr(reply.text, "str-from-n"));
        assert(strstr(reply.text, http ? "HTTP enabled" : "HTTP unavailable"));
        check_lines(reply.text);
    }
    repl_help_data_write(NULL, NULL, false);
    puts("PASS");
    return 0;
}
