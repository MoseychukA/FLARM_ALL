#include "EEPROMRF.h"
#include "ESP32RF.h"
#include "DeviceInfo.h"
#include <string.h>

eeprom_t eeprom_block = {};
settings_t* settings = nullptr;

static void normalizeSettings(settings_t* s)
{
    if (!s) return;
    if (s->alarm_attention <= 0) s->alarm_attention = 3000;
    if (s->alarm_warning <= 0)   s->alarm_warning = 2000;
    if (s->alarm_danger <= 0)    s->alarm_danger = 1000;
    if (s->alarm_height <= 0)    s->alarm_height = 300;
    if (s->display_set > INFO_DISPLAY_MAXI) s->display_set = INFO_DISPLAY_MAXI;
    if (s->rssi_view > VIEW_RSSI_ON) s->rssi_view = VIEW_RSSI_OFF;
    if (s->radar_range_mode > 1) s->radar_range_mode = 0;
    if (s->lan_state_view > 1) s->lan_state_view = 1;
    if (s->gps_state_view > 1) s->gps_state_view = 1;
    if (s->power_current_view > 1) s->power_current_view = 1;
    if (s->power_voltage_view > 1) s->power_voltage_view = 1;
    if (s->power_battery_view > 1) s->power_battery_view = 1;
    if (s->udp_port == 0) s->udp_port = 10110;
    s->mode = FLYRF_MODE_NORMAL;
    s->input_coordinates = IMPUT_COORD_GNSS;
    s->display_sos = 1;
}

void EEPROM_defaults(void)
{
    memset(&eeprom_block, 0, sizeof(eeprom_block));
    eeprom_block.field.magic = FLYRF_EEPROM_MAGIC;
    eeprom_block.field.version = FLYRF_EEPROM_VERSION;
    settings = &eeprom_block.field.settings;
    settings->mode = FLYRF_MODE_NORMAL;
    settings->input_coordinates = IMPUT_COORD_GNSS;
    settings->rssi_view = VIEW_RSSI_OFF;
    settings->display_set = INFO_DISPLAY_MAXI;
    settings->lan_state_view = 1;
    settings->gps_state_view = 1;
    settings->tft_memory_view = 0;
    settings->power_current_view = 1;
    settings->power_voltage_view = 1;
    settings->power_battery_view = 1;
    settings->radar_range_mode = 0;
    settings->alarm_attention = 3000;
    settings->alarm_warning = 2000;
    settings->alarm_danger = 1000;
    settings->alarm_height = 300;
    settings->display_sos = 1;
    settings->udp_port = 10110;
    settings->local_latitude = 0.0f;
    settings->local_longitude = 0.0f;
    normalizeSettings(settings);
}

void EEPROM_setup(void)
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(0, eeprom_block);
    if (eeprom_block.field.magic != FLYRF_EEPROM_MAGIC ||
        (eeprom_block.field.version != FLYRF_EEPROM_VERSION && eeprom_block.field.version != FLYRF_EEPROM_PREV_VERSION))
    {
        EEPROM_defaults();
        EEPROM_store();
    }
    else
    {
        settings = &eeprom_block.field.settings;
        if (eeprom_block.field.version == FLYRF_EEPROM_PREV_VERSION)
        {
            settings->gps_state_view = 1;
            eeprom_block.field.version = FLYRF_EEPROM_VERSION;
            EEPROM_store();
        }
        normalizeSettings(settings);
    }
}

void EEPROM_store(void)
{
    eeprom_block.field.magic = FLYRF_EEPROM_MAGIC;
    eeprom_block.field.version = FLYRF_EEPROM_VERSION;
    normalizeSettings(&eeprom_block.field.settings);
    EEPROM.put(0, eeprom_block);
    EEPROM.commit();
}

void EEPROM_clear(void)
{
    EEPROM_defaults();
    EEPROM_store();
}

settings_t* EEPROM_getSettings(void)
{
    if (!settings) EEPROM_defaults();
    return settings;
}

void EEPROM_setThresholdLevel(int16_t level, bool storeNow)
{
    EEPROM_getSettings()->threshold_level = level;
    if (storeNow) EEPROM_store();
}
