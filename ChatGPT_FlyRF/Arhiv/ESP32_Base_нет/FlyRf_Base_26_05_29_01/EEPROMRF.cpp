/*
  Модуль EEPROMRF.cpp
  Назначение:
  - Хранение, загрузка и нормализация всех пользовательских настроек проекта.

  Основные задачи модуля:
  - Читать настройки из EEPROM и проверять их корректность.
  - Заполнять значения по умолчанию при первом запуске или повреждении данных.
  - Сохранять изменения, внесенные через WEB-интерфейс и другие модули.
  - Поддерживать согласованность настроек вывода, режимов работы и порогов оповещения.
*/

#include "EEPROMRF.h"
#include "NMEA.h"
#include "DeviceInfo.h"
#include "Bluetooth.h"
#include "WiFiRF.h"
#include "RadioProtocols.h"
#include "RF.h"
#include <freqplan.h>
#include <string.h>

eeprom_t eeprom_block = {};  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
settings_t* settings = nullptr;  // Структура настроек, состояния или набора рабочих данных.



//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `normalizeFixed868Settings` и обрабатывает fixed868 настройки в контексте модуля EEPROMRF.cpp.
// Локальные переменные: changed — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
static bool normalizeFixed868Settings(settings_t* s)
{
    if (s == nullptr)
    {
        return false;
    }

    bool changed = false;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.

    if (s->band != RF_BAND_RU)
    {
        s->band = RF_BAND_RU; // Только 868.x МГц с шагом 0.1 МГц
        changed = true;
    }
    if (s->lora_fixed_channel != 1U)
    {
        s->lora_fixed_channel = 1U;
        changed = true;
    }
    if (s->lora_fixed_freq > 9U)
    {
        s->lora_fixed_freq = 8U;
        changed = true;
    }
    if (s->txpower != 20U)
    {
        s->txpower = 20U;
        changed = true;
    }

    if (s->lora_profile > 4U)
    {
        s->lora_profile = 0U;
        changed = true;
    }
    if (s->lora_custom_sf < 7U || s->lora_custom_sf > 12U)
    {
        s->lora_custom_sf = 12U;
        changed = true;
    }
    if (s->lora_custom_bw > 2U)
    {
        s->lora_custom_bw = 0U;
        changed = true;
    }
    if (s->lora_custom_cr > 3U)
    {
        s->lora_custom_cr = 3U;
        changed = true;
    }

    return changed;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `clampAlarmValue` и обрабатывает clamp порог тревоги value в контексте модуля EEPROMRF.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
static int16_t clampAlarmValue(int value, int lo, int hi)
{
    if (value < lo) return (int16_t)lo;
    if (value > hi) return (int16_t)hi;
    return (int16_t)value;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `normalizeAlarmSettings` и обрабатывает порог тревоги настройки в контексте модуля EEPROMRF.cpp.
// Локальные переменные: changed — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; int16_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
static bool normalizeAlarmSettings(settings_t* s)
{
    if (s == nullptr)
    {
        return false;
    }

    bool changed = false;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.

    const int16_t attention = clampAlarmValue(s->alarm_attention, 2000, 3000);
    const int16_t warning   = clampAlarmValue(s->alarm_warning,   200, 2000);
    const int16_t danger    = clampAlarmValue(s->alarm_danger,     10,  500);
    const int16_t height    = clampAlarmValue(s->alarm_height,     30,  300);

    if (s->alarm_attention != attention) { s->alarm_attention = attention; changed = true; }
    if (s->alarm_warning   != warning)   { s->alarm_warning   = warning;   changed = true; }
    if (s->alarm_danger    != danger)    { s->alarm_danger    = danger;    changed = true; }
    if (s->alarm_height    != height)    { s->alarm_height    = height;    changed = true; }

    if (s->alarm_warning > s->alarm_attention)
    {
        s->alarm_warning = s->alarm_attention;
        changed = true;
    }
    if (s->alarm_danger > s->alarm_warning)
    {
        s->alarm_danger = s->alarm_warning;
        changed = true;
    }

    return changed;
}

//------------------------------------------------------------------------------
// Назначение функции: Устанавливает defaults body, проверяет входные данные и при необходимости запускает сопутствующие обновления.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
static void setDefaultsBody()
{
    memset(&eeprom_block, 0, sizeof(eeprom_block));

    eeprom_block.field.magic   = FLYRF_EEPROM_MAGIC;
    eeprom_block.field.version = FLYRF_EEPROM_VERSION;

    eeprom_block.field.settings.mode = FLYRF_MODE_NORMAL;
    eeprom_block.field.settings.test_mode = TEST_MODE_STATIC;
    eeprom_block.field.settings.input_coordinates = IMPUT_COORD_GNSS;
    eeprom_block.field.settings.local_latitude  = 55.95501f;
    eeprom_block.field.settings.local_longitude = 37.23166f;
    eeprom_block.field.settings.serial_out = OUTPUT_MODE_OFF;
    eeprom_block.field.settings.rf_protocol = RF_PROTOCOL_OGNTP;
    eeprom_block.field.settings.band = RF_BAND_RU;
    eeprom_block.field.settings.bluetooth = BLUETOOTH_OFF;
    eeprom_block.field.settings.display_enabled = 1;
    eeprom_block.field.settings.lan_enabled = 1;
    eeprom_block.field.settings.wifi_mode = WIFI_CONTROL_ON;
    eeprom_block.field.settings.lan_state_view = 1;
    eeprom_block.field.settings.gps_state_view = 1;
    eeprom_block.field.settings.tft_memory_view = 0;
    eeprom_block.field.settings.display_sos = 1;
    eeprom_block.field.settings.rs485_out = OUTPUT_MODE_RS485_DISPLAY;
    eeprom_block.field.settings.nmea_out = NMEA_OUTPUT_UDP;
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
    eeprom_block.field.settings.lora_fixed_freq = 8; // 868.8 MHz default, Web: 868.0..868.9 MHz step 0.1
    eeprom_block.field.settings.lora_profile = 0;    // OGN compatible по умолчанию для совместимости
    eeprom_block.field.settings.lora_custom_sf = 12; // Пользовательский режим: максимальная дальность по умолчанию
    eeprom_block.field.settings.lora_custom_bw = 0;  // BW125
    eeprom_block.field.settings.lora_custom_cr = 3;  // CR4/8
    eeprom_block.field.settings.lora_fixed_channel = 1;
    eeprom_block.field.settings.radar_range_mode = 0; // Auto by default
    eeprom_block.field.settings.udp_port = FLYRF_DEFAULT_NMEA_UDP_PORT;
    eeprom_block.field.settings.block_addr[0] = 0U;
    eeprom_block.field.settings.block_addr[1] = 0U;
    eeprom_block.field.settings.block_addr[2] = 0U;
    eeprom_block.field.settings.alarm_attention = 3000;
    eeprom_block.field.settings.alarm_warning = 2000;
    eeprom_block.field.settings.alarm_danger = 500;
    eeprom_block.field.settings.alarm_height = 300;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `EEPROM_defaults` и обрабатывает EEPROM defaults в контексте модуля EEPROMRF.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
void EEPROM_defaults(void)
{
    setDefaultsBody();
    settings = &eeprom_block.field.settings;
}

//------------------------------------------------------------------------------
// Назначение функции: Инициализирует EEPROM, подготавливает связанные объекты и включает работу соответствующего узла.
// Локальные переменные: changed — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

    if (eeprom_block.field.magic != FLYRF_EEPROM_MAGIC)
    {
        EEPROM_defaults();
        EEPROM_store();
    }
    else
    {
        settings = &eeprom_block.field.settings;
        if (eeprom_block.field.version != FLYRF_EEPROM_VERSION)
        {
            if (eeprom_block.field.version == FLYRF_EEPROM_PREV_VERSION)
            {
                settings->gps_state_view = 1;
                settings->display_enabled = 1;
                settings->lan_enabled = 1;
                settings->wifi_mode = WIFI_CONTROL_ON;
                eeprom_block.field.version = FLYRF_EEPROM_VERSION;
                EEPROM_store();
            }
            else
            {
                EEPROM_defaults();
                EEPROM_store();
            }
        }
    }

    bool changed = false;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    if (settings != nullptr)
    {
        if (settings->nmea_out > 3)
        {
            settings->nmea_out = NMEA_OUTPUT_UDP;
            changed = true;
        }
        if (settings->rf_protocol != RF_PROTOCOL_OGNTP && settings->rf_protocol != RF_PROTOCOL_MAVLINK)
        {
            settings->rf_protocol = RF_PROTOCOL_OGNTP;
            changed = true;
        }
        if (settings->tracker_send != 2)
        {
            settings->tracker_send = 2;
            changed = true;
        }
        if (settings->udp_port == 0)
        {
            settings->udp_port = FLYRF_DEFAULT_NMEA_UDP_PORT;
            changed = true;
        }
        if (settings->lan_state_view > 1)
        {
            settings->lan_state_view = 1;
            changed = true;
        }
        if (settings->display_enabled > 1)
        {
            settings->display_enabled = 1;
            changed = true;
        }
        if (settings->lan_enabled > 1)
        {
            settings->lan_enabled = 1;
            changed = true;
        }
        if (settings->wifi_mode == WIFI_CONTROL_AP)
        {
            settings->wifi_mode = WIFI_CONTROL_ON;
            changed = true;
        }
        if (settings->wifi_mode > WIFI_CONTROL_ON)
        {
            settings->wifi_mode = WIFI_CONTROL_ON;
            changed = true;
        }
        if (settings->gps_state_view > 1)
        {
            settings->gps_state_view = 1;
            changed = true;
        }
        if (settings->display_sos > 1)
        {
            settings->display_sos = 1;
            changed = true;
        }
        if (settings->mode > FLYRF_MODE_TXRX_TEST_MAX)
        {
            settings->mode = FLYRF_MODE_NORMAL;
            changed = true;
        }
        if (settings->bluetooth > BLUETOOTH_AUTO)
        {
            settings->bluetooth = BLUETOOTH_OFF;
            changed = true;
        }
        const uint8_t targetBluetooth = (settings->nmea_out == NMEA_OUTPUT_BLUETOOTH) ? BLUETOOTH_LE : BLUETOOTH_OFF;
        if (settings->bluetooth != targetBluetooth)
        {
            settings->bluetooth = targetBluetooth;
            changed = true;
        }
        for (size_t i = 0; i < 3; ++i)
        {
            if (settings->block_addr[i] > 0xFFFFFFUL)
            {
                settings->block_addr[i] = 0U;
                changed = true;
            }
        }
        if (settings->test_mode > TEST_MODE_RESERVED2)
        {
            settings->test_mode = TEST_MODE_STATIC;
            changed = true;
        }
        const uint8_t desiredCoordMode = FlyRfMode_usesLocalCoordinates(settings->mode) ? IMPUT_COORD_MANUAL : IMPUT_COORD_GNSS;
        if (settings->input_coordinates != desiredCoordMode)
        {
            settings->input_coordinates = desiredCoordMode;
            changed = true;
        }
        if (normalizeAlarmSettings(settings))
        {
            changed = true;
        }
        if (normalizeFixed868Settings(settings))
        {
            changed = true;
        }
    }

    if (changed)
    {
        EEPROM_store();
    }
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `EEPROM_store` и обрабатывает EEPROM store в контексте модуля EEPROMRF.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `EEPROM_clear` и обрабатывает EEPROM в контексте модуля EEPROMRF.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `EEPROM_getSettings` и обрабатывает EEPROM настройки в контексте модуля EEPROMRF.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
settings_t* EEPROM_getSettings(void)
{
    return settings;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `EEPROM_setThresholdLevel` и обрабатывает EEPROM threshold level в контексте модуля EEPROMRF.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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
