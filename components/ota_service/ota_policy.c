#include "ota_policy.h"
#include <string.h>

bool ota_target_allowed(uint32_t running, uint32_t target, uint32_t size, uint8_t subtype)
{
    return size == OTA_SLOT_SIZE && ((running == 0x10000 && target == 0x160000 && subtype == 0x11) ||
                                     (running == 0x160000 && target == 0x10000 && subtype == 0x10));
}

bool ota_authorized(const char *header, size_t length, const uint8_t token[32])
{
    if (header == NULL || token == NULL || length != 71 || memcmp(header, "Bearer ", 7) != 0) {
        return false;
    }
    static const char hex[] = "0123456789abcdef";
    volatile unsigned difference = 0;
    for (size_t i = 0; i < 32; ++i) {
        difference |= (unsigned)((uint8_t)header[7 + i * 2] ^ (uint8_t)hex[token[i] >> 4]);
        difference |= (unsigned)((uint8_t)header[8 + i * 2] ^ (uint8_t)hex[token[i] & 15]);
    }
    return difference == 0;
}

static int hex_value(unsigned char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

bool ota_decode_sha256(const char *hex, size_t length, uint8_t output[32])
{
    if (output == NULL)
        return false;
    memset(output, 0, 32);
    if (hex == NULL || length != 64)
        return false;
    for (size_t i = 0; i < 32; ++i) {
        int high = hex_value((unsigned char)hex[i * 2]);
        int low = hex_value((unsigned char)hex[i * 2 + 1]);
        if (high < 0 || low < 0) {
            memset(output, 0, 32);
            return false;
        }
        output[i] = (uint8_t)(high * 16 + low);
    }
    return true;
}
