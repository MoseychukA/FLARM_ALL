#pragma once
#include <Arduino.h>
#include "adsb_structs.h"

// Состояние CPR для двух кадров even/odd по одному ICAO
struct CPRState {
  bool     has_even=false, has_odd=false;
  uint32_t t_even_ms=0, t_odd_ms=0;
  uint32_t icao=0;
  uint32_t lat_even=0, lon_even=0;
  uint32_t lat_odd=0,  lon_odd=0;
};

struct CPRResult {
  bool ok=false;
  float lat=0, lon=0;
};

void cpr_reset(CPRState &s);
void cpr_push_even(CPRState &s, uint32_t icao, uint32_t lat, uint32_t lon, uint32_t t_ms);
void cpr_push_odd (CPRState &s, uint32_t icao, uint32_t lat, uint32_t lon, uint32_t t_ms);

// NL(lat)
int cpr_NL(float lat);
bool cpr_global(const CPRState &s, CPRResult &r);
bool cpr_local(uint32_t lat_cpr, uint32_t lon_cpr, bool fflag, float ref_lat, float ref_lon, CPRResult &r);
