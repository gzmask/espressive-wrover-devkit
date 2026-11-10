#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    LCD_TERMINAL_WIDTH = 320,
    LCD_TERMINAL_HEIGHT = 240,
    LCD_TERMINAL_COLUMNS = 40,
    LCD_TERMINAL_ROWS = 20,
    LCD_TERMINAL_OUTPUT_ROWS = 19,
    LCD_TERMINAL_CELL_WIDTH = 8,
    LCD_TERMINAL_CELL_HEIGHT = 12,
};
typedef struct {
    char cells[LCD_TERMINAL_ROWS][LCD_TERMINAL_COLUMNS];
    unsigned row, column, input_cursor;
    bool wrap_pending;
} lcd_terminal_t;
void lcd_terminal_init(lcd_terminal_t *terminal);
void lcd_terminal_write(lcd_terminal_t *terminal, const char *text, size_t length);
/* Last row is reserved for an editable input window; printable ASCII only. */
void lcd_terminal_input(lcd_terminal_t *terminal, const char *prompt, const char *text, size_t length);
/* Pure RGB565 big-endian renderer. No framebuffer, GPIO or allocation. */
bool lcd_terminal_render_strip(const lcd_terminal_t *terminal, uint8_t *out, size_t capacity,
                               unsigned first_row, unsigned rows);
