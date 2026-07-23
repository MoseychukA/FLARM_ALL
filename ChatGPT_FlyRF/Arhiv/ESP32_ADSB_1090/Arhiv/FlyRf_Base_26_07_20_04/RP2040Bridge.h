/*
  Модуль RP2040Bridge.h
  Назначение:
  - Описание форматов пакетов и интерфейса связи с RP2040.

  Что содержит файл:
  - Структуры входящих пакетов, диагностических данных и настроек усиления.
  - Объявления функций обмена, приема и управления параметрами RP2040-моста.
  - Прием пакетов по каналу 1090 мГц
*/

#pragma once
#include <Arduino.h>
#include "TrafficTypes.h"

#ifndef SerialRP2040
#define SerialRP2040 Serial1
#endif

#ifndef RP2040BRIDGE_STREAM
#define RP2040BRIDGE_STREAM SerialRP2040
#endif

#ifndef SERIAL_RP2040_SPEED
#define SERIAL_RP2040_SPEED 115200UL
#endif

#ifndef SERIAL_IN_BITS
#define SERIAL_IN_BITS SERIAL_8N1
#endif

#ifndef SOC_GPIO_PIN_RP2040_RX
#define SOC_GPIO_PIN_RP2040_RX 40
#endif

#ifndef SOC_GPIO_PIN_RP2040_TX
#define SOC_GPIO_PIN_RP2040_TX 41
#endif


#ifndef RP2040BRIDGE_RX_BUFFER_SIZE
#define RP2040BRIDGE_RX_BUFFER_SIZE 512U
#endif
#ifndef START_MARK
#define START_MARK 0x55U
#endif

#ifndef END_MARK
#define END_MARK 0xAAAAU
#endif

#ifndef RP2040BRIDGE_GAIN_VALUES_MAX
#define RP2040BRIDGE_GAIN_VALUES_MAX 8U
#endif

#ifndef RP2040BRIDGE_DEFAULT_GAIN
#define RP2040BRIDGE_DEFAULT_GAIN 910U
#endif

struct __attribute__((packed)) ToDUMP1090_RAW
{
    uint32_t addr;
    uint16_t squawk;
    char callsign[8];
    int32_t altitude;
    int32_t speed;
    int32_t course;
    int32_t vert_rate;
    float lat_msg;
    float lon_msg;
    uint8_t rssi_rp2040;  // Параметр радиоканала или протокола: описывает частоту, мощность, профиль, режим передачи или текущее состояние RF.
    char endOfPacket[3];
};

struct ToDUMP1090
{
    uint32_t addr;
    uint16_t squawk;
    char callsign[8];
    int32_t altitude;
    int32_t speed;
    int32_t course;
    int32_t vert_rate;
    float lat_msg;
    float lon_msg;
    uint8_t rssi_rp2040;  // Параметр радиоканала или протокола: описывает частоту, мощность, профиль, режим передачи или текущее состояние RF.
};

#ifndef RP2040BRIDGE_PACKET_SIZE
#define RP2040BRIDGE_PACKET_SIZE ((uint16_t)sizeof(ToDUMP1090_RAW))
#endif

struct RP2040BridgeDiag
{
    bool ready;
    bool taskRunning;
    uint32_t bytesReceived;
    uint32_t packetsReceived;
    uint32_t packetsAccepted;
    uint32_t packetsRejected;
    uint32_t unpackFailedCount;
    uint32_t overflowCount;
    uint32_t badTailCount;
    uint32_t gainPacketsSent;
    uint32_t restartCount;
    uint32_t lastPacketMs;
    uint32_t lastAddress;
    int lastRssi;
    uint8_t lastPacketSize;
};

void RP2040Bridge_setup();
void RP2040Bridge_loop();
bool RP2040Bridge_processPacket(const uint8_t* data, size_t len);
bool RP2040Bridge_getDiag(RP2040BridgeDiag& outDiag);

void RP2040Bridge_setGainValues(const uint16_t* data, uint8_t count);
void RP2040Bridge_sendGainNow();
void RP2040Bridge_sendGainSingle(uint16_t gain);

void receiveRP2040(void* param);
bool unpack_ToDUMP1090(const ToDUMP1090_RAW* inRaw, ToDUMP1090* packet);
