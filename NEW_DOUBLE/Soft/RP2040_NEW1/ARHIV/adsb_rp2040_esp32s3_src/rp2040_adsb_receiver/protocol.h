#pragma once
#include <Arduino.h>

// Outgoing frame structure (host byte order unless specified otherwise)
struct FOFrame {
  uint32_t addr;        // ICAO address
  uint16_t Squawk;      // squawk
  char     flight[9];   // 8 chars + NUL
  int32_t  altitude;    // geo altitude (m)
  int32_t  pressure_altitude; // pressure altitude (m)
  uint16_t speed;       // km/h
  uint16_t course;      // degrees
  int16_t  vert_rate;   // fpm (can be mapped)
  double   latitude;    // deg
  double   longitude;   // deg
  uint32_t seen;        // ms since last seen
  uint32_t timestamp;   // epoch seconds
  uint8_t  signal_source;
  uint8_t  aircraft_type;
};

// Packed form to transmit over Serial2 (fixed size, avoid alignment issues)
#pragma pack(push, 1)
struct PackedFO {
  uint32_t magic;       // 'FOv1'
  uint32_t addr;
  uint16_t Squawk;
  char     flight[9];
  int32_t  altitude;
  int32_t  pressure_altitude;
  uint16_t speed;
  uint16_t course;
  int16_t  vert_rate;
  int32_t  lat_mdeg;    // 1e-6 deg scaled
  int32_t  lon_mdeg;
  uint32_t seen;
  uint32_t timestamp;
  uint8_t  signal_source;
  uint8_t  aircraft_type;
};
#pragma pack(pop)

static inline void packFO(const FOFrame &in, PackedFO &out){
  out.magic = 0x3141764Fu; // 'FOv1'
  out.addr = in.addr;
  out.Squawk = in.Squawk;
  memcpy(out.flight, in.flight, sizeof(out.flight));
  out.altitude = in.altitude;
  out.pressure_altitude = in.pressure_altitude;
  out.speed = in.speed;
  out.course = in.course;
  out.vert_rate = in.vert_rate;
  out.lat_mdeg = (int32_t)round(in.latitude * 1e6);
  out.lon_mdeg = (int32_t)round(in.longitude * 1e6);
  out.seen = in.seen;
  out.timestamp = in.timestamp;
  out.signal_source = in.signal_source;
  out.aircraft_type = in.aircraft_type;
}

// Raw packet from demod
struct RawPacket {
  uint8_t  bytes[32];
  uint16_t bitlen; // 56 or 112
  uint8_t  channel; // 0..2
  bool     ok;
};
