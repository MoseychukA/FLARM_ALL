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
    uint32_t addr;  // Параметр конфигурации: хранит адрес.
    uint16_t squawk;  
    char callsign[8]; 
    int32_t altitude;  
    int32_t speed;  
    int32_t course;  
    int32_t vert_rate; 
    float lat_msg;  
    float lon_msg;  
    uint8_t rssi_rp2040;  
    char endOfPacket[3];  
};

struct ToDUMP1090
{
    uint32_t addr;  // Параметр конфигурации: хранит выбранный адрес.
    uint16_t squawk; 
    char callsign[8]; 
    int32_t altitude;  
    int32_t speed;  
    int32_t course; 
    int32_t vert_rate;  
    float lat_msg;  
    float lon_msg;  
    uint8_t rssi_rp2040;  
};

#ifndef RP2040BRIDGE_PACKET_SIZE
#define RP2040BRIDGE_PACKET_SIZE ((uint16_t)sizeof(ToDUMP1090_RAW))
#endif

struct RP2040BridgeDiag
{
    bool ready;  
    bool taskRunning;       // Объект многозадачности FreeRTOS: используется для задачи, синхронизации или очереди обмена.
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
    uint32_t samplerBursts;
    uint32_t samplerEdges;
    uint32_t preambleFound;
    uint32_t rawFrames;
    uint32_t rawPairErrors;
    uint32_t rawShortFrames;
    uint32_t crcOkFrames;
    uint32_t crcFailFrames;
    uint32_t crcFixFrames;
    uint32_t crcBadFrames;
    uint8_t lastPreambleOffset;
    uint8_t lastDf;
    uint16_t lastFrameLenBits;
    uint8_t lastCrcClass;
    int8_t lastPhase;
    uint8_t lastInvert;
    uint32_t decodedFrames;       // Пакеты, которые PacketDecoder выдал в decoded_1090_packet_out_queue
    uint32_t dictionaryFrames;    // Пакеты, принятые AircraftDictionary / Container
    uint32_t lastCalcCrc;         // CRC24 как в ADSBee/RP2040, по payload без последних 3 байт
    uint32_t lastParity;          // Последние 24 бита кадра
    uint32_t lastSyndrome;        // calculated_crc ^ parity
    uint32_t lastWord0;
    uint32_t lastWord1;
    uint32_t lastWord2;
    uint32_t lastWord3;
};

void RP2040Bridge_setup();
void RP2040Bridge_loop();
bool RP2040Bridge_processPacket(const uint8_t* data, size_t len);
bool RP2040Bridge_getDiag(RP2040BridgeDiag& outDiag);
bool RP2040Bridge_isReady();

void RP2040Bridge_setGainValues(const uint16_t* data, uint8_t count);
void RP2040Bridge_sendGainNow();
void RP2040Bridge_sendGainSingle(uint16_t gain);

void receiveRP2040(void* param);
bool unpack_ToDUMP1090(const ToDUMP1090_RAW* inRaw, ToDUMP1090* packet);
