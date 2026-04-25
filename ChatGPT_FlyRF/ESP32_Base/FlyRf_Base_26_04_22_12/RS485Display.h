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

const uint32_t PACKET_HEADER = 0xAABBCCDD; 
const uint32_t PACKET_FOOTER = 0xDDCCBBAA;  

typedef struct __attribute__((packed)) {
    uint32_t addr;        // Параметр конфигурации: хранит адрес.
    int      squawk; 
    uint8_t  callsign[8];  
    float    altitude;  
    float    pressure_altitude;  
    float    course;  
    float    speed;  
    float    distance;  
    float    bearing;  
    int      vert_rate; 
    float    latitude; 
    float    longitude; 
    time_t   timestamp; 
    int8_t   rssi_LoRa;  
    int8_t   rssi_rp2040;  
    uint8_t  signal_source; 
} ufo_net_t;

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 390
#endif

typedef struct __attribute__((packed)) {
    uint8_t  new_button_M;  //
    bool     new_message;  
    bool     message_received;  
    bool     confirm_message_M;  
    char     msg_resp_M[BUFFER_SIZE];  
    uint8_t  Time_Hour_M;             // Графический спрайт или буфер отрисовки, используемый для подготовки части экрана без мерцания.
    uint8_t  Time_Minute_M;           // Графический спрайт или буфер отрисовки, используемый для подготовки части экрана без мерцания.
    bool     new_SOS_flag_M;  
    bool     isValidGNSS_M;  
} aux_t;

typedef struct __attribute__((packed)) {
    ufo_net_t ThisAircraft;  
    ufo_net_t Container[MAX_TRACKING_OBJECTS];  // Контейнер данных, таблица, база или вспомогательный массив.
    aux_t     AuxData;  //
    uint8_t   BUTTON1;  //
    uint8_t   BUTTON2;  //
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
