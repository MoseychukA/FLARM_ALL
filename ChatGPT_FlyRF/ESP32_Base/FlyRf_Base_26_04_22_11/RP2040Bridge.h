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
    uint32_t addr;  // Параметр конфигурации интерфейса, адресации или выбранного режима.
    uint16_t squawk;  // Параметр геометрии, координаты, размера или угла.
    char callsign[8];  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    int32_t altitude;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    int32_t speed;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    int32_t course;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    int32_t vert_rate;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    float lat_msg;  // Временная отметка, интервал или значение тайм-аута.
    float lon_msg;  // Временная отметка, интервал или значение тайм-аута.
    uint8_t rssi_rp2040;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    char endOfPacket[3];  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
};

struct ToDUMP1090
{
    uint32_t addr;  // Параметр конфигурации интерфейса, адресации или выбранного режима.
    uint16_t squawk;  // Параметр геометрии, координаты, размера или угла.
    char callsign[8];  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    int32_t altitude;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    int32_t speed;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    int32_t course;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    int32_t vert_rate;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    float lat_msg;  // Временная отметка, интервал или значение тайм-аута.
    float lon_msg;  // Временная отметка, интервал или значение тайм-аута.
    uint8_t rssi_rp2040;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
};

#ifndef RP2040BRIDGE_PACKET_SIZE
#define RP2040BRIDGE_PACKET_SIZE ((uint16_t)sizeof(ToDUMP1090_RAW))
#endif

struct RP2040BridgeDiag
{
    bool ready;  // Логический флаг состояния, разрешения или наличия данных.
    bool taskRunning;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    uint32_t bytesReceived;  // Параметр геометрии, координаты, размера или угла.
    uint32_t packetsReceived;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    uint32_t packetsAccepted;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    uint32_t packetsRejected;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    uint32_t unpackFailedCount;  // Счетчик, индекс, позиция или номер элемента.
    uint32_t overflowCount;  // Счетчик, индекс, позиция или номер элемента.
    uint32_t badTailCount;  // Счетчик, индекс, позиция или номер элемента.
    uint32_t gainPacketsSent;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    uint32_t restartCount;  // Счетчик, индекс, позиция или номер элемента.
    uint32_t lastPacketMs;  // Временная отметка, интервал или значение тайм-аута.
    uint32_t lastAddress;  // Параметр конфигурации интерфейса, адресации или выбранного режима.
    int lastRssi;  // Буфер, текстовая строка или рабочее сообщение.
    uint8_t lastPacketSize;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
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
