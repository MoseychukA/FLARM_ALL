#pragma once
#include <stdint.h>
#include "raw1090packet.h"

// Расшифрованные ADS-B данные
struct DecodedAdsb {
    uint8_t df = 0;       // Downlink Format
    uint32_t icao = 0;    // ICAO адрес
    uint8_t  tc = 0;      // Type Code
    char     callsign[9] = {0};
    int32_t  alt = 0;     // Высота
};
