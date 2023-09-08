
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


//static String ESP32_getResetInfo()
//{
////    switch (rtc_get_reset_reason(0))
////    {
////    case POWERON_RESET: return F("Vbat power on reset");
////    case DEEPSLEEP_RESET: return F("Deep Sleep reset digital core");
////    case TG0WDT_SYS_RESET: return F("Timer Group0 Watch dog reset digital core");
////    case TG1WDT_SYS_RESET: return F("Timer Group1 Watch dog reset digital core");
////    case RTCWDT_SYS_RESET: return F("RTC Watch dog Reset digital core");
////    case INTRUSION_RESET: return F("Instrusion tested to reset CPU");
////    case RTCWDT_CPU_RESET: return F("RTC Watch dog Reset CPU");
////    case RTCWDT_BROWN_OUT_RESET: return F("Reset when the vdd voltage is not stable");
////    case RTCWDT_RTC_RESET: return F("RTC Watch dog reset digital core and rtc module");
//#if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32S3)
//   /* case SW_RESET: return F("Software reset digital core");
//    case OWDT_RESET: return F("Legacy watch dog reset digital core");
//    case SDIO_RESET: return F("Reset by SLC module, reset digital core");
//    case TGWDT_CPU_RESET: return F("Time Group reset CPU");
//    case SW_CPU_RESET: return F("Software reset CPU");
//    case EXT_CPU_RESET: return F("for APP CPU, reseted by PRO CPU");*/
//#endif /* CONFIG_IDF_TARGET_ESP32S2 */
//   /* default: */return F("No reset information available");
////    }
//}



void ESP32Class::ESP32_post_init()
{
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    if (hw_info.model == SOFTRF_MODEL_PRIME_MK3)
    {
        Serial.println();
        Serial.println(F("Power-on Self Test"));
        Serial.println();
        Serial.flush();

        Serial.println(F("Built-in components:"));

        Serial.print(F("RADIO   : "));
        Serial.println(hw_info.rf == RF_IC_SX1262 ||
            hw_info.rf == RF_IC_SX1276 ? F("PASS") : F("FAIL"));
        Serial.flush();
        Serial.print(F("GNSS    : "));
        Serial.println(hw_info.gnss != GNSS_MODULE_NONE ? F("PASS") : F("FAIL"));
        Serial.flush();
        Serial.print(F("DISPLAY : "));
        Serial.println(hw_info.display == DISPLAY_OLED_1_3 ? F("PASS") : F("FAIL"));
        Serial.flush();
        Serial.print(F("RTC     : "));
        Serial.println(hw_info.rtc == RTC_PCF8563 ? F("PASS") : F("FAIL"));
        Serial.flush();
        Serial.print(F("BARO    : "));
        Serial.println(hw_info.baro == BARO_MODULE_BMP280 ? F("PASS") : F("N/A"));
        Serial.flush();
#if !defined(EXCLUDE_IMU)
        Serial.print(F("IMU     : "));
        Serial.println(hw_info.imu != IMU_NONE ? F("PASS") : F("FAIL"));
        Serial.flush();
#endif /* EXCLUDE_IMU */
        Serial.print(F("MAG     : "));
        Serial.println(hw_info.mag != MAG_NONE ? F("PASS") : F("FAIL"));
        Serial.flush();

        Serial.println();
        Serial.println(F("External components:"));
        Serial.print(F("CARD    : "));
        Serial.println(hw_info.storage == STORAGE_CARD ||
            hw_info.storage == STORAGE_FLASH_AND_CARD
            ? F("PASS") : F("N/A"));
        Serial.flush();

        Serial.println();
        Serial.println(F("Power-on Self Test is complete."));
        Serial.flush();
    }

    Serial.println();

    if (!uSD_is_mounted) {
        Serial.println(F("WARNING: unable to mount micro-SD card."));
    }
    else {
        // The number of 512 byte sectors in the card
        // or zero if an error occurs.
        size_t cardSize = uSD.card()->cardSize();

        if (cardSize == 0) {
            Serial.println(F("WARNING: invalid micro-SD card size."));
        }
        else {
            uint8_t cardType = uSD.card()->type();

            Serial.print(F("SD Card Type: "));
            if (cardType == SD_CARD_TYPE_SD1) {
                Serial.println(F("V1"));
            }
            else if (cardType == SD_CARD_TYPE_SD2) {
                Serial.println(F("V2"));
            }
            else if (cardType == SD_CARD_TYPE_SDHC) {
                Serial.println(F("SDHC"));
            }
            else {
                Serial.println(F("UNKNOWN"));
            }

            Serial.print("SD Card Size: ");
            Serial.print(cardSize / (2 * 1024));
            Serial.println(" MB");

            uSD.fsBegin();
        }
    }
#endif /* CONFIG_IDF_TARGET_ESP32S3 */

    Serial.println();
    Serial.println(F("Data output device(s):"));

    Serial.print(F("NMEA   - "));
    switch (settings->nmea_out)
    {
        //case NMEA_UART:  Serial.println(F("UART"));      break;
        //case NMEA_USB:  Serial.println(F("USB CDC"));   break;
        //case NMEA_UDP:  Serial.println(F("UDP"));       break;
        //case NMEA_TCP:  Serial.println(F("TCP"));       break;
        //    // case NMEA_BLUETOOTH  :  Serial.println(F("Bluetooth")); break;
        //case NMEA_OFF:
        default:  Serial.println(F("NULL"));      break;
    }

    Serial.print(F("GDL90  - "));
    switch (settings->gdl90)
    {
    case GDL90_UART:  Serial.println(F("UART"));      break;
    case GDL90_USB:  Serial.println(F("USB CDC"));   break;
    case GDL90_UDP:  Serial.println(F("UDP"));       break;
        // case GDL90_BLUETOOTH :  Serial.println(F("Bluetooth")); break;
    case GDL90_OFF:
    default:  Serial.println(F("NULL"));      break;
    }

    //Serial.print(F("D1090  - "));
    //switch (settings->d1090)
    //{
    //  case D1090_UART      :  Serial.println(F("UART"));      break;
    //  case D1090_USB       :  Serial.println(F("USB CDC"));   break;
    //  case D1090_BLUETOOTH :  Serial.println(F("Bluetooth")); break;
    // // case D1090_OFF       :
    //  default              :  Serial.println(F("NULL"));      break;
    //}

    Serial.println();
    Serial.flush();

    switch (hw_info.display)
    {
#if defined(USE_OLED)
    case DISPLAY_OLED_TTGO:
    case DISPLAY_OLED_HELTEC:
    case DISPLAY_OLED_1_3:
        OLED_info1();

#if defined(CONFIG_IDF_TARGET_ESP32S3)
        if (hw_info.model == SOFTRF_MODEL_PRIME_MK3)
        {
            char key[8];
            char out[64];
            uint8_t tokens[3] = { 0 };
            cdbResult rt;
            int c, i = 0, token_cnt = 0;

            int acfts;
            char* reg, * mam, * cn;
            reg = mam = cn = NULL;

            OLED_info2();

            if (ADB_is_open) {
                acfts = ucdb.recordsNumber();

                snprintf(key, sizeof(key), "%06X", ThisAircraft.addr);

                rt = ucdb.findKey(key, strlen(key));

                switch (rt) {
                case KEY_FOUND:
                    while ((c = ucdb.readValue()) != -1 && i < (sizeof(out) - 1)) {
                        if (c == '|') {
                            if (token_cnt < (sizeof(tokens) - 1)) {
                                token_cnt++;
                                tokens[token_cnt] = i + 1;
                            }
                            c = 0;
                        }
                        out[i++] = (char)c;
                    }
                    out[i] = 0;

                    reg = out + tokens[1];
                    mam = out + tokens[0];
                    cn = out + tokens[2];

                    break;

                case KEY_NOT_FOUND:
                default:
                    break;
                }

                reg = (reg != NULL) && strlen(reg) ? reg : (char*)"REG: N/A";
                mam = (mam != NULL) && strlen(mam) ? mam : (char*)"M&M: N/A";
                cn = (cn != NULL) && strlen(cn) ? cn : (char*)" CN: N/A";

            }
            else {
                acfts = -1;
            }

            OLED_info3(acfts, reg, mam, cn);
        }
#endif /* CONFIG_IDF_TARGET_ESP32S3 */

        break;
#endif /* USE_OLED */
    case DISPLAY_NONE:
    default:
        break;
    }
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



