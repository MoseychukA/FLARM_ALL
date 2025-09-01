#pragma once
#include <Arduino.h>

bool modes_crc_ok(const uint8_t *msg, int nBits);
uint32_t crc32_ieee(const uint8_t *data, size_t len);
