#pragma once

#include <Arduino.h>

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "This project target is ESP32-S3-WROOM-1-N16R8. Select an ESP32-S3 board in Arduino IDE."
#endif

#define PROJECT_ESP32_MODULE_NAME "ESP32-S3-WROOM-1-N16R8"
#define PROJECT_FLASH_SIZE_BYTES  (16UL * 1024UL * 1024UL)
#define PROJECT_PSRAM_SIZE_BYTES  (8UL * 1024UL * 1024UL)
#define PROJECT_USE_GL050001C0_40 1
// Рабочая страница и RS485 включены. Wi-Fi/Web проверяются отдельно.
#define PROJECT_GL050001C0_40_STATIC_IMAGE 0
#define PROJECT_DIAGNOSTIC_DISABLE_WIFI_WEB 1
#define PROJECT_RS485_DISPLAY_REFRESH_MS 1000UL

// Pins reserved by the existing FlyRF display project. Do not assign them to
// the GL050001C0-40 display.
#define PROJECT_PIN_BOOT          0
#define PROJECT_PIN_RS485_RX      38
#define PROJECT_PIN_RS485_TX      39
#define PROJECT_PIN_RS485_DE      40
#define PROJECT_PIN_BUTTON_1      42
#define PROJECT_PIN_POWER_HOLD    41
#define PROJECT_PIN_POWER_ON_SENSE 19
#define PROJECT_PIN_U0TXD         43
#define PROJECT_PIN_U0RXD         44

#define PROJECT_PIN_I2C_SDA       8
#define PROJECT_PIN_I2C_SCL       9
#define PROJECT_INA219_I2C_ADDR   0x40

// GPIO21 is used as PWM output for AP3032 CTRL backlight dimming.
// The display DISP pin must be tied to 3.3 V on the PCB.
#define PROJECT_PIN_LCD_BL_PWM    21
#define PROJECT_LCD_BL_PWM_FREQ   20000
#define PROJECT_LCD_BL_PWM_BITS   12

extern float project_light_current_mA;
extern uint8_t project_backlight_percent;
