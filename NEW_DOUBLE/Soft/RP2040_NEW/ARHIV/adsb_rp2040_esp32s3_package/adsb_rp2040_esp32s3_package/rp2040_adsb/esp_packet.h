#pragma once
#include <stdint.h>

struct __attribute__((packed)) FlightOutput {
  uint32_t addr;           // ICAO
  int32_t Squawk;          // octal squawk as int
  char flight[8];          // callsign
  int32_t altitude;        // meters (geo)
  int32_t pressure_altitude;// meters (baro)
  int32_t speed;           // km/h
  int32_t course;          // degrees
  int32_t vert_rate;       // m/min
  float latitude;          // deg
  float longitude;         // deg
  uint32_t seen;           // ms
  uint32_t timestamp;      // ms
  uint8_t signal_source;   // 1
  uint8_t aircraft_type;   // 9 default
};
