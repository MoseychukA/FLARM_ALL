#pragma once
#include <Arduino.h>

// Аппаратные настройки внешнего дисплея из FlyRf_Disp_25_12_09_00
#define RS485_SERIAL   Serial1
#define RS485_TX_PIN   39
#define RS485_RX_PIN   38
#define RS485_DE_PIN   40
#define RS485_BAUD     115200
#define RS485_CONFIG   SERIAL_8N1
#define RS485_LINK_TIMEOUT_MS 3000UL

#define DISPLAY_BUTTON_PIN 42
#define DISPLAY_BUTTON_ACTIVE_LEVEL LOW
#define DISPLAY_BUTTON_DEBOUNCE_MS 30UL
#define DISPLAY_BUTTON_DOUBLE_MS 350UL
#define DISPLAY_BUTTON_LONG_MS 900UL

#define BUTTON_2_PIN -1
#define POWER_IN_PIN 19
#define POWER_ON_PIN 41
#define POWER_TRACKER_PIN 20
#define LED_PIN -1
#define LED_LCD_PIN 21

#ifndef INA219_BATTERY_MIN_V
#define INA219_BATTERY_MIN_V 3.30f
#endif
#ifndef INA219_BATTERY_MAX_V
#define INA219_BATTERY_MAX_V 4.20f
#endif

// Current is calculated from the measured VIN- voltage:
// I = (fixed input voltage - VIN-) / shunt resistance.
#ifndef INA219_FIXED_INPUT_VOLTAGE_V
#define INA219_FIXED_INPUT_VOLTAGE_V 12.50f
#endif
#ifndef INA219_SHUNT_RESISTANCE_OHM
#define INA219_SHUNT_RESISTANCE_OHM 10.0f
#endif

// На внешнем дисплее отдельного входа SOS нет: состояние SOS приходит по RS485 из базового модуля.
#define SOS_INPUT_PIN 42
#define SOS_INPUT_ACTIVE_LEVEL HIGH

#ifndef FLYRF_WIFI_AP_SSID_PREFIX
#define FLYRF_WIFI_AP_SSID_PREFIX "FlyRf_Disp"
#endif
#ifndef FLYRF_WIFI_AP_PASSWORD
#define FLYRF_WIFI_AP_PASSWORD "12345678"
#endif
