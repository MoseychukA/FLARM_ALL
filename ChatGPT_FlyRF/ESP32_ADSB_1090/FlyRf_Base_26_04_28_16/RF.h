/*
  Модуль RF.h
  Назначение:
  - Публичный интерфейс радиоподсистемы.

  Что содержит файл:
  - Аппаратные определения пинов радиомодуля.
  - Структуры и константы для работы с пакетами.
  - Объявления функций настройки, приема, передачи и диагностики RF-модуля.
*/

#pragma once

#include <Arduino.h>


/* SPI (LoRa32 pins mapping) */

#ifndef GPIO_PIN_MOSI
#define GPIO_PIN_MOSI       11
#endif
#ifndef GPIO_PIN_MISO
#define GPIO_PIN_MISO       13
#endif
#ifndef GPIO_PIN_SCK
#define GPIO_PIN_SCK        12
#endif
#ifndef GPIO_PIN_SS
#define GPIO_PIN_SS         46
#endif
#ifndef GPIO_PIN_RST
#define GPIO_PIN_RST        7
#endif
#ifndef GPIO_PIN_SDA
#define GPIO_PIN_SDA        8
#endif
#ifndef GPIO_PIN_SCL
#define GPIO_PIN_SCL        9
#endif

#ifndef FLARM_RFM95_SCK_PIN
#define FLARM_RFM95_SCK_PIN   GPIO_PIN_SCK
#endif
#ifndef FLARM_RFM95_MISO_PIN
#define FLARM_RFM95_MISO_PIN  GPIO_PIN_MISO
#endif
#ifndef FLARM_RFM95_MOSI_PIN
#define FLARM_RFM95_MOSI_PIN  GPIO_PIN_MOSI
#endif
#ifndef FLARM_RFM95_CS_PIN
#define FLARM_RFM95_CS_PIN    GPIO_PIN_SS
#endif
#ifndef FLARM_RFM95_RST_PIN
#define FLARM_RFM95_RST_PIN   GPIO_PIN_RST
#endif

//#ifndef FLARM_RFM95_FREQ_HZ
//#define FLARM_RFM95_FREQ_HZ   868000000UL
//#endif
#ifndef FLARM_RFM95_SPI_HZ
#define FLARM_RFM95_SPI_HZ    8000000UL
#endif
#ifndef FLARM_RFM95_ENABLE_TEST_TARGET
#define FLARM_RFM95_ENABLE_TEST_TARGET 0
#endif
#ifndef FLARM_RFM95_MAX_PACKET
#define FLARM_RFM95_MAX_PACKET 64
#endif
#ifndef FLARM_RFM95_RX_POLL_MS
#define FLARM_RFM95_RX_POLL_MS 5UL
#endif
#ifndef FLARM_RFM95_NO_PACKET_RESTART_MS
#define FLARM_RFM95_NO_PACKET_RESTART_MS 15000UL
#endif


//===========================================================================
#include <TimeLib.h>
#include <basicmac.h>
#include <hal/hal.h>
#include <lib_crc.h>
#include <protocol.h>
#include "RadioProtocols.h"
#include <freqplan.h>
#include "OGNTP.h"
#include "ESP32RF.h"
#include "DeviceInfo.h"

#define maxof2(a,b)       (a > b ? a : b)
#define maxof3(a,b,c)     maxof2(maxof2(a,b),c)
#define maxof5(a,b,c,d,e) maxof2(maxof2(a,b),maxof3(c,d,e))

/* Max. paket's payload size for all supported RF protocols */
#define MAX_PKT_SIZE  64 /* запас под MAVLink over LoRa */

#define RXADDR {0x31, 0xfa , 0xb6} // Address of this device (4 bytes)
#define TXADDR {0x31, 0xfa , 0xb6} // Address of device to send to (4 bytes)

enum
{
    RF_IC_NONE,
    RF_IC_SX1276,
};

typedef struct rfchip_ops_struct {
    byte type;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    const char name[8];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    bool (*probe)();
    void (*setup)();
    void (*channel)(int8_t);
    bool (*receive)();
    void (*transmit)();
} rfchip_ops_t;


typedef struct Slot_descr_struct {
    uint16_t begin;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    uint16_t duration;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    unsigned long tmarker;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
} Slot_descr_t;

typedef struct Slots_descr_struct {
    uint32_t      interval_min;  // Временной параметр или отметка времени: используется для тайм-аутов, задержек, мигания или контроля давности данных.
    uint32_t      interval_max;  // Временной параметр или отметка времени: используется для тайм-аутов, задержек, мигания или контроля давности данных.
    uint32_t      interval_mid;  // Временной параметр или отметка времени: используется для тайм-аутов, задержек, мигания или контроля давности данных.
    uint32_t      adj;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    uint16_t      air_time;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    Slot_descr_t  s0;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    Slot_descr_t  s1;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    uint8_t       current;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
} Slots_descr_t;

String Bin2Hex(byte*, size_t);
uint8_t parity(uint32_t);

//===========================================================================

struct RF_LoraRuntimeInfo
{
    bool valid;  // Логический флаг состояния: показывает, разрешена ли операция, активен ли режим или есть ли данные.
    bool fromRegisters;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    uint32_t frequencyHz;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    uint8_t spreadingFactor;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    uint16_t bandwidthKHz;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    uint8_t codingRateDenom;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    uint8_t syncWord;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    uint8_t rxSymbols;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    int8_t txPowerDbm;  // Контейнер данных, таблица, база или вспомогательный массив.
    bool lowDataRateOptimize;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    bool crcOnPayload;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    bool implicitHeader;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
};

struct FlarmRawPacket
{
    uint8_t data[FLARM_RFM95_MAX_PACKET];  // Структура настроек, состояния или набора рабочих данных.
    size_t length;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    int rssi;  // Параметр радиоканала или протокола: описывает частоту, мощность, профиль, режим передачи или текущее состояние RF.
    float snr;  // Параметр радиоканала или протокола: описывает частоту, мощность, профиль, режим передачи или текущее состояние RF.
    uint32_t receivedAt;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    bool valid;  // Логический флаг состояния: показывает, разрешена ли операция, активен ли режим или есть ли данные.
};


//===========================================================================
byte    RF_setup(void);
void    RF_loop(void);
void    RF_SetChannel(void);
size_t  RF_Encode(ufo_t*);
bool    RF_Transmit(size_t, bool);
bool    RF_TransmitThisAircraft(bool);
bool    RF_Receive(void);
uint8_t RF_Payload_Size(uint8_t);
void ParseData(void);
void RF_GetPacketCounters(uint32_t &tx, uint32_t &rx);
uint32_t RF_GetCurrentFrequencyHz(void);
const char* RF_GetProfileName(uint8_t profile);
uint8_t RF_GetAutoLoRaProfile(uint8_t rfProtocol);
uint32_t RF_GetSelectedFrequencyHz(void);
bool RF_GetLoraRuntimeInfo(RF_LoraRuntimeInfo &info);
bool RF_GetLastRawPacket(FlarmRawPacket &outPacket);
String RF_GetLoraProfileDetailsText(void);
String RF_GetLoraRegistersSourceText(void);
void RF_NotifySettingsChanged(void);

extern byte TxBuffer[MAX_PKT_SIZE], RxBuffer[MAX_PKT_SIZE];
extern unsigned long TxTimeMarker;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
extern const rfchip_ops_t* rf_chip;  // Параметр радиоканала или протокола: описывает частоту, мощность, профиль, режим передачи или текущее состояние RF.
extern bool RF_SX12XX_RST_is_connected;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
extern size_t(*protocol_encode)(void*, ufo_t*);
extern bool (*protocol_decode)(void*, ufo_t*, ufo_t*);
extern int8_t RF_last_rssi;  // Временной параметр или отметка времени: используется для тайм-аутов, задержек, мигания или контроля давности данных.
//===========================================================================
