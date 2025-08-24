#include "crc.h"

// Mode S CRC (24-bit parity), generator polynomial per standard
static const uint32_t MODE_S_POLY = 0xFFFA0480; // reflected representation (per dump1090 approach)

// Compute parity over nBits
static uint32_t modes_checksum(const uint8_t *msg, int nBits){
  uint32_t crc = 0;
  int nBytes = (nBits+7)/8;
  for (int i=0;i<nBits;i++){
    int byte = i>>3; int bit = 7-(i&7);
    uint8_t b = (msg[byte]>>bit)&1u;
    uint8_t top = (crc>>23)&1u;
    crc = ((crc<<1)&0xFFFFFF) | b;
    if (top) crc ^= MODE_S_POLY;
  }
  return crc & 0xFFFFFF;
}

bool modes_crc_ok(const uint8_t *msg, int nBits){
  if (nBits!=56 && nBits!=112) return false;
  uint32_t crc = modes_checksum(msg, nBits-24);
  // Extract received parity
  int pStart = nBits-24;
  uint32_t rcv=0;
  for (int i=0;i<24;i++){
    int idx = pStart + i;
    int byte = idx>>3; int bit = 7-(idx&7);
    rcv = (rcv<<1) | ((msg[byte]>>bit)&1u);
  }
  return (crc == rcv);
}

// IEEE CRC32
uint32_t crc32_ieee(const uint8_t *data, size_t len){
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i=0;i<len;i++){
    uint8_t b = data[i];
    crc ^= b;
    for (int k=0;k<8;k++){
      uint32_t mask = -(crc & 1u);
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc;
}
