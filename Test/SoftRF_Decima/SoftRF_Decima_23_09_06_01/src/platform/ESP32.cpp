/*
 * Platform_ESP32.cpp
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

#include "ESP32.h"

#include "sdkconfig.h"

#include <SPI.h>
#include <esp_err.h>
#include <esp_wifi.h>
//#if !defined(CONFIG_IDF_TARGET_ESP32S2)
//#include <esp_bt.h>
//#include <BLEDevice.h>
//#endif /* CONFIG_IDF_TARGET_ESP32S2 */
#include <soc/rtc_cntl_reg.h>
#include <soc/efuse_reg.h>
#include <Wire.h>
#include <rom/rtc.h>
#include <rom/spi_flash.h>
#include <soc/adc_channel.h>
//#include <flashchips.h>
//#include <axp20x.h>
#define  XPOWERS_CHIP_AXP2102
//#include <XPowersLib.h>
//#include <pcf8563.h>

#include "../system/SoC.h"
//#include "../system/Time.h"
//#include "../driver/Sound.h"
//#include "../driver/EEPROM.h"
//#include "../driver/RF.h"
//#include "../driver/WiFi.h"
//#include "../driver/Bluetooth.h"
//#include "../driver/LED.h"
//#include "../driver/Baro.h"
#include "../driver/Battery.h"
//#include "../driver/OLED.h"
//#include "../protocol/data/NMEA.h"
//#include "../protocol/data/GDL90.h"
//#include "../protocol/data/D1090.h"

//#if defined(USE_TFT)
//#include <TFT_eSPI.h>
//#endif /* USE_TFT */
//
//#include <battery.h>
//
//// SX12xx pin mapping
//lmic_pinmap lmic_pins = {
//    .nss  = SOC_GPIO_PIN_SS,
//    .txe  = LMIC_UNUSED_PIN,
//    .rxe  = LMIC_UNUSED_PIN,
//    .rst  = SOC_GPIO_PIN_RST,
//    .dio  = {LMIC_UNUSED_PIN, LMIC_UNUSED_PIN, LMIC_UNUSED_PIN},
//    .busy = SOC_GPIO_PIN_TXE,
//    .tcxo = LMIC_UNUSED_PIN,
//};

WebServer server ( 80 );

//#if defined(USE_NEOPIXELBUS_LIBRARY)
//NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PIX_NUM, SOC_GPIO_PIN_LED);
//#else /* USE_ADAFRUIT_NEO_LIBRARY */
//// Parameter 1 = number of pixels in strip
//// Parameter 2 = Arduino pin number (most are valid)
//// Parameter 3 = pixel type flags, add together as needed:
////   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
////   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
////   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
////   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIX_NUM, SOC_GPIO_PIN_LED,
//                              NEO_GRB + NEO_KHZ800);
//#endif /* USE_NEOPIXELBUS_LIBRARY */

//#if defined(USE_OLED)
////U8X8_OLED_I2C_BUS_TYPE u8x8_ttgo  (TTGO_V2_OLED_PIN_RST);
//U8X8_OLED_I2C_BUS_TYPE u8x8_heltec(HELTEC_OLED_PIN_RST);
////U8X8_SH1106_128X64_NONAME_HW_I2C u8x8_1_3(U8X8_PIN_NONE);
//#endif /* USE_OLED */

//#if defined(USE_TFT)
//static TFT_eSPI *tft = NULL;
//
//void TFT_off()
//{
//#ifndef ST7735_DRIVER
//    tft->writecommand(TFT_DISPOFF);
//    tft->writecommand(TFT_SLPIN);
//#else
//    tft->writecommand(ST7735_DISPOFF);
//    tft->writecommand(ST7735_SLPIN);
//#endif /* ST7735_DRIVER */
//}
//
//void TFT_backlight_adjust(uint8_t level)
//{
//    ledcWrite(BACKLIGHT_CHANNEL, level);
//}
//
//bool TFT_isBacklightOn()
//{
//    return (bool)ledcRead(BACKLIGHT_CHANNEL);
//}
//
//void TFT_backlight_off()
//{
//    ledcWrite(BACKLIGHT_CHANNEL, 0);
//}
//
//void TFT_backlight_on()
//{
//    ledcWrite(BACKLIGHT_CHANNEL, 250);
//}
//#endif /* USE_TFT */
//
//AXP20X_Class axp_xxx;
//XPowersPMU   axp_2xxx;
//
//static int esp32_board = ESP32_DEVKIT; /* default */
//static size_t ESP32_Min_AppPart_Size = 0;
//
//static portMUX_TYPE GNSS_PPS_mutex = portMUX_INITIALIZER_UNLOCKED;
//static portMUX_TYPE PMU_mutex      = portMUX_INITIALIZER_UNLOCKED;
//volatile bool PMU_Irq = false;
//
//static bool GPIO_21_22_are_busy = false;

//static union {
//  uint8_t efuse_mac[6];
//  uint64_t chipmacid;
//};

//static bool TFT_display_frontpage = false;
//static uint32_t prev_tx_packets_counter = 0;
//static uint32_t prev_rx_packets_counter = 0;
//extern uint32_t tx_packets_counter, rx_packets_counter;
//extern bool loopTaskWDTEnabled;
//
//const char *ESP32SX_Device_Manufacturer = SOFTRF_IDENT;
//const char *ESP32SX_Model_Stand  = "Standalone Edition"; /* 303a:8132 */
//const char *ESP32S3_Model_Prime3 = "Prime Edition Mk.3"; /* 303a:8133 */
//const char *ESP32S3_Model_Ham    = "Ham Edition";        /* 303a:818F */
//const char *ESP32S3_Model_Midi   = "Midi Edition";       /* 303a:81A0 */
//const uint16_t ESP32SX_Device_Version = SOFTRF_USB_FW_VERSION;

////#if defined(EXCLUDE_WIFI)
////// Dummy definition to satisfy build sequence
////char UDPpacketBuffer[UDP_PACKET_BUFSIZE];
////#endif /* EXCLUDE_WIFI */
////
////#if defined(CONFIG_IDF_TARGET_ESP32S3)
//////#define SPI_DRIVER_SELECT 3
////#include <Adafruit_SPIFlash.h>
////#include "../driver/EPD.h"
////#include "uCDB.hpp"
////
////SPIClass uSD_SPI(HSPI);
////#define  SD_CONFIG SdSpiConfig(uSD_SS_pin, SHARED_SPI, SD_SCK_MHZ(16), &uSD_SPI)
////SdFat    uSD;
////
////static bool uSD_is_attached = false;
////
////Adafruit_FlashTransport_ESP32 HWFlashTransport;
////Adafruit_SPIFlash QSPIFlash(&HWFlashTransport);
////
////static Adafruit_SPIFlash *SPIFlash = &QSPIFlash;
////
/////// Flash device list count
////enum {
////  EXTERNAL_FLASH_DEVICE_COUNT
////};
////
/////// List of all possible flash devices used by ESP32 boards
////static SPIFlash_Device_t possible_devices[] = { };
////
////PCF8563_Class *rtc              = nullptr;
////I2CBus        *i2c              = nullptr;
////
////static bool ESP32_has_spiflash  = false;
////static uint32_t spiflash_id     = 0;
////static bool FATFS_is_mounted    = false;
////static bool ADB_is_open         = false;
////static bool RTC_sync            = false;
////
////RTC_Date fw_build_date_time     = RTC_Date(__DATE__, __TIME__);
////
////#if CONFIG_TINYUSB_MSC_ENABLED
////  #if defined(USE_ADAFRUIT_MSC)
////    #include "Adafruit_TinyUSB.h"
////
////    // USB Mass Storage object
////    Adafruit_USBD_MSC usb_msc;
////  #else
////    #include "USBMSC.h"
////
////    // USB Mass Storage object
////    USBMSC usb_msc;
////  #endif /* USE_ADAFRUIT_MSC */
////#endif /* CONFIG_TINYUSB_MSC_ENABLED */
////
////// file system object from SdFat
////FatVolume fatfs;
////
////ui_settings_t ui_settings = {
////    .units        = UNITS_METRIC,
////    .zoom         = ZOOM_MEDIUM,
////    .protocol     = PROTOCOL_NMEA,
////    .rotate       = ROTATE_0,
////    .orientation  = DIRECTION_TRACK_UP,
////    .adb          = DB_NONE,
////    .idpref       = ID_TYPE,
////    .vmode        = VIEW_MODE_STATUS,
////    .voice        = VOICE_OFF,
////    .aghost       = ANTI_GHOSTING_OFF,
////    .filter       = TRAFFIC_FILTER_OFF,
////    .team         = 0
////};
////
////ui_settings_t *ui;
////uCDB<FatVolume, File32> ucdb(fatfs);
////
////#if CONFIG_TINYUSB_MSC_ENABLED
////#if defined(USE_ADAFRUIT_MSC)
////// Callback invoked when received READ10 command.
////// Copy disk's data to buffer (up to bufsize) and
////// return number of copied bytes (must be multiple of block size)
////static int32_t ESP32_msc_read_cb (uint32_t lba, void* buffer, uint32_t bufsize)
////{
////  // Note: SPIFLash Bock API: readBlocks/writeBlocks/syncBlocks
////  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
////  return SPIFlash->readBlocks(lba, (uint8_t*) buffer, bufsize/512) ? bufsize : -1;
////}
////
////// Callback invoked when received WRITE10 command.
////// Process data in buffer to disk's storage and
////// return number of written bytes (must be multiple of block size)
////static int32_t ESP32_msc_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize)
////{
////  // Note: SPIFLash Bock API: readBlocks/writeBlocks/syncBlocks
////  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
////  return SPIFlash->writeBlocks(lba, buffer, bufsize/512) ? bufsize : -1;
////}
////
////// Callback invoked when WRITE10 command is completed (status received and accepted by host).
////// used to flush any pending cache.
////static void ESP32_msc_flush_cb (void)
////{
////  // sync with flash
////  SPIFlash->syncBlocks();
////
////  // clear file system's cache to force refresh
////  fatfs.cacheClear();
////}
////
////#else
////
////// Callback invoked when received READ10 command.
////// Copy disk's data to buffer (up to bufsize) and
////// return number of copied bytes (must be multiple of block size)
////static int32_t ESP32_msc_read_cb (uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
////{
////  // Note: SPIFLash Bock API: readBlocks/writeBlocks/syncBlocks
////  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
////  return SPIFlash->readBlocks(lba, offset, (uint8_t*) buffer, bufsize/512) ?
////         bufsize : -1;
////}
////
////// Callback invoked when received WRITE10 command.
////// Process data in buffer to disk's storage and
////// return number of written bytes (must be multiple of block size)
////static int32_t ESP32_msc_write_cb (uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
////{
////  // Note: SPIFLash Bock API: readBlocks/writeBlocks/syncBlocks
////  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
////  int32_t rval = SPIFlash->writeBlocks(lba, offset, buffer, bufsize/512) ?
////                 bufsize : -1;
////
////#if 1
////  // sync with flash
////  SPIFlash->syncBlocks();
////
////  // clear file system's cache to force refresh
////  fatfs.cacheClear();
////#endif
////
////  return rval;
////}
////#endif /* USE_ADAFRUIT_MSC */
////#endif /* CONFIG_TINYUSB_MSC_ENABLED */
////
////#if !defined(EXCLUDE_IMU)
////#include <SensorQMI8658.hpp>
////#include <MPU9250.h>
////
////#define IMU_UPDATE_INTERVAL 500 /* ms */
////#define OLED_FLIP_THRESHOLD 0.6
////
////SensorQMI8658 imu_qmi8658;
////MPU9250       imu_mpu9250;
////
////static unsigned long IMU_Time_Marker = 0;
////
////#if defined(USE_OLED)
////extern int32_t IMU_g_x10;
////#endif /* USE_OLED */
////#endif /* EXCLUDE_IMU */
////
////#if !defined(EXCLUDE_MAG)
////#include <SensorQMC6310.hpp>
////
////#define MAG_UPDATE_INTERVAL 500 /* ms */
////
////SensorQMC6310 mag_qmc6310;
////
////static unsigned long MAG_Time_Marker = 0;
////
////#if defined(USE_OLED)
////extern int32_t MAG_heading;
////#endif /* USE_OLED */
////#endif /* EXCLUDE_MAG */
////
////#include "soc/rtc.h"
////static uint32_t calibrate_one(rtc_cal_sel_t cal_clk, const char *name)
////{
////    const uint32_t cal_count = 1000;
////    const float factor = (1 << 19) * 1000.0f;
////    uint32_t cali_val;
////    for (int i = 0; i < 5; ++i) {
////        cali_val = rtc_clk_cal(cal_clk, cal_count);
////    }
////    return cali_val;
////}
////
////#define CALIBRATE_ONE(cali_clk) calibrate_one(cali_clk, #cali_clk)
////
//////#define DEBUG_X32K(s) Serial.println(s)
////#define DEBUG_X32K(s) {}
////
////static bool ESP32_has_32k_xtal = false;
////
////#if defined(USE_NEOPIXELBUS_LIBRARY)
////NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> TWR2_Pixel(1, SOC_GPIO_PIN_TWR2_NEOPIXEL);
////#endif /* USE_NEOPIXELBUS_LIBRARY */
////
////#if defined(USE_SA8X8)
////#include <SA818Controller.h>
////extern SA818Controller controller;
////#endif /* USE_SA8X8 */
////#endif /* CONFIG_IDF_TARGET_ESP32S3 */
////
////#if defined(ENABLE_D1090_INPUT)
////#include <mode-s.h>
////
////mode_s_t state;
////#endif /* ENABLE_D1090_INPUT */
//
//static void IRAM_ATTR ESP32_PMU_Interrupt_handler() {
//  portENTER_CRITICAL_ISR(&PMU_mutex);
//  PMU_Irq = true;
//  portEXIT_CRITICAL_ISR(&PMU_mutex);
//}
//
//static uint32_t ESP32_getFlashId()
//{
//  return g_rom_flashchip.device_id;
//}

//#if defined(CORE_DEBUG_LEVEL) && CORE_DEBUG_LEVEL>0 && !defined(TAG)
//#define TAG "MAC"
//#endif
//
//static void ESP32_setup()
//{
//#if !defined(SOFTRF_ADDRESS)
//
//  esp_err_t ret = ESP_OK;
//  uint8_t null_mac[6] = {0};
//
//  ret = esp_efuse_mac_get_custom(efuse_mac);
//  if (ret != ESP_OK) {
//      ESP_LOGE(TAG, "Get base MAC address from BLK3 of EFUSE error (%s)", esp_err_to_name(ret));
//    /* If get custom base MAC address error, the application developer can decide what to do:
//     * abort or use the default base MAC address which is stored in BLK0 of EFUSE by doing
//     * nothing.
//     */
//
//    ESP_LOGI(TAG, "Use base MAC address which is stored in BLK0 of EFUSE");
//    chipmacid = ESP.getEfuseMac();
//  } else {
//    if (memcmp(efuse_mac, null_mac, 6) == 0) {
//      ESP_LOGI(TAG, "Use base MAC address which is stored in BLK0 of EFUSE");
//      chipmacid = ESP.getEfuseMac();
//    }
//  }
//#endif /* SOFTRF_ADDRESS */
//
//#if ESP32_DISABLE_BROWNOUT_DETECTOR
//  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
//#endif
//
//  size_t flash_size = spi_flash_get_chip_size();
//  size_t min_app_size = flash_size;
//
//  esp_partition_iterator_t it;
//  const esp_partition_t *part;
//
//  it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
//  if (it) {
//    do {
//      part = esp_partition_get(it);
//      if (part->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
//        continue;
//      }
//      if (part->size < min_app_size) {
//        min_app_size = part->size;
//      }
//    } while (it = esp_partition_next(it));
//
//    if (it) esp_partition_iterator_release(it);
//  }
//
//  if (min_app_size && (min_app_size != flash_size)) {
//    ESP32_Min_AppPart_Size = min_app_size;
//  }
//
//  if (psramFound()) {
//
//    uint32_t flash_id = ESP32_getFlashId();
//
//    /*
//     *    Board         |   Module      |  Flash memory IC
//     *  ----------------+---------------+--------------------
//     *  DoIt ESP32      | WROOM         | GIGADEVICE_GD25Q32
//     *  TTGO T3  V2.0   | PICO-D4 IC    | GIGADEVICE_GD25Q32
//     *  TTGO T3  V2.1.6 | PICO-D4 IC    | GIGADEVICE_GD25Q32
//     *  TTGO T22 V06    |               | WINBOND_NEX_W25Q32_V
//     *  TTGO T22 V08    |               | WINBOND_NEX_W25Q32_V
//     *  TTGO T22 V11    |               | BOYA_BY25Q32AL
//     *  TTGO T8  V1.8   | WROVER        | GIGADEVICE_GD25LQ32
//     *  TTGO T8 S2 V1.1 |               | WINBOND_NEX_W25Q32_V
//     *  TTGO T5S V1.9   |               | WINBOND_NEX_W25Q32_V
//     *  TTGO T5S V2.8   |               | BOYA_BY25Q32AL
//     *  TTGO T5  4.7    | WROVER-E      | XMC_XM25QH128C
//     *  TTGO T-Watch    |               | WINBOND_NEX_W25Q128_V
//     *  Ai-T NodeMCU-S3 | ESP-S3-12K    | GIGADEVICE_GD25Q64C
//     *  TTGO T-Dongle   |               | BOYA_BY25Q32AL
//     *  TTGO S3 Core    |               | GIGADEVICE_GD25Q64C
//     *  TTGO T-01C3     |               | BOYA_BY25Q32AL
//     *                  | ESP-C3-12F    | XMC_XM25QH32B
//     *  LilyGO T-TWR    | WROOM-1-N16R8 | GIGADEVICE_GD25Q128
//     *  Heltec Tracker  |               | GIGADEVICE_GD25Q64
//     */
//
//    switch(flash_id)
//    {
//    case MakeFlashId(GIGADEVICE_ID, GIGADEVICE_GD25LQ32):
//      /* ESP32-WROVER module with ESP32-NODEMCU-ADAPTER */
//      hw_info.model = SOFTRF_MODEL_STANDALONE;
//      break;
//    case MakeFlashId(WINBOND_NEX_ID, WINBOND_NEX_W25Q128_V):
//      hw_info.model = SOFTRF_MODEL_SKYWATCH;
//      break;
//#if defined(CONFIG_IDF_TARGET_ESP32)
//    case MakeFlashId(WINBOND_NEX_ID, WINBOND_NEX_W25Q32_V):
//    case MakeFlashId(BOYA_ID, BOYA_BY25Q32AL):
//    default:
//      hw_info.model = SOFTRF_MODEL_PRIME_MK2;
//#elif defined(CONFIG_IDF_TARGET_ESP32S2)
//    default:
//      esp32_board   = ESP32_S2_T8_V1_1;
//#elif defined(CONFIG_IDF_TARGET_ESP32S3)
//    case MakeFlashId(GIGADEVICE_ID, GIGADEVICE_GD25Q128):
//      hw_info.model = SOFTRF_MODEL_HAM;
//      break;
//    case MakeFlashId(GIGADEVICE_ID, GIGADEVICE_GD25Q64):
//    default:
//      hw_info.model = SOFTRF_MODEL_PRIME_MK3;
//#elif defined(CONFIG_IDF_TARGET_ESP32C3)
//    case MakeFlashId(ST_ID, XMC_XM25QH32B):
//    default:
//      esp32_board   = ESP32_C3_DEVKIT;
//#else
//#error "This ESP32 family build variant is not supported!"
//#endif
//      break;
//    }
//  } else {
//#if defined(CONFIG_IDF_TARGET_ESP32)
//    uint32_t chip_ver = REG_GET_FIELD(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_VER_PKG);
//    uint32_t pkg_ver  = chip_ver & 0x7;
//    if (pkg_ver == EFUSE_RD_CHIP_VER_PKG_ESP32PICOD4) {
//      esp32_board    = ESP32_TTGO_V2_OLED;
//      lmic_pins.rst  = SOC_GPIO_PIN_TBEAM_RF_RST_V05;
//      lmic_pins.busy = SOC_GPIO_PIN_TBEAM_RF_BUSY_V08;
//    }
////#elif defined(CONFIG_IDF_TARGET_ESP32S2)
////    esp32_board      = ESP32_S2_T8_V1_1;
////#elif defined(CONFIG_IDF_TARGET_ESP32S3)
////    if (ESP32_getFlashId() == MakeFlashId(GIGADEVICE_ID, GIGADEVICE_GD25Q128)) {
////      hw_info.model  = SOFTRF_MODEL_HAM;  /* allow psramFound() to fail */
////    } else if (ESP32_getFlashId() == MakeFlashId(GIGADEVICE_ID, GIGADEVICE_GD25Q64)) {
////      esp32_board    = ESP32_HELTEC_TRACKER;
////      hw_info.model  = SOFTRF_MODEL_MIDI;
////    } else {
////      esp32_board    = ESP32_S3_DEVKIT;
////    }
////#elif defined(CONFIG_IDF_TARGET_ESP32C3)
////    esp32_board      = ESP32_C3_DEVKIT;
//#endif /* CONFIG_IDF_TARGET_ESP32 */
//  }
//
//  /*if (SOC_GPIO_PIN_BUZZER != SOC_UNUSED_PIN) {
//    ledcAttachPin(SOC_GPIO_PIN_BUZZER, LEDC_CHANNEL_BUZZER);
//    ledcSetup(LEDC_CHANNEL_BUZZER, 0, LEDC_RESOLUTION_BUZZER);
//  }*/
//
//  if (hw_info.model == SOFTRF_MODEL_SKYWATCH) 
//  {
//    esp32_board = ESP32_TTGO_T_WATCH;
//    hw_info.rtc = RTC_PCF8563;
//    hw_info.imu = ACC_BMA423;
//
//    Wire1.begin(SOC_GPIO_PIN_TWATCH_SEN_SDA , SOC_GPIO_PIN_TWATCH_SEN_SCL);
//    Wire1.beginTransmission(AXP202_SLAVE_ADDRESS);
//    bool has_axp202 = (Wire1.endTransmission() == 0);
//    if (has_axp202) {
//
//      hw_info.pmu = PMU_AXP202;
//
//      axp_xxx.begin(Wire1, AXP202_SLAVE_ADDRESS);
//
//      axp_xxx.enableIRQ(AXP202_ALL_IRQ, AXP202_OFF);
//      axp_xxx.adc1Enable(0xFF, AXP202_OFF);
//
//      axp_xxx.setChgLEDMode(AXP20X_LED_LOW_LEVEL);
//
//      axp_xxx.setPowerOutPut(AXP202_LDO2, AXP202_ON); // BL
//      axp_xxx.setPowerOutPut(AXP202_LDO3, AXP202_ON); // S76G (MCU + LoRa)
//      axp_xxx.setLDO4Voltage(AXP202_LDO4_1800MV);
//      axp_xxx.setPowerOutPut(AXP202_LDO4, AXP202_ON); // S76G (Sony GNSS)
//
//      pinMode(SOC_GPIO_PIN_TWATCH_PMU_IRQ, INPUT_PULLUP);
//
//      attachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_TWATCH_PMU_IRQ),
//                      ESP32_PMU_Interrupt_handler, FALLING);
//
//      axp_xxx.adc1Enable(AXP202_BATT_VOL_ADC1, AXP202_ON);
//      axp_xxx.enableIRQ(AXP202_PEK_LONGPRESS_IRQ | AXP202_PEK_SHORTPRESS_IRQ, true);
//      axp_xxx.clearIRQ();
//    } else {
//      WIRE_FINI(Wire1);
//    }
//  } else if (hw_info.model == SOFTRF_MODEL_PRIME_MK2) {
//    esp32_board = ESP32_TTGO_T_BEAM;
//
//    Wire1.begin(TTGO_V2_OLED_PIN_SDA , TTGO_V2_OLED_PIN_SCL);
//    Wire1.beginTransmission(AXP192_SLAVE_ADDRESS);
//    bool has_axp = (Wire1.endTransmission() == 0);
//
//    bool has_axp192 = has_axp &&
//                      (axp_xxx.begin(Wire1, AXP192_SLAVE_ADDRESS) == AXP_PASS);
//
//    if (has_axp192) {
//
//      hw_info.revision = 8;
//      hw_info.pmu = PMU_AXP192;
//
//      axp_xxx.setChgLEDMode(AXP20X_LED_LOW_LEVEL);
//
//      axp_xxx.setPowerOutPut(AXP192_LDO2,  AXP202_ON);
//      axp_xxx.setPowerOutPut(AXP192_LDO3,  AXP202_ON);
//      axp_xxx.setPowerOutPut(AXP192_DCDC1, AXP202_ON);
//      axp_xxx.setPowerOutPut(AXP192_DCDC2, AXP202_ON); // NC
//      axp_xxx.setPowerOutPut(AXP192_EXTEN, AXP202_ON);
//
//      axp_xxx.setDCDC1Voltage(3300); //       AXP192 power-on value: 3300
//      axp_xxx.setLDO2Voltage (3300); // LoRa, AXP192 power-on value: 3300
//      axp_xxx.setLDO3Voltage (3000); // GPS,  AXP192 power-on value: 2800
//
//      pinMode(SOC_GPIO_PIN_TBEAM_V08_PMU_IRQ, INPUT /* INPUT_PULLUP */);
//
//      attachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_TBEAM_V08_PMU_IRQ),
//                      ESP32_PMU_Interrupt_handler, FALLING);
//
//      axp_xxx.enableIRQ(AXP202_PEK_LONGPRESS_IRQ | AXP202_PEK_SHORTPRESS_IRQ, true);
//      axp_xxx.clearIRQ();
//    } else {
//      bool has_axp2101 = has_axp && axp_2xxx.begin(Wire1,
//                                                   AXP2101_SLAVE_ADDRESS,
//                                                   TTGO_V2_OLED_PIN_SDA,
//                                                   TTGO_V2_OLED_PIN_SCL);
//      if (has_axp2101) {
//
//        // Set the minimum common working voltage of the PMU VBUS input,
//        // below this value will turn off the PMU
//        axp_2xxx.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);
//
//        // Set the maximum current of the PMU VBUS input,
//        // higher than this value will turn off the PMU
//        axp_2xxx.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);
//
//        // DCDC1 1500~3400mV, IMAX=2A
//        axp_2xxx.setDC1Voltage(3300); // ESP32,  AXP2101 power-on value: 3300
//
//        // ALDO 500~3500V, 100mV/step, IMAX=300mA
//        axp_2xxx.setButtonBatteryChargeVoltage(3100); // GNSS battery
//
//        axp_2xxx.setALDO2Voltage(3300); // LoRa, AXP2101 power-on value: 2800
//        axp_2xxx.setALDO3Voltage(3300); // GPS,  AXP2101 power-on value: 3300
//
//        // axp_2xxx.enableDC1();
//        axp_2xxx.enableButtonBatteryCharge();
//
//        axp_2xxx.enableALDO2();
//        axp_2xxx.enableALDO3();
//
//        axp_2xxx.setChargingLedMode(XPOWERS_CHG_LED_ON);
//
//        pinMode(SOC_GPIO_PIN_TBEAM_V08_PMU_IRQ, INPUT /* INPUT_PULLUP */);
//
//        attachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_TBEAM_V08_PMU_IRQ),
//                        ESP32_PMU_Interrupt_handler, FALLING);
//
//        axp_2xxx.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
//        axp_2xxx.clearIrqStatus();
//
//        axp_2xxx.enableIRQ(XPOWERS_AXP2101_PKEY_LONG_IRQ |
//                           XPOWERS_AXP2101_PKEY_SHORT_IRQ);
//
//        hw_info.revision = 12;
//        hw_info.pmu = PMU_AXP2101;
//      } else {
//        WIRE_FINI(Wire1);
//        hw_info.revision = 2;
//      }
//    }
//    lmic_pins.rst  = SOC_GPIO_PIN_TBEAM_RF_RST_V05;
//    lmic_pins.busy = SOC_GPIO_PIN_TBEAM_RF_BUSY_V08;
////#if defined(CONFIG_IDF_TARGET_ESP32S2)
////  } else if (esp32_board == ESP32_S2_T8_V1_1) {
////    lmic_pins.nss  = SOC_GPIO_PIN_T8_S2_LORA_SS;
////    lmic_pins.rst  = SOC_GPIO_PIN_T8_S2_LORA_RST;
////    lmic_pins.busy = LMIC_UNUSED_PIN;
////
////    pinMode(SOC_GPIO_PIN_T8_S2_PWR_EN, INPUT_PULLUP);
////
////#if defined(USE_USB_HOST)
////    Serial.end();
////    Serial.begin(SERIAL_OUT_BR, SERIAL_IN_BITS,
////                 SOC_GPIO_PIN_T8_S2_CONS_RX, SOC_GPIO_PIN_T8_S2_CONS_TX);
////#endif /* USE_USB_HOST */
////
////#endif /* CONFIG_IDF_TARGET_ESP32S2 */
//
////#if defined(CONFIG_IDF_TARGET_ESP32S3)
////  } else if (hw_info.model == SOFTRF_MODEL_PRIME_MK3 ||
////             esp32_board   == ESP32_S3_DEVKIT) {
////    Wire1.begin(SOC_GPIO_PIN_S3_PMU_SDA , SOC_GPIO_PIN_S3_PMU_SCL);
////    Wire1.beginTransmission(AXP2101_SLAVE_ADDRESS);
////    bool has_axp2101 = (Wire1.endTransmission() == 0) &&
////                       axp_2xxx.begin(Wire1, AXP2101_SLAVE_ADDRESS,
////                                      SOC_GPIO_PIN_S3_PMU_SDA,
////                                      SOC_GPIO_PIN_S3_PMU_SCL);
////    if (has_axp2101) {
////      esp32_board   = ESP32_TTGO_T_BEAM_SUPREME;
////      hw_info.model = SOFTRF_MODEL_PRIME_MK3; /* allow psramFound() to fail */
////      hw_info.pmu   = PMU_AXP2101;
////
////      /* inactivate tinyUF2 LED output setting */
////      pinMode(SOC_GPIO_PIN_S3_GNSS_PPS, INPUT);
////
////      // Set the minimum common working voltage of the PMU VBUS input,
////      // below this value will turn off the PMU
////      axp_2xxx.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);
////
////      // Set the maximum current of the PMU VBUS input,
////      // higher than this value will turn off the PMU
////      axp_2xxx.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);
////
////      // DCDC1 1500~3400mV, IMAX=2A
////      axp_2xxx.setDC1Voltage(3300);
////
////#if defined(USE_USB_HOST)
////      // DCDC5 1400~3700mV, 100mV/step, 24 steps, IMAX=1A
////      axp_2xxx.setDC5Voltage(3700);
////#endif /* USE_USB_HOST */
////
////      // ALDO 500~3500V, 100mV/step, IMAX=300mA
////      axp_2xxx.setALDO3Voltage(3300); // LoRa, AXP2101 power-on value: 3300
////      axp_2xxx.setALDO4Voltage(3300); // GNSS, AXP2101 power-on value: 2900
////
////      axp_2xxx.setALDO2Voltage(3300); // RTC
////      axp_2xxx.setALDO1Voltage(3300); // sensors, OLED
////      axp_2xxx.setBLDO1Voltage(3300); // uSD
////
////      // axp_2xxx.enableDC1();
////
////#if defined(USE_USB_HOST)
////      axp_2xxx.enableDC5();
////#endif /* USE_USB_HOST */
////
////      axp_2xxx.enableALDO3();
////      axp_2xxx.enableALDO4();
////
////      axp_2xxx.enableALDO2();
////      axp_2xxx.enableALDO1();
////      axp_2xxx.enableBLDO1();
////
////      axp_2xxx.setChargingLedMode(XPOWERS_CHG_LED_ON);
////
////      pinMode(SOC_GPIO_PIN_S3_PMU_IRQ, INPUT /* INPUT_PULLUP */);
////
////      attachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_S3_PMU_IRQ),
////                      ESP32_PMU_Interrupt_handler, FALLING);
////
////      axp_2xxx.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
////      axp_2xxx.clearIrqStatus();
////
////      axp_2xxx.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);
////      axp_2xxx.disableTSPinMeasure();
////      axp_2xxx.enableBattVoltageMeasure();
////
////      axp_2xxx.enableIRQ(XPOWERS_AXP2101_PKEY_LONG_IRQ |
////                         XPOWERS_AXP2101_PKEY_SHORT_IRQ);
////
////      /* Wake up Quectel L76K GNSS */
////      digitalWrite(SOC_GPIO_PIN_S3_GNSS_WAKE, HIGH);
////      pinMode(SOC_GPIO_PIN_S3_GNSS_WAKE, OUTPUT);
////
////      Wire1.beginTransmission(PCF8563_SLAVE_ADDRESS);
////      bool esp32_has_rtc = (Wire1.endTransmission() == 0);
////      if (!esp32_has_rtc) {
////        delay(200);
////        Wire1.beginTransmission(PCF8563_SLAVE_ADDRESS);
////        esp32_has_rtc = (Wire1.endTransmission() == 0);
////        if (!esp32_has_rtc) {
////          delay(200);
////          Wire1.beginTransmission(PCF8563_SLAVE_ADDRESS);
////          esp32_has_rtc = (Wire1.endTransmission() == 0);
////        }
////      }
////
////      i2c = new I2CBus(Wire1);
////
////      if (esp32_has_rtc && (i2c != nullptr)) {
////        rtc = new PCF8563_Class(*i2c);
////
////        pinMode(SOC_GPIO_PIN_S3_RTC_IRQ, INPUT);
////        hw_info.rtc = RTC_PCF8563;
////      }
////
////      /* wait until every LDO voltage will settle down */
////      delay(200);
////
////#if !defined(EXCLUDE_MAG)
////      bool has_qmc = mag_qmc6310.begin(Wire, QMC6310_SLAVE_ADDRESS,
////                                       SOC_GPIO_PIN_S3_SDA, SOC_GPIO_PIN_S3_SCL);
////      if (has_qmc) {
////        mag_qmc6310.configMagnetometer(
////            /*
////            * Run Mode
////            * MODE_SUSPEND
////            * MODE_NORMAL
////            * MODE_SINGLE
////            * MODE_CONTINUOUS
////            * * */
////            SensorQMC6310::MODE_NORMAL,
////            /*
////            * Full Range
////            * RANGE_30G
////            * RANGE_12G
////            * RANGE_8G
////            * RANGE_2G
////            * * */
////            SensorQMC6310::RANGE_2G,
////            /*
////            * Output data rate
////            * DATARATE_10HZ
////            * DATARATE_50HZ
////            * DATARATE_100HZ
////            * DATARATE_200HZ
////            * * */
////            SensorQMC6310::DATARATE_100HZ,
////            /*
////            * Over sample Ratio1
////            * OSR_8
////            * OSR_4
////            * OSR_2
////            * OSR_1
////            * * * */
////            SensorQMC6310::OSR_1,
////
////            /*
////            * Down sample Ratio1
////            * DSR_8
////            * DSR_4
////            * DSR_2
////            * DSR_1
////            * * */
////            SensorQMC6310::DSR_1);
////
////        hw_info.mag = MAG_QMC6310;
////      } else {
////        WIRE_FINI(Wire);
////      }
////
////      MAG_Time_Marker = millis();
////#endif /* EXCLUDE_MAG */
////
////#if !defined(EXCLUDE_IMU)
////      imu_qmi8658.setSpiSetting(4000000, MSBFIRST, SPI_MODE0);
////      bool has_qmi = imu_qmi8658.begin(SOC_GPIO_PIN_S3_IMU_SS,
////                                       SOC_GPIO_PIN_S3_IMU_MOSI,
////                                       SOC_GPIO_PIN_S3_IMU_MISO,
////                                       SOC_GPIO_PIN_S3_IMU_SCK,
////                                       uSD_SPI);
////      if (has_qmi) {
////        imu_qmi8658.configAccelerometer(
////            /*
////             * ACC_RANGE_2G
////             * ACC_RANGE_4G
////             * ACC_RANGE_8G
////             * ACC_RANGE_16G
////             * */
////            SensorQMI8658::ACC_RANGE_4G,
////            /*
////             * ACC_ODR_1000H
////             * ACC_ODR_500Hz
////             * ACC_ODR_250Hz
////             * ACC_ODR_125Hz
////             * ACC_ODR_62_5Hz
////             * ACC_ODR_31_25Hz
////             * ACC_ODR_LOWPOWER_128Hz
////             * ACC_ODR_LOWPOWER_21Hz
////             * ACC_ODR_LOWPOWER_11Hz
////             * ACC_ODR_LOWPOWER_3H
////            * */
////            SensorQMI8658::ACC_ODR_1000Hz,
////            /*
////            *  LPF_MODE_0     //2.66% of ODR
////            *  LPF_MODE_1     //3.63% of ODR
////            *  LPF_MODE_2     //5.39% of ODR
////            *  LPF_MODE_3     //13.37% of ODR
////            * */
////            SensorQMI8658::LPF_MODE_0,
////            // selfTest enable
////            true);
////
////        imu_qmi8658.configGyroscope(
////            /*
////            * GYR_RANGE_16DPS
////            * GYR_RANGE_32DPS
////            * GYR_RANGE_64DPS
////            * GYR_RANGE_128DPS
////            * GYR_RANGE_256DPS
////            * GYR_RANGE_512DPS
////            * GYR_RANGE_1024DPS
////            * */
////            SensorQMI8658::GYR_RANGE_64DPS,
////            /*
////             * GYR_ODR_7174_4Hz
////             * GYR_ODR_3587_2Hz
////             * GYR_ODR_1793_6Hz
////             * GYR_ODR_896_8Hz
////             * GYR_ODR_448_4Hz
////             * GYR_ODR_224_2Hz
////             * GYR_ODR_112_1Hz
////             * GYR_ODR_56_05Hz
////             * GYR_ODR_28_025H
////             * */
////            SensorQMI8658::GYR_ODR_896_8Hz,
////            /*
////            *  LPF_MODE_0     //2.66% of ODR
////            *  LPF_MODE_1     //3.63% of ODR
////            *  LPF_MODE_2     //5.39% of ODR
////            *  LPF_MODE_3     //13.37% of ODR
////            * */
////            SensorQMI8658::LPF_MODE_3,
////            // selfTest enable
////            true);
////
////        // In 3DOF mode,
////        imu_qmi8658.enableAccelerometer();
////
////        hw_info.imu = IMU_QMI8658;
////      }
////
////      IMU_Time_Marker = millis();
////
////      if (hw_info.imu == IMU_NONE) {
////        uSD_SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
////        digitalWrite(SOC_GPIO_PIN_S3_IMU_SS, LOW);
////
////        // reset device
////        uSD_SPI.transfer(MPU6886_REG_PWR_MGMT_1);
////        uSD_SPI.transfer(0x80);
////
////        digitalWrite(SOC_GPIO_PIN_S3_IMU_SS, HIGH);
////        uSD_SPI.endTransaction();
////
////        delay(100);
////
////        uSD_SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
////        digitalWrite(SOC_GPIO_PIN_S3_IMU_SS, LOW);
////
////        uSD_SPI.transfer(MPU6886_REG_WHOAMI | 0x80 /* read */);
////        hw_info.imu = (uSD_SPI.transfer(0x00) ==  0x19) ? IMU_MPU6886 : IMU_NONE;
////
////        digitalWrite(SOC_GPIO_PIN_S3_IMU_SS, HIGH);
////        uSD_SPI.endTransaction();
////      }
////
////      uSD_SPI.end();
////#endif /* EXCLUDE_IMU */
////    } else {
////      WIRE_FINI(Wire1);
////      esp32_board      = ESP32_S3_DEVKIT;
////      hw_info.model    = SOFTRF_MODEL_STANDALONE;
////      hw_info.revision = 203;
////
////#if !defined(EXCLUDE_IMU)
////#if 0
////      uSD_SPI.begin(SOC_GPIO_PIN_S3_IMU_SCK,
////                    SOC_GPIO_PIN_S3_IMU_MISO,
////                    SOC_GPIO_PIN_S3_IMU_MOSI,
////                    SOC_GPIO_PIN_S3_IMU_SS);
////      uSD_SPI.setHwCs(false);
////
////      pinMode(SOC_GPIO_PIN_S3_IMU_SS, OUTPUT);
////      digitalWrite(SOC_GPIO_PIN_S3_IMU_SS, HIGH);
////
////      delay(50);
////
////      uSD_SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
////      digitalWrite(SOC_GPIO_PIN_S3_IMU_SS, LOW);
////
////      // reset device
////      uSD_SPI.transfer(MPU9250_REG_PWR_MGMT_1);
////      uSD_SPI.transfer(0x80);
////
////      digitalWrite(SOC_GPIO_PIN_S3_IMU_SS, HIGH);
////      uSD_SPI.endTransaction();
////
////      delay(100);
////
////      uSD_SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
////      digitalWrite(SOC_GPIO_PIN_S3_IMU_SS, LOW);
////
////      uSD_SPI.transfer(MPU9250_REG_WHOAMI | 0x80 /* read */);
////      uint8_t whoami = uSD_SPI.transfer(0x00);
////      hw_info.imu = (whoami == 0x71 || whoami == 0x73) ? IMU_MPU9250 : IMU_NONE;
////
////      digitalWrite(SOC_GPIO_PIN_S3_IMU_SS, HIGH);
////      uSD_SPI.endTransaction();
////
////      uSD_SPI.end();
////
////      hw_info.mag = (hw_info.imu == IMU_MPU9250) ? MAG_AK8963 : hw_info.mag;
////#endif
////#endif /* EXCLUDE_IMU */
////    }
////
////#if ARDUINO_USB_CDC_ON_BOOT
////    SerialOutput.begin(SERIAL_OUT_BR, SERIAL_OUT_BITS,
////                       SOC_GPIO_PIN_S3_CONS_RX,
////                       SOC_GPIO_PIN_S3_CONS_TX);
////#endif /* ARDUINO_USB_CDC_ON_BOOT */
////
////    lmic_pins.nss  = SOC_GPIO_PIN_S3_SS;
////    lmic_pins.rst  = SOC_GPIO_PIN_S3_RST;
////    lmic_pins.busy = SOC_GPIO_PIN_S3_BUSY;
////
////    ESP32_has_spiflash = SPIFlash->begin(possible_devices,
////                                         EXTERNAL_FLASH_DEVICE_COUNT);
////    if (ESP32_has_spiflash) {
////      spiflash_id = SPIFlash->getJEDECID();
////
////      uint32_t capacity = spiflash_id & 0xFF;
////      if (capacity >= 0x17) { /* equal or greater than 1UL << 23 (8 MiB) */
////        hw_info.storage = STORAGE_FLASH;
////
////#if CONFIG_TINYUSB_MSC_ENABLED
////  #if defined(USE_ADAFRUIT_MSC)
////        // Set disk vendor id, product id and revision
////        // with string up to 8, 16, 4 characters respectively
////        usb_msc.setID(ESP32SX_Device_Manufacturer, "Internal Flash", "1.0");
////
////        // Set callback
////        usb_msc.setReadWriteCallback(ESP32_msc_read_cb,
////                                     ESP32_msc_write_cb,
////                                     ESP32_msc_flush_cb);
////
////        // Set disk size, block size should be 512 regardless of spi flash page size
////        usb_msc.setCapacity(SPIFlash->size()/512, 512);
////
////        // MSC is ready for read/write
////        usb_msc.setUnitReady(true);
////
////        usb_msc.begin();
////
////  #else
////
////        // Set disk vendor id, product id and revision
////        // with string up to 8, 16, 4 characters respectively
////        usb_msc.vendorID(ESP32SX_Device_Manufacturer);
////        usb_msc.productID("Internal Flash");
////        usb_msc.productRevision("1.0");
////
////        // Set callback
////        usb_msc.onRead(ESP32_msc_read_cb);
////        usb_msc.onWrite(ESP32_msc_write_cb);
////
////        // MSC is ready for read/write
////        usb_msc.mediaPresent(true);
////
////        // Set disk size, block size should be 512 regardless of spi flash page size
////        usb_msc.begin(SPIFlash->size()/512, 512);
////  #endif /* USE_ADAFRUIT_MSC */
////#endif /* CONFIG_TINYUSB_MSC_ENABLED */
////
////        FATFS_is_mounted = fatfs.begin(SPIFlash);
////      }
////    }
////
////    int uSD_SS_pin = (esp32_board == ESP32_S3_DEVKIT) ?
////                     SOC_GPIO_PIN_S3_SD_SS_DK : SOC_GPIO_PIN_S3_SD_SS_TBEAM;
////
////    /* uSD-SPI init */
////    uSD_SPI.begin(SOC_GPIO_PIN_S3_SD_SCK,
////                  SOC_GPIO_PIN_S3_SD_MISO,
////                  SOC_GPIO_PIN_S3_SD_MOSI,
////                  uSD_SS_pin);
////
////    pinMode(uSD_SS_pin, OUTPUT);
////    digitalWrite(uSD_SS_pin, HIGH);
////
////    uSD_is_attached = uSD.cardBegin(SD_CONFIG);
////
////    if (uSD_is_attached && uSD.card()->cardSize() > 0) {
////      hw_info.storage = (hw_info.storage == STORAGE_FLASH) ?
////                        STORAGE_FLASH_AND_CARD : STORAGE_CARD;
////    }
////
////    ui = &ui_settings;
////
////  } else if (hw_info.model == SOFTRF_MODEL_HAM) {
////    Wire.begin(SOC_GPIO_PIN_TWR2_SDA , SOC_GPIO_PIN_TWR2_SCL);
////    Wire.beginTransmission(AXP2101_SLAVE_ADDRESS);
////    bool has_axp2101 = (Wire.endTransmission() == 0) &&
////                       axp_2xxx.begin(Wire, AXP2101_SLAVE_ADDRESS,
////                                      SOC_GPIO_PIN_TWR2_SDA,
////                                      SOC_GPIO_PIN_TWR2_SCL);
////    if (has_axp2101) {
////      esp32_board      = ESP32_LILYGO_T_TWR_V2_0;
////      hw_info.revision = 20;
////      hw_info.pmu      = PMU_AXP2101;
////
////      // Set the minimum common working voltage of the PMU VBUS input,
////      // below this value will turn off the PMU
////      axp_2xxx.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);
////
////      // Set the maximum current of the PMU VBUS input,
////      // higher than this value will turn off the PMU
////      axp_2xxx.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);
////
////      // DCDC1 1500~3400mV, IMAX=2A
////      axp_2xxx.setDC1Voltage(3300); // WROOM, OLED
////
////      // ALDO 500~3500V, 100mV/step, IMAX=300mA
////      axp_2xxx.setALDO2Voltage(3300); // micro-SD
////      axp_2xxx.setALDO4Voltage(3300); // GNSS, AXP2101 power-on value: 2900
////
////      axp_2xxx.setBLDO1Voltage(3300); // Mic
////
////      // axp_2xxx.enableDC1();
////
////      axp_2xxx.enableALDO2();
////      axp_2xxx.enableALDO4();
////
////      axp_2xxx.enableBLDO1();
////
////      axp_2xxx.setChargingLedMode(XPOWERS_CHG_LED_ON);
////
////      pinMode(SOC_GPIO_PIN_TWR2_PMU_IRQ, INPUT /* INPUT_PULLUP */);
////
////      attachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_TWR2_PMU_IRQ),
////                      ESP32_PMU_Interrupt_handler, FALLING);
////
////      axp_2xxx.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
////      axp_2xxx.clearIrqStatus();
////
////      axp_2xxx.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);
////      axp_2xxx.disableTSPinMeasure();
////      axp_2xxx.enableBattVoltageMeasure();
////
////      axp_2xxx.enableIRQ(XPOWERS_AXP2101_PKEY_LONG_IRQ |
////                         XPOWERS_AXP2101_PKEY_SHORT_IRQ);
////
////      /* wait until every LDO voltage will settle down */
////      delay(200);
////
////      lmic_pins.nss  = SOC_GPIO_PIN_TWR2_SS;
////      lmic_pins.rst  = SOC_UNUSED_PIN;
////      lmic_pins.busy = SOC_UNUSED_PIN;
////
////#if defined(USE_NEOPIXELBUS_LIBRARY)
////      TWR2_Pixel.Begin();
////      TWR2_Pixel.Show(); // Initialize all pixels to 'off'
////#endif /* USE_NEOPIXELBUS_LIBRARY */
////    } else {
////      WIRE_FINI(Wire);
////
////      /* TBD */
////    }
////
////#if ARDUINO_USB_CDC_ON_BOOT
////    SerialOutput.begin(SERIAL_OUT_BR, SERIAL_OUT_BITS,
////                       SOC_GPIO_PIN_TWR2_CONS_RX,
////                       SOC_GPIO_PIN_TWR2_CONS_TX);
////#endif /* ARDUINO_USB_CDC_ON_BOOT */
////
////    ESP32_has_spiflash = SPIFlash->begin(possible_devices,
////                                         EXTERNAL_FLASH_DEVICE_COUNT);
////    if (ESP32_has_spiflash) {
////      spiflash_id = SPIFlash->getJEDECID();
////
////      uint32_t capacity = spiflash_id & 0xFF;
////      if (capacity >= 0x18) { /* equal or greater than 1UL << 24 (16 MiB) */
////        hw_info.storage = STORAGE_FLASH;
////
////#if CONFIG_TINYUSB_MSC_ENABLED
////  #if defined(USE_ADAFRUIT_MSC)
////        // Set disk vendor id, product id and revision
////        // with string up to 8, 16, 4 characters respectively
////        usb_msc.setID(ESP32SX_Device_Manufacturer, "Internal Flash", "1.0");
////
////        // Set callback
////        usb_msc.setReadWriteCallback(ESP32_msc_read_cb,
////                                     ESP32_msc_write_cb,
////                                     ESP32_msc_flush_cb);
////
////        // Set disk size, block size should be 512 regardless of spi flash page size
////        usb_msc.setCapacity(SPIFlash->size()/512, 512);
////
////        // MSC is ready for read/write
////        usb_msc.setUnitReady(true);
////
////        usb_msc.begin();
////
////  #else
////
////        // Set disk vendor id, product id and revision
////        // with string up to 8, 16, 4 characters respectively
////        usb_msc.vendorID(ESP32SX_Device_Manufacturer);
////        usb_msc.productID("Internal Flash");
////        usb_msc.productRevision("1.0");
////
////        // Set callback
////        usb_msc.onRead(ESP32_msc_read_cb);
////        usb_msc.onWrite(ESP32_msc_write_cb);
////
////        // MSC is ready for read/write
////        usb_msc.mediaPresent(true);
////
////        // Set disk size, block size should be 512 regardless of spi flash page size
////        usb_msc.begin(SPIFlash->size()/512, 512);
////  #endif /* USE_ADAFRUIT_MSC */
////#endif /* CONFIG_TINYUSB_MSC_ENABLED */
////
////        FATFS_is_mounted = fatfs.begin(SPIFlash);
////      }
////    }
////
////    if (esp32_board == ESP32_LILYGO_T_TWR_V2_0) {
////      int uSD_SS_pin = SOC_GPIO_PIN_TWR2_SD_SS;
////
////      /* uSD-SPI init */
////      uSD_SPI.begin(SOC_GPIO_PIN_TWR2_SD_SCK,
////                    SOC_GPIO_PIN_TWR2_SD_MISO,
////                    SOC_GPIO_PIN_TWR2_SD_MOSI,
////                    uSD_SS_pin);
////
////      pinMode(uSD_SS_pin, OUTPUT);
////      digitalWrite(uSD_SS_pin, HIGH);
////
////      uSD_is_attached = uSD.cardBegin(SD_CONFIG);
////
////      if (uSD_is_attached && uSD.card()->cardSize() > 0) {
////        hw_info.storage = (hw_info.storage == STORAGE_FLASH) ?
////                          STORAGE_FLASH_AND_CARD : STORAGE_CARD;
////      }
////    }
////  } else if (hw_info.model == SOFTRF_MODEL_MIDI) {
////
////#if ARDUINO_USB_CDC_ON_BOOT
////    SerialOutput.begin(SERIAL_OUT_BR, SERIAL_OUT_BITS,
////                       SOC_GPIO_PIN_S3_CONS_RX,
////                       SOC_GPIO_PIN_S3_CONS_TX);
////#endif /* ARDUINO_USB_CDC_ON_BOOT */
////
////    lmic_pins.nss  = SOC_GPIO_PIN_HELTRK_SS;
////    lmic_pins.rst  = SOC_GPIO_PIN_HELTRK_RST;
////    lmic_pins.busy = SOC_GPIO_PIN_HELTRK_BUSY;
////
////#endif /* CONFIG_IDF_TARGET_ESP32S3 */
//
////#if defined(CONFIG_IDF_TARGET_ESP32C3)
////  } else if (esp32_board == ESP32_C3_DEVKIT) {
////
////    lmic_pins.nss  = SOC_GPIO_PIN_C3_SS;
////    lmic_pins.rst  = LMIC_UNUSED_PIN;
////    lmic_pins.busy = SOC_GPIO_PIN_C3_TXE;
////
////    /* TBD */
////
////#endif /* CONFIG_IDF_TARGET_ESP32C3 */
//  }
//
#if ARDUINO_USB_CDC_ON_BOOT && \
    (defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3))
  if (USB.manufacturerName(ESP32SX_Device_Manufacturer)) {
    char usb_serial_number[16];
    uint16_t pid;

    pid = (esp32_board == ESP32_TTGO_T_BEAM_SUPREME) ? SOFTRF_USB_PID_PRIME_MK3  :
          (esp32_board == ESP32_S2_T8_V1_1         ) ? SOFTRF_USB_PID_WEBTOP     :
          (esp32_board == ESP32_S3_DEVKIT          ) ? SOFTRF_USB_PID_STANDALONE :
          (esp32_board == ESP32_LILYGO_T_TWR_V2_0  ) ? SOFTRF_USB_PID_HAM        :
          (esp32_board == ESP32_HELTEC_TRACKER     ) ? SOFTRF_USB_PID_MIDI       :
          USB_PID /* 0x1001 */ ;

    snprintf(usb_serial_number, sizeof(usb_serial_number),
             "%02X%02X%02X%02X%02X%02X",
             efuse_mac[0], efuse_mac[1], efuse_mac[2],
             efuse_mac[3], efuse_mac[4], efuse_mac[5]);

    USB.VID(USB_VID); // USB_ESPRESSIF_VID = 0x303A
    USB.PID(pid);
    USB.productName(esp32_board == ESP32_TTGO_T_BEAM_SUPREME ? ESP32S3_Model_Prime3 :
                    esp32_board == ESP32_LILYGO_T_TWR_V2_0   ? ESP32S3_Model_Ham    :
                    esp32_board == ESP32_HELTEC_TRACKER      ? ESP32S3_Model_Midi   :
                    ESP32SX_Model_Stand);
    USB.firmwareVersion(ESP32SX_Device_Version);
    USB.serialNumber(usb_serial_number);
    USB.begin();
  }

  Serial.begin(SERIAL_OUT_BR);

  for (int i=0; i < 20; i++) {if (Serial) break; else delay(100);}

#if 0 /* TBD */
  if (Serial.rebootEnabled()) {
    Serial.enableReboot(false);
  }
#endif /* TBD */

//#else
//  Serial.begin(SERIAL_OUT_BR, SERIAL_OUT_BITS);
#endif /* ARDUINO_USB_CDC_ON_BOOT && (CONFIG_IDF_TARGET_ESP32S2 || S3) */

//#if defined(CONFIG_IDF_TARGET_ESP32S3)
//  if (esp32_board == ESP32_TTGO_T_BEAM_SUPREME)
//  {
//    rtc_clk_32k_enable(true);
//
//    CALIBRATE_ONE(RTC_CAL_RTC_MUX);
//    uint32_t cal_32k = CALIBRATE_ONE(RTC_CAL_32K_XTAL);
//
//    if (cal_32k == 0) {
//        DEBUG_X32K("32K XTAL OSC has not started up");
//    } else {
//        rtc_clk_slow_freq_set(RTC_SLOW_FREQ_32K_XTAL);
//        DEBUG_X32K("Switching of RTC clock source onto 32768 Hz XTAL is successful.");
//        CALIBRATE_ONE(RTC_CAL_RTC_MUX);
//        CALIBRATE_ONE(RTC_CAL_32K_XTAL);
//    }
//    CALIBRATE_ONE(RTC_CAL_RTC_MUX);
//    CALIBRATE_ONE(RTC_CAL_32K_XTAL);
//    if (rtc_clk_slow_freq_get() != RTC_SLOW_FREQ_32K_XTAL) {
//        DEBUG_X32K("Warning: Failed to switch RTC clock source onto 32768 Hz XTAL !");
//    } else {
//        ESP32_has_32k_xtal = true;
//    }
//#if !defined(EXCLUDE_IMU)
//    if (hw_info.imu             == IMU_QMI8658 /* && */
//     /* rtc_get_reset_reason(0) == POWERON_RESET */) {
//      for (int i=0; i<50; i++) {
//        if (imu_qmi8658.getDataReady()) {
//          float a_x, a_y, a_z;
//          if (imu_qmi8658.getAccelerometer(a_x, a_y, a_z)) {
//            if (a_x > OLED_FLIP_THRESHOLD) {
//#if defined(USE_OLED)
//              OLED_flip = 1;
//#endif /* USE_OLED */
//            }
//          }
//          break;
//        } else {
//          delay(10);
//        }
//      }
//    }
//#endif /* EXCLUDE_IMU */
//  } else if (esp32_board == ESP32_LILYGO_T_TWR_V2_0) {
//
//    /* turn SA868 power off  to make sure that SQL is inactive */
//    digitalWrite(SOC_GPIO_PIN_TWR2_RADIO_PD, LOW);
//    pinMode(SOC_GPIO_PIN_TWR2_RADIO_PD, OUTPUT);
//
//    delay(200);
//
//    calibrate_voltage(ADC1_GPIO2_CHANNEL);
//    uint16_t gpio2_voltage = read_voltage(); // avg. of 32 samples
//
//    if (gpio2_voltage > 1900) {
//      esp32_board = ESP32_LILYGO_T_TWR_V2_1;
//      hw_info.revision = 21;
//
//      axp_2xxx.setALDO3Voltage(3300); // V2.1 - SA868, NeoPixel
//      axp_2xxx.enableALDO3();
//
//#if defined(USE_SA8X8)
//      if (gpio2_voltage > 2400) {
//        controller.setBand(Band::VHF);
//      } else {
//        controller.setBand(Band::UHF);
//      }
//#endif /* USE_SA8X8 */
//    } else {
//      axp_2xxx.setDC3Voltage  (3400); // V2.0 - SA868, NeoPixel
//      axp_2xxx.enableDC3();
//
//      pinMode(SOC_GPIO_PIN_TWR2_RADIO_HL, OUTPUT_OPEN_DRAIN);
//      digitalWrite(SOC_GPIO_PIN_TWR2_RADIO_HL, LOW);
//    }
//
//    digitalWrite(SOC_GPIO_PIN_TWR2_RADIO_PTT, HIGH);
//    pinMode(SOC_GPIO_PIN_TWR2_RADIO_PTT,  INPUT_PULLUP);
//    pinMode(SOC_GPIO_PIN_TWR2_MIC_CH_SEL, INPUT_PULLUP);
//
//  } else if (esp32_board == ESP32_HELTEC_TRACKER) {
//
//    rtc_clk_32k_enable(true);
//
//    CALIBRATE_ONE(RTC_CAL_RTC_MUX);
//    uint32_t cal_32k = CALIBRATE_ONE(RTC_CAL_32K_XTAL);
//
//    if (cal_32k == 0) {
//        DEBUG_X32K("32K XTAL OSC has not started up");
//    } else {
//        rtc_clk_slow_freq_set(RTC_SLOW_FREQ_32K_XTAL);
//        DEBUG_X32K("Switching of RTC clock source onto 32768 Hz XTAL is successful.");
//        CALIBRATE_ONE(RTC_CAL_RTC_MUX);
//        CALIBRATE_ONE(RTC_CAL_32K_XTAL);
//    }
//    CALIBRATE_ONE(RTC_CAL_RTC_MUX);
//    CALIBRATE_ONE(RTC_CAL_32K_XTAL);
//    if (rtc_clk_slow_freq_get() != RTC_SLOW_FREQ_32K_XTAL) {
//        DEBUG_X32K("Warning: Failed to switch RTC clock source onto 32768 Hz XTAL !");
//    } else {
//        ESP32_has_32k_xtal = true;
//    }
//
//    hw_info.revision = ESP32_has_32k_xtal ? 5 : 3;
//
//    if (hw_info.revision > 3) {
//      pinMode(SOC_GPIO_PIN_HELTRK_VEXT_EN, INPUT_PULLUP);
//    } else {
//      digitalWrite(SOC_GPIO_PIN_HELTRK_GNSS_EN, LOW);
//      pinMode(SOC_GPIO_PIN_HELTRK_GNSS_EN, OUTPUT);
//
//      pinMode(SOC_GPIO_PIN_HELTRK_TFT_EN,  INPUT_PULLDOWN);
//      pinMode(SOC_GPIO_PIN_HELTRK_VEXT_EN, INPUT_PULLDOWN);
//    }
//
//    digitalWrite(SOC_GPIO_PIN_HELTRK_GNSS_RST, LOW);
//    pinMode(SOC_GPIO_PIN_HELTRK_GNSS_RST,  OUTPUT);
//    delay(100);
//    digitalWrite(SOC_GPIO_PIN_HELTRK_GNSS_RST, HIGH);
//
//    pinMode(SOC_GPIO_PIN_HELTRK_ADC_EN,    INPUT_PULLUP);
//
//    digitalWrite(SOC_GPIO_PIN_HELTRK_LED,  LOW);
//    pinMode(SOC_GPIO_PIN_HELTRK_LED,       OUTPUT);
//
//  } else {
//#if !defined(EXCLUDE_IMU)
//    Wire.begin(SOC_GPIO_PIN_S3_SDA, SOC_GPIO_PIN_S3_SCL);
//    Wire.beginTransmission(MPU9250_ADDRESS);
//    bool has_mpu = (Wire.endTransmission() == 0);
//
//    if (has_mpu && imu_mpu9250.setup(MPU9250_ADDRESS)) {
//      imu_mpu9250.verbose(false);
//      if (imu_mpu9250.isSleeping()) {
//        imu_mpu9250.sleep(false);
//      }
//      hw_info.imu = IMU_MPU9250;
//      IMU_Time_Marker = millis();
//    } else {
//      WIRE_FINI(Wire);
//    }
//
//    hw_info.mag = (hw_info.imu == IMU_MPU9250) ? MAG_AK8963 : hw_info.mag;
//#endif /* EXCLUDE_IMU */
//  }
//#endif /* CONFIG_IDF_TARGET_ESP32S3 */
//}
//
//static void ESP32_post_init()
//{
//#if defined(CONFIG_IDF_TARGET_ESP32S3)
//  if (hw_info.model == SOFTRF_MODEL_PRIME_MK3)
//  {
//    Serial.println();
//    Serial.println(F("Power-on Self Test"));
//    Serial.println();
//    Serial.flush();
//
//    Serial.println(F("Built-in components:"));
//
//    Serial.print(F("RADIO    : "));
//    Serial.println(hw_info.rf      == RF_IC_SX1262 ||
//                   hw_info.rf      == RF_IC_SX1276     ? F("PASS") : F("FAIL"));
//    Serial.flush();
//    Serial.print(F("GNSS     : "));
//    Serial.println(hw_info.gnss    != GNSS_MODULE_NONE ? F("PASS") : F("FAIL"));
//    Serial.flush();
//    Serial.print(F("32K XTAL : "));
//    Serial.println(ESP32_has_32k_xtal                  ? F("PASS") : F("FAIL"));
//    Serial.flush();
//    Serial.print(F("DISPLAY  : "));
//    Serial.println(hw_info.display == DISPLAY_OLED_1_3 ? F("PASS") : F("FAIL"));
//    Serial.flush();
//    Serial.print(F("RTC      : "));
//    Serial.println(hw_info.rtc     == RTC_PCF8563      ? F("PASS") : F("FAIL"));
//    Serial.flush();
//    Serial.print(F("BARO     : "));
//    Serial.println(hw_info.baro  == BARO_MODULE_BMP280 ? F("PASS") : F("N/A"));
//    Serial.flush();
//#if !defined(EXCLUDE_IMU)
//    Serial.print(F("IMU      : "));
//    Serial.println(hw_info.imu     != IMU_NONE         ? F("PASS") : F("FAIL"));
//    Serial.flush();
//#endif /* EXCLUDE_IMU */
//#if !defined(EXCLUDE_MAG)
//    Serial.print(F("MAG      : "));
//    Serial.println(hw_info.mag     != MAG_NONE         ? F("PASS") : F("FAIL"));
//    Serial.flush();
//#endif /* EXCLUDE_MAG */
//
//    Serial.println();
//    Serial.println(F("External components:"));
//    Serial.print(F("CARD     : "));
//    Serial.println(hw_info.storage == STORAGE_CARD ||
//                   hw_info.storage == STORAGE_FLASH_AND_CARD
//                                                       ? F("PASS") : F("N/A"));
//    Serial.flush();
//
//    Serial.println();
//    Serial.println(F("Power-on Self Test is complete."));
//    Serial.flush();
//  }
//
//  if (hw_info.model == SOFTRF_MODEL_PRIME_MK3  ||
//      esp32_board   == ESP32_LILYGO_T_TWR_V2_0 ||
//      esp32_board   == ESP32_LILYGO_T_TWR_V2_1)
//  {
//    Serial.println();
//
//    if (!uSD_is_attached) {
//      Serial.println(F("WARNING: unable to attach micro-SD card."));
//    } else {
//      // The number of 512 byte sectors in the card
//      // or zero if an error occurs.
//      size_t cardSize = uSD.card()->cardSize();
//
//      if (cardSize == 0) {
//        Serial.println(F("WARNING: invalid micro-SD card size."));
//      } else {
//        uint8_t cardType = uSD.card()->type();
//
//        Serial.print(F("SD Card Type: "));
//        if(cardType == SD_CARD_TYPE_SD1){
//            Serial.println(F("V1"));
//        } else if(cardType == SD_CARD_TYPE_SD2){
//            Serial.println(F("V2"));
//        } else if(cardType == SD_CARD_TYPE_SDHC){
//            Serial.println(F("SDHC"));
//        } else {
//            Serial.println(F("UNKNOWN"));
//        }
//
//        Serial.print("SD Card Size: ");
//        Serial.print(cardSize / (2 * 1024));
//        Serial.println(" MB");
//      }
//    }
//  }
//#endif /* CONFIG_IDF_TARGET_ESP32S3 */
//
//  Serial.println();
//  Serial.println(F("Data output device(s):"));
//
//  Serial.print(F("NMEA   - "));
//  switch (settings->nmea_out)
//  {
//    case NMEA_UART       :  Serial.println(F("UART"));      break;
//    case NMEA_USB        :  Serial.println(F("USB CDC"));   break;
//    case NMEA_UDP        :  Serial.println(F("UDP"));       break;
//    case NMEA_TCP        :  Serial.println(F("TCP"));       break;
//    case NMEA_BLUETOOTH  :  Serial.println(F("Bluetooth")); break;
//    case NMEA_OFF        :
//    default              :  Serial.println(F("NULL"));      break;
//  }
//
//  Serial.print(F("GDL90  - "));
//  switch (settings->gdl90)
//  {
//    case GDL90_UART      :  Serial.println(F("UART"));      break;
//    case GDL90_USB       :  Serial.println(F("USB CDC"));   break;
//    case GDL90_UDP       :  Serial.println(F("UDP"));       break;
//    case GDL90_BLUETOOTH :  Serial.println(F("Bluetooth")); break;
//    case GDL90_OFF       :
//    default              :  Serial.println(F("NULL"));      break;
//  }
//
//  Serial.print(F("D1090  - "));
//  switch (settings->d1090)
//  {
//    case D1090_UART      :  Serial.println(F("UART"));      break;
//    case D1090_USB       :  Serial.println(F("USB CDC"));   break;
//    case D1090_BLUETOOTH :  Serial.println(F("Bluetooth")); break;
//    case D1090_OFF       :
//    default              :  Serial.println(F("NULL"));      break;
//  }
//
//  Serial.println();
//  Serial.flush();
//
//  switch (hw_info.display)
//  {
//#if defined(USE_OLED)
//  case DISPLAY_OLED_TTGO:
//  case DISPLAY_OLED_HELTEC:
//  case DISPLAY_OLED_1_3:
//    OLED_info1();
//
//#if defined(CONFIG_IDF_TARGET_ESP32S3)
//    if (hw_info.model == SOFTRF_MODEL_PRIME_MK3)
//    {
//      char key[8];
//      char out[64];
//      uint8_t tokens[3] = { 0 };
//      cdbResult rt;
//      int c, i = 0, token_cnt = 0;
//
//      int acfts;
//      char *reg, *mam, *cn;
//      reg = mam = cn = NULL;
//
//      OLED_info2();
//
//      if (ADB_is_open) {
//        acfts = ucdb.recordsNumber();
//
//        snprintf(key, sizeof(key),"%06X", ThisAircraft.addr);
//
//        rt = ucdb.findKey(key, strlen(key));
//
//        switch (rt) {
//          case KEY_FOUND:
//            while ((c = ucdb.readValue()) != -1 && i < (sizeof(out) - 1)) {
//              if (c == '|') {
//                if (token_cnt < (sizeof(tokens) - 1)) {
//                  token_cnt++;
//                  tokens[token_cnt] = i+1;
//                }
//                c = 0;
//              }
//              out[i++] = (char) c;
//            }
//            out[i] = 0;
//
//            reg = out + tokens[1];
//            mam = out + tokens[0];
//            cn  = out + tokens[2];
//
//            break;
//
//          case KEY_NOT_FOUND:
//          default:
//            break;
//        }
//
//        reg = (reg != NULL) && strlen(reg) ? reg : (char *) "REG: N/A";
//        mam = (mam != NULL) && strlen(mam) ? mam : (char *) "M&M: N/A";
//        cn  = (cn  != NULL) && strlen(cn)  ? cn  : (char *) " CN: N/A";
//
//      } else {
//        acfts = -1;
//      }
//
//      OLED_info3(acfts, reg, mam, cn);
//    }
//#endif /* CONFIG_IDF_TARGET_ESP32S3 */
//
//    break;
//#endif /* USE_OLED */
//  case DISPLAY_NONE:
//  default:
//    break;
//  }
//}
//
//static void ESP32_loop()
//{
//  bool is_irq = false;
//  bool down = false;
//
//  switch (hw_info.pmu)
//  {
//  case PMU_AXP192:
//  case PMU_AXP202:
//
//    portENTER_CRITICAL_ISR(&PMU_mutex);
//    is_irq = PMU_Irq;
//    portEXIT_CRITICAL_ISR(&PMU_mutex);
//
//    if (is_irq) {
//
//      if (axp_xxx.readIRQ() == AXP_PASS) {
//
//        if (axp_xxx.isPEKLongtPressIRQ()) {
//          down = true;
//#if 0
//          Serial.println(F("Long press IRQ"));
//          Serial.flush();
//#endif
//        }
//        if (axp_xxx.isPEKShortPressIRQ()) {
//#if 0
//          Serial.println(F("Short press IRQ"));
//          Serial.flush();
//#endif
//#if defined(USE_OLED)
//          OLED_Next_Page();
//#endif
//        }
//
//        axp_xxx.clearIRQ();
//      }
//
//      portENTER_CRITICAL_ISR(&PMU_mutex);
//      PMU_Irq = false;
//      portEXIT_CRITICAL_ISR(&PMU_mutex);
//
//      if (down) {
//        shutdown(SOFTRF_SHUTDOWN_BUTTON);
//      }
//    }
//
//    if (isTimeToBattery()) {
//      if (Battery_voltage() > Battery_threshold()) {
//        axp_xxx.setChgLEDMode(AXP20X_LED_LOW_LEVEL);
//      } else {
//        axp_xxx.setChgLEDMode(AXP20X_LED_BLINK_1HZ);
//      }
//    }
//    break;
//
//  case PMU_AXP2101:
//    portENTER_CRITICAL_ISR(&PMU_mutex);
//    is_irq = PMU_Irq;
//    portEXIT_CRITICAL_ISR(&PMU_mutex);
//
//    if (is_irq) {
//
//      axp_2xxx.getIrqStatus();
//
//      if (axp_2xxx.isPekeyLongPressIrq()) {
//        down = true;
//      }
//      if (axp_2xxx.isPekeyShortPressIrq()) {
//#if defined(USE_OLED)
//        OLED_Next_Page();
//#endif
//      }
//
//      axp_2xxx.clearIrqStatus();
//
//      portENTER_CRITICAL_ISR(&PMU_mutex);
//      PMU_Irq = false;
//      portEXIT_CRITICAL_ISR(&PMU_mutex);
//
//      if (down) {
//        shutdown(SOFTRF_SHUTDOWN_BUTTON);
//      }
//    }
//
//    if (isTimeToBattery()) {
//      if (Battery_voltage() > Battery_threshold()) {
//        axp_2xxx.setChargingLedMode(XPOWERS_CHG_LED_ON);
//      } else {
//        axp_2xxx.setChargingLedMode(XPOWERS_CHG_LED_BLINK_1HZ);
//      }
//    }
//    break;
//
//  case PMU_NONE:
//  default:
//    break;
//  }
//
//#if defined(CONFIG_IDF_TARGET_ESP32S3)
//  if (!RTC_sync) {
//    if (rtc &&
//        gnss.date.isValid()                         &&
//        gnss.time.isValid()                         &&
//        gnss.date.year() >= fw_build_date_time.year &&
//        gnss.date.year() <  fw_build_date_time.year + 15 ) {
//      rtc->setDateTime(gnss.date.year(),   gnss.date.month(),
//                       gnss.date.day(),    gnss.time.hour(),
//                       gnss.time.minute(), gnss.time.second());
//      RTC_sync = true;
//    }
//  }
//
//  #if !defined(EXCLUDE_IMU)
//  if ((millis() - IMU_Time_Marker) > IMU_UPDATE_INTERVAL) {
//
//    switch (hw_info.imu)
//    {
//    case IMU_MPU9250:
//      if (imu_mpu9250.update()) {
//        float a_x = imu_mpu9250.getAccX();
//        float a_y = imu_mpu9250.getAccY();
//        float a_z = imu_mpu9250.getAccZ();
//    #if defined(USE_OLED)
//        IMU_g_x10 = (int) (sqrtf(a_x*a_x + a_y*a_y + a_z*a_z) * 10);
//    #endif /* USE_OLED */
//      }
//      break;
//    case IMU_QMI8658:
//      if (imu_qmi8658.getDataReady()) {
//        float a_x, a_y, a_z;
//        if (imu_qmi8658.getAccelerometer(a_x, a_y, a_z)) {
//    #if 0
//            Serial.print("{ACCEL: ");
//            Serial.print(a_x);
//            Serial.print(",");
//            Serial.print(a_y);
//            Serial.print(",");
//            Serial.print(a_z);
//            Serial.println("}");
//    #endif
//    #if defined(USE_OLED)
//          IMU_g_x10 = (int) (sqrtf(a_x*a_x + a_y*a_y + a_z*a_z) * 10);
//    #endif /* USE_OLED */
//        }
//      }
//      break;
//    case IMU_NONE:
//    default:
//      break;
//    }
//
//    IMU_Time_Marker = millis();
//  }
//  #endif /* !EXCLUDE_IMU */
//
//  #if !defined(EXCLUDE_MAG)
//  if ((millis() - MAG_Time_Marker) > MAG_UPDATE_INTERVAL) {
//
//    switch (hw_info.mag)
//    {
//    case MAG_QMC6310:
//      if (mag_qmc6310.isDataReady()) {
//        mag_qmc6310.readData();
//
//        float m_x = mag_qmc6310.getX();
//        float m_y = mag_qmc6310.getY();
//        float m_z = mag_qmc6310.getZ();
//        float angle = atan2(-m_z, m_x);
//        if (angle < 0) {
//          angle += 2 * PI;
//        }
//    #if defined(USE_OLED)
//        MAG_heading = (int) (angle * 180 / M_PI);
//    #endif /* USE_OLED */
//    #if 0
//        Serial.print("MAG");
//        Serial.print(" X:");
//        Serial.print(m_x);
//        Serial.print(" Y:");
//        Serial.print(m_y);
//        Serial.print(" Z:");
//        Serial.print(m_z);
//        Serial.print(" uT");
//        Serial.print(" H:");
//        Serial.println(MAG_heading);
//    #endif
//      }
//      break;
//    case MAG_NONE:
//    default:
//      break;
//    }
//
//    MAG_Time_Marker = millis();
//  }
//  #endif /* !EXCLUDE_MAG */
//
//  if (esp32_board == ESP32_HELTEC_TRACKER) {
//    digitalWrite(SOC_GPIO_PIN_HELTRK_LED,
//                 digitalRead(SOC_GPIO_PIN_HELTRK_GNSS_PPS));
//  }
//#endif /* CONFIG_IDF_TARGET_ESP32S3 */
//}
//
//static void ESP32_fini(int reason)
//{
//#if defined(CONFIG_IDF_TARGET_ESP32S3)
//  if (ESP32_has_spiflash) {
//#if CONFIG_TINYUSB_MSC_ENABLED
//  #if defined(USE_ADAFRUIT_MSC)
//    usb_msc.setUnitReady(false);
////  usb_msc.end(); /* N/A */
//  #else
//    usb_msc.mediaPresent(false);
//    usb_msc.end();
//  #endif /* USE_ADAFRUIT_MSC */
//#endif /* CONFIG_TINYUSB_MSC_ENABLED */
//  }
//
//  if (SPIFlash != NULL) SPIFlash->end();
//
//#if !defined(EXCLUDE_IMU)
//  switch (hw_info.imu)
//  {
//  case IMU_MPU9250:
//    imu_mpu9250.sleep(true);
//    break;
//  case IMU_QMI8658:
//    imu_qmi8658.deinit();
//    break;
//  case IMU_NONE:
//  default:
//    break;
//  }
//#endif /* EXCLUDE_IMU */
//
//#if !defined(EXCLUDE_MAG)
//  switch (hw_info.mag)
//  {
//  case MAG_QMC6310:
//    mag_qmc6310.deinit();
//    break;
//  case MAG_NONE:
//  default:
//    break;
//  }
//#endif /* EXCLUDE_MAG */
//
//  if (hw_info.storage == STORAGE_CARD ||
//      hw_info.storage == STORAGE_FLASH_AND_CARD) {
//    uSD.end();
//  }
//
//  uSD_SPI.end();
//#endif /* CONFIG_IDF_TARGET_ESP32S3 */
//
//  SPI.end();
//
//  esp_wifi_stop();
//
//#if defined(CONFIG_IDF_TARGET_ESP32)
//  esp_bt_controller_disable();
//#endif /* CONFIG_IDF_TARGET_ESP32 */
//
//  if (hw_info.model == SOFTRF_MODEL_SKYWATCH) {
//
//    axp_xxx.setChgLEDMode(AXP20X_LED_OFF);
//
//    axp_xxx.setPowerOutPut(AXP202_LDO2, AXP202_OFF); // BL
//    axp_xxx.setPowerOutPut(AXP202_LDO4, AXP202_OFF); // S76G (Sony GNSS)
//    axp_xxx.setPowerOutPut(AXP202_LDO3, AXP202_OFF); // S76G (MCU + LoRa)
//
//    delay(20);
//
//#if !defined(CONFIG_IDF_TARGET_ESP32C3)
//    esp_sleep_enable_ext1_wakeup(1ULL << SOC_GPIO_PIN_TWATCH_PMU_IRQ,
//                                 ESP_EXT1_WAKEUP_ALL_LOW);
//#endif /* CONFIG_IDF_TARGET_ESP32C3 */
//  } else if (hw_info.model == SOFTRF_MODEL_PRIME_MK2 ||
//             hw_info.model == SOFTRF_MODEL_PRIME_MK3) {
//
//    switch (hw_info.pmu)
//    {
//    case PMU_AXP192:
//      axp_xxx.setChgLEDMode(AXP20X_LED_OFF);
//
//#if PMK2_SLEEP_MODE == 2
//      { int ret;
//      // PEK or GPIO edge wake-up function enable setting in Sleep mode
//      do {
//          // In order to ensure that it is set correctly,
//          // the loop waits for it to return the correct return value
//          ret = axp_xxx.setSleep();
//          delay(500);
//      } while (ret != AXP_PASS) ; }
//
//      // Turn off all power channels, only use PEK or AXP GPIO to wake up
//
//      // After setting AXP202/AXP192 to sleep,
//      // it will start to record the status of the power channel that was turned off after setting,
//      // it will restore the previously set state after PEK button or GPIO wake up
//
//#endif /* PMK2_SLEEP_MODE */
//
//      axp_xxx.setPowerOutPut(AXP192_LDO2,  AXP202_OFF);
//      axp_xxx.setPowerOutPut(AXP192_LDO3,  AXP202_OFF);
//      axp_xxx.setPowerOutPut(AXP192_DCDC2, AXP202_OFF);
//
//      /* workaround against AXP I2C access blocking by 'noname' OLED */
//#if defined(USE_OLED)
//      if (u8x8 == NULL)
//#endif /* USE_OLED */
//      {
//        axp_xxx.setPowerOutPut(AXP192_DCDC1, AXP202_OFF);
//      }
//      axp_xxx.setPowerOutPut(AXP192_EXTEN, AXP202_OFF);
//
//      delay(20);
//
//      /*
//       * When driven by SoftRF the V08+ T-Beam takes:
//       * in 'full power' - 160 - 180 mA
//       * in 'stand by'   - 600 - 900 uA
//       * in 'power off'  -  50 -  90 uA
//       * of current from 3.7V battery
//       */
//#if   PMK2_SLEEP_MODE == 1
//      /* Deep sleep with wakeup by power button click */
//      esp_sleep_enable_ext1_wakeup(1ULL << SOC_GPIO_PIN_TBEAM_V08_PMU_IRQ,
//                                 ESP_EXT1_WAKEUP_ALL_LOW);
//#elif PMK2_SLEEP_MODE == 2
//      // Cut MCU power off, PMU remains in sleep until wakeup by PEK button press
//      axp_xxx.setPowerOutPut(AXP192_DCDC3, AXP202_OFF);
//#else
//      /*
//       * Complete power off
//       *
//       * to power back on either:
//       * - press and hold PWR button for 1-2 seconds then release, or
//       * - cycle micro-USB power
//       */
//      axp_xxx.shutdown();
//#endif /* PMK2_SLEEP_MODE */
//      break;
//
//    case PMU_AXP2101:
//      axp_2xxx.setChargingLedMode(XPOWERS_CHG_LED_OFF);
//
//      axp_2xxx.disableButtonBatteryCharge();
//
//      axp_2xxx.disableALDO2();
//      axp_2xxx.disableALDO3();
//
//      delay(20);
//
//      /*
//       * Complete power off
//       *
//       * to power back on either:
//       * - press and hold PWR button for 1-2 seconds then release, or
//       * - cycle micro-USB power
//       */
//      axp_2xxx.shutdown();
//      break;
//
//    case PMU_NONE:
//    default:
//      break;
//    }
//  } else if (esp32_board == ESP32_S2_T8_V1_1) {
//    pinMode(SOC_GPIO_PIN_T8_S2_PWR_EN, INPUT);
//
//#if !defined(CONFIG_IDF_TARGET_ESP32C3)
//    esp_sleep_enable_ext1_wakeup(1ULL << SOC_GPIO_PIN_T8_S2_BUTTON,
//                                 ESP_EXT1_WAKEUP_ALL_LOW);
//#endif /* CONFIG_IDF_TARGET_ESP32C3 */
//
//  } else if (esp32_board == ESP32_LILYGO_T_TWR_V2_0 ||
//             esp32_board == ESP32_LILYGO_T_TWR_V2_1) {
//
//#if defined(CONFIG_IDF_TARGET_ESP32S3)
//#if defined(USE_NEOPIXELBUS_LIBRARY)
//    TWR2_Pixel.SetPixelColor(0, LED_COLOR_BLACK);
//    TWR2_Pixel.Show();
//#endif /* USE_NEOPIXELBUS_LIBRARY */
//#endif /* CONFIG_IDF_TARGET_ESP32S3 */
//
//    switch (hw_info.pmu)
//    {
//    case PMU_AXP2101:
//      axp_2xxx.setChargingLedMode(XPOWERS_CHG_LED_OFF);
//
//      axp_2xxx.disableButtonBatteryCharge();
//
//      axp_2xxx.disableBLDO1();
//      axp_2xxx.disableALDO4();
//      axp_2xxx.disableALDO2();
//
//      axp_2xxx.disableALDO3();
//      axp_2xxx.disableDC3();
//
//      delay(20);
//
//      /*
//       * Complete power off
//       *
//       * to power back on either:
//       * - press and hold PWR button for 1-2 seconds then release, or
//       * - cycle micro-USB power
//       */
//      axp_2xxx.shutdown();
//      break;
//
//    case PMU_NONE:
//    default:
//      break;
//    }
//  } else if (esp32_board == ESP32_HELTEC_TRACKER) {
//    if (hw_info.revision < 5) {
//      pinMode(SOC_GPIO_PIN_HELTRK_GNSS_EN, INPUT);
//      pinMode(SOC_GPIO_PIN_HELTRK_TFT_EN,  INPUT);
//    }
//
//    pinMode(SOC_GPIO_PIN_HELTRK_GNSS_RST,  INPUT);
//    pinMode(SOC_GPIO_PIN_HELTRK_ADC_EN,    INPUT);
//    pinMode(SOC_GPIO_PIN_HELTRK_VEXT_EN,   INPUT);
//    pinMode(SOC_GPIO_PIN_HELTRK_LED,       INPUT);
//
//#if !defined(CONFIG_IDF_TARGET_ESP32C3)
//    esp_sleep_enable_ext1_wakeup(1ULL << SOC_GPIO_PIN_S3_BUTTON,
//                                 ESP_EXT1_WAKEUP_ALL_LOW);
//#endif /* CONFIG_IDF_TARGET_ESP32C3 */
//  }
//
//  esp_deep_sleep_start();
//}
//
//static void ESP32_reset()
//{
//  ESP.restart();
//}


//static struct rst_info reset_info = {.reason = REASON_DEFAULT_RST,};

//static void* ESP32_getResetInfoPtr()
//{
//    switch (rtc_get_reset_reason(0))
//    {
//    case POWERON_RESET: reset_info.reason = REASON_DEFAULT_RST; break;
//    case DEEPSLEEP_RESET: reset_info.reason = REASON_DEEP_SLEEP_AWAKE; break;
//    case TG0WDT_SYS_RESET: reset_info.reason = REASON_WDT_RST; break;
//    case TG1WDT_SYS_RESET: reset_info.reason = REASON_WDT_RST; break;
//    case RTCWDT_SYS_RESET: reset_info.reason = REASON_WDT_RST; break;
//    case INTRUSION_RESET: reset_info.reason = REASON_EXCEPTION_RST; break;
//    case RTCWDT_CPU_RESET: reset_info.reason = REASON_WDT_RST; break;
//    case RTCWDT_BROWN_OUT_RESET: reset_info.reason = REASON_EXT_SYS_RST; break;
//    case RTCWDT_RTC_RESET:
//        /* Slow start of GD25LQ32 causes one read fault at boot time with current ESP-IDF */
//        //if (ESP32_getFlashId() == MakeFlashId(GIGADEVICE_ID, GIGADEVICE_GD25LQ32))
//        //    reset_info.reason = REASON_DEFAULT_RST;
//        //else
//            reset_info.reason = REASON_WDT_RST;
//        break;
//#if defined(CONFIG_IDF_TARGET_ESP32)
//    case SW_RESET: reset_info.reason = REASON_SOFT_RESTART; break;
//    case OWDT_RESET: reset_info.reason = REASON_WDT_RST; break;
//    case SDIO_RESET: reset_info.reason = REASON_EXCEPTION_RST; break;
//    case TGWDT_CPU_RESET: reset_info.reason = REASON_WDT_RST; break;
//    case SW_CPU_RESET: reset_info.reason = REASON_SOFT_RESTART; break;
//    case EXT_CPU_RESET: reset_info.reason = REASON_EXT_SYS_RST; break;
//#endif /* CONFIG_IDF_TARGET_ESP32 */
//    default: reset_info.reason = REASON_DEFAULT_RST;
//    }
//
//    return (void*)&reset_info;
//}

//static String ESP32_getResetInfo()
//{
//    switch (rtc_get_reset_reason(0))
//    {
//    case POWERON_RESET: return F("Vbat power on reset");
//    case DEEPSLEEP_RESET: return F("Deep Sleep reset digital core");
//    case TG0WDT_SYS_RESET: return F("Timer Group0 Watch dog reset digital core");
//    case TG1WDT_SYS_RESET: return F("Timer Group1 Watch dog reset digital core");
//    case RTCWDT_SYS_RESET: return F("RTC Watch dog Reset digital core");
//    case INTRUSION_RESET: return F("Instrusion tested to reset CPU");
//    case RTCWDT_CPU_RESET: return F("RTC Watch dog Reset CPU");
//    case RTCWDT_BROWN_OUT_RESET: return F("Reset when the vdd voltage is not stable");
//    case RTCWDT_RTC_RESET: return F("RTC Watch dog reset digital core and rtc module");
//#if defined(CONFIG_IDF_TARGET_ESP32)
//    case SW_RESET: return F("Software reset digital core");
//    case OWDT_RESET: return F("Legacy watch dog reset digital core");
//    case SDIO_RESET: return F("Reset by SLC module, reset digital core");
//    case TGWDT_CPU_RESET: return F("Time Group reset CPU");
//    case SW_CPU_RESET: return F("Software reset CPU");
//    case EXT_CPU_RESET: return F("for APP CPU, reseted by PRO CPU");
//#endif /* CONFIG_IDF_TARGET_ESP32 */
//    default: return F("No reset information available");
//    }
//}

//static String ESP32_getResetReason()
//{
//
//    switch (rtc_get_reset_reason(0))
//    {
//    case POWERON_RESET: return F("POWERON_RESET");
//    case DEEPSLEEP_RESET: return F("DEEPSLEEP_RESET");
//    case TG0WDT_SYS_RESET: return F("TG0WDT_SYS_RESET");
//    case TG1WDT_SYS_RESET: return F("TG1WDT_SYS_RESET");
//    case RTCWDT_SYS_RESET: return F("RTCWDT_SYS_RESET");
//    case INTRUSION_RESET: return F("INTRUSION_RESET");
//    case RTCWDT_CPU_RESET: return F("RTCWDT_CPU_RESET");
//    case RTCWDT_BROWN_OUT_RESET: return F("RTCWDT_BROWN_OUT_RESET");
//    case RTCWDT_RTC_RESET: return F("RTCWDT_RTC_RESET");
//#if defined(CONFIG_IDF_TARGET_ESP32)
//    case SW_RESET: return F("SW_RESET");
//    case OWDT_RESET: return F("OWDT_RESET");
//    case SDIO_RESET: return F("SDIO_RESET");
//    case TGWDT_CPU_RESET: return F("TGWDT_CPU_RESET");
//    case SW_CPU_RESET: return F("SW_CPU_RESET");
//    case EXT_CPU_RESET: return F("EXT_CPU_RESET");
//#endif /* CONFIG_IDF_TARGET_ESP32 */
//    default: return F("NO_MEAN");
//    }
//}

//static uint32_t ESP32_getFreeHeap()
//{
//    return ESP.getFreeHeap();
//}

//static long ESP32_random(long howsmall, long howBig)
//{
//    return random(howsmall, howBig);
//}



const SoC_ops_t ESP32_ops = {
#if defined(CONFIG_IDF_TARGET_ESP32)
  SOC_ESP32,
  "ESP32",

#else
#error "This ESP32 family build variant is not supported!"
#endif /* CONFIG_IDF_TARGET_ESP32-S2-S3-C3 */
 // ESP32_setup,
  //ESP32_post_init,
  //ESP32_loop,
  //ESP32_fini,
  //ESP32_reset,
  //ESP32_getChipId,
  //ESP32_getResetInfoPtr,
  //ESP32_getResetInfo,
  //ESP32_getResetReason,
  //ESP32_getFreeHeap,
  //ESP32_random,
  //ESP32_Sound_test,
  //ESP32_Sound_tone,
  //ESP32_maxSketchSpace,
  //ESP32_WiFi_set_param,
  //ESP32_WiFi_transmit_UDP,
  //ESP32_WiFiUDP_stopAll,
  //ESP32_WiFi_hostname,
  //ESP32_WiFi_clients_count,
  //ESP32_EEPROM_begin,
  //ESP32_EEPROM_extension,
  //ESP32_SPI_begin,
  //ESP32_swSer_begin,
  //ESP32_swSer_enableRx,

  //ESP32_Display_setup,
  //ESP32_Display_loop,
  //ESP32_Display_fini,
  //ESP32_Battery_setup,
  //ESP32_Battery_param,
  //ESP32_GNSS_PPS_Interrupt_handler,
  //ESP32_get_PPS_TimeMarker,
  //ESP32_Baro_setup,
  //ESP32_UATSerial_begin,
  //ESP32_UATModule_restart,
  //ESP32_WDT_setup,
  //ESP32_WDT_fini,
  //ESP32_Button_setup,
  //ESP32_Button_loop,
  //ESP32_Button_fini,

};



#endif /* ESP32 */
