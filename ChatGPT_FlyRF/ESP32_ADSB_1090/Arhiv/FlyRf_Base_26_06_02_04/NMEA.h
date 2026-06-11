/*
  Модуль NMEA.h
  Назначение:
  - Публичный интерфейс подсистемы NMEA и текстового вывода данных.

  Что содержит файл:
  - Константы режимов вывода в Serial и RS485.
  - Структуру диагностических счетчиков NMEADiag.
  - Объявления функций запуска, циклического обслуживания и чтения статистики.
*/

#pragma once
#include <Arduino.h>

// Output mode for Serial / RS485
#define OUTPUT_MODE_OFF       0U
#define OUTPUT_MODE_CONTAINER 1U
#define OUTPUT_MODE_NMEA      2U
#define OUTPUT_MODE_RP2040    3U
#define OUTPUT_MODE_FLARM     4U
#define OUTPUT_MODE_LORA_RAW  5U
#define OUTPUT_MODE_MAVLINK   6U
#define OUTPUT_MODE_RS485_DISPLAY 3U

#ifndef NMEA_UPDATE_INTERVAL_MS
#define NMEA_UPDATE_INTERVAL_MS 1000UL
#endif

#ifndef NMEA_UDP_PORT
#define NMEA_UDP_PORT 10110U
#endif

#ifndef FLYRF_DEFAULT_NMEA_UDP_PORT
#define FLYRF_DEFAULT_NMEA_UDP_PORT NMEA_UDP_PORT
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
    uint32_t sentencesBluetooth;
    uint32_t sentencesUDP;
    uint32_t sentencesTCP;
    uint32_t batchesSent;
    uint32_t lastBatchMs;
    uint8_t serialMode;
    uint8_t rs485Mode;
    uint8_t bluetoothMode;
    uint8_t nmeaOutput;
    uint16_t udpPort;  // Параметр радиоканала или протокола: описывает частоту, мощность, профиль, режим передачи или текущее состояние RF.
};

#define NMEA_OUTPUT_OFF       0U
#define NMEA_OUTPUT_SERIAL    1U
#define NMEA_OUTPUT_UDP       2U
#define NMEA_OUTPUT_BLUETOOTH 3U

void NMEA_setup();
void NMEA_loop();
void NMEA_fini();
bool NMEA_getDiag(NMEADiag& outDiag);
void NMEA_announceSerialModeIfNeeded();
