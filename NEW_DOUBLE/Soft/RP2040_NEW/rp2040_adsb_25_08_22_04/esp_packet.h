#pragma once
#include <stdint.h>
struct __attribute__((packed)) FlightOutput{ uint32_t addr; int32_t Squawk; char flight[8]; int32_t altitude; int32_t pressure_altitude; int32_t speed; int32_t course; int32_t vert_rate; float latitude; float longitude; uint32_t seen; uint32_t timestamp; uint8_t signal_source; uint8_t aircraft_type; };
