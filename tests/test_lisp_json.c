#include "lisp_json.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static lisp_json_document_t doc;
static const char *parse(const char *s)
{
    return lisp_json_parse(&doc, s, strlen(s));
}
int main(void)
{
    assert(!parse("{\"current\":{\"temperature\":12.5},\"hourly\":[1,-2,3e2],\"x\":null,\"b\":false}"));
    assert(doc.tokens[0].type == LISP_JSON_OBJECT && doc.count == 14);
    const char *bad[] = {"",
                         "{",
                         "[] trailing",
                         "01",
                         "+1",
                         "NaN",
                         "1e999",
                         "9223372036854775808",
                         "{\"x\":1,}",
                         "[1,]",
                         "{x:1}",
                         "[truefalse]",
                         "\"\\u0000\"",
                         "\"\\uD800\"",
                         "\"\\uDC00\"",
                         "\"a\n\"",
                         "\"\xc0\xaf\"",
                         "\"\xed\xa0\x80\"",
                         "\"\xf4\x90\x80\x80\""};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
        assert(parse(bad[i]));
    assert(!parse("9223372036854775807"));
    assert(!parse("-9223372036854775808"));
    const char *unicode = "\"A\\u00e9\\uD83D\\uDE00\"";
    assert(!parse(unicode));
    char decoded[32];
    size_t n;
    assert(lisp_json_string(unicode, &doc.tokens[0], decoded, sizeof(decoded), &n));
    assert(n == 7 && !strcmp(decoded, "A\xc3\xa9\xf0\x9f\x98\x80"));
    assert(!lisp_json_string(unicode, &doc.tokens[0], decoded, 2, &n));
    char large[LISP_JSON_BYTES + 2];
    memset(large, ' ', sizeof(large));
    assert(lisp_json_parse(&doc, large, sizeof(large)));
    memset(large, 'x', 1025);
    large[0] = '"';
    large[1025] = '"';
    large[1026] = 0;
    assert(!parse(large));
    large[1025] = 'x';
    large[1026] = '"';
    large[1027] = 0;
    assert(parse(large));
    memset(large, '[', LISP_JSON_DEPTH + 2);
    size_t k = LISP_JSON_DEPTH + 2;
    large[k++] = '0';
    for (unsigned i = 0; i < LISP_JSON_DEPTH + 2; i++)
        large[k++] = ']';
    large[k] = 0;
    assert(parse(large));
    strcpy(large, "[");
    for (unsigned i = 0; i < LISP_JSON_NODES; i++)
        strcat(large, i ? ",0" : "0");
    strcat(large, "]");
    assert(parse(large));
    for (unsigned byte = 0; byte < 256; byte++) {
        char single[] = {'"', (char)byte, '"'};
        bool valid = byte >= 32 && byte <= 127 && byte != 92 && byte != '"';
        assert((lisp_json_parse(&doc, single, sizeof(single)) == NULL) == valid);
    }
    uint32_t seed = 0x6751u;
    for (unsigned iteration = 0; iteration < 10000; iteration++) {
        size_t length = iteration % 128;
        for (size_t i = 0; i < length; i++) {
            seed = seed * 1664525u + 1013904223u;
            large[i] = (char)(seed >> 24);
        }
        (void)lisp_json_parse(&doc, large, length);
        assert(doc.count <= LISP_JSON_NODES);
    }
    puts("PASS: JSON grammar, Unicode, numeric and resource bounds");
}
