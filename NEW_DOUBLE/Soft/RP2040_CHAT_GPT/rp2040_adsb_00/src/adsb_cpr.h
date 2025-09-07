#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
  bool   have_even;
  bool   have_odd;
  uint32_t t_even_ms;
  uint32_t t_odd_ms;
  uint32_t icao;
  uint32_t lat_even, lon_even; // 17-bit
  uint32_t lat_odd,  lon_odd;
} cpr_pair_t;

bool  cpr_global_decode(const cpr_pair_t* pair, float* lat, float* lon);
bool  cpr_local_decode(float refLat, float refLon, uint32_t cprLat, uint32_t cprLon, bool odd, float* outLat, float* outLon);
int   NL(float lat);
float cpr_dlon(float lat, bool odd);

// Публичные настройки CPR/декодера
void adsb_set_local_ref(double lat_deg, double lon_deg);        // опорная точка для локальной CPR
void adsb_set_pair_window_ms(uint32_t win_ms);                   // окно сопряжения even/odd (по умолчанию 10000 мс)
void adsb_reset_cpr_cache();

// Главная функция: декод одного сообщения Mode S/ADS-B (после CRC)
// На вход: AdsbPacket (raw[14], bits=56/112, rssi, timestamp мс)
// На выход: заполненный DecodedADSB; возвращает true, если удалось извлечь что-то полезное
bool adsb_decode(const AdsbPacket &pkt, DecodedADSB &out);

// Удобная «обвязка» для вашего кода
inline DecodedADSB adsb_decode_packet(const AdsbPacket &pkt) {
    DecodedADSB f{}; adsb_decode(pkt, f); return f;
}