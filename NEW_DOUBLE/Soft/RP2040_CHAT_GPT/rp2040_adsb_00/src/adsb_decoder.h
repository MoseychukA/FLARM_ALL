#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_BITS    120
#define MAX_BYTES   15

typedef struct {
  uint8_t  chan;           // 1..3
  uint32_t t0_us;          // preamble start estimate
  uint8_t  nbits;          // 56 or 112
  uint8_t  data[MAX_BYTES];// raw (7 or 14 bytes)
  int      rssi;           // RSSI estimate
  bool     crc_ok;
  bool     corrected;      // single-bit corrected
} adsb_packet_t;

typedef struct {
  uint32_t addr;
  char     squawk[8];
  char     flight[16];
  int32_t  altitude;     // meters
  uint16_t speed;        // km/h
  uint16_t track;        // deg
  int16_t  vert_rate;    // m/min
  float    lat, lon;
  uint32_t seen_time;    // ms
  uint8_t  type;         // aircraft type (rough)
} adsb_decoded_t;

void adsb_init();
bool adsb_decode(const adsb_packet_t* pkt, adsb_decoded_t* out);

// correlation/preamble detector params tweak
void set_corr_profile(bool high_emi);

// raw dump toggle
extern volatile bool g_rawDump;

// push raw dump (pre-CRC) to USB
void dump_raw(const adsb_packet_t* pkt);

// callback from sampler (core1) to push packets
bool enqueue_packet_from_core1(const adsb_packet_t& pkt);
