#include "lisp_json.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int hex(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
static bool hex4(const char *s, size_t n, size_t *i, uint32_t *cp)
{
    if (n - *i < 4)
        return false;
    *cp = 0;
    for (unsigned k = 0; k < 4; k++) {
        int h = hex(s[(*i)++]);
        if (h < 0)
            return false;
        *cp = (*cp << 4) | (unsigned)h;
    }
    return true;
}
bool lisp_json_string(const char *s, const lisp_json_token_t *t, char *out, size_t cap, size_t *length)
{
    if (!s || !t || t->type != LISP_JSON_STRING || !length)
        return false;
    s += t->start;
    size_t i = 0, used = 0, n = t->length;
    while (i < n) {
        uint32_t cp = (unsigned char)s[i++];
        if (cp == '\\') {
            if (i == n)
                return false;
            char c = s[i++];
            switch (c) {
            case '"':
            case '\\':
            case '/':
                cp = (unsigned char)c;
                break;
            case 'b':
                cp = 8;
                break;
            case 'f':
                cp = 12;
                break;
            case 'n':
                cp = 10;
                break;
            case 'r':
                cp = 13;
                break;
            case 't':
                cp = 9;
                break;
            case 'u':
                if (!hex4(s, n, &i, &cp))
                    return false;
                if (cp >= 0xd800 && cp <= 0xdbff) {
                    uint32_t low;
                    if (n - i < 6 || s[i] != '\\' || s[i + 1] != 'u')
                        return false;
                    i += 2;
                    if (!hex4(s, n, &i, &low) || low < 0xdc00 || low > 0xdfff)
                        return false;
                    cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
                } else if (cp >= 0xdc00 && cp <= 0xdfff)
                    return false;
                break;
            default:
                return false;
            }
        } else if (cp < 32 || cp == '"')
            return false;
        else if (cp >= 128) {
            unsigned more;
            uint32_t min;
            if (cp >= 0xc2 && cp <= 0xdf) {
                more = 1;
                min = 0x80;
                cp &= 0x1f;
            } else if (cp >= 0xe0 && cp <= 0xef) {
                more = 2;
                min = 0x800;
                cp &= 0xf;
            } else if (cp >= 0xf0 && cp <= 0xf4) {
                more = 3;
                min = 0x10000;
                cp &= 7;
            } else
                return false;
            if (n - i < more)
                return false;
            for (unsigned k = 0; k < more; k++) {
                unsigned char b = (unsigned char)s[i++];
                if ((b & 0xc0) != 0x80)
                    return false;
                cp = (cp << 6) | (b & 0x3f);
            }
            if (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
                return false;
        }
        if (!cp)
            return false; /* Lisp strings are NUL terminated. */
        unsigned bytes = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
        if (used + bytes > LISP_JSON_STRING_MAX || (out && used + bytes >= cap))
            return false;
        if (out) {
            if (bytes == 1)
                out[used] = (char)cp;
            else {
                out[used] = (char)((bytes == 2   ? 0xc0u
                                    : bytes == 3 ? 0xe0u
                                                 : 0xf0u) |
                                   (cp >> (6 * (bytes - 1))));
                for (unsigned k = 1; k < bytes; k++)
                    out[used + k] = (char)(0x80u | ((cp >> (6 * (bytes - k - 1))) & 0x3f));
            }
        }
        used += bytes;
    }
    if (out) {
        if (used >= cap)
            return false;
        out[used] = 0;
    }
    *length = used;
    return true;
}
typedef struct {
    lisp_json_document_t *doc;
    const char *text;
    size_t length, pos;
    const char *error;
} parser_t;
static void space(parser_t *p)
{
    while (p->pos < p->length && (p->text[p->pos] == ' ' || p->text[p->pos] == '\n' ||
                                  p->text[p->pos] == '\r' || p->text[p->pos] == '\t'))
        p->pos++;
}
static bool digit(char c)
{
    return c >= '0' && c <= '9';
}
static bool number(parser_t *p, lisp_json_token_t *t)
{
    size_t start = p->pos;
    if (p->text[p->pos] == '-')
        p->pos++;
    if (p->pos == p->length || !digit(p->text[p->pos]))
        return false;
    if (p->text[p->pos++] != '0')
        while (p->pos < p->length && digit(p->text[p->pos]))
            p->pos++;
    bool real = false;
    if (p->pos < p->length && p->text[p->pos] == '.') {
        real = true;
        p->pos++;
        if (p->pos == p->length || !digit(p->text[p->pos]))
            return false;
        while (p->pos < p->length && digit(p->text[p->pos]))
            p->pos++;
    }
    if (p->pos < p->length && (p->text[p->pos] == 'e' || p->text[p->pos] == 'E')) {
        real = true;
        p->pos++;
        if (p->pos < p->length && (p->text[p->pos] == '+' || p->text[p->pos] == '-'))
            p->pos++;
        if (p->pos == p->length || !digit(p->text[p->pos]))
            return false;
        while (p->pos < p->length && digit(p->text[p->pos]))
            p->pos++;
    }
    size_t n = p->pos - start;
    if (n > 63)
        return false;
    char text[64];
    memcpy(text, p->text + start, n);
    text[n] = 0;
    errno = 0;
    if (real) {
        double value = strtod(text, NULL);
        if (errno || !isfinite(value))
            return false;
    } else {
        (void)strtoll(text, NULL, 10);
        if (errno)
            return false;
    }
    t->type = real ? LISP_JSON_NUMBER : LISP_JSON_INTEGER;
    return true;
}
static uint16_t value(parser_t *p, unsigned depth)
{
    space(p);
    if (depth > LISP_JSON_DEPTH) {
        p->error = "depth-limit";
        return LISP_JSON_NONE;
    }
    if (p->doc->count == LISP_JSON_NODES) {
        p->error = "node-limit";
        return LISP_JSON_NONE;
    }
    if (p->pos == p->length)
        return LISP_JSON_NONE;
    uint16_t ix = p->doc->count++;
    lisp_json_token_t *t = &p->doc->tokens[ix];
    *t = (lisp_json_token_t){.start = (uint16_t)p->pos, .child = LISP_JSON_NONE, .next = LISP_JSON_NONE};
    char c = p->text[p->pos];
    if (c == '[' || c == '{') {
        bool object = c == '{';
        t->type = object ? LISP_JSON_OBJECT : LISP_JSON_ARRAY;
        char end = object ? '}' : ']';
        p->pos++;
        space(p);
        uint16_t previous = LISP_JSON_NONE;
        if (p->pos < p->length && p->text[p->pos] == end)
            p->pos++;
        else
            for (;;) {
                if (object && (p->pos == p->length || p->text[p->pos] != '"'))
                    return LISP_JSON_NONE;
                uint16_t child = value(p, depth + 1);
                if (child == LISP_JSON_NONE)
                    return child;
                if (previous == LISP_JSON_NONE)
                    t->child = child;
                else
                    p->doc->tokens[previous].next = child;
                previous = child;
                if (object) {
                    space(p);
                    if (p->pos == p->length || p->text[p->pos++] != ':')
                        return LISP_JSON_NONE;
                    child = value(p, depth + 1);
                    if (child == LISP_JSON_NONE)
                        return child;
                    p->doc->tokens[previous].next = child;
                    previous = child;
                }
                space(p);
                if (p->pos == p->length)
                    return LISP_JSON_NONE;
                if (p->text[p->pos] == end) {
                    p->pos++;
                    break;
                }
                if (p->text[p->pos++] != ',')
                    return LISP_JSON_NONE;
                space(p);
            }
    } else if (c == '"') {
        t->type = LISP_JSON_STRING;
        t->start = (uint16_t)++p->pos;
        while (p->pos < p->length && p->text[p->pos] != '"') {
            if (p->text[p->pos++] == '\\') {
                if (p->pos == p->length)
                    return LISP_JSON_NONE;
                p->pos++;
            }
        }
        if (p->pos == p->length)
            return LISP_JSON_NONE;
        t->length = (uint16_t)(p->pos++ - t->start);
        size_t decoded;
        if (!lisp_json_string(p->text, t, NULL, 0, &decoded))
            return LISP_JSON_NONE;
        return ix;
    } else if (c == '-' || digit(c)) {
        if (!number(p, t))
            return LISP_JSON_NONE;
    } else {
        const char *literal = c == 't' ? "true" : c == 'f' ? "false" : "null";
        t->type = c == 't' ? LISP_JSON_TRUE : c == 'f' ? LISP_JSON_FALSE : LISP_JSON_NULL;
        size_t n = strlen(literal);
        if (p->length - p->pos < n || memcmp(p->text + p->pos, literal, n))
            return LISP_JSON_NONE;
        p->pos += n;
    }
    t->length = (uint16_t)(p->pos - t->start);
    return ix;
}
const char *lisp_json_parse(lisp_json_document_t *doc, const char *text, size_t length)
{
    if (!doc || !text || !length)
        return "invalid-json";
    doc->count = 0;
    if (length > LISP_JSON_BYTES)
        return "too-large";
    parser_t p = {.doc = doc, .text = text, .length = length};
    uint16_t root = value(&p, 0);
    space(&p);
    return root == 0 && p.pos == length ? NULL : p.error ? p.error : "invalid-json";
}
