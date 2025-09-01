#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "ringbuffer.h"
struct DecodedFrame
{
	uint32_t icao;
	char callsign[9];
	int squawk; 
	float lat,lon;
	int altitude_m_geo;
	int altitude_m_baro;
	int speed_kmh; 
	int track_deg; 
	int vert_rate_mpm;
	uint32_t seen_time_ms; 
	int aircraft_type; 
	int src_type; 
	int vr_mpm; 
	int baro_geo_diff_m;
	bool pos_surface; 
};

enum CrcStatus
{
	CRC_OK, CRC_FIXED1, CRC_FAIL 
};

CrcStatus check_and_fix_crc(RawMessage &rm);
bool decode_adsb_frame(const RawMessage &rm, DecodedFrame &df, const struct Config &cfg);
uint32_t crc24_modes(const uint8_t *msg, int bits);
uint32_t crc32(const uint8_t* data, size_t len);
bool cpr_decode_global(uint32_t even_lat_cpr, uint32_t even_lon_cpr, uint32_t odd_lat_cpr, uint32_t odd_lon_cpr, bool surface, float &out_lat, float &out_lon);
bool cpr_decode_local(uint32_t lat_cpr, uint32_t lon_cpr, bool surface, float ref_lat, float ref_lon, float &out_lat, float &out_lon);
int gillham2alt(uint16_t ac13, bool qbit);
