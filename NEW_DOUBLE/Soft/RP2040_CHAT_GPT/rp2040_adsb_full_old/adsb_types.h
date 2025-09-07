#pragma once
#include <stdint.h>

struct AdsbPacket {
  uint8_t  raw[14];          // 112 бит максимум
  int      bits;             // 56 или 112
  int      rssi;             // относительный score коррелятора
  uint32_t timestamp_ms;     // ms
  int      ch;               // канал
};

struct DecodedADSB {
  uint32_t addr;             // ICAO
  char     flight[9];        // callsign
  float    latitude;         // градусы
  float    longitude;        // градусы
  int      altitude;         // футы (баро)
  float    speed;            // узлы
  float    course;           // градусы
  int      vert_rate;        // фт/мин (если доступно)
  int      Squawk;           // Mode A (если видели DF5/21 отдельно)
  uint8_t  signal_source;    // 1=ADSB
  uint32_t timestamp;        // сек с запуска
  int      df;               // тип DF
  int      tc;               // type code (DF17/ME)
};
