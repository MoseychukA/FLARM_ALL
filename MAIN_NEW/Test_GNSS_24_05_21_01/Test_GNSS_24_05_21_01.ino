 /*
 *Test_GNSS_24_05_20_02(.ino) firmware
 * Copyright (C) 2023-2024 OOO "Decima"
 *
 * Author: Alexander Moseychuk, Aleksandr.Moseychuk@decima.ru
 *
 * Web: https://t.me/flyrf_Support
 
 *Компиляция 
 * Arduino IDE 
 * Visual Studio 2019 с расширением vMicro
 * ESP32 Dev Module
 * Настройки компиляции в файле DOC\Настройки.png
 * 
 */

#include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <TFT_eSPI.h>             // Поддержка TFT дисплея  
#include "TFTModule.h" 
#include "Configuration_ESP32.h"
#include <esp_task_wdt.h>
#include "ESP32RF.h"
#include "OTA.h"
#include "TimeRF.h"
#include "LED.h"
#include "GNSS.h"
#include "EEPROMRF.h"
#include "BatteryRF.h"
#include "NMEA.h"
#include "SoC.h"
#include "WiFiRF.h"
#include "WebRF.h"
#include <TimeLib.h>
#include "Settings.h"

TFTModule tftModule;

int set_air = 0;   //  


uint32_t screenIdleTimer = 0;
 
#if !defined(SERIAL_FLUSH)
#define SERIAL_FLUSH() Serial.flush()
#endif

#define DEBUG 0
#define DEBUG_TIMING 0

#define isTimeToDisplay() (millis() - LEDTimeMarker     > 1000)
#define isTimeToExport()  (millis() - ExportTimeMarker  > 1000)

ufo_t ThisAircraft;

hardware_info_t hw_info = {
  .model    = DEFAULT_SOFTRF_MODEL,
  .revision = 0,
  .soc      = SOC_NONE,
  .gnss     = GNSS_MODULE_NONE,
  .display  = DISPLAY_NONE,
  .storage  = STORAGE_NONE,
  .rtc      = RTC_NONE,
  .pmu      = PMU_NONE,
};

unsigned long LEDTimeMarker = 0;
unsigned long ExportTimeMarker = 0;

void setup()
{
  rst_info *resetInfo;

  hw_info.soc = SoC_setup(); // Has to be very first procedure in the execution order

  resetInfo = (rst_info *) SoC->getResetInfoPtr();

  Serial.println();
  Serial.print(F(SOFTRF_IDENT "-"));
  Serial.print(SoC->name);
  Serial.print(F(" FW.REV: " SOFTRF_FIRMWARE_VERSION " DEV.ID: "));
  Serial.println(String(SoC->getChipId(), HEX));

  String ver_soft = __FILE__;
  int val_srt = ver_soft.lastIndexOf('\\');
  ver_soft.remove(0, val_srt+1);
  val_srt = ver_soft.lastIndexOf('.');
  ver_soft.remove(val_srt);
  Serial.println(ver_soft);

  SERIAL_FLUSH();

  if (resetInfo) 
  {
    Serial.println(""); Serial.print(F("Reset reason: ")); Serial.println(resetInfo->reason);
  }
  Serial.println(SoC->getResetReason());
  Serial.print(F("Free heap size: ")); Serial.println(SoC->getFreeHeap());
  Serial.println(SoC->getResetInfo()); Serial.println("");

  SERIAL_FLUSH();

  EEPROM_setup();

  SoC->Button_setup();

  ThisAircraft.addr = SoC->getChipId() & 0x00FFFFFF;

  delay(100);

  hw_info.display = SoC->Display_setup();

  hw_info.gnss = GNSS_setup();
 
  Battery_setup();

  SoC->swSer_enableRx(false);

  LED_setup();

  WiFi_setup();
  OTA_setup();
  Web_setup();
  NMEA_setup();

  delay(1000);

  /* expedite restart on WDT reset */
  if (resetInfo->reason != REASON_WDT_RST) 
  {
    LED_test();
  }
 
  SoC->post_init();
  tftModule.Setup();
  Settings.saveVer(ver_soft);  // Сохранить строку с текущей версией.
  SoC->WDT_setup();
}



void loop()
{

  // Сначала займитесь обычными радиочастотными делами

  esp_task_wdt_reset();
 
  normal();

  // Show status info on tiny OLED display
  SoC->Display_loop();
  esp_task_wdt_reset();
  // battery status LED
  LED_loop();

  // Handle DNS
  WiFi_loop();

  // Handle Web
  Web_loop();

  // Handle OTA update.
  OTA_loop();

 
  SoC->loop();

 /*
  if (SoC->UART_ops) {
     SoC->UART_ops->loop();
  }*/

  Battery_loop();

  SoC->Button_loop();
  tftModule.Update();
 
  Time_loop();

  yield();
}

void ClearExpired()
{
   /* for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
    {
        if (Container[i].addr && (ThisAircraft.timestamp - Container[i].timestamp) > ENTRY_EXPIRATION_TIME)
        {
            Container[i] = EmptyFO;
        }
    }*/
}


void normal()
{
    bool success;

    GNSS_loop();

    ThisAircraft.timestamp = now();
    if (isValidFix())
    {
        ThisAircraft.latitude = gnss.location.lat();
        ThisAircraft.longitude = gnss.location.lng();
        ThisAircraft.altitude = gnss.altitude.meters();
        ThisAircraft.course = gnss.course.deg();
        ThisAircraft.speed = gnss.speed.knots();
        ThisAircraft.hdop = (uint16_t)gnss.hdop.value();
        ThisAircraft.geoid_separation = gnss.separation.meters();
        int sats = gnss.satellites.value();
        Settings.setNumSat(sats);

#if !defined(EXCLUDE_EGM96)
        /*
         * When geoidal separation is zero or not available - use approx. EGM96 value
         */
        if (ThisAircraft.geoid_separation == 0.0) {
            ThisAircraft.geoid_separation = (float)LookupSeparation(
                ThisAircraft.latitude,
                ThisAircraft.longitude
            );
            /* we can assume the GPS unit is giving ellipsoid height */
            ThisAircraft.altitude -= ThisAircraft.geoid_separation;
        }
#endif /* EXCLUDE_EGM96 */

    }
}

