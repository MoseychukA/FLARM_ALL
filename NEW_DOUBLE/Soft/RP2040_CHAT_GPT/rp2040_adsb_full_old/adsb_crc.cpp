#include "adsb_crc.h"
#include <string.h>

#define MODES_GENERATOR (0xFFFA0480ULL)

static uint32_t modes_compute_crc(const uint8_t *msg, int bits) {
  int nbytes = (bits + 7) / 8;
  uint64_t data = 0;
  for (int i = 0; i < nbytes; i++) data = (data << 8) | msg[i];
  const int payloadBits = bits - 24;
  for (int i = payloadBits - 1; i >= 0; i--) {
    if (data & ((uint64_t)1 << (i + 24))) data ^= (uint64_t)MODES_GENERATOR << i;
  }
  return (uint32_t)(data & 0xFFFFFF);
}
uint32_t adsb_crc24(const uint8_t *msg, int bits) { return modes_compute_crc(msg, bits); }

bool adsb_crc_check(const uint8_t *msg, int bits) {
  if (modes_compute_crc(msg, bits) == 0) return true;
  // 1-битная коррекция
  int usableBits = bits - 24;
  uint8_t tmp[14]; int nbytes = (bits + 7) / 8;
  for (int i = 0; i < usableBits; i++) {
    memcpy(tmp, msg, nbytes);
    int byte = i / 8, bit = 7 - (i % 8);
    tmp[byte] ^= (1u << bit);
    if (modes_compute_crc(tmp, bits) == 0) return true;
  }
  return false;
}

