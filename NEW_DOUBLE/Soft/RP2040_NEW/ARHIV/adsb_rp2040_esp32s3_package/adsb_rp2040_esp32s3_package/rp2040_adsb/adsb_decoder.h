#pragma once
#include <Arduino.h>
#include "ringbuffer.h"
#include <stdint.h>

struct DecodedFrame {
  uint32_t icao;       // ICAO hex
  char callsign[9];    // 8 chars + NUL
  int squawk;          // octal squawk
  float lat, lon;      // degrees
  int altitude_m_geo;  // meters
  int altitude_m_baro; // meters
  int speed_kmh;       // km/h
  int track_deg;       // degrees
  int vert_rate_mpm;   // m/min
  uint32_t seen_time_ms;
  int aircraft_type;   // basic category
};

enum CrcStatus { CRC_OK, CRC_FIXED1, CRC_FAIL };

CrcStatus check_and_fix_crc(RawMessage &rm);
bool decode_adsb_frame(const RawMessage &rm, DecodedFrame &df, const struct Config &cfg);

uint32_t crc24_modes(const uint8_t *msg, int bits);
uint32_t crc32(const uint8_t* data, size_t len);

// CPR helpers
bool cpr_decode_global(const uint32_t even_lat_cpr, const uint32_t even_lon_cpr,
                       const uint32_t odd_lat_cpr,  const uint32_t odd_lon_cpr,
                       bool surface, float &out_lat, float &out_lon);
bool cpr_decode_local(uint32_t lat_cpr, uint32_t lon_cpr, bool surface,
                      float ref_lat, float ref_lon, float &out_lat, float &out_lon);

// Altitude decoders
int gillham2alt(uint16_t ac13, bool qbit);
