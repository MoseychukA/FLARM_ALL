#include "EEPROMRF.h"
#include "NMEA.h"
#include <string.h>

eeprom_t eeprom_block = {};
settings_t* settings = nullptr;

static void setDefaultsBody()
{
    memset(&eeprom_block, 0, sizeof(eeprom_block));

    eeprom_block.field.magic   = FLYRF_EEPROM_MAGIC;
    eeprom_block.field.version = FLYRF_EEPROM_VERSION;

    eeprom_block.field.settings.nmea_g = true;
    eeprom_block.field.settings.nmea_l = true;
    eeprom_block.field.settings.nmea_s = true;
    eeprom_block.field.settings.nmea_out = 0;
    eeprom_block.field.settings.local_latitude  = 55.95501f;
    eeprom_block.field.settings.local_longitude = 37.23166f;
    eeprom_block.field.settings.serial_out = OUTPUT_MODE_OFF;
    eeprom_block.field.settings.lan_state_view = 1;
    eeprom_block.field.settings.tft_memory_view = 0;
    eeprom_block.field.settings.rs485_out = OUTPUT_MODE_OFF;
    eeprom_block.field.settings.nmea_out = NMEA_OUT_UDP;
    eeprom_block.field.settings.tracker_send = 2; // Auto by default
    eeprom_block.field.settings.threshold_level = 910;
    eeprom_block.field.settings.txpower = 20;
    eeprom_block.field.settings.g_localIP[0] = 192;
    eeprom_block.field.settings.g_localIP[1] = 168;
    eeprom_block.field.settings.g_localIP[2] = 1;
    eeprom_block.field.settings.g_localIP[3] = 155;
    eeprom_block.field.settings.g_gatewayIP[0] = 192;
    eeprom_block.field.settings.g_gatewayIP[1] = 168;
    eeprom_block.field.settings.g_gatewayIP[2] = 1;
    eeprom_block.field.settings.g_gatewayIP[3] = 1;
    eeprom_block.field.settings.g_subnetMask[0] = 255;
    eeprom_block.field.settings.g_subnetMask[1] = 255;
    eeprom_block.field.settings.g_subnetMask[2] = 255;
    eeprom_block.field.settings.g_subnetMask[3] = 0;
    eeprom_block.field.settings.g_dns_server[0] = 8;
    eeprom_block.field.settings.g_dns_server[1] = 8;
    eeprom_block.field.settings.g_dns_server[2] = 8;
    eeprom_block.field.settings.g_dns_server[3] = 8;
    eeprom_block.field.settings.lora_fixed_freq = 8; // 868.8 MHz by default
    eeprom_block.field.settings.lora_profile = 0;    // OGN compatible
    eeprom_block.field.settings.lora_fixed_channel = 1;
    eeprom_block.field.settings.radar_range_mode = 0; // Auto by default
    eeprom_block.field.settings.udp_port = NMEA_UDP_PORT;
}

void EEPROM_defaults(void)
{
    setDefaultsBody();
    settings = &eeprom_block.field.settings;
}

void EEPROM_setup(void)
{
    if (!EEPROM.begin(EEPROM_SIZE))
    {
        Serial.println("[EEPROM] begin failed, using defaults");
        EEPROM_defaults();
        return;
    }

    for (size_t i = 0; i < sizeof(eeprom_t); ++i)
    {
        eeprom_block.raw[i] = EEPROM.read((int)i);
    }

    if (eeprom_block.field.magic != FLYRF_EEPROM_MAGIC ||
        eeprom_block.field.version != FLYRF_EEPROM_VERSION)
    {
        EEPROM_defaults();
        EEPROM_store();
    }
    else
    {
        settings = &eeprom_block.field.settings;
    }

    bool changed = false;
    if (settings != nullptr)
    {
        if (settings->nmea_out != NMEA_OUT_UDP)
        {
            settings->nmea_out = NMEA_OUT_UDP;
            changed = true;
        }
        if (settings->tracker_send != 2)
        {
            settings->tracker_send = 2;
            changed = true;
        }
        if (settings->udp_port == 0)
        {
            settings->udp_port = NMEA_UDP_PORT;
            changed = true;
        }
    }

    if (changed)
    {
        EEPROM_store();
    }
}

void EEPROM_store(void)
{
    if (settings == nullptr)
    {
        settings = &eeprom_block.field.settings;
    }

    for (size_t i = 0; i < sizeof(eeprom_t); ++i)
    {
        EEPROM.write((int)i, eeprom_block.raw[i]);
    }

    EEPROM.commit();
}

void EEPROM_clear(void)
{
    for (size_t i = 0; i < EEPROM_SIZE; ++i)
    {
        EEPROM.write((int)i, 0xFF);
    }
    EEPROM.commit();
    EEPROM_defaults();
    EEPROM_store();
}

settings_t* EEPROM_getSettings(void)
{
    return settings;
}

void EEPROM_setThresholdLevel(int16_t level, bool storeNow)
{
    if (settings == nullptr)
    {
        EEPROM_defaults();
    }

    settings->threshold_level = level;
    if (storeNow)
    {
        EEPROM_store();
    }
}


