#include "board_sdcard_diagnostics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    board_sdcard_diagnostic_t diagnostic = {0};
    board_sdcard_diagnostic_begin(&diagnostic, 64);
    assert(diagnostic.sequence == 1 && diagnostic.phase == BOARD_SD_QUEUED);
    assert(diagnostic.fat_result == -1 && diagnostic.close_result == -1);
    assert(diagnostic.stack_min_free == UINT32_MAX);
    board_sdcard_diagnostic_record(&diagnostic, BOARD_SD_WRITE, 0, -1, 4000, 0, false);
    board_sdcard_diagnostic_record(&diagnostic, BOARD_SD_WRITE, -1, 1, 3800, 32, true);
    board_sdcard_diagnostic_record(&diagnostic, BOARD_SD_CLOSE, -1, 2, 3900, 32, true);
    assert(diagnostic.failed_phase == BOARD_SD_WRITE && diagnostic.error == -1);
    assert(diagnostic.fat_result == 1 && diagnostic.close_result == 2);
    assert(diagnostic.stack_min_free == 3800 && diagnostic.quarantined);
    char text[BOARD_SDCARD_DIAGNOSTIC_MAX];
    assert(board_sdcard_diagnostic_format(&diagnostic, text, sizeof(text)));
    assert(strstr(text, "phase=close failed=write error=-1 fat=1 close=2") != NULL);
    assert(strstr(text, "stack_min=3800 bytes=32/64 seq=1 quarantine=1") != NULL);
    char small[2] = {'x', 'y'};
    assert(!board_sdcard_diagnostic_format(&diagnostic, small, sizeof(small)));
    assert(small[0] == 0);
    assert(!board_sdcard_diagnostic_format(NULL, text, sizeof(text)));
    assert(!board_sdcard_diagnostic_format(&diagnostic, NULL, 0));
    board_sdcard_diagnostic_begin(&diagnostic, 0);
    assert(diagnostic.sequence == 2 && diagnostic.failed_phase == BOARD_SD_IDLE);
    assert(!diagnostic.quarantined && diagnostic.error == 0);
    assert(board_sdcard_diagnostic_format(&diagnostic, text, sizeof(text)));
    assert(strstr(text, "stack_min=unknown") != NULL);
    diagnostic.sequence = UINT32_MAX;
    board_sdcard_diagnostic_begin(&diagnostic, 512);
    assert(diagnostic.sequence == UINT32_MAX);
    assert(!strcmp(board_sdcard_phase_name((board_sdcard_phase_t)999), "unknown"));
    puts("PASS diagnostic first-failure/close preservation, low-water minimum, reset and bounded report");
    return 0;
}
