#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "adsb_decoder.h"

// CPR глобальный/локальный
struct CPRState {
  bool validEven;
  bool validOdd;
  uint32_t icao;
  uint32_t tEven_ms;
  uint32_t tOdd_ms;
  uint32_t rawEven;
  uint32_t rawOdd;
  int      fEven; // fmt
  int      fOdd;
};

void cpr_reset();
bool cpr_decode_airborne(uint32_t icao, bool odd, uint32_t yz, uint32_t xz,
                         float *lat_deg, float *lon_deg, bool *globalOk);
bool cpr_decode_surface(uint32_t icao, bool odd, uint32_t yz, uint32_t xz,
                        float *lat_deg, float *lon_deg, bool *globalOk);
