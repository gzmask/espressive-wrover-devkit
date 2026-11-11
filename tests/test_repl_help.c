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

static void check(bool app, bool url, bool tcp)
{
    repl_remote_reply_t reply;
    repl_remote_reply_init(&reply);
    repl_help_write(collect, &reply, app, url, tcp);
    repl_remote_reply_finish(&reply);
    const char *text = reply.text;
    assert(!reply.truncated);
    assert(strstr(text, ":help") && strstr(text, ":clear") && strstr(text, ":reset"));
    assert(strstr(text, "ALL contexts") && strstr(text, "definitions"));
    assert((strstr(text, "app-load-url") != NULL) == url);
    assert((strstr(text, ":load-status") != NULL) == url);
    assert((strstr(text, ":load-cancel") != NULL) == url);
    assert((strstr(text, "app-run demo") != NULL) == app);
    assert((strstr(text, "TCP :stop") != NULL) == (app && tcp));
    assert((strstr(text, ":key") != NULL) == (app && tcp));
    assert((strstr(text, "TCP Ctrl-C") != NULL) == tcp);
    assert((strstr(text, "trusted LAN") != NULL) == tcp);
    if (url) {
        assert(strstr(text, "loaded") && strstr(text, "no auto-run"));
        assert(strstr(text, "eval clears ALL"));
    }
    if (app)
        assert(strstr(text, "wait") && strstr(text, "retains definitions"));
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
    assert(line == 0 && lines < LCD_TERMINAL_OUTPUT_ROWS);
}

int main(void)
{
    for (int app = 0; app <= 1; app++)
        for (int url = 0; url <= app; url++)
            for (int tcp = 0; tcp <= 1; tcp++)
                check(app != 0, url != 0, tcp != 0);
    repl_help_write(NULL, NULL, true, true, true);
    puts("PASS");
    return 0;
}
