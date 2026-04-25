/*
  Модуль RS485Display.h
  Назначение:
  - Публичный интерфейс обмена по RS485 с внешним дисплеем.

  Что содержит файл:
  - Настройки пинов и скорости RS485.
  - Структуры сетевого представления кадров.
  - Объявления функций передачи, приема и получения состояния кнопок/aux-данных.
*/

#pragma once
#include <Arduino.h>
#include <time.h>
#include "Container.h"

#define RS485_SERIAL   Serial2
#ifndef RS485_TX_PIN
#define RS485_TX_PIN   18
#endif
#ifndef RS485_RX_PIN
#define RS485_RX_PIN   17
#endif
#ifndef RS485_DE_PIN
#define RS485_DE_PIN   21
#endif
#ifndef RS485_BAUD
#define RS485_BAUD     115200
#endif
#ifndef RS485_CONFIG
#define RS485_CONFIG   SERIAL_8N1
#endif

const uint32_t PACKET_HEADER = 0xAABBCCDD;  // Параметр геометрии, координаты, размера или угла.
const uint32_t PACKET_FOOTER = 0xDDCCBBAA;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.

typedef struct __attribute__((packed)) {
    uint32_t addr;  // Параметр конфигурации интерфейса, адресации или выбранного режима.
    int      squawk;  // Параметр геометрии, координаты, размера или угла.
    uint8_t  callsign[8];  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    float    altitude;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    float    pressure_altitude;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    float    course;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    float    speed;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    float    distance;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    float    bearing;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    int      vert_rate;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    float    latitude;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    float    longitude;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    time_t   timestamp;  // Временная отметка, интервал или значение тайм-аута.
    int8_t   rssi_LoRa;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    int8_t   rssi_rp2040;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    uint8_t  signal_source;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
} ufo_net_t;

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 390
#endif

typedef struct __attribute__((packed)) {
    uint8_t  new_button_M;  // Параметр геометрии, координаты, размера или угла.
    bool     new_message;  // Временная отметка, интервал или значение тайм-аута.
    bool     message_received;  // Временная отметка, интервал или значение тайм-аута.
    bool     confirm_message_M;  // Временная отметка, интервал или значение тайм-аута.
    char     msg_resp_M[BUFFER_SIZE];  // Временная отметка, интервал или значение тайм-аута.
    uint8_t  Time_Hour_M;  // Временная отметка, интервал или значение тайм-аута.
    uint8_t  Time_Minute_M;  // Временная отметка, интервал или значение тайм-аута.
    bool     new_SOS_flag_M;  // Логический флаг состояния, разрешения или наличия данных.
    bool     isValidGNSS_M;  // Логический флаг состояния, разрешения или наличия данных.
} aux_t;

typedef struct __attribute__((packed)) {
    ufo_net_t ThisAircraft;  // Параметр геометрии, координаты, размера или угла.
    ufo_net_t Container[MAX_TRACKING_OBJECTS];  // Контейнер данных, таблица, база или вспомогательный массив.
    aux_t     AuxData;  // Параметр геометрии, координаты, размера или угла.
    uint8_t   BUTTON1;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    uint8_t   BUTTON2;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
} full_packet_net_t;

void RS485Display_setup();
void RS485Display_loop();
void RS485Display_fini();
bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2);
void rxTask(void* param);
void net_to_ufo_Container(const ufo_t* src, ufo_net_t* dst);
void net_to_ufo_ThisAircraft(const ufo_t* src, ufo_net_t* dst);

void RS485Display_setOutgoingAux(const aux_t* aux);
void RS485Display_getOutgoingAux(aux_t* aux);
void RS485Display_getIncomingAux(aux_t* aux, uint8_t* btn1 = nullptr, uint8_t* btn2 = nullptr);
bool RS485Display_hasIncomingButton();
uint8_t RS485Display_takeIncomingButton();
uint32_t RS485Display_lastRxMs();

size_t RS485Display_payloadSize();
size_t RS485Display_frameSize();
