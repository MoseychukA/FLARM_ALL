#include "adsb_crc.h"
#include <string.h>

// Полином CRC Mode-S (24-bit)
#define MODES_GENERATOR (0xFFFA0480ULL)

uint32_t adsb_crc24(const uint8_t *msg, int bits) {
  // bits = 56 или 112
  int bytes = (bits+7)/8;
  uint64_t data = 0;
  for (int i=0;i<bytes;i++) data = (data<<8) | msg[i];

  int msgBits = bits - 24;
  for (int i=msgBits-1; i>=0; --i) {
    if (data & ((uint64_t)1 << (i+24))) {
      data ^= (uint64_t)MODES_GENERATOR << i;
    }
  }
  return (uint32_t)(data & 0xFFFFFF);
}

bool adsb_crc_check(const uint8_t *msg, int bits) {
  return adsb_crc24(msg, bits) == 0;
}

bool adsb_crc_fix_1bit(uint8_t *msg, int bits) {
  // Перебираем биты полезной части
  int payBits = bits - 24;
  uint8_t tmp[14];
  for (int i=0;i<payBits;i++) {
    memcpy(tmp, msg, (bits+7)/8);
    int byte = i/8;
    int bit  = 7 - (i%8);
    tmp[byte] ^= (1<<bit);
    if (adsb_crc24(tmp, bits) == 0) {
      memcpy(msg, tmp, (bits+7)/8);
      return true;
    }
  }
  return false;
}
