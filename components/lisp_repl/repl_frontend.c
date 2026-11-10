#include "repl_frontend.h"
#include <string.h>

bool repl_source_needs_more(const char *source, size_t length)
{
    if (source == NULL)
        return false;
    int depth = 0;
    bool string = false, escaped = false, comment = false;
    for (size_t i = 0; i < length; i++) {
        char ch = source[i];
        if (comment) {
            if (ch == '\n')
                comment = false;
            continue;
        }
        if (string) {
            if (escaped)
                escaped = false;
            else if (ch == '\\')
                escaped = true;
            else if (ch == '"')
                string = false;
            continue;
        }
        /* LispBM characters: \#x or \#\n. Their punctuation is not syntax. */
        if (ch == '\\' && i + 2 < length && source[i + 1] == '#') {
            i += 2;
            if (source[i] == '\\' && i + 1 < length)
                i++;
            continue;
        }
        if (ch == ';')
            comment = true;
        else if (ch == '"')
            string = true;
        else if (ch == '(' || ch == '[' || ch == '{')
            depth++;
        else if (ch == ')' || ch == ']' || ch == '}') {
            if (--depth < 0)
                return false;
        }
    }
    return string || depth > 0;
}
static void input_window(repl_frontend_t *frontend)
{
    lcd_terminal_input(&frontend->terminal, frontend->line_start ? "...> " : "lbm> ",
                       frontend->source + frontend->line_start, frontend->length - frontend->line_start);
}
void repl_frontend_init(repl_frontend_t *frontend)
{
    if (frontend == NULL)
        return;
    memset(frontend, 0, sizeof(*frontend));
    lcd_terminal_init(&frontend->terminal);
}
void repl_frontend_finish(repl_frontend_t *frontend)
{
    if (frontend == NULL)
        return;
    frontend->length = 0;
    frontend->line_start = 0;
    frontend->source[0] = 0;
    frontend->ready = false;
    frontend->overflowed = false;
    frontend->escape_state = 0;
    input_window(frontend);
}
repl_input_event_t repl_frontend_feed(repl_frontend_t *frontend, uint8_t byte)
{
    if (frontend == NULL)
        return REPL_IGNORED;
    if (byte == 3) {
        repl_frontend_finish(frontend);
        return REPL_CANCELLED;
    }
    if (byte == '\n' && frontend->suppress_lf) {
        frontend->suppress_lf = false;
        return REPL_IGNORED;
    }
    frontend->suppress_lf = false;
    if (frontend->ready)
        return REPL_IGNORED;
    if (frontend->escape_state) {
        if (frontend->escape_state == 1 && (byte == '[' || byte == 'O'))
            frontend->escape_state = 2;
        else if (frontend->escape_state == 1 || (byte >= 0x40 && byte <= 0x7e))
            frontend->escape_state = 0;
        return REPL_IGNORED;
    }
    if (byte == 27) {
        frontend->escape_state = 1;
        return REPL_IGNORED;
    }
    if (byte == 8 || byte == 127) {
        if (frontend->length == frontend->line_start)
            return REPL_IGNORED;
        frontend->source[--frontend->length] = 0;
        input_window(frontend);
        return REPL_CHANGED;
    }
    if (byte == 21) {
        frontend->length = frontend->line_start;
        frontend->source[frontend->length] = 0;
        frontend->overflowed = false;
        input_window(frontend);
        return REPL_CHANGED;
    }
    if (byte == '\n' || byte == '\r') {
        frontend->suppress_lf = byte == '\r';
        if (frontend->overflowed)
            return REPL_OVERFLOW;
        lcd_terminal_write(&frontend->terminal, frontend->line_start ? "...> " : "lbm> ", 5);
        lcd_terminal_write(&frontend->terminal, frontend->source + frontend->line_start,
                           frontend->length - frontend->line_start);
        lcd_terminal_write(&frontend->terminal, "\n", 1);
        if (frontend->length == 0) {
            input_window(frontend);
            return REPL_CHANGED;
        }
        if (repl_source_needs_more(frontend->source, frontend->length)) {
            if (frontend->length == REPL_SOURCE_CAPACITY - 1) {
                frontend->overflowed = true;
                return REPL_OVERFLOW;
            }
            frontend->source[frontend->length++] = '\n';
            frontend->source[frontend->length] = 0;
            frontend->line_start = frontend->length;
            input_window(frontend);
            return REPL_CHANGED;
        }
        frontend->ready = true;
        lcd_terminal_input(&frontend->terminal, "eval> ", "", 0);
        return REPL_READY;
    }
    if (byte == '\t')
        byte = ' ';
    if (byte < 32 || byte > 126)
        return REPL_IGNORED;
    if (frontend->length == REPL_SOURCE_CAPACITY - 1) {
        frontend->overflowed = true;
        return REPL_OVERFLOW;
    }
    frontend->source[frontend->length++] = (char)byte;
    frontend->source[frontend->length] = 0;
    input_window(frontend);
    return REPL_CHANGED;
}
