#include "wifi_provision.h"
#include <string.h>

void wifi_provision_wipe(void *data, size_t length)
{
    volatile uint8_t *bytes = data;
    if (bytes != NULL)
        while (length-- > 0)
            *bytes++ = 0;
}
static bool copy_credentials(const uint8_t *ssid, size_t sn, const uint8_t *password, size_t pn,
                             wifi_credentials_t *out)
{
    if (sn < 1 || sn > 32 || pn < 8 || pn > 64)
        return false;
    for (size_t i = 0; i < sn; i++)
        if (ssid[i] < 0x20 || ssid[i] == 0x7f)
            return false;
    for (size_t i = 0; i < pn; i++) {
        uint8_t c = password[i];
        if (pn == 64) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                return false;
        } else if (c < 0x20 || c > 0x7e)
            return false;
    }
    memcpy(out->ssid, ssid, sn);
    memcpy(out->password, password, pn);
    out->ssid_length = (uint8_t)sn;
    out->password_length = (uint8_t)pn;
    return true;
}

uint32_t wifi_provision_crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = UINT32_MAX;
    if (data == NULL && length != 0)
        return 0;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((crc & 1U) ? UINT32_C(0xedb88320) : 0);
    }
    return crc ^ UINT32_MAX;
}
size_t wifi_provision_frame_size(const uint8_t *header, size_t length)
{
    if (header == NULL || length < WIFI_PROVISION_HEADER_BYTES || memcmp(header, "WRV1", 4) != 0 ||
        header[12] < 1 || header[12] > 32 || header[13] < 8 || header[13] > 64)
        return 0;
    return (size_t)WIFI_PROVISION_HEADER_BYTES + header[12] + header[13] + 4;
}
bool wifi_provision_decode(const uint8_t *frame, size_t length, uint64_t nonce, wifi_credentials_t *out)
{
    if (out == NULL)
        return false;
    wifi_provision_wipe(out, sizeof(*out));
    size_t expected = wifi_provision_frame_size(frame, length);
    if (expected == 0 || expected != length)
        return false;
    uint64_t token = 0;
    for (unsigned i = 0; i < 8; i++)
        token |= (uint64_t)frame[4 + i] << (8 * i);
    if (token != nonce)
        return false;
    uint32_t crc = 0;
    for (unsigned i = 0; i < 4; i++)
        crc |= (uint32_t)frame[length - 4 + i] << (8 * i);
    if (wifi_provision_crc32(frame, length - 4) != crc)
        return false;
    size_t sn = frame[12], pn = frame[13];
    return copy_credentials(frame + 14, sn, frame + 14 + sn, pn, out);
}

bool wifi_credentials_from_text(const uint8_t *data, size_t length, wifi_credentials_t *out)
{
    if (out == NULL)
        return false;
    wifi_provision_wipe(out, sizeof(*out));
    if (data == NULL || length == 0 || length > 256)
        return false;
    if (data[length - 1] == '\n')
        length--;
    const uint8_t *separator = memchr(data, '\n', length);
    if (separator == NULL)
        return false;
    size_t sn = (size_t)(separator - data);
    const uint8_t *password = separator + 1;
    size_t pn = length - sn - 1;
    if (sn > 0 && data[sn - 1] == '\r')
        sn--;
    if (pn > 0 && password[pn - 1] == '\r')
        pn--;
    return copy_credentials(data, sn, password, pn, out);
}
