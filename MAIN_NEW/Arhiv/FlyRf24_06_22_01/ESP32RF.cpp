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
  if (ret != ESP_OK) 
  {
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
  }
  else 
  {
#if defined(CONFIG_IDF_TARGET_ESP32)
    uint32_t chip_ver = REG_GET_FIELD(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_VER_PKG);
    uint32_t pkg_ver  = chip_ver & 0x7;
    if (pkg_ver == EFUSE_RD_CHIP_VER_PKG_ESP32PICOD4)
    {
      esp32_board    = ESP32_TTGO_V2_OLED;
      lmic_pins.rst  = SOC_GPIO_PIN_TBEAM_RF_RST_V05;
      lmic_pins.busy = SOC_GPIO_PIN_TBEAM_RF_BUSY_V08;
    }
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


  Serial.begin(SERIAL_OUT_BR, SERIAL_OUT_BITS);

//#endif /* ARDUINO_USB_CDC_ON_BOOT && (CONFIG_IDF_TARGET_ESP32S2 || S3) */
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
    case NMEA_OFF        :
    default              :  Serial.println(F("NULL"));      break;
  }

  /*Serial.print(F("GDL90  - "));
  switch (settings->gdl90)
  {
    case GDL90_UART      :  Serial.println(F("UART"));      break;
    case GDL90_USB       :  Serial.println(F("USB CDC"));   break;
    case GDL90_UDP       :  Serial.println(F("UDP"));       break;
    default              :  Serial.println(F("NULL"));      break;
  }*/

  Serial.print(F("D1090  - "));
  switch (settings->d1090)
  {
    case D1090_UART      :  Serial.println(F("UART"));      break;
    case D1090_USB       :  Serial.println(F("USB CDC"));   break;
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
  }
//  else if (esp32_board == ESP32_S2_T8_V1_1) 
//{
//    pinMode(SOC_GPIO_PIN_T8_S2_PWR_EN, INPUT);
//
//#if !defined(CONFIG_IDF_TARGET_ESP32C3)
//    esp_sleep_enable_ext1_wakeup(1ULL << SOC_GPIO_PIN_T8_S2_BUTTON,
//                                 ESP_EXT1_WAKEUP_ALL_LOW);
//#endif /* CONFIG_IDF_TARGET_ESP32C3 */
//
//  }
  else if (esp32_board == ESP32_LILYGO_T_TWR_V2_0 || esp32_board == ESP32_LILYGO_T_TWR_V2_1) 
  {
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
  }
  else 
  {
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
    }
    else 
    {
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

    if (r == 0x08 || r == 0x00 || r == 0x0C) 
    {
       // rval = DISPLAY_OLED_1_3;  // SH1106
    }
    else if (r == 0x03 || r == 0x04 || r == 0x06 || r == 0x07) 
    {
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
      esp32_board != ESP32_S2_T8_V1_1 /*  &&
      esp32_board != ESP32_HELTEC_TRACKER*/) 
  {

#if defined(USE_OLED)
    bool has_oled = false;

     if (GPIO_21_22_are_busy) 
    {
      if (hw_info.model == SOFTRF_MODEL_PRIME_MK2 && hw_info.revision >= 8) {
        Wire1.begin(TTGO_V2_OLED_PIN_SDA , TTGO_V2_OLED_PIN_SCL);
        Wire1.beginTransmission(SSD1306_OLED_I2C_ADDR);
        has_oled = (Wire1.endTransmission() == 0);
        if (has_oled) {
#if 0
         /* rval = ESP32_OLED_ident(&Wire1);
          if (rval == DISPLAY_OLED_1_3) {
            u8x8 = &u8x8_1_3;
          } else {
            u8x8 = &u8x8_ttgo;
          }*/
#else
          u8x8 = &u8x8_ttgo;
          rval = DISPLAY_OLED_TTGO;
#endif
        }
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
        /*if (rval == DISPLAY_OLED_1_3) {
          u8x8 = &u8x8_1_3;
        } else {
          u8x8 = &u8x8_ttgo;
        }*/
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
    }

    if (u8x8) 
    {
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
  if (hw_info.model == SOFTRF_MODEL_SKYWATCH) 
  {

    return false;

  }
  else if (hw_info.model != SOFTRF_MODEL_PRIME_MK2) 
  {

    if ((hw_info.rf != RF_IC_SX1276 && hw_info.rf != RF_IC_SX1262) ||  RF_SX12XX_RST_is_connected) 
    {
      return false;
    }

#if DEBUG
    Serial.println(F("INFO: RESET pin of SX12xx radio is not connected to MCU."));
#endif

   // Wire.setPins(SOC_GPIO_PIN_SDA, SOC_GPIO_PIN_SCL);

  }
  else
  {

    if (hw_info.revision == 2 && RF_SX12XX_RST_is_connected) 
    {
      hw_info.revision = 5;
    }

    /* Start from 1st I2C bus */
   // Wire.setPins(SOC_GPIO_PIN_TBEAM_SDA, SOC_GPIO_PIN_TBEAM_SCL);
    if (Baro_probe())
      return true;

    WIRE_FINI(Wire);

    if (hw_info.revision == 2)
      return false;

#if !defined(ENABLE_AHRS)
    /* Try out OLED I2C bus */
    Wire.begin(TTGO_V2_OLED_PIN_SDA, TTGO_V2_OLED_PIN_SCL);
    if (hw_info.model == SOFTRF_MODEL_PRIME_MK2 && hw_info.revision >= 8) 
    {
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
  int button_pin = SOC_GPIO_PIN_TBEAM_V08_BUTTON;//SOC_GPIO_PIN_TBEAM_V05_BUTTON;
}

static void ESP32_Button_loop()
{
  if (esp32_board == ESP32_TTGO_T_BEAM         ||
      esp32_board == ESP32_TTGO_T_BEAM_SUPREME ||
      esp32_board == ESP32_S2_T8_V1_1          ||
      esp32_board == ESP32_LILYGO_T_TWR_V2_0   ||
      esp32_board == ESP32_S3_DEVKIT)
  {
    button_1.check();
  }
}

static void ESP32_Button_fini()
{
  //if (esp32_board == ESP32_S2_T8_V1_1        ||
  //    esp32_board == ESP32_LILYGO_T_TWR_V2_0 ||
  // /*   esp32_board == ESP32_HELTEC_TRACKER    ||*/
  //    esp32_board == ESP32_S3_DEVKIT) 
  //{
  //    int button_pin = esp32_board == ESP32_S2_T8_V1_1 ? SOC_GPIO_PIN_T8_S2_BUTTON :
  //        esp32_board == ESP32_LILYGO_T_TWR_V2_0 ?
  //        SOC_GPIO_PIN_TWR2_ENC_BUTTON : SOC_GPIO_PIN_TWR2_ENC_BUTTON;/*SOC_GPIO_PIN_S3_BUTTON;*/
  //  while (digitalRead(button_pin) == LOW);
  //}
}

const SoC_ops_t ESP32_ops = {
  SOC_ESP32,
  "ESP32",
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
  & ESP32_Bluetooth_ops,
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
  NULL
};

#endif /* ESP32 */
