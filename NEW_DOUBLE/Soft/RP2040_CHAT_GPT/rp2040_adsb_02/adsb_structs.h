#pragma once
#include <stdint.h>
#include <time.h>

struct AdsbPacket {
    uint8_t  raw[14];     // 112 бит макс
    uint16_t bits;        // 56 или 112
    int16_t  rssi;        // условная шкала 0..4095
    uint32_t t0_us;       // метка детекции начала пакета
    uint8_t  channel;     // 1..3
};

struct DecodedADSB {
    time_t    timestamp;
    uint32_t  addr;
    uint8_t   addr_type;
    float     latitude;
    float     longitude;
    float     altitude;           // геометрическая (если доступно), иначе баро
    float     pressure_altitude;  // баро
    float     course;
    float     speed;              // узлы
    uint8_t   aircraft_type;
    char      flight[16];
    int       vert_rate;          // ft/min
    int       Squawk;
    time_t    seen;
    uint8_t   signal_source;      // 1=ADSB
};

enum FrameType {
    DF_SHORT = 56,
    DF_LONG  = 112
};

struct PreambleHit {
    bool     ok;
    uint32_t t0_index; // позиция начала (в индексах выборок)
    float    score;
};
