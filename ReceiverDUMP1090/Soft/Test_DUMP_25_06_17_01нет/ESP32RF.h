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
#include "CoreCommandBuffer.h"

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



enum
{
    DISPLAY_NONE,
    DISPLAY_OLED,
    DISPLAY_TFT_24,
    DISPLAY_TFT_TTGO_240,
    DISPLAY_TFT_TTGO_135,
    DISPLAY_TFT_HELTEC_80,
};

enum
{
    D1090_UART_OFF,
    D1090_UART_MINI,
    D1090_UART_FULL,
};

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
    LORA_OFF,
    LORA_SX1276,
    LORA_SX1262,
    SX1262_LLCC68
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
 

enum
{
    VIEW_AKK_OFF,
    VIEW_AKK_ON
};

enum
{
    VIEW_SOS_OFF,
    VIEW_SOS_ON
};

enum
{
    IMPUT_COORD_AUTO,
    IMPUT_COORD_MANUAL
};

enum
{
    IMPUT_N,
    IMPUT_S
};

enum
{
    IMPUT_E,
    IMPUT_W
};

enum
{
    VIEW_TEST_COORD_ON,
    VIEW_TEST_COORD_OFF
};

enum
{
    COORD_MIN,
    COORD_MAX
};




/* Peripherals */
#define SOC_GPIO_PIN_GNSS_RX    15
#define SOC_GPIO_PIN_GNSS_TX    16
#define SOC_GPIO_PIN_GNSS_PPS   47

#define SOC_GPIO_PIN_TRACKER_RX 38 // Трекер
#define SOC_GPIO_PIN_TRACKER_TX 39 // Трекер

#define SOC_GPIO_PIN_SOS        42 //Кнопка SOS
//#define SOC_GPIO_PIN_AKK        3 //Напряжение аккумулятора

#define SOC_GPIO_PIN_LORA_TXE   -1//SOC_UNUSED_PIN //Управление PA LoRa
#define SOC_GPIO_PIN_LORA_RXE   -1//SOC_UNUSED_PIN //Управление PA LoRa

#define SOC_GPIO_PIN_LED        4 /* TBD 14? */
#define SOC_GPIO_PIN_TFT_LED   21 

#define SOC_GPIO_PIN_STATUS SOC_UNUSED_PIN


/* SPI (LoRa32 pins mapping) */
#define SOC_GPIO_PIN_MOSI       11
#define SOC_GPIO_PIN_MISO       13
#define SOC_GPIO_PIN_SCK        12
#define SOC_GPIO_PIN_SS         46
#define SOC_GPIO_PIN_RST        7
#define SOC_GPIO_PIN_BUSY       18//SOC_UNUSED_PIN

#define SOC_GPIO_PIN_SDA        8
#define SOC_GPIO_PIN_SCL        9

// Hardware pin definitions for Heltec and TTGO-V1 LoRa-32 Boards with OLED SSD1306 I2C Display
#define OLED_PIN_RST             U8X8_PIN_NONE // 16
#define OLED_PIN_SDA             8
#define OLED_PIN_SCL             9

#define SOC_GPIO_PIN__BUTTON    48

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

#define NMEA_TCP_SERVICE
#define USE_NMEALIB
//#define USE_OLED
#define EXCLUDE_OLED_049
#define USE_TFT
#define USE_NMEA_CFG

/*BasicMAC — это переносимая реализация спецификации LoRaWAN™ от LoRa™ Alliance на языке программирования C.
Это ответвление библиотеки LMiC от IBM, которое поддерживает несколько регионов, выбираемых во время компиляции и/или выполнения.
Оно может работать с устройствами класса A, класса B и класса C.*/
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

static void waiting_txt(void); // Вывод текста "ОПРЕДЕЛЕНИЕ МЕСТОПОЛОЖЕНИЯ"
static void Draw_circular_scale(void);
static void drawMessage();
static void display_state_SOS_button();
static void execution_state_SOS_button();
int mb_strlen(char* source, int letter_n);
static void clearMSG(void);
float bearing_calc(float lat, float lon, float lat2, float lon2);
double distance_form(double lat1, double long1, double lat2, double long2);
int alien_count();
bool coordinates_waiting();
float battery_read();                //Вывод напряжения аккумулятора в процентах


#endif /* ESP32 */
