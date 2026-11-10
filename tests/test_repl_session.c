#include "lisp_runtime.h"
#include "lisp_test_storage.h"
#include "repl_frontend.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static repl_frontend_t frontend;
static void output(void *user, const char *text, size_t length)
{
    (void)user;
    lcd_terminal_write(&frontend.terminal, text, length);
}
static void command(const char *text)
{
    while (*text) {
        repl_input_event_t event = repl_frontend_feed(&frontend, (uint8_t)*text++);
        if (event == REPL_READY) {
            assert(lisp_runtime_submit(frontend.source, frontend.length, 0));
            for (uint64_t now = 0; lisp_runtime_busy() && now < 6000; now++)
                lisp_runtime_step(now);
            assert(!lisp_runtime_busy());
            repl_frontend_finish(&frontend);
        }
    }
}
int main(void)
{
    repl_frontend_init(&frontend);
    void *storage = lisp_test_allocate(lisp_runtime_storage_size());
    assert(storage && lisp_runtime_init(storage, lisp_runtime_storage_size(), output, NULL));
    command("(+ 1 2)\r\n");
    assert(memcmp(frontend.terminal.cells[0], "lbm> (+ 1 2)", 12) == 0);
    assert(memcmp(frontend.terminal.cells[1], "=> 3", 4) == 0);
    command("(define twice\n (lambda (x) (* x 2)))\n");
    command("(twice 21)\n");
    bool found = false;
    for (unsigned row = 0; row < LCD_TERMINAL_OUTPUT_ROWS; row++) {
        if (memcmp(frontend.terminal.cells[row], "=> 42", 5) == 0)
            found = true;
    }
    assert(found && frontend.length == 0);
    uint8_t strip[LCD_TERMINAL_WIDTH * 16 * 2];
    for (unsigned y = 0; y < LCD_TERMINAL_HEIGHT; y += 16)
        assert(lcd_terminal_render_strip(&frontend.terminal, strip, sizeof(strip), y, 16));
    lisp_test_free(storage, lisp_runtime_storage_size());
    puts("PASS: UART-byte frontend -> actual LispBM -> LCD text -> RGB565 strips (no hardware)");
    return 0;
}
