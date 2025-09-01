
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef struct {
  uint32_t tbl[256];
  uint8_t  inited;
} crc24_ctx_t;

static crc24_ctx_t g_crc24_ctx;

static inline void crc24_init_table() {
  if (g_crc24_ctx.inited) return;
  const uint32_t POLY = 0x864CFB; // CRC-24 polynomial
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t crc = i << 16;
    for (int j = 0; j < 8; j++) {
      crc <<= 1;
      if (crc & 0x1000000) crc ^= POLY;
    }
    g_crc24_ctx.tbl[i] = crc & 0xFFFFFF;
  }
  g_crc24_ctx.inited = 1;
}

static inline uint32_t crc24_update(uint32_t crc, uint8_t data) {
  uint8_t idx = (uint8_t)((crc >> 16) ^ data);
  crc = ((crc << 8) & 0xFFFFFF) ^ g_crc24_ctx.tbl[idx];
  return crc & 0xFFFFFF;
}

static inline uint32_t modes_crc24(const uint8_t* data, size_t nbytes) {
  crc24_init_table();
  uint32_t crc = 0;
  for (size_t i = 0; i < nbytes; i++) crc = crc24_update(crc, data[i]);
  return crc & 0xFFFFFF;
}

static inline uint32_t modes_parity_syndrome(const uint8_t* full, size_t nbytes) {
  uint32_t parity = (full[nbytes-3] << 16) | (full[nbytes-2] << 8) | full[nbytes-1];
  uint32_t crc = modes_crc24(full, nbytes - 3);
  return (crc ^ parity) & 0xFFFFFF;
}

static inline int modes_try_single_bit_fix(uint8_t* msg, size_t nbits) {
  crc24_init_table();
  const size_t nbytes = (nbits + 7) / 8;
  for (size_t b = 0; b < nbits; b++) {
    size_t i = b >> 3;
    uint8_t m = 1u << (7 - (b & 7));
    msg[i] ^= m;
    uint32_t synd = modes_parity_syndrome(msg, nbytes);
    if (synd == 0) return (int)b;
    msg[i] ^= m;
  }
  return -1;
}
