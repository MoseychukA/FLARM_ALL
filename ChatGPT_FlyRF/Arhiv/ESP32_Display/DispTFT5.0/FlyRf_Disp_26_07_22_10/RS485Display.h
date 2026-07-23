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

// Кратковременный пропуск кадров RS485 не считается потерей связи с базой.
static constexpr uint32_t BASE_LINK_TIMEOUT_MS = 6000UL;
#include "HardwareConfig.h"

#ifndef RS485_SERIAL
#define RS485_SERIAL   Serial2
#endif
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
    uint32_t addr;
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
    uint8_t  new_button_M;            // Событие кнопки pin45/внешнего дисплея: 1/2/3
    bool     new_message;             // флаг наличия нового сообщения
    bool     message_received;        // Уточню
    bool     confirm_message_M;       // Флаг подтверждения прочтения сообщения
    char     msg_resp_M[BUFFER_SIZE]; // Содержимое сообщения
    uint8_t  Time_Hour_M;             // Время GPS
    uint8_t  Time_Minute_M;           // Время GPS
    bool     new_SOS_flag_M;          // Флаг наличия сигнала SOS
    bool     isValidGNSS_M;           // Флаг приема сигналов GPS
    bool     gps_time_valid_M;
    bool     gps_satellites_valid_M;
    bool     gps_waiting_M;           // База ожидает фикса/восстановления GNSS
    bool     gps_no_data_M;           // На базе истек тайм-аут отсутствия GNSS
    bool     base_test_mode_M;
    bool     gps_rx_M;
    uint8_t  gps_satellites_M;
    bool     display_coord_valid_M;
    bool     display_coord_is_local_M;
    float    display_latitude_M;
    float    display_longitude_M;
    uint32_t lora_tx_packets_M;
    uint32_t lora_rx_packets_M;
    uint32_t lora_rf_hz_M;
    bool     lan_state_view_M;
    bool     lan_ready_M;
    bool     lan_link_up_M;
    bool     lan_udp_working_M;
    bool     lan_dhcp_M;
    uint8_t  lan_ip_M[4];
    uint16_t lan_udp_port_M;
    uint32_t lan_tx_packets_M;
    uint32_t lan_rx_packets_M;
    uint32_t lan_udp_tx_packets_M;
    uint32_t lan_udp_rx_packets_M;
} aux_t;

typedef struct __attribute__((packed)) {
    ufo_net_t ThisAircraft;
    ufo_net_t Container[MAX_TRACKING_OBJECTS];
    aux_t     AuxData;
    uint8_t   BUTTON1;               // Кнопка резерв
    uint8_t   BUTTON2;               // Кнопка резерв
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
void RS485Display_setLocalButtonEvent(uint8_t event);
uint32_t RS485Display_lastRxMs();
uint32_t RS485Display_rxPackets();
uint32_t RS485Display_txPackets();

size_t RS485Display_payloadSize();
size_t RS485Display_frameSize();
