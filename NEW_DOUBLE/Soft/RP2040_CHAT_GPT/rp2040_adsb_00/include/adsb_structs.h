#pragma once
#include <stdint.h>
#include <time.h>

typedef struct {
    uint8_t raw[14];   // максимум 112 бит = 14 байт
    uint8_t bits;
    int rssi;
    uint32_t timestamp;
} AdsbPacket;

typedef struct {
    time_t    timestamp;
    uint32_t  addr;
    uint8_t   addr_type;
    float     latitude;
    float     longitude;
    float     altitude;
    float     pressure_altitude;
    float     course;
    float     speed;
    uint8_t   aircraft_type;
    char      flight[16];
    int       vert_rate;
    int       Squawk;
    time_t    seen;
    uint8_t   signal_source;
} DecodedADSB;