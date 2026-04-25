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
#include <freqplan.h>
#include "OGNTP.h"
#include "ESP32RF.h"
#include "DeviceInfo.h"

#define maxof2(a,b)       (a > b ? a : b)
#define maxof3(a,b,c)     maxof2(maxof2(a,b),c)
#define maxof5(a,b,c,d,e) maxof2(maxof2(a,b),maxof3(c,d,e))

/* Max. paket's payload size for all supported RF protocols */
#define MAX_PKT_SIZE  32 /* 48 = UAT LONG_FRAME_DATA_BYTES */

#define RXADDR {0x31, 0xfa , 0xb6} // Address of this device (4 bytes)
#define TXADDR {0x31, 0xfa , 0xb6} // Address of device to send to (4 bytes)

enum
{
    RF_IC_NONE,
    RF_IC_SX1276,
};

typedef struct rfchip_ops_struct {
    byte type;
    const char name[8];
    bool (*probe)();
    void (*setup)();
    void (*channel)(int8_t);
    bool (*receive)();
    void (*transmit)();
} rfchip_ops_t;


typedef struct Slot_descr_struct {
    uint16_t begin;
    uint16_t duration;
    unsigned long tmarker;
} Slot_descr_t;

typedef struct Slots_descr_struct {
    uint32_t      interval_min;
    uint32_t      interval_max;
    uint32_t      interval_mid;
    uint32_t      adj;
    uint16_t      air_time;
    Slot_descr_t  s0;
    Slot_descr_t  s1;
    uint8_t       current;
} Slots_descr_t;

String Bin2Hex(byte*, size_t);
uint8_t parity(uint32_t);

//===========================================================================

struct RF_LoraRuntimeInfo
{
    bool valid;
    bool fromRegisters;
    uint32_t frequencyHz;
    uint8_t spreadingFactor;
    uint16_t bandwidthKHz;
    uint8_t codingRateDenom;
    uint8_t syncWord;
    uint8_t rxSymbols;
    int8_t txPowerDbm;
    bool lowDataRateOptimize;
    bool crcOnPayload;
    bool implicitHeader;
};

struct FlarmRawPacket
{
    uint8_t data[FLARM_RFM95_MAX_PACKET];
    size_t length;
    int rssi;
    float snr;
    uint32_t receivedAt;
    bool valid;
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
uint32_t RF_GetSelectedFrequencyHz(void);
bool RF_GetLoraRuntimeInfo(RF_LoraRuntimeInfo &info);
bool RF_GetLastRawPacket(FlarmRawPacket &outPacket);
String RF_GetLoraProfileDetailsText(void);
String RF_GetLoraRegistersSourceText(void);
void RF_NotifySettingsChanged(void);

extern byte TxBuffer[MAX_PKT_SIZE], RxBuffer[MAX_PKT_SIZE];
extern unsigned long TxTimeMarker;
extern const rfchip_ops_t* rf_chip;
extern bool RF_SX12XX_RST_is_connected;
extern size_t(*protocol_encode)(void*, ufo_t*);
extern bool (*protocol_decode)(void*, ufo_t*, ufo_t*);
extern int8_t RF_last_rssi;
//===========================================================================
