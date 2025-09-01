#pragma once
#include <Arduino.h>

struct CPRContext {
  double ref_lat = 0.0;
  double ref_lon = 0.0;
  uint32_t last_even_ms = 0;
  uint32_t last_odd_ms = 0;
  uint32_t last_even_raw = 0;
  uint32_t last_odd_raw = 0;
  double last_even_lat = 0.0;
  double last_even_lon = 0.0;
  double last_odd_lat = 0.0;
  double last_odd_lon = 0.0;

  void init(double lat0, double lon0){ ref_lat=lat0; ref_lon=lon0; }
};

bool cpr_decode_global(CPRContext &ctx, bool even, uint32_t rawLat, uint32_t rawLon, uint32_t t_ms, double &oLat, double &oLon);
bool cpr_decode_local(double refLat, double refLon, bool even, uint32_t rawLat, uint32_t rawLon, double &oLat, double &oLon);
