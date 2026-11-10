#include "lcd_terminal.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static void text(void)
{
    lcd_terminal_t t;
    lcd_terminal_init(&t);
    lcd_terminal_write(&t, "Abc(+ 1 2)", 10);
    assert(memcmp(t.cells[0], "Abc(+ 1 2)", 10) == 0);
    assert(t.column == 10 && t.row == 0);
    lcd_terminal_write(&t, "\rX\nY\tZ", 7);
    assert(t.cells[0][0] == 'X' && t.cells[1][0] == 'Y' && t.cells[1][4] == 'Z');
    lcd_terminal_write(&t, "\b", 1);
    assert(t.cells[1][4] == ' ');
}
static void wrap(void)
{
    lcd_terminal_t t;
    lcd_terminal_init(&t);
    char line[40];
    memset(line, 'a', sizeof(line));
    lcd_terminal_write(&t, line, sizeof(line));
    assert(t.row == 0 && t.wrap_pending);
    lcd_terminal_write(&t, "\nB", 2);
    assert(t.row == 1 && t.column == 1);
    lcd_terminal_init(&t);
    lcd_terminal_write(&t, line, sizeof(line));
    lcd_terminal_write(&t, "B", 1);
    assert(t.row == 1 && t.cells[1][0] == 'B');
}
static void scroll(void)
{
    lcd_terminal_t t;
    lcd_terminal_init(&t);
    for (unsigned i = 0; i < 25; i++) {
        char line[3] = {(char)('A' + i), '\n', 0};
        lcd_terminal_write(&t, line, 2);
    }
    assert(t.row == 18 && t.cells[0][0] == 'H' && t.cells[17][0] == 'Y');
    assert(t.cells[19][0] == 'l'); /* Input row never scrolls. */
}
static void input(void)
{
    lcd_terminal_t t;
    lcd_terminal_init(&t);
    char line[100];
    memset(line, 'x', sizeof(line));
    line[99] = 'Z';
    lcd_terminal_input(&t, "lbm> ", line, sizeof(line));
    assert(memcmp(t.cells[19], "lbm> ", 5) == 0 && t.cells[19][38] == 'Z');
    assert(t.input_cursor == 39);
    lcd_terminal_input(&t, "...> ", "abc", 3);
    assert(t.cells[19][8] == ' ' && t.input_cursor == 8);
}
static void render(void)
{
    lcd_terminal_t t;
    lcd_terminal_init(&t);
    uint8_t a[320 * 16 * 2], b[sizeof(a)], row[320 * 2];
    lcd_terminal_write(&t, "a()[];\"+-*/", 11);
    assert(lcd_terminal_render_strip(&t, a, sizeof(a), 0, 16));
    lcd_terminal_init(&t);
    lcd_terminal_write(&t, "A()[];\"+-*/", 11);
    assert(lcd_terminal_render_strip(&t, b, sizeof(b), 0, 16));
    assert(memcmp(a, b, sizeof(a)) != 0);
    for (unsigned y = 0; y < 240; y += 16) {
        assert(lcd_terminal_render_strip(&t, a, sizeof(a), y, 16));
        for (unsigned i = 0; i < 16; i++) {
            assert(lcd_terminal_render_strip(&t, row, sizeof(row), y + i, 1));
            assert(memcmp(row, a + i * sizeof(row), sizeof(row)) == 0);
        }
    }
}
static void bounds(void)
{
    lcd_terminal_t t;
    lcd_terminal_init(&t);
    uint8_t guard[642];
    memset(guard, 0xa5, sizeof(guard));
    assert(lcd_terminal_render_strip(&t, guard + 1, 640, 239, 1));
    assert(guard[0] == 0xa5 && guard[641] == 0xa5);
    assert(!lcd_terminal_render_strip(&t, guard, 639, 0, 1));
    assert(!lcd_terminal_render_strip(&t, guard, sizeof(guard), UINT_MAX, 1));
    assert(!lcd_terminal_render_strip(&t, guard, sizeof(guard), 0, UINT_MAX));
    assert(!lcd_terminal_render_strip(NULL, guard, sizeof(guard), 0, 1));
    assert(!lcd_terminal_render_strip(&t, NULL, sizeof(guard), 0, 1));
    assert(!lcd_terminal_render_strip(&t, guard, sizeof(guard), 0, 0));
    lcd_terminal_write(&t, "\033\377", 2);
    assert(t.cells[0][0] == '?' && t.column == 1);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "text"))
        text();
    else if (!strcmp(argv[1], "wrap"))
        wrap();
    else if (!strcmp(argv[1], "scroll"))
        scroll();
    else if (!strcmp(argv[1], "input"))
        input();
    else if (!strcmp(argv[1], "render"))
        render();
    else if (!strcmp(argv[1], "bounds"))
        bounds();
    else
        return 1;
    puts("PASS");
    return 0;
}
