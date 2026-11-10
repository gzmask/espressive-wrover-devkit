#include "lcd_terminal.h"
#include <string.h>

/* Five-column, seven-pixel ASCII glyphs; fixed cells leave room for a cursor.
 * Kept separate from the uppercase diagnostic font so that Lisp case survives. */
static const uint8_t font[95][5] = {
    {0, 0, 0, 0, 0},
    {0, 0, 0x5f, 0, 0},
    {0, 7, 0, 7, 0},
    {0x14, 0x7f, 0x14, 0x7f, 0x14},
    {0x24, 0x2a, 0x7f, 0x2a, 0x12},
    {0x23, 0x13, 8, 0x64, 0x62},
    {0x36, 0x49, 0x55, 0x22, 0x50},
    {0, 5, 3, 0, 0},
    {0, 0x1c, 0x22, 0x41, 0},
    {0, 0x41, 0x22, 0x1c, 0},
    {0x14, 8, 0x3e, 8, 0x14},
    {8, 8, 0x3e, 8, 8},
    {0, 0x50, 0x30, 0, 0},
    {8, 8, 8, 8, 8},
    {0, 0x60, 0x60, 0, 0},
    {0x20, 0x10, 8, 4, 2},
    {0x3e, 0x51, 0x49, 0x45, 0x3e},
    {0, 0x42, 0x7f, 0x40, 0},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4b, 0x31},
    {0x18, 0x14, 0x12, 0x7f, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3c, 0x4a, 0x49, 0x49, 0x30},
    {1, 0x71, 9, 5, 3},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {6, 0x49, 0x49, 0x29, 0x1e},
    {0, 0x36, 0x36, 0, 0},
    {0, 0x56, 0x36, 0, 0},
    {8, 0x14, 0x22, 0x41, 0},
    {0x14, 0x14, 0x14, 0x14, 0x14},
    {0, 0x41, 0x22, 0x14, 8},
    {2, 1, 0x51, 9, 6},
    {0x32, 0x49, 0x79, 0x41, 0x3e},
    {0x7e, 0x11, 0x11, 0x11, 0x7e},
    {0x7f, 0x49, 0x49, 0x49, 0x36},
    {0x3e, 0x41, 0x41, 0x41, 0x22},
    {0x7f, 0x41, 0x41, 0x22, 0x1c},
    {0x7f, 0x49, 0x49, 0x49, 0x41},
    {0x7f, 9, 9, 9, 1},
    {0x3e, 0x41, 0x49, 0x49, 0x7a},
    {0x7f, 8, 8, 8, 0x7f},
    {0, 0x41, 0x7f, 0x41, 0},
    {0x20, 0x40, 0x41, 0x3f, 1},
    {0x7f, 8, 0x14, 0x22, 0x41},
    {0x7f, 0x40, 0x40, 0x40, 0x40},
    {0x7f, 2, 0x0c, 2, 0x7f},
    {0x7f, 4, 8, 0x10, 0x7f},
    {0x3e, 0x41, 0x41, 0x41, 0x3e},
    {0x7f, 9, 9, 9, 6},
    {0x3e, 0x41, 0x51, 0x21, 0x5e},
    {0x7f, 9, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31},
    {1, 1, 0x7f, 1, 1},
    {0x3f, 0x40, 0x40, 0x40, 0x3f},
    {0x1f, 0x20, 0x40, 0x20, 0x1f},
    {0x3f, 0x40, 0x38, 0x40, 0x3f},
    {0x63, 0x14, 8, 0x14, 0x63},
    {7, 8, 0x70, 8, 7},
    {0x61, 0x51, 0x49, 0x45, 0x43},
    {0, 0x7f, 0x41, 0x41, 0},
    {2, 4, 8, 0x10, 0x20},
    {0, 0x41, 0x41, 0x7f, 0},
    {4, 2, 1, 2, 4},
    {0x40, 0x40, 0x40, 0x40, 0x40},
    {0, 1, 2, 4, 0},
    {0x20, 0x54, 0x54, 0x54, 0x78},
    {0x7f, 0x48, 0x44, 0x44, 0x38},
    {0x38, 0x44, 0x44, 0x44, 0x20},
    {0x38, 0x44, 0x44, 0x48, 0x7f},
    {0x38, 0x54, 0x54, 0x54, 0x18},
    {8, 0x7e, 9, 1, 2},
    {0x0c, 0x52, 0x52, 0x52, 0x3e},
    {0x7f, 8, 4, 4, 0x78},
    {0, 0x44, 0x7d, 0x40, 0},
    {0x20, 0x40, 0x44, 0x3d, 0},
    {0x7f, 0x10, 0x28, 0x44, 0},
    {0, 0x41, 0x7f, 0x40, 0},
    {0x7c, 4, 0x18, 4, 0x78},
    {0x7c, 8, 4, 4, 0x78},
    {0x38, 0x44, 0x44, 0x44, 0x38},
    {0x7c, 0x14, 0x14, 0x14, 8},
    {8, 0x14, 0x14, 0x18, 0x7c},
    {0x7c, 8, 4, 4, 8},
    {0x48, 0x54, 0x54, 0x54, 0x20},
    {4, 0x3f, 0x44, 0x40, 0x20},
    {0x3c, 0x40, 0x40, 0x20, 0x7c},
    {0x1c, 0x20, 0x40, 0x20, 0x1c},
    {0x3c, 0x40, 0x30, 0x40, 0x3c},
    {0x44, 0x28, 0x10, 0x28, 0x44},
    {0x0c, 0x50, 0x50, 0x50, 0x3c},
    {0x44, 0x64, 0x54, 0x4c, 0x44},
    {0, 8, 0x36, 0x41, 0},
    {0, 0, 0x7f, 0, 0},
    {0, 0x41, 0x36, 8, 0},
    {8, 4, 8, 0x10, 8},
};

bool lcd_terminal_glyph_pixel(unsigned char character, unsigned x, unsigned y)
{
    if (character < 32 || character > 126)
        character = '?';
    return x >= 1 && x <= 5 && y >= 2 && y <= 8 && (font[character - 32][x - 1] & (1u << (y - 2))) != 0;
}

void lcd_terminal_input(lcd_terminal_t *terminal, const char *prompt, const char *text, size_t length)
{
    if (terminal == NULL || prompt == NULL || (text == NULL && length != 0))
        return;
    char *line = terminal->cells[LCD_TERMINAL_OUTPUT_ROWS];
    memset(line, ' ', LCD_TERMINAL_COLUMNS);
    size_t prefix = 0;
    while (prefix < 10 && prompt[prefix] != '\0') {
        line[prefix] = prompt[prefix];
        prefix++;
    }
    size_t available = LCD_TERMINAL_COLUMNS - prefix - 1;
    size_t visible = length < available ? length : available;
    for (size_t i = 0; i < visible; i++) {
        unsigned char byte = (unsigned char)text[length - visible + i];
        line[prefix + i] = byte >= 32 && byte <= 126 ? (char)byte : '?';
    }
    terminal->input_cursor = (unsigned)(prefix + visible);
}

void lcd_terminal_init(lcd_terminal_t *terminal)
{
    if (terminal == NULL)
        return;
    memset(terminal, 0, sizeof(*terminal));
    memset(terminal->cells, ' ', sizeof(terminal->cells));
    lcd_terminal_input(terminal, "lbm> ", "", 0);
}
static void newline(lcd_terminal_t *terminal)
{
    terminal->column = 0;
    terminal->wrap_pending = false;
    if (terminal->row < LCD_TERMINAL_OUTPUT_ROWS - 1)
        terminal->row++;
    else {
        memmove(terminal->cells[0], terminal->cells[1],
                (LCD_TERMINAL_OUTPUT_ROWS - 1) * LCD_TERMINAL_COLUMNS);
        memset(terminal->cells[LCD_TERMINAL_OUTPUT_ROWS - 1], ' ', LCD_TERMINAL_COLUMNS);
    }
}
static void printable(lcd_terminal_t *terminal, char character)
{
    if (terminal->wrap_pending)
        newline(terminal);
    terminal->cells[terminal->row][terminal->column] = character;
    if (terminal->column == LCD_TERMINAL_COLUMNS - 1)
        terminal->wrap_pending = true;
    else
        terminal->column++;
}
void lcd_terminal_write(lcd_terminal_t *terminal, const char *text, size_t length)
{
    if (terminal == NULL || text == NULL || terminal->row >= LCD_TERMINAL_OUTPUT_ROWS ||
        terminal->column >= LCD_TERMINAL_COLUMNS)
        return;
    for (size_t i = 0; i < length; i++) {
        unsigned char byte = (unsigned char)text[i];
        if (byte == '\n')
            newline(terminal);
        else if (byte == '\r') {
            terminal->column = 0;
            terminal->wrap_pending = false;
        } else if (byte == '\b') {
            if (terminal->wrap_pending)
                terminal->wrap_pending = false;
            else if (terminal->column > 0)
                terminal->column--;
            terminal->cells[terminal->row][terminal->column] = ' ';
        } else if (byte == '\t') {
            do {
                printable(terminal, ' ');
            } while (!terminal->wrap_pending && terminal->column % 4 != 0);
        } else if (byte >= 32)
            printable(terminal, byte <= 126 ? (char)byte : '?');
    }
}
bool lcd_terminal_render_strip(const lcd_terminal_t *terminal, uint8_t *out, size_t capacity,
                               unsigned first_row, unsigned rows)
{
    if (terminal == NULL || out == NULL || rows == 0 || first_row >= LCD_TERMINAL_HEIGHT ||
        rows > LCD_TERMINAL_HEIGHT - first_row || capacity < (size_t)rows * LCD_TERMINAL_WIDTH * 2 ||
        terminal->input_cursor >= LCD_TERMINAL_COLUMNS)
        return false;
    for (unsigned y = 0; y < rows; y++) {
        unsigned absolute_y = first_row + y, cell_y = absolute_y / LCD_TERMINAL_CELL_HEIGHT;
        unsigned glyph_y = absolute_y % LCD_TERMINAL_CELL_HEIGHT;
        for (unsigned x = 0; x < LCD_TERMINAL_WIDTH; x++) {
            unsigned cell_x = x / LCD_TERMINAL_CELL_WIDTH, glyph_x = x % LCD_TERMINAL_CELL_WIDTH;
            unsigned char ch = (unsigned char)terminal->cells[cell_y][cell_x];
            if (ch < 32 || ch > 126)
                ch = '?';
            bool ink = lcd_terminal_glyph_pixel(ch, glyph_x, glyph_y);
            bool input = cell_y == LCD_TERMINAL_OUTPUT_ROWS;
            bool cursor = input && cell_x == terminal->input_cursor && glyph_y == 10;
            uint16_t color = cursor ? 0x07ff : ink ? (input ? 0x07ff : 0xffff) : 0;
            size_t offset = ((size_t)y * LCD_TERMINAL_WIDTH + x) * 2;
            out[offset] = (uint8_t)(color >> 8);
            out[offset + 1] = (uint8_t)color;
        }
    }
    return true;
}
