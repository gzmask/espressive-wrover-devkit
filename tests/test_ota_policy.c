#include "ota_policy.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    uint8_t token[32] = {0};
    char auth[72] = "Bearer ";
    memset(auth + 7, '0', 64);
    auth[71] = 0;
    assert(ota_authorized(auth, 71, token));
    for (size_t i = 7; i < 71; ++i) {
        auth[i] = '1';
        assert(!ota_authorized(auth, 71, token));
        auth[i] = '0';
    }
    assert(!ota_authorized(auth, 70, token));
    assert(!ota_authorized(NULL, 71, token));
    assert(!ota_authorized(auth, 71, NULL));
    auth[0] = 'b';
    assert(!ota_authorized(auth, 71, token));

    uint8_t digest[32];
    char hex[65];
    memset(hex, 'a', 64);
    hex[64] = 0;
    assert(ota_decode_sha256(hex, 64, digest));
    for (size_t i = 0; i < 32; ++i)
        assert(digest[i] == 0xaa);
    hex[63] = 'z';
    assert(!ota_decode_sha256(hex, 64, digest));
    for (size_t i = 0; i < 32; ++i)
        assert(digest[i] == 0);
    assert(!ota_decode_sha256(hex, 63, digest));
    assert(!ota_decode_sha256(NULL, 64, digest));
    assert(!ota_decode_sha256(hex, 64, NULL));

    assert(ota_target_allowed(0x10000, 0x160000, OTA_SLOT_SIZE, 0x11));
    assert(ota_target_allowed(0x160000, 0x10000, OTA_SLOT_SIZE, 0x10));
    assert(!ota_target_allowed(0x10000, 0x10000, OTA_SLOT_SIZE, 0x10));
    assert(!ota_target_allowed(0x160000, 0x160000, OTA_SLOT_SIZE, 0x11));
    assert(!ota_target_allowed(0x10000, 0x9000, OTA_SLOT_SIZE, 0x11));
    assert(!ota_target_allowed(0x10000, 0x160000, OTA_SLOT_SIZE - 1, 0x11));
    assert(!ota_target_allowed(0x10000, 0x160000, OTA_SLOT_SIZE, 0x10));
    assert(!ota_target_allowed(0x20000, 0x160000, OTA_SLOT_SIZE, 0x11));
    return 0;
}
