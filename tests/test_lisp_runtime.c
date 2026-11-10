#include "lisp_runtime.h"
#include "lisp_test_storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char output[16384];
static size_t used;
static void emit(void *user, const char *s, size_t n)
{
    (void)user;
    assert(n < sizeof(output) - used);
    memcpy(output + used, s, n);
    used += n;
    output[used] = 0;
}
static void evaluate(const char *source)
{
    used = 0;
    output[0] = 0;
    assert(lisp_runtime_submit(source, strlen(source), 0));
    for (uint64_t i = 0; i < 6000 && lisp_runtime_busy(); i++)
        lisp_runtime_step(i);
    assert(!lisp_runtime_busy());
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    size_t n = lisp_runtime_storage_size();
    size_t allocation = n + LISP_RUNTIME_STORAGE_ALIGNMENT;
    void *storage = lisp_test_allocate(allocation);
    assert(storage);
    assert(lisp_runtime_storage_alignment() == 8);
    assert(!lisp_runtime_init((char *)storage + 4, n, emit, NULL));
    assert(!strcmp(lisp_runtime_init_stage(), "storage"));
    assert(!lisp_runtime_init(storage, n - 1, emit, NULL));
    assert(lisp_runtime_init(storage, n, emit, NULL));
    assert(!strcmp(lisp_runtime_init_stage(), "ready"));
    if (!strcmp(argv[1], "selftest")) {
        assert(!lisp_runtime_selftest_passed());
        assert(lisp_runtime_selftest_start(0));
        for (uint64_t i = 0; lisp_runtime_busy() && i < 6000; i++)
            lisp_runtime_step(i);
        assert(lisp_runtime_selftest_passed());
        assert(used == 0 && output[0] == 0); /* Boot probe must not scroll the terminal. */
        evaluate("(+ 2 3)");
        assert(strstr(output, "=> 5"));
        assert(lisp_runtime_reset());
        assert(lisp_runtime_ready() && lisp_runtime_selftest_passed());
    } else if (!strcmp(argv[1], "arithmetic")) {
        evaluate("(+ 1 2)");
        assert(strstr(output, "3") != NULL);
        evaluate("(* 2.5 4.0)");
        assert(strstr(output, "10") != NULL);
    } else if (!strcmp(argv[1], "definitions")) {
        evaluate("(define twice (lambda (x) (* x 2)))");
        evaluate("(twice 21)");
        assert(strstr(output, "42") != NULL);
    } else if (!strcmp(argv[1], "errors")) {
        evaluate("(no-such-function 1)");
        assert(strstr(output, "ERROR") != NULL);
        evaluate("(+ 1");
        assert(strstr(output, "ERROR") != NULL);
        evaluate("(+ 2 3)");
        assert(strstr(output, "5") != NULL);
    } else if (!strcmp(argv[1], "print")) {
        evaluate("(print \"hello\") (+ 3 4)");
        assert(strstr(output, "hello") && strstr(output, "7"));
    } else if (!strcmp(argv[1], "reset")) {
        for (unsigned i = 0; i < 30; i++) {
            evaluate("(define foo 123)");
            assert(lisp_runtime_reset());
            evaluate("foo");
            assert(strstr(output, "ERROR"));
        }
        evaluate("(+ 1 2)");
        assert(strstr(output, "3"));
    } else if (!strcmp(argv[1], "timeout")) {
        evaluate("(define forever (lambda () (forever))) (forever)");
        assert(strstr(output, "timeout"));
        evaluate("(+ 1 2)");
        assert(strstr(output, "3"));
    } else if (!strcmp(argv[1], "output")) {
        evaluate("(define spam (lambda (n) (if (= n 0) t (progn (print "
                 "\"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\") (spam (- n 1)))))) "
                 "(spam 200)");
        assert(strstr(output, "WARNING: print output truncated"));
        assert(strstr(output, "=> t"));
    } else if (!strcmp(argv[1], "background")) {
        evaluate("(define forever (lambda () (forever))) (spawn forever)");
        assert(strstr(output, "detached contexts"));
        evaluate("(+ 1 2)");
        assert(strstr(output, "3"));
    } else if (!strcmp(argv[1], "bounds")) {
        char source[1100];
        memset(source, 'a', sizeof(source));
        assert(!lisp_runtime_submit(source, sizeof(source), 0));
        assert(!lisp_runtime_submit(NULL, 0, 0));
        assert(lisp_runtime_submit("(+ 1 2)", 7, 0));
        assert(!lisp_runtime_submit("3", 1, 0));
        assert(lisp_runtime_reset());
    } else if (!strcmp(argv[1], "gc")) {
        for (unsigned i = 0; i < 1500; i++)
            evaluate("(list 1 2 3 4 5)");
        evaluate("(+ 1 2)");
        assert(strstr(output, "3"));
    } else
        return 1;
    lisp_test_free(storage, allocation);
    puts("PASS");
    return 0;
}
