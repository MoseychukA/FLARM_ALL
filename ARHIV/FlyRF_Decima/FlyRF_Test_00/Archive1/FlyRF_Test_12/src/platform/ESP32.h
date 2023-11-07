/*
 * Platform_ESP32.h
 * Copyright (C) 2018-2023 Linar Yusupov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#if defined(ESP32)

#ifndef PLATFORM_ESP32_H
#define PLATFORM_ESP32_H

#include "sdkconfig.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFiClient.h>

/* Maximum of tracked flying objects is now SoC-specific constant */
#define MAX_TRACKING_OBJECTS    8

#define DEFAULT_SOFTRF_MODEL    SOFTRF_MODEL_STANDALONE

#define SerialOutput            Serial
#define SoftwareSerial          HardwareSerial
#define Serial_GNSS_In          Serial1
#define Serial_GNSS_Out         Serial_GNSS_In

#if defined(CONFIG_IDF_TARGET_ESP32)
#define UATSerial               Serial2
#else
#error "This ESP32 family build variant is not supported!"
#endif /* CONFIG_IDF_TARGET_ESP32 */

#define EEPROM_commit()         EEPROM.commit()

#define isValidFix()            isValidGNSSFix()

#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR>=4
#define WIRE_FINI(bus)          { bus.end(); }
#else
#define WIRE_FINI(bus)          { } /* AC 1.0.x has no Wire.end() */
#endif


#define BACKLIGHT_CHANNEL       ((uint8_t)1)

#define SOC_GPIO_PIN_GNSS_PPS SOC_UNUSED_PIN

/* SPI (does match Heltec & TTGO LoRa32 pins mapping) */
#define SOC_GPIO_PIN_MOSI       27
#define SOC_GPIO_PIN_MISO       19
#define SOC_GPIO_PIN_SCK        5
#define SOC_GPIO_PIN_SS         18

/* NRF905 */
#define SOC_GPIO_PIN_TXE        26
#define SOC_GPIO_PIN_CE         2
#define SOC_GPIO_PIN_PWR        14

/* SX1276 [RFM95W] (does match Heltec & TTGO LoRa32 pins mapping) */
#define SOC_GPIO_PIN_RST        14
#define SOC_GPIO_PIN_DIO0       26
#define SOC_GPIO_PIN_SDA        14
#define SOC_GPIO_PIN_SCL        2

#include "iomap/LilyGO_T22.h"
#include "iomap/LilyGO_T3.h"


#define LV_HOR_RES                      (240) //Horizontal
#define LV_VER_RES                      (240) //vertical

#include "iomap/LilyGO_TBeam_Supreme.h"

extern WebServer server;

enum rst_reason {
  REASON_DEFAULT_RST      = 0,  /* normal startup by power on */
  REASON_WDT_RST          = 1,  /* hardware watch dog reset */
  REASON_EXCEPTION_RST    = 2,  /* exception reset, GPIO status won't change */
  REASON_SOFT_WDT_RST     = 3,  /* software watch dog reset, GPIO status won't change */
  REASON_SOFT_RESTART     = 4,  /* software restart ,system_restart , GPIO status won't change */
  REASON_DEEP_SLEEP_AWAKE = 5,  /* wake up from deep-sleep */
  REASON_EXT_SYS_RST      = 6   /* external system reset */
};

enum esp32_board_id {
  ESP32_DEVKIT,
  ESP32_S3_DEVKIT,
  ESP32_C3_DEVKIT,
  ESP32_TTGO_V2_OLED,
  ESP32_HELTEC_OLED,
  ESP32_TTGO_T_BEAM,
  ESP32_TTGO_T_BEAM_SUPREME,
  ESP32_TTGO_T_WATCH,
  ESP32_S2_T8_V1_1,
  ESP32_LILYGO_T_TWR_V2_0,
  ESP32_LILYGO_T_TWR_V2_1,
  //ESP32_HELTEC_TRACKER,
};

/* https://github.com/espressif/usb-pids/blob/main/allocated-pids.txt#L313 */
enum softrf_usb_pid {
  SOFTRF_USB_PID_WEBTOP     = 0x8131,
  SOFTRF_USB_PID_STANDALONE = 0x8132,
  SOFTRF_USB_PID_PRIME_MK3  = 0x8133,
  SOFTRF_USB_PID_UF2_BOOT   = 0x8134,
  SOFTRF_USB_PID_HAM        = 0x818F,
  SOFTRF_USB_PID_MIDI       = 0x81A0,
};

struct rst_info {
  uint32_t reason;
  uint32_t exccause;
  uint32_t epc1;
  uint32_t epc2;
  uint32_t epc3;
  uint32_t excvaddr;
  uint32_t depc;
};

/* Boya Microelectronics Inc. */
#define BOYA_ID                 0x68
#define BOYA_BY25Q32AL          0x4016

/* ST / SGS/Thomson / Numonyx / XMC(later acquired by Micron) */
#define ST_ID                   0x20
#define XMC_XM25QH32B           0x4016

#define MakeFlashId(v,d)        ((v << 16) | d)

/* Disable brownout detection (avoid unexpected reset on some boards) */
#define ESP32_DISABLE_BROWNOUT_DETECTOR 0

#define NMEA_TCP_SERVICE
#define USE_NMEALIB
#define USE_OLED
#define EXCLUDE_OLED_049
#define USE_TFT
#define USE_NMEA_CFG
#define USE_BASICMAC

#define USE_TIME_SLOTS

/* Experimental */
#define USE_BLE_MIDI
//#define USE_GDL90_MSL
#define USE_OGN_ENCRYPTION
//#define ENABLE_PROL

//#define EXCLUDE_GNSS_UBLOX    /* Neo-6/7/8, M10 */
#define ENABLE_UBLOX_RFS        /* вернуть заводские настройки (при необходимости)  */
#define EXCLUDE_GNSS_GOKE       /* 'Air530' GK9501 GPS/GLO/BDS (GAL inop.)   */
//#define EXCLUDE_GNSS_AT65     /* L76K, Air530Z */
#define EXCLUDE_GNSS_SONY
#define EXCLUDE_GNSS_MTK
//#define EXCLUDE_GNSS_UC65

#define EXCLUDE_SOFTRF_HEARTBEAT
#define EXCLUDE_LK8EX1
//#define EXCLUDE_IMU
//#define EXCLUDE_MAG

//#if !defined(CONFIG_IDF_TARGET_ESP32)
//
//#endif /* NOT CONFIG_IDF_TARGET_ESP32 */

#define POWER_SAVING_WIFI_TIMEOUT 600000UL /* 10 minutes */

//#define PMK2_SLEEP_MODE 1    // 0.6 mA : esp_deep_sleep_start()
//#define PMK2_SLEEP_MODE 2    // 0.9 mA : axp.setSleep()
#define PMK2_SLEEP_MODE 3      //  60 uA : axp.shutdown()

#if defined(USE_OLED)
#define U8X8_OLED_I2C_BUS_TYPE  U8X8_SSD1306_128X64_NONAME_2ND_HW_I2C
#endif /* USE_OLED */

#endif /* PLATFORM_ESP32_H */


#endif /* ESP32 */
