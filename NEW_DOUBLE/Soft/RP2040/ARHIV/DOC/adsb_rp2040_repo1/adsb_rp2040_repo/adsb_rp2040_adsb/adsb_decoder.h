
#pragma once
#include <Arduino.h>

struct DecodedInfo {
  uint8_t df;
  uint32_t icao;
  char callsign[9];
  bool has_pos; double lat, lon;
  bool has_vel; double gs, trk;
  int alt;
};

void decode_callsign(const uint8_t* me, char* out8);
bool cpr_global_decode(double& lat, double& lon, const uint8_t* even_me, const uint8_t* odd_me);
int  decode_altitude(const uint8_t* me);
bool decode_velocity(const uint8_t* me, double& gs, double& trk);
