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

#include "sdkconfig.h"

#include <SPI.h>
#include <esp_err.h>
#include <esp_wifi.h>
#if !defined(CONFIG_IDF_TARGET_ESP32S2)
#include <esp_bt.h>
#include <BLEDevice.h>
#endif /* CONFIG_IDF_TARGET_ESP32S2 */
#include <soc/rtc_cntl_reg.h>
#include <soc/efuse_reg.h>
#include <Wire.h>
#include <rom/rtc.h>
#include <rom/spi_flash.h>
#include <soc/adc_channel.h>
#include <flashchips.h>
#include <axp20x.h>
#define  XPOWERS_CHIP_AXP2102
#include <XPowersLib.h>
#include <pcf8563.h>

#include "ESP32RF.h"
#include "SoC.h"
#include "TimeRF.h"
#include "EEPROMRF.h"
#include "RF.h"
#include "WiFiRF.h"
#include "Bluetooth.h"
#include "LED.h"
#include "Baro.h"
#include "BatteryRF.h"
#include "OLED.h"
#include "NMEA.h"
#include "GDL90.h"
#include "D1090.h"

//#if defined(USE_TFT)
//#include <TFT_eSPI.h>
//#endif /* USE_TFT */

#include <battery.h>

// SX12xx pin mapping
lmic_pinmap lmic_pins = {
    .nss  = SOC_GPIO_PIN_SS,
    .txe  = LMIC_UNUSED_PIN,
    .rxe  = LMIC_UNUSED_PIN,
    .rst  = SOC_GPIO_PIN_RST,
    .dio  = {LMIC_UNUSED_PIN, LMIC_UNUSED_PIN, LMIC_UNUSED_PIN},
    //.busy = SOC_GPIO_PIN_TXE,
    .tcxo = LMIC_UNUSED_PIN,
};

WebServer server ( 80 );

#if defined(USE_NEOPIXELBUS_LIBRARY)
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PIX_NUM, SOC_GPIO_PIN_LED);
#else /* USE_ADAFRUIT_NEO_LIBRARY */
// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIX_NUM, SOC_GPIO_PIN_LED,
                              NEO_GRB + NEO_KHZ800);
#endif /* USE_NEOPIXELBUS_LIBRARY */

#if defined(USE_OLED)
U8X8_OLED_I2C_BUS_TYPE u8x8_ttgo  (TTGO_V2_OLED_PIN_RST);
U8X8_SH1106_128X64_NONAME_HW_I2C u8x8_1_3(U8X8_PIN_NONE);
#endif /* USE_OLED */

AXP20X_Class axp_xxx;
XPowersPMU   axp_2xxx;

static int esp32_board = ESP32_DEVKIT; /* default */
static size_t ESP32_Min_AppPart_Size = 0;

static portMUX_TYPE GNSS_PPS_mutex = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE PMU_mutex      = portMUX_INITIALIZER_UNLOCKED;
volatile bool PMU_Irq = false;

static bool GPIO_21_22_are_busy = false;

static union {
  uint8_t efuse_mac[6];
  uint64_t chipmacid;
};

static bool TFT_display_frontpage = false;
static uint32_t prev_tx_packets_counter = 0;
static uint32_t prev_rx_packets_counter = 0;
extern uint32_t tx_packets_counter, rx_packets_counter;
extern bool loopTaskWDTEnabled;

const char *ESP32SX_Device_Manufacturer = SOFTRF_IDENT;
const char *ESP32SX_Model_Stand  = "Standalone Edition"; /* 303a:8132 */
const char *ESP32S3_Model_Prime3 = "Prime Edition Mk.3"; /* 303a:8133 */
const char *ESP32S3_Model_Ham    = "Ham Edition";        /* 303a:818F */
const char *ESP32S3_Model_Midi   = "Midi Edition";       /* 303a:81A0 */
const uint16_t ESP32SX_Device_Version = SOFTRF_USB_FW_VERSION;

#include <mode-s.h>

mode_s_t state;

static void IRAM_ATTR ESP32_PMU_Interrupt_handler() {
  portENTER_CRITICAL_ISR(&PMU_mutex);
  PMU_Irq = true;
  portEXIT_CRITICAL_ISR(&PMU_mutex);
}

static uint32_t ESP32_getFlashId()
{
  return g_rom_flashchip.device_id;
}

#if defined(CORE_DEBUG_LEVEL) && CORE_DEBUG_LEVEL>0 && !defined(TAG)
#define TAG "MAC"
#endif

static void ESP32_setup()
{
#if !defined(SOFTRF_ADDRESS)

  esp_err_t ret = ESP_OK;
  uint8_t null_mac[6] = {0};

  ret = esp_efuse_mac_get_custom(efuse_mac);
  if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Get base MAC address from BLK3 of EFUSE error (%s)", esp_err_to_name(ret));
    /* If get custom base MAC address error, the application developer can decide what to do:
     * abort or use the default base MAC address which is stored in BLK0 of EFUSE by doing
     * nothing.
     */

    ESP_LOGI(TAG, "Use base MAC address which is stored in BLK0 of EFUSE");
    chipmacid = ESP.getEfuseMac();
  } else {
    if (memcmp(efuse_mac, null_mac, 6) == 0) {
      ESP_LOGI(TAG, "Use base MAC address which is stored in BLK0 of EFUSE");
      chipmacid = ESP.getEfuseMac();
    }
  }
#endif /* SOFTRF_ADDRESS */

  size_t flash_size = spi_flash_get_chip_size();
  size_t min_app_size = flash_size;

  esp_partition_iterator_t it;
  const esp_partition_t *part;

  it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
  if (it) {
    do {
      part = esp_partition_get(it);
      if (part->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
        continue;
      }
      if (part->size < min_app_size) {
        min_app_size = part->size;
      }
    } while (it = esp_partition_next(it));

    if (it) esp_partition_iterator_release(it);
  }

  if (min_app_size && (min_app_size != flash_size)) {
    ESP32_Min_AppPart_Size = min_app_size;
  }

  if (psramFound()) {

    uint32_t flash_id = ESP32_getFlashId();

    /*
     *    Board         |   Module      |  Flash memory IC
     *  ----------------+---------------+--------------------
     *  DoIt ESP32      | WROOM         | GIGADEVICE_GD25Q32
     *  TTGO T3  V2.0   | PICO-D4 IC    | GIGADEVICE_GD25Q32
     *  TTGO T3  V2.1.6 | PICO-D4 IC    | GIGADEVICE_GD25Q32
     *  TTGO T22 V06    |               | WINBOND_NEX_W25Q32_V
     *  TTGO T22 V08    |               | WINBOND_NEX_W25Q32_V
     *  TTGO T22 V11    |               | BOYA_BY25Q32AL
     *  TTGO T8  V1.8   | WROVER        | GIGADEVICE_GD25LQ32
     *  TTGO T8 S2 V1.1 |               | WINBOND_NEX_W25Q32_V
     *  TTGO T5S V1.9   |               | WINBOND_NEX_W25Q32_V
     *  TTGO T5S V2.8   |               | BOYA_BY25Q32AL
     *  TTGO T5  4.7    | WROVER-E      | XMC_XM25QH128C
     *  TTGO T-Watch    |               | WINBOND_NEX_W25Q128_V
     *  Ai-T NodeMCU-S3 | ESP-S3-12K    | GIGADEVICE_GD25Q64C
     *  TTGO T-Dongle   |               | BOYA_BY25Q32AL
     *  TTGO S3 Core    |               | GIGADEVICE_GD25Q64C
     *  TTGO T-01C3     |               | BOYA_BY25Q32AL
     *                  | ESP-C3-12F    | XMC_XM25QH32B
     *  LilyGO T-TWR    | WROOM-1-N16R8 | GIGADEVICE_GD25Q128
     *  Heltec Tracker  |               | GIGADEVICE_GD25Q64
     */

    switch(flash_id)
    {
    case MakeFlashId(GIGADEVICE_ID, GIGADEVICE_GD25LQ32):
      /* ESP32-WROVER module with ESP32-NODEMCU-ADAPTER */
      hw_info.model = SOFTRF_MODEL_STANDALONE;
      break;
    case MakeFlashId(WINBOND_NEX_ID, WINBOND_NEX_W25Q128_V):
      hw_info.model = SOFTRF_MODEL_SKYWATCH;
      break;
#if defined(CONFIG_IDF_TARGET_ESP32)
    case MakeFlashId(WINBOND_NEX_ID, WINBOND_NEX_W25Q32_V):
    case MakeFlashId(BOYA_ID, BOYA_BY25Q32AL):
    default:
      hw_info.model = SOFTRF_MODEL_PRIME_MK2;
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    default:
      esp32_board   = ESP32_S2_T8_V1_1;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    case MakeFlashId(GIGADEVICE_ID, GIGADEVICE_GD25Q128):
      hw_info.model = SOFTRF_MODEL_HAM;
      break;
    case MakeFlashId(GIGADEVICE_ID, GIGADEVICE_GD25Q64):
    default:
      hw_info.model = SOFTRF_MODEL_PRIME_MK3;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    case MakeFlashId(ST_ID, XMC_XM25QH32B):
    default:
      esp32_board   = ESP32_C3_DEVKIT;
#else
#error "This ESP32 family build variant is not supported!"
#endif
      break;
    }
  } else {
#if defined(CONFIG_IDF_TARGET_ESP32)
    uint32_t chip_ver = REG_GET_FIELD(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_VER_PKG);
    uint32_t pkg_ver  = chip_ver & 0x7;
    if (pkg_ver == EFUSE_RD_CHIP_VER_PKG_ESP32PICOD4)
    {
      esp32_board    = ESP32_TTGO_V2_OLED;
      lmic_pins.rst  = SOC_GPIO_PIN_TBEAM_RF_RST_V05;
      lmic_pins.busy = SOC_GPIO_PIN_TBEAM_RF_BUSY_V08;
    }
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    esp32_board      = ESP32_S2_T8_V1_1;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    if (ESP32_getFlashId() == MakeFlashId(GIGADEVICE_ID, GIGADEVICE_GD25Q128)) {
      hw_info.model  = SOFTRF_MODEL_HAM;  /* allow psramFound() to fail */
    } else if (ESP32_getFlashId() == MakeFlashId(GIGADEVICE_ID, GIGADEVICE_GD25Q64)) {
      esp32_board    = ESP32_HELTEC_TRACKER;
      hw_info.model  = SOFTRF_MODEL_MIDI;
    } else {
      esp32_board    = ESP32_S3_DEVKIT;
    }
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    esp32_board      = ESP32_C3_DEVKIT;
#endif /* CONFIG_IDF_TARGET_ESP32 */
  }

 
  if (hw_info.model == SOFTRF_MODEL_SKYWATCH) {
    esp32_board = ESP32_TTGO_T_WATCH;
    hw_info.rtc = RTC_PCF8563;
    hw_info.imu = ACC_BMA423;

    Wire1.begin(SOC_GPIO_PIN_TWATCH_SEN_SDA , SOC_GPIO_PIN_TWATCH_SEN_SCL);
    Wire1.beginTransmission(AXP202_SLAVE_ADDRESS);
    bool has_axp202 = (Wire1.endTransmission() == 0);
    if (has_axp202) {

      hw_info.pmu = PMU_AXP202;

      axp_xxx.begin(Wire1, AXP202_SLAVE_ADDRESS);

      axp_xxx.enableIRQ(AXP202_ALL_IRQ, AXP202_OFF);
      axp_xxx.adc1Enable(0xFF, AXP202_OFF);

      axp_xxx.setChgLEDMode(AXP20X_LED_LOW_LEVEL);

      axp_xxx.setPowerOutPut(AXP202_LDO2, AXP202_ON); // BL
      axp_xxx.setPowerOutPut(AXP202_LDO3, AXP202_ON); // S76G (MCU + LoRa)
      axp_xxx.setLDO4Voltage(AXP202_LDO4_1800MV);
      axp_xxx.setPowerOutPut(AXP202_LDO4, AXP202_ON); // S76G (Sony GNSS)

      pinMode(SOC_GPIO_PIN_TWATCH_PMU_IRQ, INPUT_PULLUP);

      attachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_TWATCH_PMU_IRQ),
                      ESP32_PMU_Interrupt_handler, FALLING);

      axp_xxx.adc1Enable(AXP202_BATT_VOL_ADC1, AXP202_ON);
      axp_xxx.enableIRQ(AXP202_PEK_LONGPRESS_IRQ | AXP202_PEK_SHORTPRESS_IRQ, true);
      axp_xxx.clearIRQ();
    } else {
      WIRE_FINI(Wire1);
    }
  } else if (hw_info.model == SOFTRF_MODEL_PRIME_MK2) {
    esp32_board = ESP32_TTGO_T_BEAM;

    Wire1.begin(TTGO_V2_OLED_PIN_SDA , TTGO_V2_OLED_PIN_SCL);
    Wire1.beginTransmission(AXP192_SLAVE_ADDRESS);
    bool has_axp = (Wire1.endTransmission() == 0);

    bool has_axp192 = has_axp &&
                      (axp_xxx.begin(Wire1, AXP192_SLAVE_ADDRESS) == AXP_PASS);

    if (has_axp192) {

      hw_info.revision = 8;
      hw_info.pmu = PMU_AXP192;

      axp_xxx.setChgLEDMode(AXP20X_LED_LOW_LEVEL);

      axp_xxx.setPowerOutPut(AXP192_LDO2,  AXP202_ON);
      axp_xxx.setPowerOutPut(AXP192_LDO3,  AXP202_ON);
      axp_xxx.setPowerOutPut(AXP192_DCDC1, AXP202_ON);
      axp_xxx.setPowerOutPut(AXP192_DCDC2, AXP202_ON); // NC
      axp_xxx.setPowerOutPut(AXP192_EXTEN, AXP202_ON);

      axp_xxx.setDCDC1Voltage(3300); //       AXP192 power-on value: 3300
      axp_xxx.setLDO2Voltage (3300); // LoRa, AXP192 power-on value: 3300
      axp_xxx.setLDO3Voltage (3000); // GPS,  AXP192 power-on value: 2800

      pinMode(SOC_GPIO_PIN_TBEAM_V08_PMU_IRQ, INPUT /* INPUT_PULLUP */);

      attachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_TBEAM_V08_PMU_IRQ),
                      ESP32_PMU_Interrupt_handler, FALLING);

      axp_xxx.enableIRQ(AXP202_PEK_LONGPRESS_IRQ | AXP202_PEK_SHORTPRESS_IRQ, true);
      axp_xxx.clearIRQ();
    } else {
      bool has_axp2101 = has_axp && axp_2xxx.begin(Wire1,
                                                   AXP2101_SLAVE_ADDRESS,
                                                   TTGO_V2_OLED_PIN_SDA,
                                                   TTGO_V2_OLED_PIN_SCL);
      if (has_axp2101) {

        // Set the minimum common working voltage of the PMU VBUS input,
        // below this value will turn off the PMU
        axp_2xxx.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);

        // Set the maximum current of the PMU VBUS input,
        // higher than this value will turn off the PMU
        axp_2xxx.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);

        // DCDC1 1500~3400mV, IMAX=2A
        axp_2xxx.setDC1Voltage(3300); // ESP32,  AXP2101 power-on value: 3300

        // ALDO 500~3500V, 100mV/step, IMAX=300mA
        axp_2xxx.setButtonBatteryChargeVoltage(3100); // GNSS battery

        axp_2xxx.setALDO2Voltage(3300); // LoRa, AXP2101 power-on value: 2800
        axp_2xxx.setALDO3Voltage(3300); // GPS,  AXP2101 power-on value: 3300

        // axp_2xxx.enableDC1();
        axp_2xxx.enableButtonBatteryCharge();

        axp_2xxx.enableALDO2();
        axp_2xxx.enableALDO3();

        axp_2xxx.setChargingLedMode(XPOWERS_CHG_LED_ON);

        pinMode(SOC_GPIO_PIN_TBEAM_V08_PMU_IRQ, INPUT /* INPUT_PULLUP */);

        attachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_TBEAM_V08_PMU_IRQ),
                        ESP32_PMU_Interrupt_handler, FALLING);

        axp_2xxx.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
        axp_2xxx.clearIrqStatus();

        axp_2xxx.enableIRQ(XPOWERS_AXP2101_PKEY_LONG_IRQ |
                           XPOWERS_AXP2101_PKEY_SHORT_IRQ);

        hw_info.revision = 12;
        hw_info.pmu = PMU_AXP2101;
      } else {
        WIRE_FINI(Wire1);
        hw_info.revision = 2;
      }
    }
    lmic_pins.rst  = SOC_GPIO_PIN_TBEAM_RF_RST_V05;
    lmic_pins.busy = SOC_GPIO_PIN_TBEAM_RF_BUSY_V08;

  }

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

#else
  Serial.begin(SERIAL_OUT_BR, SERIAL_OUT_BITS);
#endif /* ARDUINO_USB_CDC_ON_BOOT && (CONFIG_IDF_TARGET_ESP32S2 || S3) */


}

static void ESP32_post_init()
{

  Serial.println();
  Serial.println(F("Data output device(s):"));

  Serial.print(F("NMEA   - "));
  switch (settings->nmea_out)
  {
    case NMEA_UART       :  Serial.println(F("UART"));      break;
    case NMEA_USB        :  Serial.println(F("USB CDC"));   break;
    case NMEA_UDP        :  Serial.println(F("UDP"));       break;
    case NMEA_TCP        :  Serial.println(F("TCP"));       break;
    case NMEA_BLUETOOTH  :  Serial.println(F("Bluetooth")); break;
    case NMEA_OFF        :
    default              :  Serial.println(F("NULL"));      break;
  }

  Serial.print(F("GDL90  - "));
  switch (settings->gdl90)
  {
    case GDL90_UART      :  Serial.println(F("UART"));      break;
    case GDL90_USB       :  Serial.println(F("USB CDC"));   break;
    case GDL90_UDP       :  Serial.println(F("UDP"));       break;
    case GDL90_BLUETOOTH :  Serial.println(F("Bluetooth")); break;
    case GDL90_OFF       :
    default              :  Serial.println(F("NULL"));      break;
  }

  Serial.print(F("D1090  - "));
  switch (settings->d1090)
  {
    case D1090_UART      :  Serial.println(F("UART"));      break;
    case D1090_USB       :  Serial.println(F("USB CDC"));   break;
    case D1090_BLUETOOTH :  Serial.println(F("Bluetooth")); break;
    case D1090_OFF       :
    default              :  Serial.println(F("NULL"));      break;
  }

  Serial.println();
  Serial.flush();

  switch (hw_info.display)
  {
#if defined(USE_OLED)
  case DISPLAY_OLED_TTGO:
  case DISPLAY_OLED_HELTEC:
  case DISPLAY_OLED_1_3:
    OLED_info1();


    break;
#endif /* USE_OLED */
  case DISPLAY_NONE:
  default:
    break;
  }
}

static void ESP32_loop()
{
  bool is_irq = false;
  bool down = false;

  switch (hw_info.pmu)
  {
  case PMU_AXP192:
  case PMU_AXP202:

    portENTER_CRITICAL_ISR(&PMU_mutex);
    is_irq = PMU_Irq;
    portEXIT_CRITICAL_ISR(&PMU_mutex);

    if (is_irq) {

      if (axp_xxx.readIRQ() == AXP_PASS) {

        if (axp_xxx.isPEKLongtPressIRQ()) {
          down = true;
#if 0
          Serial.println(F("Long press IRQ"));
          Serial.flush();
#endif
        }
        if (axp_xxx.isPEKShortPressIRQ()) {
#if 0
          Serial.println(F("Short press IRQ"));
          Serial.flush();
#endif
#if defined(USE_OLED)
          OLED_Next_Page();
#endif
        }

        axp_xxx.clearIRQ();
      }

      portENTER_CRITICAL_ISR(&PMU_mutex);
      PMU_Irq = false;
      portEXIT_CRITICAL_ISR(&PMU_mutex);

      if (down) {
        shutdown(SOFTRF_SHUTDOWN_BUTTON);
      }
    }

    if (isTimeToBattery()) {
      if (Battery_voltage() > Battery_threshold()) {
        axp_xxx.setChgLEDMode(AXP20X_LED_LOW_LEVEL);
      } else {
        axp_xxx.setChgLEDMode(AXP20X_LED_BLINK_1HZ);
      }
    }
    break;

  case PMU_AXP2101:
    portENTER_CRITICAL_ISR(&PMU_mutex);
    is_irq = PMU_Irq;
    portEXIT_CRITICAL_ISR(&PMU_mutex);

    if (is_irq) {

      axp_2xxx.getIrqStatus();

      if (axp_2xxx.isPekeyLongPressIrq()) { 
        down = true;
      }
      if (axp_2xxx.isPekeyShortPressIrq()) {
#if defined(USE_OLED)
        OLED_Next_Page();
#endif
      }

      axp_2xxx.clearIrqStatus();

      portENTER_CRITICAL_ISR(&PMU_mutex);
      PMU_Irq = false;
      portEXIT_CRITICAL_ISR(&PMU_mutex);

      if (down) {
        shutdown(SOFTRF_SHUTDOWN_BUTTON);
      }
    }

    if (isTimeToBattery()) {
      if (Battery_voltage() > Battery_threshold()) {
        axp_2xxx.setChargingLedMode(XPOWERS_CHG_LED_ON);
      } else {
        axp_2xxx.setChargingLedMode(XPOWERS_CHG_LED_BLINK_1HZ);
      }
    }
    break;

  case PMU_NONE:
  default:
    break;
  }


}

static void ESP32_fini(int reason)
{

  SPI.end();

  esp_wifi_stop();

#if defined(CONFIG_IDF_TARGET_ESP32)
  esp_bt_controller_disable();
#endif /* CONFIG_IDF_TARGET_ESP32 */

  if (hw_info.model == SOFTRF_MODEL_SKYWATCH) {

    axp_xxx.setChgLEDMode(AXP20X_LED_OFF);

    axp_xxx.setPowerOutPut(AXP202_LDO2, AXP202_OFF); // BL
    axp_xxx.setPowerOutPut(AXP202_LDO4, AXP202_OFF); // S76G (Sony GNSS)
    axp_xxx.setPowerOutPut(AXP202_LDO3, AXP202_OFF); // S76G (MCU + LoRa)

    delay(20);

#if !defined(CONFIG_IDF_TARGET_ESP32C3)
    esp_sleep_enable_ext1_wakeup(1ULL << SOC_GPIO_PIN_TWATCH_PMU_IRQ,
                                 ESP_EXT1_WAKEUP_ALL_LOW);
#endif /* CONFIG_IDF_TARGET_ESP32C3 */
  } else if (hw_info.model == SOFTRF_MODEL_PRIME_MK2 ||
             hw_info.model == SOFTRF_MODEL_PRIME_MK3) {

    switch (hw_info.pmu)
    {
    case PMU_AXP192:
      axp_xxx.setChgLEDMode(AXP20X_LED_OFF);

#if PMK2_SLEEP_MODE == 2
      { int ret;
      // PEK or GPIO edge wake-up function enable setting in Sleep mode
      do {
          // In order to ensure that it is set correctly,
          // the loop waits for it to return the correct return value
          ret = axp_xxx.setSleep();
          delay(500);
      } while (ret != AXP_PASS) ; }

      // Turn off all power channels, only use PEK or AXP GPIO to wake up

      // After setting AXP202/AXP192 to sleep,
      // it will start to record the status of the power channel that was turned off after setting,
      // it will restore the previously set state after PEK button or GPIO wake up

#endif /* PMK2_SLEEP_MODE */

      axp_xxx.setPowerOutPut(AXP192_LDO2,  AXP202_OFF);
      axp_xxx.setPowerOutPut(AXP192_LDO3,  AXP202_OFF);
      axp_xxx.setPowerOutPut(AXP192_DCDC2, AXP202_OFF);

      /* workaround against AXP I2C access blocking by 'noname' OLED */
#if defined(USE_OLED)
      if (u8x8 == NULL)
#endif /* USE_OLED */
      {
        axp_xxx.setPowerOutPut(AXP192_DCDC1, AXP202_OFF);
      }
      axp_xxx.setPowerOutPut(AXP192_EXTEN, AXP202_OFF);

      delay(20);

      /*
       * When driven by SoftRF the V08+ T-Beam takes:
       * in 'full power' - 160 - 180 mA
       * in 'stand by'   - 600 - 900 uA
       * in 'power off'  -  50 -  90 uA
       * of current from 3.7V battery
       */
#if   PMK2_SLEEP_MODE == 1
      /* Deep sleep with wakeup by power button click */
      esp_sleep_enable_ext1_wakeup(1ULL << SOC_GPIO_PIN_TBEAM_V08_PMU_IRQ,
                                 ESP_EXT1_WAKEUP_ALL_LOW);
#elif PMK2_SLEEP_MODE == 2
      // Cut MCU power off, PMU remains in sleep until wakeup by PEK button press
      axp_xxx.setPowerOutPut(AXP192_DCDC3, AXP202_OFF);
#else
      /*
       * Complete power off
       *
       * to power back on either:
       * - press and hold PWR button for 1-2 seconds then release, or
       * - cycle micro-USB power
       */
      axp_xxx.shutdown();
#endif /* PMK2_SLEEP_MODE */
      break;

    case PMU_AXP2101:
      axp_2xxx.setChargingLedMode(XPOWERS_CHG_LED_OFF);

      axp_2xxx.disableButtonBatteryCharge();

      axp_2xxx.disableALDO2();
      axp_2xxx.disableALDO3();

      delay(20);

      /*
       * Complete power off
       *
       * to power back on either:
       * - press and hold PWR button for 1-2 seconds then release, or
       * - cycle micro-USB power
       */
      axp_2xxx.shutdown();
      break;

    case PMU_NONE:
    default:
      break;
    }
  } else if (esp32_board == ESP32_S2_T8_V1_1) {
    pinMode(SOC_GPIO_PIN_T8_S2_PWR_EN, INPUT);

#if !defined(CONFIG_IDF_TARGET_ESP32C3)
    esp_sleep_enable_ext1_wakeup(1ULL << SOC_GPIO_PIN_T8_S2_BUTTON,
                                 ESP_EXT1_WAKEUP_ALL_LOW);
#endif /* CONFIG_IDF_TARGET_ESP32C3 */

  } else if (esp32_board == ESP32_LILYGO_T_TWR_V2_0 ||
             esp32_board == ESP32_LILYGO_T_TWR_V2_1) {

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#if defined(USE_NEOPIXELBUS_LIBRARY)
    TWR2_Pixel.SetPixelColor(0, LED_COLOR_BLACK);
    TWR2_Pixel.Show();
#endif /* USE_NEOPIXELBUS_LIBRARY */
#endif /* CONFIG_IDF_TARGET_ESP32S3 */

    switch (hw_info.pmu)
    {
    case PMU_AXP2101:
      axp_2xxx.setChargingLedMode(XPOWERS_CHG_LED_OFF);

      axp_2xxx.disableButtonBatteryCharge();

      axp_2xxx.disableBLDO1();
      axp_2xxx.disableALDO4();
      axp_2xxx.disableALDO2();

      axp_2xxx.disableALDO3();
      axp_2xxx.disableDC3();

      delay(20);

      /*
       * Complete power off
       *
       * to power back on either:
       * - press and hold PWR button for 1-2 seconds then release, or
       * - cycle micro-USB power
       */
      axp_2xxx.shutdown();
      break;

    case PMU_NONE:
    default:
      break;
    }
  } else if (esp32_board == ESP32_HELTEC_TRACKER) {
    if (hw_info.revision < 5) {
      pinMode(SOC_GPIO_PIN_HELTRK_GNSS_EN, INPUT);
      pinMode(SOC_GPIO_PIN_HELTRK_TFT_EN,  INPUT);
    }

    pinMode(SOC_GPIO_PIN_HELTRK_GNSS_RST,  INPUT);
    pinMode(SOC_GPIO_PIN_HELTRK_ADC_EN,    INPUT);
    pinMode(SOC_GPIO_PIN_HELTRK_VEXT_EN,   INPUT);
    pinMode(SOC_GPIO_PIN_HELTRK_LED,       INPUT);

#if !defined(CONFIG_IDF_TARGET_ESP32C3)
    esp_sleep_enable_ext1_wakeup(1ULL << SOC_GPIO_PIN_S3_BUTTON,
                                 ESP_EXT1_WAKEUP_ALL_LOW);
#endif /* CONFIG_IDF_TARGET_ESP32C3 */
  }

  esp_deep_sleep_start();
}

static void ESP32_reset()
{
  ESP.restart();
}

static uint32_t ESP32_getChipId()
{
#if !defined(SOFTRF_ADDRESS)
  uint32_t id = (uint32_t) efuse_mac[5]        | ((uint32_t) efuse_mac[4] << 8) | \
               ((uint32_t) efuse_mac[3] << 16) | ((uint32_t) efuse_mac[2] << 24);

  return DevID_Mapper(id);
#else
  return (SOFTRF_ADDRESS & 0xFFFFFFFFU );
#endif /* SOFTRF_ADDRESS */
}

static struct rst_info reset_info = {
  .reason = REASON_DEFAULT_RST,
};

static void* ESP32_getResetInfoPtr()
{
  switch (rtc_get_reset_reason(0))
  {
    case POWERON_RESET          : reset_info.reason = REASON_DEFAULT_RST; break;
    case DEEPSLEEP_RESET        : reset_info.reason = REASON_DEEP_SLEEP_AWAKE; break;
    case TG0WDT_SYS_RESET       : reset_info.reason = REASON_WDT_RST; break;
    case TG1WDT_SYS_RESET       : reset_info.reason = REASON_WDT_RST; break;
    case RTCWDT_SYS_RESET       : reset_info.reason = REASON_WDT_RST; break;
    case INTRUSION_RESET        : reset_info.reason = REASON_EXCEPTION_RST; break;
    case RTCWDT_CPU_RESET       : reset_info.reason = REASON_WDT_RST; break;
    case RTCWDT_BROWN_OUT_RESET : reset_info.reason = REASON_EXT_SYS_RST; break;
    case RTCWDT_RTC_RESET       :
      /* Slow start of GD25LQ32 causes one read fault at boot time with current ESP-IDF */
      if (ESP32_getFlashId() == MakeFlashId(GIGADEVICE_ID, GIGADEVICE_GD25LQ32))
                                  reset_info.reason = REASON_DEFAULT_RST;
      else
                                  reset_info.reason = REASON_WDT_RST;
                                  break;
#if defined(CONFIG_IDF_TARGET_ESP32)
    case SW_RESET               : reset_info.reason = REASON_SOFT_RESTART; break;
    case OWDT_RESET             : reset_info.reason = REASON_WDT_RST; break;
    case SDIO_RESET             : reset_info.reason = REASON_EXCEPTION_RST; break;
    case TGWDT_CPU_RESET        : reset_info.reason = REASON_WDT_RST; break;
    case SW_CPU_RESET           : reset_info.reason = REASON_SOFT_RESTART; break;
    case EXT_CPU_RESET          : reset_info.reason = REASON_EXT_SYS_RST; break;
#endif /* CONFIG_IDF_TARGET_ESP32 */
    default                     : reset_info.reason = REASON_DEFAULT_RST;
  }

  return (void *) &reset_info;
}

static String ESP32_getResetInfo()
{
  switch (rtc_get_reset_reason(0))
  {
    case POWERON_RESET          : return F("Vbat power on reset");
    case DEEPSLEEP_RESET        : return F("Deep Sleep reset digital core");
    case TG0WDT_SYS_RESET       : return F("Timer Group0 Watch dog reset digital core");
    case TG1WDT_SYS_RESET       : return F("Timer Group1 Watch dog reset digital core");
    case RTCWDT_SYS_RESET       : return F("RTC Watch dog Reset digital core");
    case INTRUSION_RESET        : return F("Instrusion tested to reset CPU");
    case RTCWDT_CPU_RESET       : return F("RTC Watch dog Reset CPU");
    case RTCWDT_BROWN_OUT_RESET : return F("Reset when the vdd voltage is not stable");
    case RTCWDT_RTC_RESET       : return F("RTC Watch dog reset digital core and rtc module");
#if defined(CONFIG_IDF_TARGET_ESP32)
    case SW_RESET               : return F("Software reset digital core");
    case OWDT_RESET             : return F("Legacy watch dog reset digital core");
    case SDIO_RESET             : return F("Reset by SLC module, reset digital core");
    case TGWDT_CPU_RESET        : return F("Time Group reset CPU");
    case SW_CPU_RESET           : return F("Software reset CPU");
    case EXT_CPU_RESET          : return F("for APP CPU, reseted by PRO CPU");
#endif /* CONFIG_IDF_TARGET_ESP32 */
    default                     : return F("No reset information available");
  }
}

static String ESP32_getResetReason()
{

  switch (rtc_get_reset_reason(0))
  {
    case POWERON_RESET          : return F("POWERON_RESET");
    case DEEPSLEEP_RESET        : return F("DEEPSLEEP_RESET");
    case TG0WDT_SYS_RESET       : return F("TG0WDT_SYS_RESET");
    case TG1WDT_SYS_RESET       : return F("TG1WDT_SYS_RESET");
    case RTCWDT_SYS_RESET       : return F("RTCWDT_SYS_RESET");
    case INTRUSION_RESET        : return F("INTRUSION_RESET");
    case RTCWDT_CPU_RESET       : return F("RTCWDT_CPU_RESET");
    case RTCWDT_BROWN_OUT_RESET : return F("RTCWDT_BROWN_OUT_RESET");
    case RTCWDT_RTC_RESET       : return F("RTCWDT_RTC_RESET");
#if defined(CONFIG_IDF_TARGET_ESP32)
    case SW_RESET               : return F("SW_RESET");
    case OWDT_RESET             : return F("OWDT_RESET");
    case SDIO_RESET             : return F("SDIO_RESET");
    case TGWDT_CPU_RESET        : return F("TGWDT_CPU_RESET");
    case SW_CPU_RESET           : return F("SW_CPU_RESET");
    case EXT_CPU_RESET          : return F("EXT_CPU_RESET");
#endif /* CONFIG_IDF_TARGET_ESP32 */
    default                     : return F("NO_MEAN");
  }
}

static uint32_t ESP32_getFreeHeap()
{
  return ESP.getFreeHeap();
}

static long ESP32_random(long howsmall, long howBig)
{
  return random(howsmall, howBig);
}

#if !defined(CONFIG_IDF_TARGET_ESP32S2) && defined(USE_BLE_MIDI)
extern bool deviceConnected;
extern BLECharacteristic* pMIDICharacteristic;

uint8_t midiPacket[] = {
   0x80,  // header
   0x80,  // timestamp, not implemented
   0x00,  // status
   0x3c,  // 0x3c == 60 == middle c
   0x00   // velocity
};

byte note_sequence[] = {62,65,69,65,67,67,65,64,69,69,67,67,62,62};
#endif /* USE_BLE_MIDI */


static uint32_t ESP32_maxSketchSpace()
{
  return ESP32_Min_AppPart_Size ? ESP32_Min_AppPart_Size :
           SoC->id == SOC_ESP32S3 ?
             0x200000  /* 8MB-tinyuf2.csv */ :
             0x1E0000; /* min_spiffs.csv */
}

static const int8_t ESP32_dBm_to_power_level[21] = {
  8,  /* 2    dBm, #0 */
  8,  /* 2    dBm, #1 */
  8,  /* 2    dBm, #2 */
  8,  /* 2    dBm, #3 */
  8,  /* 2    dBm, #4 */
  20, /* 5    dBm, #5 */
  20, /* 5    dBm, #6 */
  28, /* 7    dBm, #7 */
  28, /* 7    dBm, #8 */
  34, /* 8.5  dBm, #9 */
  34, /* 8.5  dBm, #10 */
  44, /* 11   dBm, #11 */
  44, /* 11   dBm, #12 */
  52, /* 13   dBm, #13 */
  52, /* 13   dBm, #14 */
  60, /* 15   dBm, #15 */
  60, /* 15   dBm, #16 */
  68, /* 17   dBm, #17 */
  74, /* 18.5 dBm, #18 */
  76, /* 19   dBm, #19 */
  78  /* 19.5 dBm, #20 */
};

static void ESP32_WiFi_set_param(int ndx, int value)
{
#if !defined(EXCLUDE_WIFI)
  uint32_t lt = value * 60; /* in minutes */

  switch (ndx)
  {
  case WIFI_PARAM_TX_POWER:
    if (value > 20) {
      value = 20; /* dBm */
    }

    if (value < 0) {
      value = 0; /* dBm */
    }

    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(ESP32_dBm_to_power_level[value]));
    break;
  case WIFI_PARAM_DHCP_LEASE_TIME:
    tcpip_adapter_dhcps_option(
      (tcpip_adapter_dhcp_option_mode_t) TCPIP_ADAPTER_OP_SET,
      (tcpip_adapter_dhcp_option_id_t)   TCPIP_ADAPTER_IP_ADDRESS_LEASE_TIME,
      (void*) &lt, sizeof(lt));
    break;
  default:
    break;
  }
#endif /* EXCLUDE_WIFI */
}

static IPAddress ESP32_WiFi_get_broadcast()
{
  tcpip_adapter_ip_info_t info;
  IPAddress broadcastIp;

  if (WiFi.getMode() == WIFI_STA) {
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &info);
  } else {
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &info);
  }
  broadcastIp = ~info.netmask.addr | info.ip.addr;

  return broadcastIp;
}

static void ESP32_WiFi_transmit_UDP(int port, byte *buf, size_t size)
{
#if !defined(EXCLUDE_WIFI)
  IPAddress ClientIP;
  WiFiMode_t mode = WiFi.getMode();
  int i = 0;

  switch (mode)
  {
  case WIFI_STA:
    ClientIP = ESP32_WiFi_get_broadcast();

    Uni_Udp.beginPacket(ClientIP, port);
    Uni_Udp.write(buf, size);
    Uni_Udp.endPacket();

    break;
  case WIFI_AP:
    wifi_sta_list_t stations;
    ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&stations));

    tcpip_adapter_sta_list_t infoList;
    ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&stations, &infoList));

    while(i < infoList.num) {
      ClientIP = infoList.sta[i++].ip.addr;

      Uni_Udp.beginPacket(ClientIP, port);
      Uni_Udp.write(buf, size);
      Uni_Udp.endPacket();
    }
    break;
  case WIFI_OFF:
  default:
    break;
  }
#endif /* EXCLUDE_WIFI */
}

static void ESP32_WiFiUDP_stopAll()
{
/* not implemented yet */
}

static bool ESP32_WiFi_hostname(String aHostname)
{
#if defined(EXCLUDE_WIFI)
  return false;
#else
  return WiFi.setHostname(aHostname.c_str());
#endif /* EXCLUDE_WIFI */
}

static int ESP32_WiFi_clients_count()
{
#if defined(EXCLUDE_WIFI)
  return 0;
#else
  WiFiMode_t mode = WiFi.getMode();

  switch (mode)
  {
  case WIFI_AP:
    wifi_sta_list_t stations;
    ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&stations));

    tcpip_adapter_sta_list_t infoList;
    ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&stations, &infoList));

    return infoList.num;
  case WIFI_STA:
  default:
    return -1; /* error */
  }
#endif /* EXCLUDE_WIFI */
}

static bool ESP32_EEPROM_begin(size_t size)
{
  bool rval = true;

#if !defined(EXCLUDE_EEPROM)
  rval = EEPROM.begin(size);
#endif

  return rval;
}

static void ESP32_EEPROM_extension(int cmd)
{
  if (cmd == EEPROM_EXT_LOAD) {
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32C3) || \
    defined(USE_USB_HOST)
    if (settings->nmea_out == NMEA_USB) {
      settings->nmea_out = NMEA_UART;
    }
    if (settings->gdl90 == GDL90_USB) {
      settings->gdl90 = GDL90_UART;
    }
    if (settings->d1090 == D1090_USB) {
      settings->d1090 = D1090_UART;
    }
#endif /* CONFIG_IDF_TARGET_ESP32 */
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || \
    defined(CONFIG_IDF_TARGET_ESP32C3)
    if (settings->bluetooth != BLUETOOTH_NONE) {
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
      settings->bluetooth = BLUETOOTH_LE_HM10_SERIAL;
#else
      settings->bluetooth = BLUETOOTH_NONE;
#endif /* CONFIG_IDF_TARGET_ESP32S3 || C3 */
    }

    // if (hw_info.model == SOFTRF_MODEL_HAM) {
    //   settings->power_save |= POWER_SAVE_NORECEIVE;
    // }
#endif /* CONFIG_IDF_TARGET_ESP32S2 || S3 || C3 */

    /* AUTO and UK RF bands are deprecated since Release v1.3 */
    if (settings->band == RF_BAND_AUTO || settings->band == RF_BAND_UK) {
      settings->band = RF_BAND_EU;
    }
  }
}

static void ESP32_SPI_begin()
{
  switch (esp32_board)
  {
    case ESP32_S2_T8_V1_1:
      SPI.begin(SOC_GPIO_PIN_T8_S2_SCK,  SOC_GPIO_PIN_T8_S2_MISO,
                SOC_GPIO_PIN_T8_S2_MOSI, SOC_GPIO_PIN_T8_S2_SS);
      break;
    case ESP32_S3_DEVKIT:
    case ESP32_TTGO_T_BEAM_SUPREME:
      SPI.begin(SOC_GPIO_PIN_S3_SCK,  SOC_GPIO_PIN_S3_MISO,
                SOC_GPIO_PIN_S3_MOSI, SOC_GPIO_PIN_S3_SS);
      break;
    case ESP32_C3_DEVKIT:
      SPI.begin(SOC_GPIO_PIN_C3_SCK,  SOC_GPIO_PIN_C3_MISO,
                SOC_GPIO_PIN_C3_MOSI, SOC_GPIO_PIN_C3_SS);
      break;
    case ESP32_LILYGO_T_TWR_V2_0:
    case ESP32_LILYGO_T_TWR_V2_1:
      SPI.begin(SOC_GPIO_PIN_TWR2_SCK,  SOC_GPIO_PIN_TWR2_MISO,
                SOC_GPIO_PIN_TWR2_MOSI, SOC_GPIO_PIN_TWR2_SS);
      break;
    case ESP32_HELTEC_TRACKER:
      SPI.begin(SOC_GPIO_PIN_HELTRK_SCK,  SOC_GPIO_PIN_HELTRK_MISO,
                SOC_GPIO_PIN_HELTRK_MOSI, SOC_GPIO_PIN_HELTRK_SS);
      break;
    default:
      SPI.begin(SOC_GPIO_PIN_SCK,  SOC_GPIO_PIN_MISO,
                SOC_GPIO_PIN_MOSI, SOC_GPIO_PIN_SS);
      break;
  }
}

static void ESP32_swSer_begin(unsigned long baud)
{
  if (hw_info.model == SOFTRF_MODEL_PRIME_MK2) {

    Serial.print(F("INFO: TTGO T-Beam rev. "));
    Serial.print(hw_info.revision);
    Serial.println(F(" is detected."));

    if (hw_info.revision >= 8) {
      Serial_GNSS_In.begin(baud, SERIAL_IN_BITS,
                           SOC_GPIO_PIN_TBEAM_V08_RX,
                           SOC_GPIO_PIN_TBEAM_V08_TX);
    } else {
      Serial_GNSS_In.begin(baud, SERIAL_IN_BITS,
                           SOC_GPIO_PIN_TBEAM_V05_RX,
                           SOC_GPIO_PIN_TBEAM_V05_TX);
    }
  } else if (hw_info.model == SOFTRF_MODEL_PRIME_MK3) {

    Serial.println(F("INFO: TTGO T-Beam Supreme is detected."));

    Serial_GNSS_In.begin(baud, SERIAL_IN_BITS,
                         SOC_GPIO_PIN_S3_GNSS_RX,
                         SOC_GPIO_PIN_S3_GNSS_TX);

  } else {
    if (esp32_board == ESP32_TTGO_T_WATCH) {
      Serial.println(F("INFO: TTGO T-Watch is detected."));
      Serial_GNSS_In.begin(baud, SERIAL_IN_BITS,
                           SOC_GPIO_PIN_TWATCH_RX, SOC_GPIO_PIN_TWATCH_TX);
    } else if (esp32_board == ESP32_TTGO_V2_OLED) {
      /* 'Mini' (TTGO T3 + GNSS) */
      Serial.print(F("INFO: TTGO T3 rev. "));
      Serial.print(hw_info.revision);
      Serial.println(F(" is detected."));
      Serial_GNSS_In.begin(baud, SERIAL_IN_BITS,
                           TTGO_V2_PIN_GNSS_RX, TTGO_V2_PIN_GNSS_TX);
    } else if (esp32_board == ESP32_S2_T8_V1_1) {
      Serial.println(F("INFO: TTGO T8_S2 rev. 1.1 is detected."));
      Serial_GNSS_In.begin(baud, SERIAL_IN_BITS,
                           SOC_GPIO_PIN_T8_S2_GNSS_RX,
                           SOC_GPIO_PIN_T8_S2_GNSS_TX);
    } else if (esp32_board == ESP32_S3_DEVKIT) {
      Serial.println(F("INFO: ESP32-S3 DevKit is detected."));
      Serial_GNSS_In.begin(baud, SERIAL_IN_BITS,
                           SOC_GPIO_PIN_S3_GNSS_RX, SOC_GPIO_PIN_S3_GNSS_TX);
    } else if (esp32_board == ESP32_C3_DEVKIT) {
      Serial.println(F("INFO: ESP32-C3 DevKit is detected."));
      Serial_GNSS_In.begin(baud, SERIAL_IN_BITS,
                           SOC_GPIO_PIN_C3_GNSS_RX, SOC_GPIO_PIN_C3_GNSS_TX);
    } else if (esp32_board == ESP32_LILYGO_T_TWR_V2_0) {
      uint64_t mac = ESP.getEfuseMac();
      Serial.println(F("INFO: LilyGO T-TWR rev. 2.0 is detected."));
      if (mac == 0x7475ac188534ULL || mac == 0x58f8ab188534ULL) {
        Serial.println(F("INFO: Audio ADC workaround has been applied."));
      }
      Serial_GNSS_In.begin(baud, SERIAL_IN_BITS,
                           SOC_GPIO_PIN_TWR2_GNSS_RX, SOC_GPIO_PIN_TWR2_GNSS_TX);
    } else if (esp32_board == ESP32_LILYGO_T_TWR_V2_1) {
      Serial.print(F("INFO: LilyGO T-TWR rev. 2.1 "));
#if defined(USE_SA8X8)
      Serial.print(controller.getBand() == Band::VHF ? "VHF " : "UHF ");
#endif /* USE_SA8X8 */
      Serial.println(F("is detected."));
      Serial_GNSS_In.begin(baud, SERIAL_IN_BITS,
                           SOC_GPIO_PIN_TWR2_GNSS_RX, SOC_GPIO_PIN_TWR2_GNSS_TX);
    } else if (esp32_board == ESP32_HELTEC_TRACKER) {
      Serial.print(F("INFO: Heltec Tracker rev. "));
      Serial.print(hw_info.revision);
      Serial.println(F(" is detected."));
      Serial_GNSS_In.begin(115200, SERIAL_IN_BITS,
                           SOC_GPIO_PIN_HELTRK_GNSS_RX,
                           SOC_GPIO_PIN_HELTRK_GNSS_TX);
    } else {
      /* open Standalone's GNSS port */
      Serial_GNSS_In.begin(baud, SERIAL_IN_BITS,
                           SOC_GPIO_PIN_GNSS_RX, SOC_GPIO_PIN_GNSS_TX);
    }
  }

  /* Default Rx buffer size (256 bytes) is sometimes not big enough */
  // Serial_GNSS_In.setRxBufferSize(512);

  /* Need to gather some statistics on variety of flash IC usage */
  Serial.print(F("Flash memory ID: "));
  Serial.println(ESP32_getFlashId(), HEX);
}

static void ESP32_swSer_enableRx(boolean arg)
{

}

#if defined(USE_OLED)
static byte ESP32_OLED_ident(TwoWire *bus)
{
  uint8_t r = 0;
  byte rval = DISPLAY_OLED_TTGO;

  bus->beginTransmission(SSD1306_OLED_I2C_ADDR);
  bus->write(0x00);
  bus->endTransmission();
  bus->requestFrom((int) SSD1306_OLED_I2C_ADDR, 1);
  if (bus->available()) {
    r = bus->read();
    r &= 0x0f;

    if (r == 0x08 || r == 0x00 || r == 0x0C) {
        rval = DISPLAY_OLED_1_3;  // SH1106
    } else if (r == 0x03 || r == 0x04 || r == 0x06 || r == 0x07) {
        rval = DISPLAY_OLED_TTGO; // SSD1306
    }
  }

#if 1
  Serial.print("INFO: OLED subtype ");
  Serial.println(r, HEX);
#endif

  return rval;
}
#endif /* USE_OLED */

static byte ESP32_Display_setup()
{
  byte rval = DISPLAY_NONE;

  if (esp32_board != ESP32_TTGO_T_WATCH &&
      esp32_board != ESP32_S2_T8_V1_1   &&
      esp32_board != ESP32_HELTEC_TRACKER) {

#if defined(USE_OLED)
    bool has_oled = false;

    /* SSD1306 I2C OLED probing */
    if (esp32_board == ESP32_S3_DEVKIT) {
      Wire.begin(SOC_GPIO_PIN_S3_SDA, SOC_GPIO_PIN_S3_SCL);
      Wire.beginTransmission(SSD1306_OLED_I2C_ADDR);
      has_oled = (Wire.endTransmission() == 0);
      if (has_oled) {
        rval = ESP32_OLED_ident(&Wire);
        if (rval == DISPLAY_OLED_1_3) {
          u8x8 = &u8x8_1_3;
        } else {
          u8x8 = &u8x8_ttgo;
        }
      }
      WIRE_FINI(Wire);
    } else if (esp32_board == ESP32_TTGO_T_BEAM_SUPREME) {
      Wire.begin(SOC_GPIO_PIN_S3_SDA, SOC_GPIO_PIN_S3_SCL);
      Wire.beginTransmission(SH1106_OLED_I2C_ADDR);
      has_oled = (Wire.endTransmission() == 0);
      WIRE_FINI(Wire);
      if (has_oled) {
        u8x8 = &u8x8_1_3;
        rval = DISPLAY_OLED_1_3;
      }
    } else if (hw_info.model == SOFTRF_MODEL_HAM) {
      Wire.begin(SOC_GPIO_PIN_TWR2_SDA, SOC_GPIO_PIN_TWR2_SCL);
      Wire.beginTransmission(SSD1306_OLED_I2C_ADDR);
      has_oled = (Wire.endTransmission() == 0);
      if (has_oled) {
        u8x8 = &u8x8_1_3;
        rval = DISPLAY_OLED_1_3;
      }
    } else if (GPIO_21_22_are_busy) {
      if (hw_info.model == SOFTRF_MODEL_PRIME_MK2 && hw_info.revision >= 8) {
        Wire1.begin(TTGO_V2_OLED_PIN_SDA , TTGO_V2_OLED_PIN_SCL);
        Wire1.beginTransmission(SSD1306_OLED_I2C_ADDR);
        has_oled = (Wire1.endTransmission() == 0);
        if (has_oled) {
#if 0
          rval = ESP32_OLED_ident(&Wire1);
          if (rval == DISPLAY_OLED_1_3) {
            u8x8 = &u8x8_1_3;
          } else {
            u8x8 = &u8x8_ttgo;
          }
#else
          u8x8 = &u8x8_ttgo;
          rval = DISPLAY_OLED_TTGO;
#endif
        }
      }
      else 
      {
       /* Wire1.begin(HELTEC_OLED_PIN_SDA , HELTEC_OLED_PIN_SCL);
        Wire1.beginTransmission(SSD1306_OLED_I2C_ADDR);
        has_oled = (Wire1.endTransmission() == 0);
        WIRE_FINI(Wire1);
        if (has_oled) 
        {
          u8x8 = &u8x8_heltec;
          esp32_board = ESP32_HELTEC_OLED;
          rval = DISPLAY_OLED_HELTEC;
        }*/
      }
    }
    else 
    {
      Wire1.begin(TTGO_V2_OLED_PIN_SDA , TTGO_V2_OLED_PIN_SCL);
      Wire1.beginTransmission(SSD1306_OLED_I2C_ADDR);
      has_oled = (Wire1.endTransmission() == 0);
      if (has_oled) {
#if 0
        rval = ESP32_OLED_ident(&Wire1);
        if (rval == DISPLAY_OLED_1_3) {
          u8x8 = &u8x8_1_3;
        } else {
          u8x8 = &u8x8_ttgo;
        }
#else
        u8x8 = &u8x8_ttgo;
        rval = DISPLAY_OLED_TTGO;
#endif
        if (hw_info.model == SOFTRF_MODEL_STANDALONE) {
          esp32_board = ESP32_TTGO_V2_OLED;

          if (RF_SX12XX_RST_is_connected) {
            hw_info.revision = 16;
          } else {
            hw_info.revision = 11;
          }
          hw_info.storage = STORAGE_CARD;
        }
      }
      else 
      {
        /*if (!(hw_info.model    == SOFTRF_MODEL_PRIME_MK2 &&
              hw_info.revision >= 8)) {
          Wire1.begin(HELTEC_OLED_PIN_SDA , HELTEC_OLED_PIN_SCL);
          Wire1.beginTransmission(SSD1306_OLED_I2C_ADDR);
          has_oled = (Wire1.endTransmission() == 0);
          WIRE_FINI(Wire1);
          if (has_oled) {
            u8x8 = &u8x8_heltec;
            esp32_board = ESP32_HELTEC_OLED;
            rval = DISPLAY_OLED_HELTEC;
          }
        }*/
      }
    }

    if (u8x8) {
      u8x8->begin();
      u8x8->setFlipMode(OLED_flip);
      u8x8->setFont(u8x8_font_chroma48medium8_r);
      u8x8->clear();

      uint8_t shift_y = hw_info.model == SOFTRF_MODEL_PRIME_MK3 ||
                        hw_info.model == SOFTRF_MODEL_HAM ? 1 : 0;

      u8x8->draw2x2String( 2, 2 - shift_y, SoftRF_text1);

      if (shift_y) {
        u8x8->drawString   ( 6, 3, SoftRF_text2);
        u8x8->draw2x2String( 2, 4, SoftRF_text3);
      }

      u8x8->drawString   ( 3, 6 + shift_y,
                           SOFTRF_FIRMWARE_VERSION
#if defined(USE_USB_HOST)
                           "H"
#endif /* USE_USB_HOST */
                         );
      u8x8->drawString   (11, 6 + shift_y, ISO3166_CC[settings->band]);
    }

    SoC->ADB_ops && SoC->ADB_ops->setup();
#endif /* USE_OLED */

  }

  return rval;
}

static void ESP32_Display_loop()
{
  char buf[16];
  uint32_t disp_value;

  uint16_t tbw;
  uint16_t tbh;

  switch (hw_info.display)
  {

#if defined(USE_OLED)
  case DISPLAY_OLED_TTGO:
  case DISPLAY_OLED_HELTEC:
  case DISPLAY_OLED_1_3:
    OLED_loop();
    break;
#endif /* USE_OLED */

  case DISPLAY_NONE:
  default:
    break;
  }
}

static void ESP32_Display_fini(int reason)
{
  switch (hw_info.display)
  {
#if defined(USE_OLED)
  case DISPLAY_OLED_TTGO:
  case DISPLAY_OLED_HELTEC:
  case DISPLAY_OLED_1_3:

    SoC->ADB_ops && SoC->ADB_ops->fini();

    OLED_fini(reason);

    if (u8x8) {

      delay(3000); /* Keep shutdown message on OLED for 3 seconds */

      u8x8->noDisplay();
    }
    break;
#endif /* USE_OLED */

  case DISPLAY_NONE:
  default:
    break;
  }
}

static void ESP32_Battery_setup()
{
  if ((hw_info.model    == SOFTRF_MODEL_PRIME_MK2  &&
       hw_info.revision >= 8)                      ||
       hw_info.model    == SOFTRF_MODEL_PRIME_MK3  ||
       hw_info.model    == SOFTRF_MODEL_SKYWATCH) {

    /* T-Beam v08+, T-Beam Supreme and T-Watch have PMU */

  } else {
#if defined(CONFIG_IDF_TARGET_ESP32)
    calibrate_voltage(hw_info.model == SOFTRF_MODEL_PRIME_MK2 ||
                     (esp32_board == ESP32_TTGO_V2_OLED && hw_info.revision == 16) ?
                      ADC1_GPIO35_CHANNEL : ADC1_GPIO36_CHANNEL);
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    calibrate_voltage(ADC1_GPIO9_CHANNEL);
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    /* use this procedure on T-TWR Plus (has PMU) to calibrate audio ADC */
    if (esp32_board == ESP32_HELTEC_TRACKER    ||
        esp32_board == ESP32_LILYGO_T_TWR_V2_0 ||
        esp32_board == ESP32_LILYGO_T_TWR_V2_1 /* TBD */) {
      calibrate_voltage(ADC1_GPIO1_CHANNEL);
    } else {
      calibrate_voltage(ADC1_GPIO2_CHANNEL);
    }
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    calibrate_voltage(ADC1_GPIO1_CHANNEL);
#else
#error "This ESP32 family build variant is not supported!"
#endif /* CONFIG_IDF_TARGET_ESP32 */
  }
}

static float ESP32_Battery_param(uint8_t param)
{
  float rval, voltage;

  switch (param)
  {
  case BATTERY_PARAM_THRESHOLD:
    rval = (hw_info.model == SOFTRF_MODEL_PRIME_MK2  && hw_info.revision ==  8) ?
            BATTERY_THRESHOLD_LIPO + 0.1 :
            hw_info.model == SOFTRF_MODEL_PRIME_MK2  ||
            hw_info.model == SOFTRF_MODEL_PRIME_MK3  || /* TBD */
            hw_info.model == SOFTRF_MODEL_HAM        || /* TBD */
            hw_info.model == SOFTRF_MODEL_MIDI       || /* TBD */
            /* TTGO T3 V2.1.6 */
           (hw_info.model == SOFTRF_MODEL_STANDALONE && hw_info.revision == 16) ?
            BATTERY_THRESHOLD_LIPO : BATTERY_THRESHOLD_NIMHX2;
    break;

  case BATTERY_PARAM_CUTOFF:
    rval = (hw_info.model == SOFTRF_MODEL_PRIME_MK2  && hw_info.revision ==  8) ?
            BATTERY_CUTOFF_LIPO + 0.2 :
            hw_info.model == SOFTRF_MODEL_PRIME_MK2  ||
            hw_info.model == SOFTRF_MODEL_PRIME_MK3  || /* TBD */
            hw_info.model == SOFTRF_MODEL_HAM        || /* TBD */
            hw_info.model == SOFTRF_MODEL_MIDI       || /* TBD */
            /* TTGO T3 V2.1.6 */
           (hw_info.model == SOFTRF_MODEL_STANDALONE && hw_info.revision == 16) ?
            BATTERY_CUTOFF_LIPO : BATTERY_CUTOFF_NIMHX2;
    break;

  case BATTERY_PARAM_CHARGE:
    voltage = Battery_voltage();
    if (voltage < Battery_cutoff())
      return 0;

    if (voltage > 4.2)
      return 100;

    if (voltage < 3.6) {
      voltage -= 3.3;
      return (voltage * 100) / 3;
    }

    voltage -= 3.6;
    rval = 10 + (voltage * 150 );
    break;

  case BATTERY_PARAM_VOLTAGE:
  default:
    voltage = 0.0;

    switch (hw_info.pmu)
    {
    case PMU_AXP192:
    case PMU_AXP202:
      if (axp_xxx.isBatteryConnect()) {
        voltage = axp_xxx.getBattVoltage();
      }
      break;

    case PMU_AXP2101:
      if (axp_2xxx.isBatteryConnect()) {
        voltage = axp_2xxx.getBattVoltage();
      }
      break;

    case PMU_NONE:
    default:
      voltage = (float) read_voltage();

      /* T-Beam v02-v07 and T3 V2.1.6 have voltage divider 100k/100k on board */
      if (hw_info.model == SOFTRF_MODEL_PRIME_MK2   ||
         (esp32_board   == ESP32_TTGO_V2_OLED && hw_info.revision == 16) ||
          esp32_board   == ESP32_S2_T8_V1_1) {
        voltage += voltage;
      } else if (esp32_board == ESP32_C3_DEVKIT) {
      /* NodeMCU has voltage divider 100k/220k on board */
        voltage *= 3.2;
      } else if (esp32_board == ESP32_HELTEC_TRACKER) {
        voltage *= 4.9;
      }
      break;
    }

    rval = voltage * 0.001;
    break;
  }

  return rval;
}

static void IRAM_ATTR ESP32_GNSS_PPS_Interrupt_handler()
{
  portENTER_CRITICAL_ISR(&GNSS_PPS_mutex);
  PPS_TimeMarker = millis();    /* millis() has IRAM_ATTR */
  portEXIT_CRITICAL_ISR(&GNSS_PPS_mutex);
}

static unsigned long ESP32_get_PPS_TimeMarker()
{
  unsigned long rval;
  portENTER_CRITICAL_ISR(&GNSS_PPS_mutex);
  rval = PPS_TimeMarker;
  portEXIT_CRITICAL_ISR(&GNSS_PPS_mutex);
  return rval;
}

static bool ESP32_Baro_setup()
{
  if (hw_info.model == SOFTRF_MODEL_SKYWATCH) {

    return false;

  } else if (esp32_board == ESP32_S2_T8_V1_1) {

    Wire.setPins(SOC_GPIO_PIN_T8_S2_SDA, SOC_GPIO_PIN_T8_S2_SCL);

  } else if (esp32_board == ESP32_S3_DEVKIT ||
             esp32_board == ESP32_TTGO_T_BEAM_SUPREME) {

    Wire.setPins(SOC_GPIO_PIN_S3_SDA, SOC_GPIO_PIN_S3_SCL);

  } else if (esp32_board == ESP32_C3_DEVKIT) {

    Wire.setPins(SOC_GPIO_PIN_C3_SDA, SOC_GPIO_PIN_C3_SCL);

  } else if (esp32_board == ESP32_LILYGO_T_TWR_V2_0 ||
             esp32_board == ESP32_LILYGO_T_TWR_V2_1) {

    Wire.setPins(SOC_GPIO_PIN_TWR2_SDA, SOC_GPIO_PIN_TWR2_SCL);

  } else if (esp32_board == ESP32_HELTEC_TRACKER) {

    Wire.setPins(SOC_GPIO_PIN_HELTRK_SDA, SOC_GPIO_PIN_HELTRK_SCL);

  } else if (hw_info.model != SOFTRF_MODEL_PRIME_MK2) {

    if ((hw_info.rf != RF_IC_SX1276 && hw_info.rf != RF_IC_SX1262) ||
        RF_SX12XX_RST_is_connected) {
      return false;
    }

#if DEBUG
    Serial.println(F("INFO: RESET pin of SX12xx radio is not connected to MCU."));
#endif

    Wire.setPins(SOC_GPIO_PIN_SDA, SOC_GPIO_PIN_SCL);

  } else {

    if (hw_info.revision == 2 && RF_SX12XX_RST_is_connected) {
      hw_info.revision = 5;
    }

    /* Start from 1st I2C bus */
    Wire.setPins(SOC_GPIO_PIN_TBEAM_SDA, SOC_GPIO_PIN_TBEAM_SCL);
    if (Baro_probe())
      return true;

    WIRE_FINI(Wire);

    if (hw_info.revision == 2)
      return false;

#if !defined(ENABLE_AHRS)
    /* Try out OLED I2C bus */
    Wire.begin(TTGO_V2_OLED_PIN_SDA, TTGO_V2_OLED_PIN_SCL);
    if (hw_info.model == SOFTRF_MODEL_PRIME_MK2 && hw_info.revision >= 8) {
      Wire1 = Wire;
    }
    if (!Baro_probe()) {
      if (!(hw_info.model == SOFTRF_MODEL_PRIME_MK2 && hw_info.revision >= 8)) {
        WIRE_FINI(Wire);
      }
      return false;
    }

    GPIO_21_22_are_busy = true;
#else
    return false;
#endif
  }

  return true;
}

static void ESP32_WDT_setup()
{
  enableLoopWDT();
}

static void ESP32_WDT_fini()
{
  disableLoopWDT();
}

#include <AceButton.h>
using namespace ace_button;

AceButton button_1(SOC_GPIO_PIN_TBEAM_V05_BUTTON);

// The event handler for the button.
void handleMainEvent(AceButton* button, uint8_t eventType,
    uint8_t buttonState) {

  switch (eventType) {
    case AceButton::kEventClicked:
    case AceButton::kEventReleased:
#if defined(USE_OLED)
      if (button == &button_1) {
        OLED_Next_Page();
      }
#endif
      break;
    case AceButton::kEventDoubleClicked:
      break;
    case AceButton::kEventLongPressed:
      if (button == &button_1) {
        shutdown(SOFTRF_SHUTDOWN_BUTTON);
      }
      break;
  }
}

void handleAuxEvent(AceButton* button, uint8_t eventType,
    uint8_t buttonState) {

  switch (eventType) {
    case AceButton::kEventClicked:
    case AceButton::kEventReleased:
#if defined(USE_OLED)
      if (button == &button_1) {
        OLED_Up();
      }
#endif
      break;
    case AceButton::kEventDoubleClicked:
      break;
  }
}

/* Callbacks for push button interrupt */
void onPageButtonEvent() {
  button_1.check();
}

static void ESP32_Button_setup()
{
  int button_pin = SOC_GPIO_PIN_TBEAM_V05_BUTTON;

  if (( hw_info.model == SOFTRF_MODEL_PRIME_MK2 &&
       (hw_info.revision == 2 || hw_info.revision == 5)) ||
       esp32_board == ESP32_S2_T8_V1_1        ||
       esp32_board == ESP32_LILYGO_T_TWR_V2_0 ||
       esp32_board == ESP32_HELTEC_TRACKER    ||
       esp32_board == ESP32_S3_DEVKIT) {
    button_pin = esp32_board == ESP32_S2_T8_V1_1 ? SOC_GPIO_PIN_T8_S2_BUTTON  :
                 esp32_board == ESP32_S3_DEVKIT  ? SOC_GPIO_PIN_S3_BUTTON     :
                 esp32_board == ESP32_HELTEC_TRACKER ? SOC_GPIO_PIN_S3_BUTTON :
                 esp32_board == ESP32_LILYGO_T_TWR_V2_0 ?
                 SOC_GPIO_PIN_TWR2_ENC_BUTTON : SOC_GPIO_PIN_TBEAM_V05_BUTTON;

    // Button(s) uses external pull up resistor.
    pinMode(button_pin, button_pin == 0 ? INPUT_PULLUP : INPUT);

    button_1.init(button_pin);

    // Configure the ButtonConfig with the event handler, and enable all higher
    // level events.
    ButtonConfig* PageButtonConfig = button_1.getButtonConfig();
    PageButtonConfig->setEventHandler(handleMainEvent);
    PageButtonConfig->setFeature(ButtonConfig::kFeatureClick);
    PageButtonConfig->setFeature(ButtonConfig::kFeatureLongPress);
    PageButtonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterClick);
//  PageButtonConfig->setDebounceDelay(15);
    PageButtonConfig->setClickDelay(600);
    PageButtonConfig->setLongPressDelay(2000);
  } else if ((hw_info.model == SOFTRF_MODEL_PRIME_MK2 && hw_info.revision >= 8) ||
             esp32_board == ESP32_TTGO_T_BEAM_SUPREME) {
    button_pin = (esp32_board == ESP32_TTGO_T_BEAM_SUPREME) ?
                 SOC_GPIO_PIN_S3_BUTTON :
                 SOC_GPIO_PIN_TBEAM_V08_BUTTON;

    // Button(s) uses external pull up resistor.
    pinMode(button_pin, button_pin == 0 ? INPUT_PULLUP : INPUT);

    button_1.init(button_pin);

    ButtonConfig* PageButtonConfig = button_1.getButtonConfig();
    PageButtonConfig->setEventHandler(handleAuxEvent);
    PageButtonConfig->setFeature(ButtonConfig::kFeatureClick);
    PageButtonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterClick);
    PageButtonConfig->setClickDelay(600);
  }
}

static void ESP32_Button_loop()
{
  if (esp32_board == ESP32_TTGO_T_BEAM         ||
      esp32_board == ESP32_TTGO_T_BEAM_SUPREME ||
      esp32_board == ESP32_S2_T8_V1_1          ||
      esp32_board == ESP32_LILYGO_T_TWR_V2_0   ||
      esp32_board == ESP32_HELTEC_TRACKER      ||
      esp32_board == ESP32_S3_DEVKIT)
  {
    button_1.check();
  }
}

static void ESP32_Button_fini()
{
  if (esp32_board == ESP32_S2_T8_V1_1        ||
      esp32_board == ESP32_LILYGO_T_TWR_V2_0 ||
      esp32_board == ESP32_HELTEC_TRACKER    ||
      esp32_board == ESP32_S3_DEVKIT) {
    int button_pin = esp32_board == ESP32_S2_T8_V1_1 ? SOC_GPIO_PIN_T8_S2_BUTTON :
                     esp32_board == ESP32_LILYGO_T_TWR_V2_0?
                     SOC_GPIO_PIN_TWR2_ENC_BUTTON : SOC_GPIO_PIN_S3_BUTTON;
    while (digitalRead(button_pin) == LOW);
  }
}

#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)

#define USB_TX_FIFO_SIZE (MAX_TRACKING_OBJECTS * 65 + 75 + 75 + 42 + 20)
#define USB_RX_FIFO_SIZE (256)

#if defined(USE_USB_HOST)

#include <cp210x_usb.hpp>
#include <ftdi_usb.hpp>
#include <ch34x_usb.hpp>

using namespace esp_usb;

#define USB_MAX_WRITE_CHUNK_SIZE    64

#define USB_HOST_PRIORITY           20

#undef  TAG
#define TAG "USB-CDC"

cbuf *USB_RX_FIFO, *USB_TX_FIFO;

// CDC-ACM driver object
typedef struct {
    usb_host_client_handle_t cdc_acm_client_hdl;        /*!< USB Host handle reused for all CDC-ACM devices in the system */
    SemaphoreHandle_t open_close_mutex;
    EventGroupHandle_t event_group;
    cdc_acm_new_dev_callback_t new_dev_cb;
    SLIST_HEAD(list_dev, cdc_dev_s) cdc_devices_list;   /*!< List of open pseudo devices */
} cdc_acm_obj_t;

extern cdc_acm_obj_t *p_cdc_acm_obj;

ESP32_USBSerial_device_t ESP32_USB_Serial = {
    .connected = false,
    .index = 0,
};

CdcAcmDevice *cdc = new CdcAcmDevice();

enum {
    USBSER_TYPE_CDC,
    USBSER_TYPE_CP210X,
    USBSER_TYPE_FTDI,
    USBSER_TYPE_CH34X,
};

const USB_Device_List_t supported_USB_devices[] = {
  { 0x0483, 0x5740, USBSER_TYPE_CDC, SOFTRF_MODEL_DONGLE, "Dongle" /* or Bracelet */, "Edition" },
  { 0x239A, 0x8029, USBSER_TYPE_CDC, SOFTRF_MODEL_BADGE, "Badge", "Edition" },
  { 0x2341, 0x804d, USBSER_TYPE_CDC, SOFTRF_MODEL_ACADEMY, "Academy", "Edition" },
  { 0x2886, 0x802f, USBSER_TYPE_CDC, SOFTRF_MODEL_ACADEMY, "Academy", "Edition" },
  { 0x1d50, 0x6089, USBSER_TYPE_CDC, SOFTRF_MODEL_ES, "ES", "Edition" },
  { 0x2e8a, 0x000a, USBSER_TYPE_CDC, SOFTRF_MODEL_LEGO, "Lego", "Edition" },
  { 0x2e8a, 0xf00a, USBSER_TYPE_CDC, SOFTRF_MODEL_LEGO, "Lego", "Edition" },
  { 0x1A86, 0x55D4, USBSER_TYPE_CDC, SOFTRF_MODEL_PRIME_MK2, "CH9102", "device" },
  { 0x303a, 0x8133, USBSER_TYPE_CDC, SOFTRF_MODEL_PRIME_MK3, "Prime 3", "Edition" },
  { 0x15ba, 0x0044, USBSER_TYPE_CDC, SOFTRF_MODEL_BALKAN, "Balkan", "Edition" },
  { 0x303a, 0x8132, USBSER_TYPE_CDC, SOFTRF_MODEL_STANDALONE, "Standalone", "Edition" },
  { 0x10c4, 0xea60, USBSER_TYPE_CP210X, SOFTRF_MODEL_UNKNOWN, "CP210X", "device" },
  { 0x0403, 0x6001, USBSER_TYPE_FTDI, SOFTRF_MODEL_UNKNOWN, "FT232", "device" },
  { 0x1a86, 0x7523, USBSER_TYPE_CH34X, SOFTRF_MODEL_UNKNOWN, "CH340", "device" },
};

enum {
  SOFTRF_DEVICE_COUNT =
      sizeof(supported_USB_devices) / sizeof(supported_USB_devices[0])
};

static void handle_rx(uint8_t *data, size_t data_len, void *arg)
{
//    ESP_LOGI(TAG, "Data received");
//    ESP_LOG_BUFFER_HEXDUMP(TAG, data, data_len, ESP_LOG_INFO);
      if (data_len > 0) {
        USB_RX_FIFO->write((char *) data,
                     USB_RX_FIFO->room() > data_len ?
                     data_len : USB_RX_FIFO->room());
      }
}

void usb_lib_task(void *arg)
{
    while (1) {
        //Start handling system events
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGI(TAG, "All clients deregistered");
            /*ESP_ERROR_CHECK*/(usb_host_device_free_all());
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            break;
        }
    }

    vTaskDelete(NULL);
}

static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    switch (event->type) {
        case CDC_ACM_HOST_ERROR:
            ESP_LOGE(TAG, "CDC-ACM error has occurred, err_no = %d", event->data.error);
            break;
        case CDC_ACM_HOST_DEVICE_DISCONNECTED:
            ESP_LOGI(TAG, "Device suddenly disconnected");
//            xSemaphoreGive(device_disconnected_sem);
#if 0
            if (ESP32_USB_Serial.device) {
              ESP32_USB_Serial.device->close();
              ESP32_USB_Serial.device = NULL;
            }
            usb_host_device_free_all();
#endif
            ESP32_USB_Serial.connected = false;
            break;
        case CDC_ACM_HOST_SERIAL_STATE:
            ESP_LOGI(TAG, "serial state notif 0x%04X", event->data.serial_state.val);
            break;
        case CDC_ACM_HOST_NETWORK_CONNECTION:
        default: break;
    }
}

static void ESP32SX_USB_setup()
{
    USB_RX_FIFO = new cbuf(USB_RX_FIFO_SIZE);
    USB_TX_FIFO = new cbuf(USB_TX_FIFO_SIZE);

    //Install USB Host driver. Should only be called once in entire application
    ESP_LOGI(TAG, "Installing USB Host");
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    // Create a task that will handle USB library events
    xTaskCreate(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), USB_HOST_PRIORITY, NULL);

    ESP_LOGI(TAG, "Installing CDC-ACM driver");
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));
}

static void ESP32SX_USB_loop()
{
    if (!ESP32_USB_Serial.connected) {
        ESP_LOGD(TAG, "Checking list of connected USB devices");
        uint8_t dev_addr_list[10];
        int num_of_devices;
        ESP_ERROR_CHECK(usb_host_device_addr_list_fill(sizeof(dev_addr_list), dev_addr_list, &num_of_devices));

        // Go through device address list and find the one we are looking for
        for (int i = 0; i < num_of_devices; i++) {
            usb_device_handle_t current_device;
            // Open USB device
            if (usb_host_device_open(p_cdc_acm_obj->cdc_acm_client_hdl, dev_addr_list[i], &current_device) != ESP_OK) {
                continue; // In case we failed to open this device, continue with next one in the list
            }
            assert(current_device);
            const usb_device_desc_t *device_desc;
            ESP_ERROR_CHECK(usb_host_get_device_descriptor(current_device, &device_desc));

            uint16_t vid = device_desc->idVendor;
            uint16_t pid = device_desc->idProduct;
            uint8_t dev_type;

            usb_host_device_close(p_cdc_acm_obj->cdc_acm_client_hdl, current_device);

            ESP_LOGI(TAG, "USB device detected, VID: %X, PID: %X", vid, pid);

            int j;
            for (j = 0; j < SOFTRF_DEVICE_COUNT; j++) {
              if (vid == supported_USB_devices[j].vid &&
                  pid == supported_USB_devices[j].pid) {
                dev_type = supported_USB_devices[j].type;
                break;
              }
            }

            if (j < SOFTRF_DEVICE_COUNT) {
              const cdc_acm_host_device_config_t dev_config = {
                  .connection_timeout_ms = 5000,
                  .out_buffer_size = 64,
                  .event_cb = handle_event /* NULL */,
                  .data_cb = handle_rx,
                  .user_arg = NULL,
              };

              CdcAcmDevice *vcp;

              cdc_acm_line_coding_t line_coding = {
                  .dwDTERate = SERIAL_OUT_BR,
                  .bCharFormat = 0,
                  .bParityType = 0,
                  .bDataBits = 8,
              };

              switch (dev_type)
              {
              case USBSER_TYPE_CDC:
                try {
                    ESP_LOGI(TAG, "Opening CDC ACM device 0x%04X:0x%04X", vid, pid);
                    cdc->open(vid, pid, 0, &dev_config);
                }

                catch (esp_err_t err) {
                    ESP_LOGE(TAG, "The required device was not opened.\nExiting...");
                    continue;
                }
                vcp = cdc;
                break;

              case USBSER_TYPE_CP210X:
                try {
                    ESP_LOGI(TAG, "Opening CP210X device");
                    vcp = CP210x::open_cp210x(pid, &dev_config);
                }

                catch (esp_err_t err) {
                    ESP_LOGE(TAG, "The required device was not opened.\nExiting...");
                    continue;
                }
                break;

              case USBSER_TYPE_FTDI:
                try {
                    ESP_LOGI(TAG, "Opening FT232 device");
                    vcp = FT23x::open_ftdi(pid, &dev_config);
                }

                catch (esp_err_t err) {
                    ESP_LOGE(TAG, "The required device was not opened.\nExiting...");
                    continue;
                }
                break;

              case USBSER_TYPE_CH34X:
                try {
                    ESP_LOGI(TAG, "Opening CH340 device");
                    vcp = CH34x::open_ch34x(pid, &dev_config);
                }

                catch (esp_err_t err) {
                    ESP_LOGE(TAG, "The required device was not opened.\nExiting...");
                    continue;
                }
                break;
              }

              ESP_ERROR_CHECK(vcp->line_coding_set(&line_coding));
              ESP_LOGI(TAG, "Line Set: Rate: %d, Stop bits: %d, Parity: %d, Databits: %d", line_coding.dwDTERate,
                       line_coding.bCharFormat, line_coding.bParityType, line_coding.bDataBits);

              ESP_ERROR_CHECK(vcp->set_control_line_state(true, true));

              ESP32_USB_Serial.connected = true;
              ESP32_USB_Serial.device = vcp;
              ESP32_USB_Serial.index = j;

            } else {
              ESP_LOGI(TAG, "USB device VID: %X, PID: %X is not supported", vid, pid);
            }
        }
    } else {
#if 0
        uint8_t dev_addr_list[10];
        int num_of_devices;
        ESP_ERROR_CHECK(usb_host_device_addr_list_fill(sizeof(dev_addr_list), dev_addr_list, &num_of_devices));
        if (num_of_devices == 0) {
          ESP_LOGI(TAG, "Closing USB device 0x%04X:0x%04X",
                   supported_USB_devices[ESP32_USB_Serial.index].vid,
                   supported_USB_devices[ESP32_USB_Serial.index].pid);
          if (ESP32_USB_Serial.device) {
            ESP32_USB_Serial.device->close();
            ESP32_USB_Serial.device = NULL;
          }
          ESP32_USB_Serial.connected = false;
          USB_TX_FIFO->flush();
          USB_RX_FIFO->flush();
        }
        else
#endif
        {
          uint8_t chunk[USB_MAX_WRITE_CHUNK_SIZE];
          size_t size = (USB_TX_FIFO->available() < USB_MAX_WRITE_CHUNK_SIZE ?
                         USB_TX_FIFO->available() : USB_MAX_WRITE_CHUNK_SIZE);

          if (size > 0) {
            USB_TX_FIFO->read((char *) chunk, size);
            ESP32_USB_Serial.device->tx_blocking(chunk, size);
          }
        }
    }
}

static void ESP32SX_USB_fini()
{
    if (ESP32_USB_Serial.device) {
      ESP32_USB_Serial.device->close();
    }

    vTaskDelay(100);
    ESP_ERROR_CHECK(cdc_acm_host_uninstall());
    vTaskDelay(100);
    ESP_ERROR_CHECK(usb_host_uninstall());

    delete(USB_RX_FIFO);
    delete(USB_TX_FIFO);
}

static int ESP32SX_USB_available()
{
  int rval = 0;

  rval = USB_RX_FIFO->available();

  return rval;
}

static int ESP32SX_USB_read()
{
  int rval = -1;

  rval = USB_RX_FIFO->read();

  return rval;
}

static size_t ESP32SX_USB_write(const uint8_t *buffer, size_t size)
{
  size_t rval = size;

  rval = USB_TX_FIFO->write((char *) buffer,
                      (USB_TX_FIFO->room() > size ? size : USB_TX_FIFO->room()));

  return rval;
}

#elif ARDUINO_USB_CDC_ON_BOOT

#define USE_ASYNC_USB_OUTPUT
#define USBSerial                Serial

#if !ARDUINO_USB_MODE && defined(USE_ASYNC_USB_OUTPUT)
#define USB_MAX_WRITE_CHUNK_SIZE CONFIG_TINYUSB_CDC_TX_BUFSIZE

cbuf *USB_TX_FIFO;
#endif /* USE_ASYNC_USB_OUTPUT */

static void ESP32SX_USB_setup()
{
  USBSerial.setRxBufferSize(USB_RX_FIFO_SIZE);
#if ARDUINO_USB_MODE
  /* native CDC (HWCDC) */
  USBSerial.setTxBufferSize(USB_TX_FIFO_SIZE);
#elif defined(USE_ASYNC_USB_OUTPUT)
  USB_TX_FIFO = new cbuf(USB_TX_FIFO_SIZE);
#endif /* ARDUINO_USB_MODE */
}

static void ESP32SX_USB_loop()
{
#if !ARDUINO_USB_MODE && defined(USE_ASYNC_USB_OUTPUT)
  if (USBSerial)
  {
    uint8_t chunk[USB_MAX_WRITE_CHUNK_SIZE];

    size_t size = USBSerial.availableForWrite();
    size = (size > USB_MAX_WRITE_CHUNK_SIZE ? USB_MAX_WRITE_CHUNK_SIZE : size);
    size = (USB_TX_FIFO->available() < size ? USB_TX_FIFO->available() : size);

    USB_TX_FIFO->read((char *) chunk, size);
    USBSerial.write(chunk, size);
  }
#endif /* USE_ASYNC_USB_OUTPUT */
}

static void ESP32SX_USB_fini()
{
#if !ARDUINO_USB_MODE && defined(USE_ASYNC_USB_OUTPUT)
  delete(USB_TX_FIFO);
#endif /* USE_ASYNC_USB_OUTPUT */
}

static int ESP32SX_USB_available()
{
  int rval = 0;

  if (USBSerial) {
    rval = USBSerial.available();
  }

  return rval;
}

static int ESP32SX_USB_read()
{
  int rval = -1;

  if (USBSerial) {
    rval = USBSerial.read();
  }

  return rval;
}

static size_t ESP32SX_USB_write(const uint8_t *buffer, size_t size)
{
  size_t rval = size;

#if ARDUINO_USB_MODE
  /* Espressif native CDC (HWCDC) */
  if (USBSerial && (size < USBSerial.availableForWrite())) {
    rval = USBSerial.write(buffer, size);
  }
#else
  /* TinyUSB CDC (USBCDC) */
#if defined(USE_ASYNC_USB_OUTPUT)
  rval = USB_TX_FIFO->write((char *) buffer,
                      (USB_TX_FIFO->room() > size ? size : USB_TX_FIFO->room()));
#else
  if (USBSerial) {
    rval = USBSerial.write(buffer, size);
  }
#endif /* USE_ASYNC_USB_OUTPUT */
#endif /* ARDUINO_USB_MODE */

  return rval;
}
#endif /* USE_USB_HOST || ARDUINO_USB_CDC_ON_BOOT */

#if ARDUINO_USB_CDC_ON_BOOT || defined(USE_USB_HOST)
IODev_ops_t ESP32SX_USBSerial_ops = {
  "ESP32SX USB",
  ESP32SX_USB_setup,
  ESP32SX_USB_loop,
  ESP32SX_USB_fini,
  ESP32SX_USB_available,
  ESP32SX_USB_read,
  ESP32SX_USB_write
};
#endif /* USE_USB_HOST || ARDUINO_USB_CDC_ON_BOOT */
#endif /* CONFIG_IDF_TARGET_ESP32S2 */

#if defined(CONFIG_IDF_TARGET_ESP32S3)
static bool ESP32_ADB_setup()
{
  if (FATFS_is_mounted) {
    const char fileName[] = "/Aircrafts/ogn.cdb";

    if (ucdb.open(fileName) != CDB_OK) {
      Serial.print("Invalid CDB: ");
      Serial.println(fileName);
    } else {
      ADB_is_open = true;
    }
  }

  return ADB_is_open;
}

static bool ESP32_ADB_fini()
{
  if (ADB_is_open) {
    ucdb.close();
    ADB_is_open = false;
  }

  return !ADB_is_open;
}

/*
 * One aircraft CDB (20000+ records) query takes:
 * 1)     FOUND : xxx milliseconds
 * 2) NOT FOUND : xxx milliseconds
 */
static bool ESP32_ADB_query(uint8_t type, uint32_t id, char *buf, size_t size)
{
  char key[8];
  char out[64];
  uint8_t tokens[3] = { 0 };
  cdbResult rt;
  int c, i = 0, token_cnt = 0;
  bool rval = false;

  if (!ADB_is_open) {
    return rval;
  }

  snprintf(key, sizeof(key),"%06X", id);

  rt = ucdb.findKey(key, strlen(key));

  switch (rt) {
    case KEY_FOUND:
      while ((c = ucdb.readValue()) != -1 && i < (sizeof(out) - 1)) {
        if (c == '|') {
          if (token_cnt < (sizeof(tokens) - 1)) {
            token_cnt++;
            tokens[token_cnt] = i+1;
          }
          c = 0;
        }
        out[i++] = (char) c;
      }
      out[i] = 0;

      switch (ui->idpref)
      {
      case ID_TAIL:
        snprintf(buf, size, "CN: %s",
          strlen(out + tokens[2]) ? out + tokens[2] : "N/A");
        break;
      case ID_MAM:
        snprintf(buf, size, "%s",
          strlen(out + tokens[0]) ? out + tokens[0] : "Unknown");
        break;
      case ID_REG:
      default:
        snprintf(buf, size, "%s",
          strlen(out + tokens[1]) ? out + tokens[1] : "REG: N/A");
        break;
      }

      rval = true;
      break;

    case KEY_NOT_FOUND:
    default:
      break;
  }

  return rval;
}

DB_ops_t ESP32_ADB_ops = {
  ESP32_ADB_setup,
  ESP32_ADB_fini,
  ESP32_ADB_query
};
#endif /* CONFIG_IDF_TARGET_ESP32S3 */

const SoC_ops_t ESP32_ops = {
#if defined(CONFIG_IDF_TARGET_ESP32)
  SOC_ESP32,
  "ESP32",
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  SOC_ESP32S2,
  "ESP32-S2",
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  SOC_ESP32S3,
  "ESP32-S3",
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  SOC_ESP32C3,
  "ESP32-C3",
#else
#error "This ESP32 family build variant is not supported!"
#endif /* CONFIG_IDF_TARGET_ESP32-S2-S3-C3 */
  ESP32_setup,
  ESP32_post_init,
  ESP32_loop,
  ESP32_fini,
  ESP32_reset,
  ESP32_getChipId,
  ESP32_getResetInfoPtr,
  ESP32_getResetInfo,
  ESP32_getResetReason,
  ESP32_getFreeHeap,
  ESP32_random,
  ESP32_maxSketchSpace,
  ESP32_WiFi_set_param,
  ESP32_WiFi_transmit_UDP,
  ESP32_WiFiUDP_stopAll,
  ESP32_WiFi_hostname,
  ESP32_WiFi_clients_count,
  ESP32_EEPROM_begin,
  ESP32_EEPROM_extension,
  ESP32_SPI_begin,
  ESP32_swSer_begin,
  ESP32_swSer_enableRx,
#if !defined(CONFIG_IDF_TARGET_ESP32S2)
  &ESP32_Bluetooth_ops,
#else
  NULL,
#endif /* CONFIG_IDF_TARGET_ESP32S2 */
#if (defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)) && \
   (ARDUINO_USB_CDC_ON_BOOT || defined(USE_USB_HOST))
  &ESP32SX_USBSerial_ops,
#else
  NULL,
#endif /* USE_USB_HOST */
  NULL,
  ESP32_Display_setup,
  ESP32_Display_loop,
  ESP32_Display_fini,
  ESP32_Battery_setup,
  ESP32_Battery_param,
  ESP32_GNSS_PPS_Interrupt_handler,
  ESP32_get_PPS_TimeMarker,
  ESP32_Baro_setup,
  NULL,
  NULL,
  ESP32_WDT_setup,
  ESP32_WDT_fini,
  ESP32_Button_setup,
  ESP32_Button_loop,
  ESP32_Button_fini,
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  &ESP32_ADB_ops
#else
  NULL
#endif /* CONFIG_IDF_TARGET_ESP32S3 */
};

#endif /* ESP32 */
