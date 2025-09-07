#pragma once
#include <stdint.h>
#include <stddef.h>

uint32_t adsb_crc24(const uint8_t *msg, size_t nbits);
bool     adsb_parity_ok(const uint8_t *msg, size_t nbits);
bool     adsb_fix_single_bit(uint8_t *msg, size_t nbits); // returns true if corrected
bool     adsb_crc_check(const uint8_t *msg, int bits);