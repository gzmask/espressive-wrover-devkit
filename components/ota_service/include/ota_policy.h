#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OTA_SLOT_SIZE UINT32_C(0x150000)
bool ota_target_allowed(uint32_t running, uint32_t target, uint32_t size, uint8_t subtype);
bool ota_authorized(const char *header, size_t length, const uint8_t token[32]);
bool ota_decode_sha256(const char *hex, size_t length, uint8_t output[32]);
