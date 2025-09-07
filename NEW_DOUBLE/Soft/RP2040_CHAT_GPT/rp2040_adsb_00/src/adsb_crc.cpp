
#include "adsb_crc.h"

// Mode-S CRC-24 generator poly
static const uint32_t GEN = 0xFFF409u;

uint32_t adsb_crc24(const uint8_t *msg, size_t nbits) {
  // bits without the last 24 parity bits
  size_t paybits = nbits - 24;
  uint32_t crc = 0;
  for (size_t i=0;i<paybits;i++) {
    size_t byte = i >> 3;
    int bit = 7 - (i & 7);
    uint8_t b = (msg[byte] >> bit) & 1;
    if (((crc >> 23) & 1) ^ b) crc = ((crc << 1) ^ GEN) & 0xFFFFFFu;
    else crc = (crc << 1) & 0xFFFFFFu;
  }
  return crc;
}

bool adsb_parity_ok(const uint8_t *msg, size_t nbits) {
  size_t nb = nbits/8;
  uint32_t p = (msg[nb-3]<<16) | (msg[nb-2]<<8) | msg[nb-1];
  return adsb_crc24(msg, nbits) == p;
}

bool adsb_fix_single_bit(uint8_t *msg, size_t nbits) {
  if (adsb_parity_ok(msg, nbits)) return true;
  // try single-bit flip across payload bits only
  size_t paybits = nbits - 24;
  for (size_t i=0;i<paybits;i++) {
    size_t byte = i >> 3;
    int bit = 7 - (i & 7);
    msg[byte] ^= (1u << bit);
    if (adsb_parity_ok(msg, nbits)) return true;
    msg[byte] ^= (1u << bit); // revert
  }
  return false;
}