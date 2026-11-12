#include "lisp_runtime.h"
#include "lisp_test_storage.h"
#include "lispbm.h"

/* Upstream image header needs lbm_uint declared first. */
#include "lbm_image.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *failure;
static bool succeeds(const char *stage)
{
    return failure == NULL || strcmp(failure, stage) != 0;
}
bool fault_lbm_memory_init(lbm_uint *data, lbm_uint size, lbm_uint *bits, lbm_uint bitmap_size)
{
    return succeeds("memory") && lbm_memory_init(data, size, bits, bitmap_size);
}
bool fault_lbm_symrepr_init(void)
{
    return succeeds("symbols") && lbm_symrepr_init();
}
bool fault_lbm_heap_init(lbm_cons_t *heap, lbm_uint cells, lbm_uint stack)
{
    return succeeds("heap") && lbm_heap_init(heap, cells, stack);
}
int fault_lbm_print_init(lbm_uint stack)
{
    return succeeds("print") ? lbm_print_init(stack) : 0;
}
bool fault_lbm_extensions_init(lbm_extension_t *storage, lbm_uint count)
{
    return succeeds("extensions") && lbm_extensions_init(storage, count);
}
bool fault_lbm_eval_init(void)
{
    return succeeds("evaluator") && lbm_eval_init();
}
bool fault_lbm_image_boot(void)
{
    return succeeds("image") && lbm_image_boot();
}
bool fault_lbm_add_extension(const char *name, extension_fptr function)
{
    bool app_extensions = !strcmp(name, "app-run");
    return succeeds(app_extensions                  ? "app-extensions"
                    : !strcmp(name, "app-load-url") ? "load-extension"
                    : !strcmp(name, "lcd-on")       ? "lcd-extensions"
                                                    : "print-extension") &&
           lbm_add_extension(name, function);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    failure = argv[1];
    size_t size = lisp_runtime_storage_size();
    void *storage = lisp_test_allocate(size);
    assert(storage);
    assert(!lisp_runtime_init(storage, size, NULL, NULL));
    assert(!strcmp(lisp_runtime_init_stage(), failure));
    assert(!lisp_runtime_ready() && !lisp_runtime_busy() && !lisp_runtime_selftest_passed());
    assert(!lisp_runtime_submit("(+ 1 2)", 7, 0));
    assert(!lisp_runtime_selftest_start(0));
    failure = NULL;
    assert(lisp_runtime_reset());
    assert(!strcmp(lisp_runtime_init_stage(), "ready"));
    assert(lisp_runtime_selftest_start(0));
    for (uint64_t now = 0; lisp_runtime_busy() && now < 6000; now++)
        lisp_runtime_step(now);
    assert(lisp_runtime_ready() && lisp_runtime_selftest_passed());
    lisp_test_free(storage, size);
    puts("PASS");
    return 0;
}
