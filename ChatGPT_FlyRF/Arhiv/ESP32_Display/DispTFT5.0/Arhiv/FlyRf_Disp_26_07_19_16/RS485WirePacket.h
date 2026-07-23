#pragma once

#include <stddef.h>
#include <stdint.h>
#include "Container.h"

// Единственный формат пакета FlyRf_Base_26_07_17_00.
// Все размеры фиксированы и не зависят от ABI/toolchain дисплея.
typedef struct __attribute__((packed))
{
    uint32_t addr;
    int32_t squawk;
    uint8_t callsign[8];
    float altitude;
    float pressure_altitude;
    float course;
    float speed;
    float distance;
    float bearing;
    int32_t vert_rate;
    float latitude;
    float longitude;
    uint32_t timestamp32;
    int8_t rssi_LoRa;
    int8_t rssi_rp2040;
    uint8_t signal_source;
} rs485_ufo_wire_t;

typedef struct __attribute__((packed))
{
    uint8_t new_button_M;
    uint8_t new_message;
    uint8_t message_received;
    uint8_t confirm_message_M;
    char msg_resp_M[390];
    uint8_t Time_Hour_M;
    uint8_t Time_Minute_M;
    uint8_t new_SOS_flag_M;
    uint8_t isValidGNSS_M;
    uint8_t gps_time_valid_M;
    uint8_t gps_satellites_valid_M;
    uint8_t gps_waiting_M;
    uint8_t gps_no_data_M;
    uint8_t base_test_mode_M;
    uint8_t gps_rx_M;
    uint8_t gps_satellites_M;
    uint8_t display_coord_valid_M;
    uint8_t display_coord_is_local_M;
    float display_latitude_M;
    float display_longitude_M;
    uint32_t lora_tx_packets_M;
    uint32_t lora_rx_packets_M;
    uint32_t lora_rf_hz_M;
    uint8_t lan_state_view_M;
    uint8_t lan_ready_M;
    uint8_t lan_link_up_M;
    uint8_t lan_udp_working_M;
    uint8_t lan_dhcp_M;
    uint8_t lan_ip_M[4];
    uint16_t lan_udp_port_M;
    uint32_t lan_tx_packets_M;
    uint32_t lan_rx_packets_M;
    uint32_t lan_udp_tx_packets_M;
    uint32_t lan_udp_rx_packets_M;
} rs485_aux_wire_t;

typedef struct __attribute__((packed))
{
    rs485_ufo_wire_t ThisAircraft;
    rs485_ufo_wire_t Container[MAX_TRACKING_OBJECTS];
    rs485_aux_wire_t AuxData;
    uint8_t BUTTON1;
    uint8_t BUTTON2;
} rs485_packet_wire_t;

static constexpr size_t RS485_PAYLOAD_SIZE = 1223U;
static constexpr size_t RS485_FRAME_SIZE = 1233U;

static_assert(sizeof(float) == 4U, "RS485 requires 32-bit float");
static_assert(sizeof(rs485_ufo_wire_t) == 59U, "RS485 aircraft must be 59 bytes");
static_assert(sizeof(rs485_aux_wire_t) == 454U, "RS485 AuxData must be 454 bytes");
static_assert(sizeof(rs485_packet_wire_t) == RS485_PAYLOAD_SIZE,
              "RS485 payload must be 1223 bytes");
