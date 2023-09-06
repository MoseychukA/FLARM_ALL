
#include "ESP32.h"
#include <Wire.h>
#include "sdkconfig.h"

#include <SPI.h>
#include <esp_err.h>
#include <esp_wifi.h>





//#if !defined(CONFIG_IDF_TARGET_ESP32S2)
//#include <esp_bt.h>
//#include <BLEDevice.h>
//#endif /* CONFIG_IDF_TARGET_ESP32S2 */
//#include <soc/rtc_cntl_reg.h>
//#include <soc/efuse_reg.h>
//#include <Wire.h>
//#include <rom/rtc.h>
//#include <rom/spi_flash.h>
//#include <soc/adc_channel.h>
////#include <flashchips.h>
////#include <axp20x.h>
//#define  XPOWERS_CHIP_AXP2102
//#include <XPowersLib.h>
//#include <pcf8563.h>


static union {
    uint8_t efuse_mac[6];
    uint64_t chipmacid;
};





//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ESP32Class Esp32;
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
ESP32Class::ESP32Class()
{

}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------


static String ESP32_getResetInfo()
{
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
//#if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32S3)
//    case SW_RESET: return F("Software reset digital core");
//    case OWDT_RESET: return F("Legacy watch dog reset digital core");
//    case SDIO_RESET: return F("Reset by SLC module, reset digital core");
//    case TGWDT_CPU_RESET: return F("Time Group reset CPU");
//    case SW_CPU_RESET: return F("Software reset CPU");
//    case EXT_CPU_RESET: return F("for APP CPU, reseted by PRO CPU");
//#endif /* CONFIG_IDF_TARGET_ESP32S2 */
//    default: return F("No reset information available");
//    }
}





void ESP32Class::ESP32_reset()
{
    ESP.restart();
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------


uint32_t ESP32Class::DevID_Mapper(uint32_t id)
{
    uint8_t id_mask = (id & 0x00FF0000UL) >> 16;

    switch (id_mask)
    {
        /* remap address to avoid overlapping with congested FLARM range */
    case 0xD0:
    case 0xDD:
    case 0xDE:
    case 0xDF:
        id += 0x100000;
        break;
        /* remap 11xxxx addresses to avoid overlapping with congested Skytraxx range */
    case 0x11:
        /*
         * OGN 0.2.8+ does not decode 'Air V6' traffic when leading byte of 24-bit Id is 0x5B
         */
    case 0x5B:
        id += 0x010000;
        break;

    default:
        break;
    }

    return id;
}



uint32_t  ESP32Class::ESP32_getChipId()
{
#if !defined(SOFTRF_ADDRESS)
    uint32_t id = (uint32_t)efuse_mac[5] | ((uint32_t)efuse_mac[4] << 8) | \
        ((uint32_t)efuse_mac[3] << 16) | ((uint32_t)efuse_mac[2] << 24);

    return DevID_Mapper(id);
#else
    return (SOFTRF_ADDRESS & 0xFFFFFFFFU);
#endif /* SOFTRF_ADDRESS */
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------


void ESP32Class::ESP32_WDT_setup()
{
    enableLoopWDT();
}

void ESP32Class::ESP32_WDT_fini()
{
    disableLoopWDT();
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------


// The event handler for the button.
//void handleEvent(AceButton * button, uint8_t eventType,
//    uint8_t buttonState) {
//
//    switch (eventType) {
//    case AceButton::kEventClicked:
//    case AceButton::kEventReleased:
//#if defined(USE_OLED)
//        if (button == &button_1) {
//            OLED_Next_Page();
//        }
//#endif
//        break;
//    case AceButton::kEventDoubleClicked:
//        break;
//    case AceButton::kEventLongPressed:
//        if (button == &button_1) {
//            shutdown(SOFTRF_SHUTDOWN_BUTTON);
//        }
//        break;
//    }
//}

/* Callbacks for push button interrupt */
void onPageButtonEvent() 
{
   // button_1.check();
}

void ESP32Class::ESP32_Button_setup()
{
    //if ((hw_info.model == SOFTRF_MODEL_PRIME_MK2 &&
    //    (hw_info.revision == 2 || hw_info.revision == 5)) ||
    //    esp32_board == ESP32_S2_T8_V1_1 ||
    //    esp32_board == ESP32_S3_DEVKIT) {
    //    int button_pin = (esp32_board == ESP32_S2_T8_V1_1) ?
    //        SOC_GPIO_PIN_T8_S2_BUTTON :
    //        (esp32_board == ESP32_S3_DEVKIT) ?
    //        SOC_GPIO_PIN_S3_BUTTON :
    //        SOC_GPIO_PIN_TBEAM_V05_BUTTON;

    //    // Button(s) uses external pull up resistor.
    //    pinMode(button_pin, button_pin == 0 ? INPUT_PULLUP : INPUT);

    //    button_1.init(button_pin);

    //    // Configure the ButtonConfig with the event handler, and enable all higher
    //    // level events.
    //    ButtonConfig* PageButtonConfig = button_1.getButtonConfig();
    //    PageButtonConfig->setEventHandler(handleEvent);
    //    PageButtonConfig->setFeature(ButtonConfig::kFeatureClick);
    //    PageButtonConfig->setFeature(ButtonConfig::kFeatureLongPress);
    //    PageButtonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterClick);
    //    //  PageButtonConfig->setDebounceDelay(15);
    //    PageButtonConfig->setClickDelay(600);
    //    PageButtonConfig->setLongPressDelay(2000);
    //}
}

void ESP32Class::ESP32_Button_loop()
{
 /*   if ((hw_info.model == SOFTRF_MODEL_PRIME_MK2 &&
        (hw_info.revision == 2 || hw_info.revision == 5)) ||
        esp32_board == ESP32_S2_T8_V1_1 ||
        esp32_board == ESP32_S3_DEVKIT) {
        button_1.check();
    }*/
}

void ESP32Class::ESP32_Button_fini()
{
  /*  if (esp32_board == ESP32_S2_T8_V1_1 ||
        esp32_board == ESP32_S3_DEVKIT) {
        int button_pin = esp32_board == ESP32_S2_T8_V1_1 ?
            SOC_GPIO_PIN_T8_S2_BUTTON :
            SOC_GPIO_PIN_S3_BUTTON;
        while (digitalRead(button_pin) == LOW);
    }*/
}







//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------



