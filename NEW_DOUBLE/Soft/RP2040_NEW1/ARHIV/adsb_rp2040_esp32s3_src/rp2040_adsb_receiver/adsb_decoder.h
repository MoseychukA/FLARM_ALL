#pragma once
#include <Arduino.h>
#include "cpr.h"

struct DecodedADSB {
  uint32_t addr = 0;
  char flight[9] = {0};
  uint16_t squawk = 0;
  double lat = 0.0;
  double lon = 0.0;
  int32_t altitude_m = 0;
  int32_t geo_alt_m = 0;
  uint16_t speed_kmh = 0;
  uint16_t track_deg = 0;
  int16_t  vert_rate_fpm = 0;
  uint32_t seen_time_ms = 0;
  uint32_t timestamp = 0;
};

bool adsb_decode(const uint8_t *bytes, int nBits, uint32_t t_ms, CPRContext &ctx, DecodedADSB &out);
