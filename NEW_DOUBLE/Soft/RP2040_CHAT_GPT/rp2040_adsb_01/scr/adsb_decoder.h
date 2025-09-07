#pragma once
#include <stdint.h>
#include <time.h>

struct AdsbPacket {
  uint8_t  raw[14];   // 112 бит максимум (MSB-first по байтам)
  int      bits;      // 56 или 112
  int      rssi;      // «псевдо» RSSI (ADC 0..4095)
  uint32_t timestamp; // t0 в микросекундах относительно запуска
  uint8_t  ch;        // 1..3 канал
};

struct DecodedADSB {
  time_t    timestamp;
  uint32_t  addr;
  uint8_t   addr_type; // 0=ICAO
  float     latitude;
  float     longitude;
  float     altitude;           // геоид (м) — тут выводим футы/метры на ваш выбор
  float     pressure_altitude;  // баро (м)
  float     course;
  float     speed;              // узлы/кмч — ниже конвертация
  uint8_t   aircraft_type;
  char      flight[16];
  int       vert_rate;          // фт/мин
  int       Squawk;             // 4-октальный
  time_t    seen;
  uint8_t   signal_source;      // 1=DUMP1090 style
};

void adsb_system_init();
void adsb_correlator_init();
void adsb_dma_init();
bool adsb_dma_fetch(AdsbPacket *pkt); // неблокирующая выдача очередного пакета

// Высокоуровневый парсер DF17/DF18/DF20/DF21
bool adsb_parse(const AdsbPacket &pkt, DecodedADSB *out);
