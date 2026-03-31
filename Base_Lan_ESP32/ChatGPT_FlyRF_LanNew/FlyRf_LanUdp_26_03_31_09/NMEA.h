#pragma once
#include <Arduino.h>

// Output mode for Serial / RS485
#define OUTPUT_MODE_OFF       0U
#define OUTPUT_MODE_CONTAINER 1U
#define OUTPUT_MODE_NMEA      2U
#define OUTPUT_MODE_RP2040    3U
#define OUTPUT_MODE_FLARM     4U
#define OUTPUT_MODE_RS485_DISPLAY 3U

// Bit mask for LAN NMEA output stored in settings->nmea_out
#define NMEA_OUT_UDP 0x01U

#ifndef NMEA_UPDATE_INTERVAL_MS
#define NMEA_UPDATE_INTERVAL_MS 1000UL
#endif

#ifndef NMEA_UDP_PORT
#define NMEA_UDP_PORT 10110U
#endif

#ifndef NMEA_TCP_PORT
#define NMEA_TCP_PORT 10110U
#endif

#ifndef SOC_GPIO_PIN_RS485_RX
#define SOC_GPIO_PIN_RS485_RX 17
#endif

#ifndef SOC_GPIO_PIN_RS485_TX
#define SOC_GPIO_PIN_RS485_TX 18
#endif

#ifndef SOC_GPIO_PIN_RS485_DE
#define SOC_GPIO_PIN_RS485_DE 21
#endif

struct NMEADiag
{
    bool rs485Ready;
    uint32_t sentencesSerial;
    uint32_t sentencesRS485;
    uint32_t sentencesUDP;
    uint32_t sentencesTCP;
    uint32_t batchesSent;
    uint32_t lastBatchMs;
    uint8_t serialMode;
    uint8_t rs485Mode;
    uint8_t lanMode;
};

void NMEA_setup();
void NMEA_loop();
void NMEA_fini();
bool NMEA_getDiag(NMEADiag& outDiag);
void NMEA_announceSerialModeIfNeeded();
