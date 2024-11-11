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

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFiClient.h>


/* Maximum of tracked flying objects is now SoC-specific constant */
#define MAX_TRACKING_OBJECTS    8

#define DEFAULT_FLYRF_MODEL    FLYRF_MODEL_STANDALONE

#define SerialOutput            Serial
#define SoftwareSerial          HardwareSerial
#define Serial_GNSS_In          Serial1
#define Serial_GNSS_Out         Serial_GNSS_In

#define EEPROM_commit()         EEPROM.commit()

#define isValidFix()            isValidGNSSFix()

#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR>=4
#define WIRE_FINI(bus)          { bus.end(); }
#else
#define WIRE_FINI(bus)          { } /* AC 1.0.x has no Wire.end() */
#endif


//enum
//{
//    DIRECTION_TRACK_UP,
//    DIRECTION_NORTH_UP,
//
//};
//
enum
{
    DISPLAY_NONE,
    DISPLAY_OLED
};

//enum
//{
//    D1090_OFF,
//    D1090_UART_MINI,
//    D1090_UART_FULL,
//};

enum
{
    VIEW_RSSI_OFF,
    VIEW_RSSI_ON
};

enum
{
    VIEW_RAM_OFF,
    VIEW_RAM_ON
};

enum
{
    GSM_SEND_OFF,
    GSM_SEND_SINGLE,
    GSM_SEND_AUTO,
    GSM_SEND_MINI
};

enum
{
    POWER_SAVE_NONE = 0,
    POWER_SAVE_WIFI = 1,
    POWER_SAVE_GNSS = 2,
    POWER_SAVE_NORECEIVE = 4
};


/* Peripherals */
#define SOC_GPIO_PIN_GNSS_RX    15
#define SOC_GPIO_PIN_GNSS_TX    16

#define SOC_GPIO_PIN_TRACKER_RX    37
#define SOC_GPIO_PIN_TRACKER_TX    38

#define SOC_GPIO_PIN_LORA_TXE   41 //”правление PA LoRa
#define SOC_GPIO_PIN_LORA_RXE   42 //”правление PA LoRa

#if defined(CONFIG_IDF_TARGET_ESP32)
#define SOC_GPIO_PIN_LED        25
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
#define SOC_GPIO_PIN_LED        7
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define SOC_GPIO_PIN_LED        SOC_UNUSED_PIN /* TBD 14? */
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define SOC_GPIO_PIN_LED        19 /* D1 */
#else
#error "This ESP32 family build variant is not supported!"
#endif

#define SOC_GPIO_PIN_STATUS SOC_UNUSED_PIN

#define SOC_GPIO_PIN_GNSS_PPS 47
 

/* SPI (LoRa32 pins mapping) */
#define SOC_GPIO_PIN_MOSI       11
#define SOC_GPIO_PIN_MISO       13
#define SOC_GPIO_PIN_SCK        12
#define SOC_GPIO_PIN_SS         46


///* SX1276 [RFM95W] */
#define SOC_GPIO_PIN_RST        7
#define SOC_GPIO_PIN_DIO0       3
#define SOC_GPIO_PIN_SDA        8
#define SOC_GPIO_PIN_SCL        9

// Hardware pin definitions for Heltec and TTGO-V1 LoRa-32 Boards with OLED SSD1306 I2C Display
#define OLED_PIN_RST             U8X8_PIN_NONE // 16
#define OLED_PIN_SDA             8
#define OLED_PIN_SCL             9

#define SOC_GPIO_PIN__BUTTON    36

#if defined(CONFIG_IDF_TARGET_ESP32S2)
#define LV_HOR_RES                      (135) //Horizontal
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define LV_HOR_RES                      (80) //Horizontal
#else
#define LV_HOR_RES                      (240) //Horizontal
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define LV_VER_RES                      (160) //vertical
#else
#define LV_VER_RES                      (240) //vertical
#endif

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
};

/* https://github.com/espressif/usb-pids/blob/main/allocated-pids.txt#L313 */
enum softrf_usb_pid {
  FLYRF_USB_PID_WEBTOP     = 0x8131,
  FLYRF_USB_PID_STANDALONE = 0x8132,
  FLYRF_USB_PID_PRIME_MK3  = 0x8133,
  FLYRF_USB_PID_UF2_BOOT   = 0x8134,
  FLYRF_USB_PID_HAM        = 0x818F,
  FLYRF_USB_PID_MIDI       = 0x81A0,
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

#define MPU6886_REG_PWR_MGMT_1  (0x6B)
#define MPU6886_REG_WHOAMI      (0x75)
#define MPU9250_ADDRESS         (0x68)
#define MPU9250_REG_PWR_MGMT_1  (0x6B)
#define MPU9250_REG_WHOAMI      (0x75)
#define QMI8658_REG_RESET       (0x60)
#define QMI8658_REG_WHOAMI      (0x00)

/* Disable brownout detection (avoid unexpected reset on some boards) */
#define ESP32_DISABLE_BROWNOUT_DETECTOR 0

#define NMEA_TCP_SERVICE
#define USE_NMEALIB
#define USE_OLED
#define EXCLUDE_OLED_049
//#define EXCLUDE_OLED_BARO_PAGE
//#define USE_TFT
#define USE_NMEA_CFG
#define USE_BASICMAC

#define USE_TIME_SLOTS

/* Experimental */
#define USE_OGN_ENCRYPTION

#define ENABLE_UBLOX_RFS        /* revert factory settings (when necessary)  */
#define EXCLUDE_GNSS_GOKE       /* 'Air530' GK9501 GPS/GLO/BDS (GAL inop.)   */
#define EXCLUDE_GNSS_SONY
#define EXCLUDE_GNSS_MTK

#define EXCLUDE_FLYRF_HEARTBEAT
#define EXCLUDE_LK8EX1

#if !defined(CONFIG_IDF_TARGET_ESP32)
//#define EXCLUDE_UATM

#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)

/* Experimental */
//#define USE_USB_HOST

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define USE_U10_EXT

#endif /* CONFIG_IDF_TARGET_ESP32S3 */

#if defined(USE_USB_HOST)
#undef  SOC_GPIO_PIN_T8_S2_CONS_RX
#undef  SOC_GPIO_PIN_T8_S2_CONS_TX
#define SOC_GPIO_PIN_T8_S2_CONS_RX      46 // 43
#define SOC_GPIO_PIN_T8_S2_CONS_TX      45 // 44

/* Experimental */
#define ENABLE_D1090_INPUT

#include <cdc_acm_host.h>

typedef struct {
    bool connected;
    int index;
    CdcAcmDevice *device;
} ESP32_USBSerial_device_t;

typedef struct {
    uint16_t vid;
    uint16_t pid;
    uint8_t type;
    uint8_t model;
    const char *first_name;
    const char *last_name;
} USB_Device_List_t;

extern ESP32_USBSerial_device_t ESP32_USB_Serial;
extern const USB_Device_List_t supported_USB_devices[];

#endif /* USE_USB_HOST */
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#undef USE_OLED
#undef USE_TFT
#endif /* CONFIG_IDF_TARGET_ESP32SX | C3 */
#endif /* NOT CONFIG_IDF_TARGET_ESP32 */

#define POWER_SAVING_WIFI_TIMEOUT 600000UL /* 10 minutes */


#if defined(USE_OLED)
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define U8X8_OLED_I2C_BUS_TYPE  U8X8_SSD1306_128X64_NONAME_HW_I2C
//#define ENABLE_OLED_TEXT_PAGE
#else
#define U8X8_OLED_I2C_BUS_TYPE  U8X8_SSD1306_128X64_NONAME_2ND_HW_I2C
#endif /* CONFIG_IDF_TARGET_ESP32S3 */
#endif /* USE_OLED */

#endif /* PLATFORM_ESP32_H */

#endif /* ESP32 */
