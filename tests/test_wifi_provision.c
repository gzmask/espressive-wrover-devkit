#include "wifi_provision.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        if (!(c)) {                                                                                          \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                                          \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
static const uint64_t nonce = UINT64_C(0x123456789abcdef0);
static size_t frame(uint8_t b[WIFI_PROVISION_FRAME_MAX], size_t sn, size_t pn)
{
    memset(b, 0, WIFI_PROVISION_FRAME_MAX);
    memcpy(b, "WRV1", 4);
    for (unsigned i = 0; i < 8; i++)
        b[4 + i] = (uint8_t)(nonce >> (8 * i));
    b[12] = (uint8_t)sn;
    b[13] = (uint8_t)pn;
    memset(b + 14, 's', sn);
    memset(b + 14 + sn, 'p', pn);
    size_t n = 14 + sn + pn;
    uint32_t crc = wifi_provision_crc32(b, n);
    for (unsigned i = 0; i < 4; i++)
        b[n + i] = (uint8_t)(crc >> (8 * i));
    return n + 4;
}
static bool zero(const void *p, size_t n)
{
    const uint8_t *b = p;
    for (size_t i = 0; i < n; i++)
        if (b[i])
            return false;
    return true;
}
static void valid(void)
{
    uint8_t b[WIFI_PROVISION_FRAME_MAX];
    wifi_credentials_t out;
    CHECK(wifi_provision_crc32((const uint8_t *)"123456789", 9) == UINT32_C(0xcbf43926));
    size_t n = frame(b, 4, 8);
    CHECK(wifi_provision_frame_size(b, 14) == n);
    CHECK(wifi_provision_decode(b, n, nonce, &out));
    CHECK(out.ssid_length == 4 && out.password_length == 8);
    CHECK(memcmp(out.ssid, "ssss", 4) == 0 && memcmp(out.password, "pppppppp", 8) == 0);
    wifi_provision_wipe(&out, sizeof(out));
    CHECK(zero(&out, sizeof(out)));
    n = frame(b, 32, 63);
    CHECK(wifi_provision_decode(b, n, nonce, &out));
    CHECK(out.ssid_length == 32 && out.password_length == 63);
    n = frame(b, 1, 64);
    CHECK(!wifi_provision_decode(b, n, nonce, &out)); /* not hexadecimal */
    memset(b + 15, 'a', 64);
    uint32_t crc = wifi_provision_crc32(b, n - 4);
    for (unsigned i = 0; i < 4; i++)
        b[n - 4 + i] = (uint8_t)(crc >> (8 * i));
    CHECK(wifi_provision_decode(b, n, nonce, &out));
}
static void malformed(void)
{
    uint8_t b[WIFI_PROVISION_FRAME_MAX];
    wifi_credentials_t out;
    size_t n = frame(b, 4, 8);
    for (size_t i = 0; i < n; i++) {
        memset(&out, 0xaa, sizeof(out));
        CHECK(!wifi_provision_decode(b, i, nonce, &out));
        CHECK(zero(&out, sizeof(out)));
    }
    for (size_t i = 0; i < n; i++) {
        b[i] ^= 1;
        memset(&out, 0xaa, sizeof(out));
        CHECK(!wifi_provision_decode(b, n, nonce, &out));
        CHECK(zero(&out, sizeof(out)));
        b[i] ^= 1;
    }
    CHECK(!wifi_provision_decode(b, n, nonce + 1, &out));
    CHECK(!wifi_provision_decode(NULL, n, nonce, &out));
    CHECK(!wifi_provision_decode(b, n, nonce, NULL));
    CHECK(wifi_provision_frame_size(NULL, 14) == 0);
    b[12] = 0;
    CHECK(wifi_provision_frame_size(b, 14) == 0);
    b[12] = 33;
    CHECK(wifi_provision_frame_size(b, 14) == 0);
    b[12] = 4;
    b[13] = 7;
    CHECK(wifi_provision_frame_size(b, 14) == 0);
    b[13] = 65;
    CHECK(wifi_provision_frame_size(b, 14) == 0);
}
static void controls(void)
{
    uint8_t b[WIFI_PROVISION_FRAME_MAX];
    wifi_credentials_t out;
    for (unsigned which = 0; which < 2; which++) {
        size_t n = frame(b, 4, 8);
        b[which ? 18 : 14] = 0x1b;
        uint32_t crc = wifi_provision_crc32(b, n - 4);
        for (unsigned i = 0; i < 4; i++)
            b[n - 4 + i] = (uint8_t)(crc >> (8 * i));
        CHECK(!wifi_provision_decode(b, n, nonce, &out) && zero(&out, sizeof(out)));
    }
}
int main(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    if (strcmp(argv[1], "valid") == 0)
        valid();
    else if (strcmp(argv[1], "malformed") == 0)
        malformed();
    else if (strcmp(argv[1], "controls") == 0)
        controls();
    else
        return 1;
    puts("PASS");
    return 0;
}
