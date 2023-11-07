#include "MODULE_ESP32.h"
#include "Configuration_ESP32.h"

//--------------------------------------------------------------------------------------------------------------------------------------
#include "sdkconfig.h"


#include <SPI.h>
#include <esp_err.h>
#include <esp_wifi.h>
#if !defined(CONFIG_IDF_TARGET_ESP32S2)
#include <esp_bt.h>
//#include <BLEDevice.h>
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
#include "SoftRF.h"
#include "SoC.h"
#include "EEPROMHelper.h"
#include "NMEA.h"
#include "GDL90.h"
#include "D1090.h"

#if defined(USE_TFT)
#include <TFT_eSPI.h>
#endif /* USE_TFT */

#ifdef HAS_DISPLAY
//#include <U8g2lib.h>
//
//#ifndef DISPLAY_MODEL
//#define DISPLAY_MODEL U8G2_SSD1306_128X64_NONAME_F_HW_I2C
//#endif
//
//DISPLAY_MODEL* u8g2 = nullptr;
#endif

#ifndef OLED_WIRE_PORT
#define OLED_WIRE_PORT Wire
#endif


//--------------------------------------------------------------------------------------------------------------------------------------
static int esp32_board = ESP32_DEVKIT; /* default */
static size_t ESP32_Min_AppPart_Size = 0;

static portMUX_TYPE GNSS_PPS_mutex = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE PMU_mutex = portMUX_INITIALIZER_UNLOCKED;
volatile bool PMU_Irq = false;

static bool GPIO_21_22_are_busy = false;

/* Здесь хранится адрес модуля*/
static union 
{
	uint8_t efuse_mac[6];
	uint64_t chipmacid;
};

static bool TFT_display_frontpage = false;
static uint32_t prev_tx_packets_counter = 0;
static uint32_t prev_rx_packets_counter = 0;
extern uint32_t tx_packets_counter, rx_packets_counter;
extern bool loopTaskWDTEnabled;

static uint32_t ESP32_getFlashId()
{
	return g_rom_flashchip.device_id;
}


/* !! Оставим на потом */
//// SX12xx pin mapping
//lmic_pinmap lmic_pins = {
//    .nss = SOC_GPIO_PIN_SS,
//    .txe = LMIC_UNUSED_PIN,
//    .rxe = LMIC_UNUSED_PIN,
//    .rst = SOC_GPIO_PIN_RST,
//    .dio = {LMIC_UNUSED_PIN, LMIC_UNUSED_PIN, LMIC_UNUSED_PIN},
//    .busy = SOC_GPIO_PIN_TXE,
//    .tcxo = LMIC_UNUSED_PIN,
//};









//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//ModuleESP32
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------


ModuleESP32 esp32sys;

ModuleESP32::ModuleESP32()
{


}

byte ModuleESP32::Setup()
{
  // настройка модуля тут

    byte id = SOC_NONE;

#if !defined(SOFTRF_ADDRESS) // Если не установлен адрес модуля

    esp_err_t ret = ESP_OK;
    uint8_t null_mac[6] = { 0 };

    ret = esp_efuse_mac_get_custom(efuse_mac);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Get base MAC address from BLK3 of EFUSE error (%s)", esp_err_to_name(ret));
        /*  Если возникает ошибка пользовательского базового MAC-адреса, разработчик приложения может решить, что делать:
          * прервать или использовать базовый MAC-адрес по умолчанию, который хранится в BLK0 EFUSE, выполнив
          * ничего.
         */

        ESP_LOGI(TAG, "Use base MAC address which is stored in BLK0 of EFUSE"); //Используйте базовый MAC-адрес, который хранится в BLK0 файла EFUSE.
        chipmacid = ESP.getEfuseMac(); // Получить МАС адрес модуля
    }
    else
    {
        if (memcmp(efuse_mac, null_mac, 6) == 0) // 
        {
            ESP_LOGI(TAG, "Use base MAC address which is stored in BLK0 of EFUSE"); // Используйте базовый MAC-адрес, который хранится в BLK0 файла EFUSE.
            chipmacid = ESP.getEfuseMac();  // Получить МАС адрес модуля
        }
    }
#endif /* SOFTRF_ADDRESS */

    //#if ESP32_DISABLE_BROWNOUT_DETECTOR
    //  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    //#endif

     /*Получите размер флэш - чипа, указанный в заголовке двоичного изображения.
      *Это значение не обязательно соответствует реальному размеру флэш - памяти.
      *return размер флэш - чипа в байтах*/
    size_t flash_size = spi_flash_get_chip_size(); // Получить размер памяти чипа
    size_t min_app_size = flash_size;              // 

    esp_partition_iterator_t it;
    const esp_partition_t* part;

    ///**
    //* @brief Найти раздел по одному или нескольким параметрам
    //* @param type Тип раздела, одно из значений esp_partition_type_t или 8-битное целое число без знака.
    //* Чтобы найти все разделы независимо от типа, используйте ESP_PARTITION_TYPE_ANY и установите
    //* аргумент подтипа для ESP_PARTITION_SUBTYPE_ANY.
    //* @param subtype Подтип раздела, одно из значений esp_partition_subtype_t или 8-битное целое число без знака.
    //* Чтобы найти все разделы данного типа, используйте ESP_PARTITION_SUBTYPE_ANY.
    //* @param label (необязательно) Метка раздела. Установите это значение, если ищете
    //* для раздела с определенным именем. В противном случае передайте NULL.
    //*
    //* @return итератор, который можно использовать для перечисления всех найденных разделов,
    //* или NULL, если разделы не найдены.
    //* Итератор, полученный с помощью этой функции, необходимо освободить.
    //* использование esp_partition_iterator_release, когда он больше не используется.
    //*/

    it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (it)
    {
        do {
            part = esp_partition_get(it);
            if (part->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY)
            {
                continue;
            }
            if (part->size < min_app_size)
            {
                min_app_size = part->size;
            }
        } while (it = esp_partition_next(it));

        if (it) esp_partition_iterator_release(it);
    }

    if (min_app_size && (min_app_size != flash_size))
    {
        ESP32_Min_AppPart_Size = min_app_size;
    }

    if (psramFound())
    {
        /*Определение типа платформы. Нам это не нужно, но оставим */
        uint32_t flash_id = ESP32_getFlashId() & 0xFF;  // Ищем флеш память

        Serial.print("flash_id - ");
        Serial.println(flash_id,HEX);
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

        //switch (flash_id)
        //{
        //case MakeFlashId(GIGADEVICE_ID, GIGADEVICE_GD25LQ32):
        //    /* ESP32-WROVER module with ESP32-NODEMCU-ADAPTER */
        //    hw_info.model = SOFTRF_MODEL_STANDALONE;
        //    break;
        //case MakeFlashId(WINBOND_NEX_ID, WINBOND_NEX_W25Q128_V):
        //    hw_info.model = SOFTRF_MODEL_SKYWATCH;
        //    break;
        //#if defined(CONFIG_IDF_TARGET_ESP32)
        //case MakeFlashId(WINBOND_NEX_ID, WINBOND_NEX_W25Q32_V):
        //case MakeFlashId(BOYA_ID, BOYA_BY25Q32AL):
        //default:
        //    hw_info.model = SOFTRF_MODEL_PRIME_MK2;
        //    break;
        //}
    }
    else
    {
        //#if defined(CONFIG_IDF_TARGET_ESP32)
        //uint32_t chip_ver = REG_GET_FIELD(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_VER_PKG);
        //uint32_t pkg_ver = chip_ver & 0x7;
        //if (pkg_ver == EFUSE_RD_CHIP_VER_PKG_ESP32PICOD4)
        //{
        //    esp32_board = ESP32_TTGO_V2_OLED;
        //    /*     lmic_pins.rst  = SOC_GPIO_PIN_TBEAM_RF_RST_V05;
        //         lmic_pins.busy = SOC_GPIO_PIN_TBEAM_RF_BUSY_V08;*/
        //}
        //#endif /* CONFIG_IDF_TARGET_ESP32 */
    }


    initBoard();
    initPMU();







    //if (hw_info.model == SOFTRF_MODEL_PRIME_MK2)
    //{
    //    esp32_board = ESP32_TTGO_T_BEAM;

    //    Wire1.begin(TTGO_V2_OLED_PIN_SDA, TTGO_V2_OLED_PIN_SCL);
    //    Wire1.beginTransmission(AXP192_SLAVE_ADDRESS);
    //    bool has_axp = (Wire1.endTransmission() == 0);

    //    bool has_axp192 = has_axp &&
    //        (axp_xxx.begin(Wire1, AXP192_SLAVE_ADDRESS) == AXP_PASS);

    //    if (has_axp192)
    //    {

    //        //hw_info.revision = 8;
    //        //hw_info.pmu = PMU_AXP192;

    //        //axp_xxx.setChgLEDMode(AXP20X_LED_LOW_LEVEL);

    //        //axp_xxx.setPowerOutPut(AXP192_LDO2,  AXP202_ON);
    //        //axp_xxx.setPowerOutPut(AXP192_LDO3,  AXP202_ON);
    //        //axp_xxx.setPowerOutPut(AXP192_DCDC1, AXP202_ON);
    //        //axp_xxx.setPowerOutPut(AXP192_DCDC2, AXP202_ON); // NC
    //        //axp_xxx.setPowerOutPut(AXP192_EXTEN, AXP202_ON);

    //        //axp_xxx.setDCDC1Voltage(3300); //       AXP192 power-on value: 3300
    //        //axp_xxx.setLDO2Voltage (3300); // LoRa, AXP192 power-on value: 3300
    //        //axp_xxx.setLDO3Voltage (3000); // GPS,  AXP192 power-on value: 2800

    //        ////pinMode(SOC_GPIO_PIN_TBEAM_V08_PMU_IRQ, INPUT /* INPUT_PULLUP */);

    //        //attachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_TBEAM_V08_PMU_IRQ),
    //        //                ESP32_PMU_Interrupt_handler, FALLING);

    //        //axp_xxx.enableIRQ(AXP202_PEK_LONGPRESS_IRQ | AXP202_PEK_SHORTPRESS_IRQ, true);
    //        //axp_xxx.clearIRQ();
    //    }
    //    else
    //    {
    //        bool has_axp2101 = has_axp && axp_2xxx.begin(Wire1,
    //            AXP2101_SLAVE_ADDRESS,
    //            TTGO_V2_OLED_PIN_SDA,
    //            TTGO_V2_OLED_PIN_SCL);
    //        if (has_axp2101)
    //        {

    //            // Set the minimum common working voltage of the PMU VBUS input,
    //            // below this value will turn off the PMU
    //            axp_2xxx.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);

    //            // Set the maximum current of the PMU VBUS input,
    //            // higher than this value will turn off the PMU
    //            axp_2xxx.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);

    //            // DCDC1 1500~3400mV, IMAX=2A
    //            axp_2xxx.setDC1Voltage(3300); // ESP32,  AXP2101 power-on value: 3300

    //            // ALDO 500~3500V, 100mV/step, IMAX=300mA
    //            axp_2xxx.setButtonBatteryChargeVoltage(3100); // GNSS battery

    //            axp_2xxx.setALDO2Voltage(3300); // LoRa, AXP2101 power-on value: 2800
    //            axp_2xxx.setALDO3Voltage(3300); // GPS,  AXP2101 power-on value: 3300

    //            // axp_2xxx.enableDC1();
    //            axp_2xxx.enableButtonBatteryCharge();

    //            axp_2xxx.enableALDO2();
    //            axp_2xxx.enableALDO3();

    //            axp_2xxx.setChargingLedMode(XPOWERS_CHG_LED_ON);

    //            pinMode(SOC_GPIO_PIN_TBEAM_V08_PMU_IRQ, INPUT /* INPUT_PULLUP */);

    //            attachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_TBEAM_V08_PMU_IRQ),
    //                ESP32_PMU_Interrupt_handler, FALLING);

    //            axp_2xxx.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    //            axp_2xxx.clearIrqStatus();

    //            axp_2xxx.enableIRQ(XPOWERS_AXP2101_PKEY_LONG_IRQ |
    //                XPOWERS_AXP2101_PKEY_SHORT_IRQ);

    //            hw_info.revision = 12;
    //            hw_info.pmu = PMU_AXP2101;
    //        }
    //        else
    //        {
    //            WIRE_FINI(Wire1);
    //            hw_info.revision = 2;
    //        }
    //    }

    //    lmic_pins.rst = SOC_GPIO_PIN_TBEAM_RF_RST_V05;
    //    lmic_pins.busy = SOC_GPIO_PIN_TBEAM_RF_BUSY_V08;

    //}

//#if ARDUINO_USB_CDC_ON_BOOT && \
//    (defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3))
//
//
//#else
//    Serial.begin(SERIAL_OUT_BR, SERIAL_OUT_BITS);
//#endif /* ARDUINO_USB_CDC_ON_BOOT && (CONFIG_IDF_TARGET_ESP32S2 || S3) */

return id;

}
//--------------------------------------------------------------------------------------------------------------------------------------
void ModuleESP32::Update(uint16_t dt)
{ 


}
//--------------------------------------------------------------------------------------------------------------------------------------
void ModuleESP32::ESP32_post_init()
{

    Serial.println();
    Serial.println(F("Data output device(s):"));

    Serial.print(F("NMEA   - "));
    switch (settings->nmea_out)
    {
        case NMEA_UART:       Serial.println(F("UART"));      break;
        case NMEA_USB:        Serial.println(F("USB CDC"));   break;
        case NMEA_UDP:        Serial.println(F("UDP"));       break;
        case NMEA_TCP:        Serial.println(F("TCP"));       break;
        case NMEA_BLUETOOTH:  Serial.println(F("Bluetooth")); break;
        case NMEA_OFF:
        default:  Serial.println(F("NULL"));      break;
    }

    Serial.print(F("GDL90  - "));
    switch (settings->gdl90)
    {
        case GDL90_UART:       Serial.println(F("UART"));      break;
        case GDL90_USB:        Serial.println(F("USB CDC"));   break;
        case GDL90_UDP:        Serial.println(F("UDP"));       break;
        case GDL90_BLUETOOTH:  Serial.println(F("Bluetooth")); break;
        case GDL90_OFF:
        default:  Serial.println(F("NULL"));      break;
    }

    Serial.print(F("D1090  - "));
    switch (settings->d1090)
    {
        case D1090_UART:       Serial.println(F("UART"));      break;
        case D1090_USB:        Serial.println(F("USB CDC"));   break;
        case D1090_BLUETOOTH:  Serial.println(F("Bluetooth")); break;
        case D1090_OFF:
        default:  Serial.println(F("NULL"));      break;
    }

    Serial.println();
    Serial.flush();

    switch (hw_info.display)
    {
    //#if defined(USE_OLED)
    //    case DISPLAY_OLED_TTGO:
    //    case DISPLAY_OLED_HELTEC:
    //    case DISPLAY_OLED_1_3:
    //        OLED_info1();
    //        break;
    //#endif /* USE_OLED */
    //    case DISPLAY_NONE:
        default:
        break;
    }
}

void ModuleESP32::ESP32_loop()
{
 /*   bool is_irq = false;
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
            }
            else {
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
            }
            else {
                axp_2xxx.setChargingLedMode(XPOWERS_CHG_LED_BLINK_1HZ);
            }
        }
        break;

    case PMU_NONE:
    default:
        break;
    }*/

}
//--------------------------------------------------------------------------------------------------------------------------------------

#if defined(HAS_PMU)
#include "XPowersLib.h"


XPowersLibInterface* PMU = NULL;

#ifndef PMU_WIRE_PORT
#define PMU_WIRE_PORT   Wire
#endif


bool pmuInterrupt;


void setPmuFlag()
{
    pmuInterrupt = true;
}


bool ModuleESP32::initPMU()
{
    if (!PMU) {
        PMU = new XPowersAXP2101(PMU_WIRE_PORT);
        if (!PMU->init())
        {
            Serial.println("Warning: Failed to find AXP2101 power management");
            delete PMU;
            PMU = NULL;
        }
        else
        {
            Serial.println("AXP2101 PMU init succeeded, using AXP2101 PMU");
        }
    }

    if (!PMU)
    {
        return false;
    }

    PMU->setChargingLedMode(XPOWERS_CHG_LED_BLINK_1HZ);

    pinMode(PMU_IRQ, INPUT_PULLUP);
    attachInterrupt(PMU_IRQ, setPmuFlag, FALLING);

    //if (PMU->getChipModel() == XPOWERS_AXP192)
    //{

    //    PMU->setProtectedChannel(XPOWERS_DCDC3);

    //    // lora
    //    PMU->setPowerChannelVoltage(XPOWERS_LDO2, 3300);
    //    // gps
    //    PMU->setPowerChannelVoltage(XPOWERS_LDO3, 3300);
    //    // oled
    //    PMU->setPowerChannelVoltage(XPOWERS_DCDC1, 3300);

    //    PMU->enablePowerOutput(XPOWERS_LDO2);
    //    PMU->enablePowerOutput(XPOWERS_LDO3);

    //    //protected oled power source
    //    PMU->setProtectedChannel(XPOWERS_DCDC1);
    //    //protected esp32 power source
    //    PMU->setProtectedChannel(XPOWERS_DCDC3);
    //    // enable oled power
    //    PMU->enablePowerOutput(XPOWERS_DCDC1);

    //    //disable not use channel
    //    PMU->disablePowerOutput(XPOWERS_DCDC2);

    //    PMU->disableIRQ(XPOWERS_AXP192_ALL_IRQ);

    //    PMU->enableIRQ(XPOWERS_AXP192_VBUS_REMOVE_IRQ |
    //        XPOWERS_AXP192_VBUS_INSERT_IRQ |
    //        XPOWERS_AXP192_BAT_CHG_DONE_IRQ |
    //        XPOWERS_AXP192_BAT_CHG_START_IRQ |
    //        XPOWERS_AXP192_BAT_REMOVE_IRQ |
    //        XPOWERS_AXP192_BAT_INSERT_IRQ |
    //        XPOWERS_AXP192_PKEY_SHORT_IRQ
    //    );

    //}
    //else 
    if (PMU->getChipModel() == XPOWERS_AXP2101)
    {

        //Unuse power channel
        PMU->disablePowerOutput(XPOWERS_DCDC2);
        PMU->disablePowerOutput(XPOWERS_DCDC3);
        PMU->disablePowerOutput(XPOWERS_DCDC4);
        PMU->disablePowerOutput(XPOWERS_DCDC5);
        // PMU->disablePowerOutput(XPOWERS_ALDO1);
        PMU->disablePowerOutput(XPOWERS_ALDO4);
        PMU->disablePowerOutput(XPOWERS_BLDO1);
        PMU->disablePowerOutput(XPOWERS_BLDO2);
        PMU->disablePowerOutput(XPOWERS_DLDO1);
        PMU->disablePowerOutput(XPOWERS_DLDO2);

        // GNSS RTC PowerVDD 3300mV
        PMU->setPowerChannelVoltage(XPOWERS_VBACKUP, 3300);
        PMU->enablePowerOutput(XPOWERS_VBACKUP);

        //ESP32 VDD 3300mV
        // ! No need to set, automatically open , Don't close it
        // PMU->setPowerChannelVoltage(XPOWERS_DCDC1, 3300);
        // PMU->setProtectedChannel(XPOWERS_DCDC1);
        PMU->setProtectedChannel(XPOWERS_DCDC1);

        // NEO-6M  батарейка ms621fe 3000mV
        PMU->setPowerChannelVoltage(XPOWERS_ALDO1, 3000);   // 
        PMU->enablePowerOutput(XPOWERS_ALDO1);

        // LoRa VDD 3300mV
        PMU->setPowerChannelVoltage(XPOWERS_ALDO2, 3300);
        PMU->enablePowerOutput(XPOWERS_ALDO2);

        //GNSS VDD 3300mV
        PMU->setPowerChannelVoltage(XPOWERS_ALDO3, 3300);
        PMU->enablePowerOutput(XPOWERS_ALDO3);

    }

    PMU->enableSystemVoltageMeasure();
    PMU->enableVbusVoltageMeasure();
    PMU->enableBattVoltageMeasure();
    // It is necessary to disable the detection function of the TS pin on the board
    // without the battery temperature detection function, otherwise it will cause abnormal charging
    PMU->disableTSPinMeasure();

    Serial.printf("=========================================\n");
    if (PMU->isChannelAvailable(XPOWERS_DCDC1)) {
        Serial.printf("DC1  : %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_DCDC1) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_DCDC1));
    }
    if (PMU->isChannelAvailable(XPOWERS_DCDC2)) {
        Serial.printf("DC2  : %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_DCDC2) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_DCDC2));
    }
    if (PMU->isChannelAvailable(XPOWERS_DCDC3)) {
        Serial.printf("DC3  : %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_DCDC3) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_DCDC3));
    }
    if (PMU->isChannelAvailable(XPOWERS_DCDC4)) {
        Serial.printf("DC4  : %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_DCDC4) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_DCDC4));
    }
    if (PMU->isChannelAvailable(XPOWERS_DCDC5)) {
        Serial.printf("DC5  : %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_DCDC5) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_DCDC5));
    }
    if (PMU->isChannelAvailable(XPOWERS_LDO2)) {
        Serial.printf("LDO2 : %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_LDO2) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_LDO2));
    }
    if (PMU->isChannelAvailable(XPOWERS_LDO3)) {
        Serial.printf("LDO3 : %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_LDO3) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_LDO3));
    }
    if (PMU->isChannelAvailable(XPOWERS_ALDO1)) {
        Serial.printf("ALDO1: %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_ALDO1) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_ALDO1));
    }
    if (PMU->isChannelAvailable(XPOWERS_ALDO2)) {
        Serial.printf("ALDO2: %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_ALDO2) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_ALDO2));
    }
    if (PMU->isChannelAvailable(XPOWERS_ALDO3)) {
        Serial.printf("ALDO3: %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_ALDO3) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_ALDO3));
    }
    if (PMU->isChannelAvailable(XPOWERS_ALDO4)) {
        Serial.printf("ALDO4: %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_ALDO4) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_ALDO4));
    }
    if (PMU->isChannelAvailable(XPOWERS_BLDO1)) {
        Serial.printf("BLDO1: %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_BLDO1) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_BLDO1));
    }
    if (PMU->isChannelAvailable(XPOWERS_BLDO2)) {
        Serial.printf("BLDO2: %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_BLDO2) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_BLDO2));
    }

    if (PMU->isVbusIn()) {
        Serial.printf("Vbus:      Voltage: %04u mV \n", PMU->getVbusVoltage());
    }
    //Serial.print("VbusVoltage:"); Serial.print(PMU->getVbusVoltage()); Serial.println("mV");
    //Serial.print("VbusCurrentLimit:"); Serial.print(PMU->getVbusCurrentLimit()); Serial.println();
    //Serial.print("VbusVoltageLimit:"); Serial.print(PMU->getVbusVoltageLimit()); Serial.println();

    if (PMU->isBatteryConnect()) {
        Serial.print("getBatteryPercent:"); Serial.print(PMU->getBatteryPercent()); Serial.println("%");
    }

    Serial.printf("=========================================\n");


    // Set the time of pressing the button to turn off
    PMU->setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
    uint8_t opt = PMU->getPowerKeyPressOffTime();
    Serial.print("PowerKeyPressOffTime:");
    switch (opt) {
    case XPOWERS_POWEROFF_4S: Serial.println("4 Second");
        break;
    case XPOWERS_POWEROFF_6S: Serial.println("6 Second");
        break;
    case XPOWERS_POWEROFF_8S: Serial.println("8 Second");
        break;
    case XPOWERS_POWEROFF_10S: Serial.println("10 Second");
        break;
    default:
        break;
    }

    return true;
}
#endif
//--------------------------------------------------------------------------------------------------------------------------------------


void ModuleESP32::initBoard()
{
    Serial.begin(115200);
    Serial.println("initBoard");
    SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);

    Wire.begin(I2C_SDA, I2C_SCL);


    pinMode(LCD_Led, OUTPUT);
    digitalWrite(LCD_Led, HIGH);  // Включить подсветку дисплея TFT

#ifdef HAS_GPS
    Serial1.begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
#endif


    initPMU();


#ifdef BOARD_LED
    /*
    * T-BeamV1.0, V1.1 LED defaults to low level as trun on,
    * so it needs to be forced to pull up
    * * * * */
#if LED_ON == LOW
    gpio_hold_dis(GPIO_NUM_4);
#endif
    pinMode(BOARD_LED, OUTPUT);
    digitalWrite(BOARD_LED, LED_ON);
#endif


#ifdef HAS_DISPLAY
    Wire.beginTransmission(0x3C);
    if (Wire.endTransmission() == 0) {
        Serial.println("Started OLED");
        u8g2 = new DISPLAY_MODEL(U8G2_R0, U8X8_PIN_NONE);
        u8g2->begin();
        u8g2->clearBuffer();
        u8g2->setFlipMode(0);
        u8g2->setFontMode(1); // Transparent
        u8g2->setDrawColor(1);
        u8g2->setFontDirection(0);
        u8g2->firstPage();
        do {
            u8g2->setFont(u8g2_font_inb19_mr);
            u8g2->drawStr(0, 30, "Decima");
            u8g2->drawHLine(2, 35, 47);
            u8g2->drawHLine(3, 36, 47);
            u8g2->drawVLine(45, 32, 12);
            u8g2->drawVLine(46, 33, 12);
            u8g2->setFont(u8g2_font_inb19_mf);
            u8g2->drawStr(58, 60, "RF");
        } while (u8g2->nextPage());
        u8g2->sendBuffer();
        u8g2->setFont(u8g2_font_fur11_tf);
        delay(3000);
    }
#endif
}





//--------------------------------------------------------------------------------------------------------------------------------------

