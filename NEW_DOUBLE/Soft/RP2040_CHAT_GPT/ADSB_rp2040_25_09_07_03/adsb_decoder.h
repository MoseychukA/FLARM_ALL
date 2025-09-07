#pragma once
#include "adsb_packet.h"

// CRC-таблица (extern)
extern const uint32_t crc24_table[256];

// Проверка CRC пакета
bool adsb_check_crc(const Raw1090Packet& pkt);

// Декодирование ADS-B
DecodedAdsb adsb_decode(const Raw1090Packet& pkt);
