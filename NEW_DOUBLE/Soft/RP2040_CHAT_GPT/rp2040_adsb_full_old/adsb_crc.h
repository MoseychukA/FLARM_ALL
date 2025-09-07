#pragma once
#include <stdint.h>
bool     adsb_crc_check(const uint8_t *msg, int bits);
uint32_t adsb_crc24(const uint8_t *msg, int bits);