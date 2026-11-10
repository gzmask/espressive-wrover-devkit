#include "repl_frontend.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void feed(repl_frontend_t *f, const char *s)
{
    while (*s)
        (void)repl_frontend_feed(f, (uint8_t)*s++);
}
static void edit(void)
{
    repl_frontend_t f;
    repl_frontend_init(&f);
    feed(&f, "(+ 1 3");
    assert(repl_frontend_feed(&f, 127) == REPL_CHANGED);
    feed(&f, "2)");
    assert(repl_frontend_feed(&f, '\r') == REPL_READY);
    assert(!strcmp(f.source, "(+ 1 2)"));
    assert(repl_frontend_feed(&f, 'x') == REPL_IGNORED);
    repl_frontend_finish(&f);
    assert(repl_frontend_feed(&f, '\r') == REPL_CHANGED);
    assert(repl_frontend_feed(&f, '\n') == REPL_IGNORED);
    feed(&f, "xyz");
    assert(repl_frontend_feed(&f, 21) == REPL_CHANGED && f.length == 0);
    feed(&f, "abc");
    assert(repl_frontend_feed(&f, 3) == REPL_CANCELLED && f.length == 0);
}
static void multiline(void)
{
    repl_frontend_t f;
    repl_frontend_init(&f);
    feed(&f, "(define add\n");
    assert(!f.ready && f.line_start > 0);
    feed(&f, " (lambda (a b) (+ a b)))\n");
    assert(f.ready);
    assert(strstr(f.source, "\n") != NULL);
    assert(repl_source_needs_more("(print \"()\" ; )\n", strlen("(print \"()\" ; )\n")));
    assert(!repl_source_needs_more("\\#( \\#)", 7));
    assert(!repl_source_needs_more("\"[()]\"", 6));
    assert(repl_source_needs_more("\"abc", 4));
    assert(!repl_source_needs_more(")", 1)); /* Let the real reader reject it. */
}
static void bounds(void)
{
    repl_frontend_t f;
    repl_frontend_init(&f);
    for (size_t i = 0; i < REPL_SOURCE_CAPACITY - 1; i++)
        assert(repl_frontend_feed(&f, 'a') == REPL_CHANGED);
    assert(repl_frontend_feed(&f, 'b') == REPL_OVERFLOW);
    assert(f.length == REPL_SOURCE_CAPACITY - 1 && f.source[f.length] == 0);
    assert(repl_frontend_feed(&f, '\n') == REPL_OVERFLOW && !f.ready);
    assert(repl_frontend_feed(&f, 3) == REPL_CANCELLED);
    feed(&f, "\033[A\033[B\033[C\033[D");
    assert(f.length == 0);
    assert(repl_frontend_feed(&f, 255) == REPL_IGNORED);
    assert(repl_frontend_feed(NULL, 'x') == REPL_IGNORED);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "edit"))
        edit();
    else if (!strcmp(argv[1], "multiline"))
        multiline();
    else if (!strcmp(argv[1], "bounds"))
        bounds();
    else
        return 1;
    puts("PASS");
    return 0;
}
