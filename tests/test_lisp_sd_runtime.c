#include "board_sdcard.h"
#include "board_sdcard_host_fake.h"
#include "lisp_runtime.h"
#include "lisp_test_storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char output[16384];
static size_t used;
static void emit(void *user, const char *text, size_t length)
{
    (void)user;
    assert(length < sizeof(output) - used);
    memcpy(output + used, text, length);
    used += length;
    output[used] = 0;
}
static void evaluate(const char *source)
{
    used = 0;
    output[0] = 0;
    assert(lisp_runtime_submit(source, strlen(source), 0));
    for (uint64_t now = 0; lisp_runtime_busy() && now < 6000; now++)
        lisp_runtime_step(now);
    assert(!lisp_runtime_busy());
}
static void expect(const char *source, const char *reply)
{
    evaluate(source);
    assert(strstr(output, reply) != NULL);
}
int main(void)
{
    board_sdcard_fake_reset();
    board_sdcard_fake_set_display(DISPLAY_ON);
    assert(board_sdcard_start() == 0);
    size_t storage_size = lisp_runtime_storage_size();
    void *storage = lisp_test_allocate(storage_size);
    assert(storage && lisp_runtime_init(storage, storage_size, emit, NULL));

    expect("(sd-status)", "=> unmounted");
    expect("(sd-diagnostics)", "phase=idle failed=idle error=0 fat=-1 close=-1 stack_min=unknown");
    expect("(sd-diagnostics 1)", "ERROR:");
    expect("(define preserved 42)", "=>");
    expect("(sd-mount)", "ERROR:");
    assert(board_sdcard_fake_request_count(BOARD_SDCARD_MOUNTING) == 0);

    board_sdcard_fake_set_display(DISPLAY_OFF);
    expect("(sd-mount)", "=> t");
    assert(board_sdcard_state() == BOARD_SDCARD_MOUNTING);
    assert(board_sdcard_fake_request_count(BOARD_SDCARD_MOUNTING) == 1);
    expect("(sd-read \"note.txt\")", "ERROR:");
    board_sdcard_fake_complete_mount(true, 0);
    expect("(sd-status)", "=> mounted");
    expect("(sd-read \"../secret\")", "ERROR:");
    expect("(sd-read \"note.txt\")", "=> t");
    expect("(sd-status)", "=> reading");
    expect("(sd-result)", "=> pending");
    board_sdcard_fake_complete_read("hello\n", 6, 0);
    expect("(sd-status)", "=> read-ready");
    expect("(sd-result)", "hello");
    expect("(sd-status)", "=> mounted");
    expect("preserved", "=> 42");
    char oversized[BOARD_SDCARD_READ_MAX + 1];
    memset(oversized, 'x', sizeof(oversized));
    expect("(sd-read \"note.txt\")", "=> t");
    board_sdcard_fake_complete_read(oversized, sizeof(oversized), 0x104);
    expect("(sd-result)", "=> error");
    expect("(sd-last-error)", "=> 260");
    expect("(sd-status)", "=> mounted");

    expect("(sd-write \"note.txt\" \"new text\")", "=> t");
    assert(board_sdcard_state() == BOARD_SDCARD_WRITING);
    expect("(sd-unmount)", "ERROR:");
    board_sdcard_fake_complete_write(true, 0);
    expect("(sd-status)", "=> write-done");
    expect("(sd-result)", "=> t");
    expect("(sd-status)", "=> mounted");

    /* Inject a completed write failure after the worker accepts the request. */
    expect("(sd-write \"note.txt\" \"partial\")", "=> t");
    board_sdcard_fake_complete_write(false, 0x101);
    board_sdcard_diagnostic_t failure = {.phase = BOARD_SD_SYNC,
                                         .failed_phase = BOARD_SD_SYNC,
                                         .error = -1,
                                         .fat_result = 1,
                                         .close_result = -1,
                                         .stack_min_free = 4000,
                                         .sequence = 7,
                                         .requested = 64,
                                         .transferred = 64};
    board_sdcard_fake_set_diagnostics(&failure);
    expect("(sd-diagnostics)",
           "phase=sync failed=sync error=-1 fat=1 close=-1 stack_min=4000 bytes=64/64 seq=7 quarantine=0");
    expect("(sd-status)", "=> operation-error");
    expect("(sd-result)", "=> error");
    expect("(sd-last-error)", "=> 257");
    expect("(sd-status)", "=> mounted");

    board_sdcard_fake_set_display(DISPLAY_ON);
    expect("(sd-unmount)", "=> t");
    assert(board_sdcard_state() == BOARD_SDCARD_UNMOUNTING);
    board_sdcard_fake_complete_unmount(true, 0);
    expect("(sd-status)", "=> unmounted");
    expect("preserved", "=> 42");
    assert(board_sdcard_fake_request_count(BOARD_SDCARD_UNMOUNTING) == 1);

    board_sdcard_fake_set_display(DISPLAY_SD_BLOCKED);
    expect("(sd-mount)", "=> t");
    board_sdcard_fake_complete_mount(false, 0x101);
    ;
    expect("(sd-status)", "=> unmounted");
    expect("(sd-last-error)", "=> 257");
    expect("(sd-read \"note.txt\")", "ERROR:");
    expect("preserved", "=> 42");

    /* A retained dirty/failed-close handle is not a consumable operation error. */
    expect("(sd-mount)", "=> t");
    board_sdcard_fake_complete_mount(true, 0);
    expect("(sd-write \"note.txt\" \"x\")", "=> t");
    failure.quarantined = true;
    board_sdcard_fake_set_diagnostics(&failure);
    board_sdcard_fake_complete_write(false, -1);
    expect("(sd-status)", "=> fault");
    expect("(sd-result)", "=> error");
    expect("(sd-status)", "=> fault");
    expect("(sd-unmount)", "ERROR:");
    expect("(sd-write \"note.txt\" \"x\")", "ERROR:");
    expect("(sd-diagnostics)", "quarantine=1");
    expect("preserved", "=> 42");

    lisp_test_free(storage, storage_size);
    puts("PASS: async LCD-off gate, SD worker API, safe paths/bounded text, result lifecycle, RAM "
         "definitions, no implicit format");
    return 0;
}
