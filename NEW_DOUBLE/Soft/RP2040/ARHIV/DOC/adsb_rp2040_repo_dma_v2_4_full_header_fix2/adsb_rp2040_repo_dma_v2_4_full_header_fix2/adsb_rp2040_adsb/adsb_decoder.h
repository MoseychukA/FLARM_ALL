#pragma once
#include <Arduino.h>
struct DecodedInfo { uint8_t df; uint32_t icao; char callsign[9]; bool has_pos; double lat, lon; bool has_vel; double gs, trk; int alt; int vs; int baro_geo; };
struct CprRef { double ref_lat; double ref_lon; };
void decode_callsign(const uint8_t* me, char* out8);
bool cpr_global_decode(double& lat, double& lon, const uint8_t* even_me, const uint8_t* odd_me);
bool cpr_local_decode(double& lat, double& lon, const uint8_t* me, const CprRef& ref);
int  decode_altitude(const uint8_t* me);
int  decode_altitude_gillham(uint16_t ac13);
bool decode_velocity_tc19(const uint8_t* me, double& gs, double& trk, int& vs, int& baro_geo);
bool parse_bds60_vs(const uint8_t* me, int& vs_baro, int& vs_inertial);
bool parse_bds61_baro_geo(const uint8_t* me, int& baro_geo_diff);
