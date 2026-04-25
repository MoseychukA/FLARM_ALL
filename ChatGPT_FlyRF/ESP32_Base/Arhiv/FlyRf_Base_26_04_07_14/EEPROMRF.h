#pragma once
#include <Arduino.h>
#include <EEPROM.h>

#define FLYRF_EEPROM_MAGIC   0xBABADEDAUL
#define FLYRF_EEPROM_VERSION 0x0000006FUL

#ifndef EEPROM_SIZE
#define EEPROM_SIZE 1024U
#endif

enum
{
    EEPROM_EXT_LOAD,
    EEPROM_EXT_DEFAULTS,
    EEPROM_EXT_STORE
};

typedef struct Settings {
    uint8_t  mode;
    uint8_t  rf_protocol;
    uint8_t  band;
    uint8_t  aircraft_type;
    uint8_t  txpower;
    uint8_t  volume;
    uint8_t  led_num;
    uint8_t  test_mode;

    bool     nmea_g:1;
    bool     nmea_p:1;
    bool     nmea_l:1;
    bool     nmea_s:1;
    bool     resvd1:1;
    uint8_t  nmea_out:3;

    uint8_t  bluetooth:3;
    uint8_t  alarm:3;
    bool     stealth:1;
    bool     no_track:1;

    uint8_t  gdl90:3;
    uint8_t  json:2;
    uint8_t  power_save;
    int8_t   freq_corr;
    uint32_t igc_key[4];

    uint8_t  tracker_send:4;
    int16_t  alarm_attention;
    int16_t  alarm_warning;
    int16_t  alarm_danger;
    int16_t  alarm_height;

    uint8_t  CountNotReadMessage;
    uint8_t  CurrentCountMessage;
    uint8_t  Message_Not_Confirmed_flag;
    uint32_t block_addr;
    uint8_t  mail[1][96];
    bool     rssi_view:2;
    bool     power_view:2;
    bool     voltage_view:2;
    bool     sos_view:2;
    bool     input_coordinates:2;
    bool     input_N_S:2;
    bool     input_E_W:2;
    float    local_latitude;
    float    local_longitude;
    bool     view_test_coord:2;
    bool     out_of_sync:2;
    bool     default_settings:2;
    uint8_t  display_set;
    uint8_t  lan_state_view;
    uint8_t  tft_memory_view;
    uint8_t  serial_out;
    uint8_t  rs485_out;
    int16_t  threshold_level;
    uint8_t  g_localIP[4];
    uint8_t  g_gatewayIP[4];
    uint8_t  g_subnetMask[4];
    uint8_t  g_dns_server[4];
    uint8_t  lora_fixed_freq;
    uint8_t  lora_profile;
    uint8_t  lora_fixed_channel;
    uint8_t  radar_range_mode;
    uint16_t udp_port;
} __attribute__((packed)) settings_t;

typedef struct EEPROM_S {
    uint32_t   magic;
    uint32_t   version;
    settings_t settings;
} eeprom_struct_t;

typedef union EEPROM_U {
    eeprom_struct_t field;
    uint8_t raw[sizeof(eeprom_struct_t)];
} eeprom_t;

void EEPROM_setup(void);
void EEPROM_defaults(void);
void EEPROM_store(void);
void EEPROM_clear(void);

settings_t* EEPROM_getSettings(void);
void EEPROM_setThresholdLevel(int16_t level, bool storeNow = true);

extern settings_t* settings;
