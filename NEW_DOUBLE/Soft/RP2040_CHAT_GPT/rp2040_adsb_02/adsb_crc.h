#pragma once
#include <stdint.h>

bool adsb_crc_check(const uint8_t *msg, int bits);
bool adsb_crc_fix_1bit(uint8_t *msg, int bits); // попытка исправления 1 бита
uint32_t adsb_crc24(const uint8_t *msg, int bits);
