#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { WIFI_PROVISION_HEADER_BYTES = 14, WIFI_PROVISION_FRAME_MAX = 114 };
typedef struct {
    uint8_t ssid[32];
    uint8_t password[64];
    uint8_t ssid_length;
    uint8_t password_length;
} wifi_credentials_t;

/* Local USB transport only: CRC detects corruption; nonce rejects stale frames.
 * Neither provides peer authentication/encryption. Input/output must not alias. */
uint32_t wifi_provision_crc32(const uint8_t *data, size_t length);
size_t wifi_provision_frame_size(const uint8_t *header, size_t length);
bool wifi_provision_decode(const uint8_t *frame, size_t length, uint64_t nonce, wifi_credentials_t *out);
void wifi_provision_wipe(void *data, size_t length);

/* Two lines: SSID and password, LF/CRLF, optional final newline. No trimming.
 * Output must not alias input; failure clears output. */
bool wifi_credentials_from_text(const uint8_t *data, size_t length, wifi_credentials_t *out);
