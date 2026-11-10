#include "repl_remote_logic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void source(void)
{
    assert(repl_remote_source_valid("(+ 1 2)", 7));
    assert(repl_remote_source_valid("(define x 3)\n(* x 2)\r\t", 22));
    assert(!repl_remote_source_valid(NULL, 1));
    assert(!repl_remote_source_valid("", 0));
    assert(!repl_remote_source_valid("x\0x", 3));
    assert(!repl_remote_source_valid("x\003", 2));
    assert(!repl_remote_source_valid("\377", 1));
}
static void limits(void)
{
    char data[1025];
    memset(data, 'x', sizeof(data));
    assert(repl_remote_source_valid(data, 1023));
    assert(!repl_remote_source_valid(data, 1024));
}
static void reply(void)
{
    repl_remote_reply_t r;
    repl_remote_reply_init(&r);
    repl_remote_reply_append(&r, "=> ", 3);
    repl_remote_reply_append(&r, "42\n", 3);
    repl_remote_reply_finish(&r);
    assert(!strcmp(r.text, "=> 42\n") && r.used == 6 && !r.truncated);
}
static void truncate(void)
{
    repl_remote_reply_t r;
    repl_remote_reply_init(&r);
    char large[5000];
    memset(large, 'a', sizeof(large));
    repl_remote_reply_append(&r, large, sizeof(large));
    assert(r.truncated);
    repl_remote_reply_finish(&r);
    assert(r.used < sizeof(r.text) && r.text[r.used] == 0);
    assert(strstr(r.text, "response truncated") != NULL);
}
static void bounds(void)
{
    struct {
        unsigned char a;
        repl_remote_reply_t r;
        unsigned char z;
    } guard;
    memset(&guard, 0xa5, sizeof(guard));
    repl_remote_reply_init(&guard.r);
    for (unsigned i = 0; i < 3000; i++)
        repl_remote_reply_append(&guard.r, "x", 1);
    repl_remote_reply_finish(&guard.r);
    assert(guard.a == 0xa5 && guard.z == 0xa5);
    assert(guard.r.used < sizeof(guard.r.text));
    repl_remote_reply_append(NULL, "x", 1);
    repl_remote_reply_init(NULL);
    repl_remote_reply_finish(NULL);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "source"))
        source();
    else if (!strcmp(argv[1], "limits"))
        limits();
    else if (!strcmp(argv[1], "reply"))
        reply();
    else if (!strcmp(argv[1], "truncate"))
        truncate();
    else if (!strcmp(argv[1], "bounds"))
        bounds();
    else
        return 1;
    puts("PASS");
    return 0;
}
