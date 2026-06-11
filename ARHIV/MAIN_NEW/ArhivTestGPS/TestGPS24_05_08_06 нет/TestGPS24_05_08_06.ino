/*
 * SoftRF(.ino) firmware
 * Copyright (C) 2016-2023 Linar Yusupov
 *
 * Author: Linar Yusupov, linar.r.yusupov@gmail.com
 *
 * Web: http://github.com/lyusupov/SoftRF
 
 */

#include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <TFT_eSPI.h>             // Поддержка TFT дисплея  
#include "TFTModule.h" 
#include "Configuration_ESP32.h"
#include <esp_task_wdt.h>
#include "CoreCommandBuffer.h"    // обработчик входящих по UART команд
#include "SettingsMail.h"

#include "ESP32RF.h"
#include "OTA.h"
#include "TimeRF.h"
#include "LED.h"
#include "GNSS.h"
#include "RF.h"
#include "EEPROMRF.h"
#include "BatteryRF.h"
#include "GDL90.h"
#include "NMEA.h"
#include "SoC.h"
#include "WiFiRF.h"
#include "WebRF.h"
#include "Baro.h"
#include <TimeLib.h>


TFTModule tftModule;

int set_air = 0;   //  


uint32_t screenIdleTimer = 0;
void txrx_test();
void uav();
void bridge();
void watchout();
void normal();

 
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
  .rf       = RF_IC_NONE,
  .gnss     = GNSS_MODULE_NONE,
  .baro     = BARO_MODULE_NONE,
  .display  = DISPLAY_NONE,
  .storage  = STORAGE_NONE,
  .rtc      = RTC_NONE,
  .imu      = IMU_NONE,
  .mag      = MAG_NONE,
  .pmu      = PMU_NONE,
};

unsigned long LEDTimeMarker = 0;
unsigned long ExportTimeMarker = 0;


void setup()
{
  rst_info *resetInfo;

  hw_info.soc = SoC_setup(); // Has to be very first procedure in the execution order

  SERIAL_TRACKER.begin(SERIAL_TRACKER_SPEED, SERIAL_IN_BITS, 39, 15);

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

  if (resetInfo) {
    Serial.println(""); Serial.print(F("Reset reason: ")); Serial.println(resetInfo->reason);
  }
  Serial.println(SoC->getResetReason());
  Serial.print(F("Free heap size: ")); Serial.println(SoC->getFreeHeap());
  Serial.println(SoC->getResetInfo()); Serial.println("");

  SERIAL_FLUSH();

  EEPROM_setup();

  SoC->Button_setup();

  ThisAircraft.addr = SoC->getChipId() & 0x00FFFFFF;

  hw_info.rf = RF_setup();

  delay(100);

  hw_info.baro = Baro_setup();
#if defined(ENABLE_AHRS)
  hw_info.imu = AHRS_setup();
#endif /* ENABLE_AHRS */
  hw_info.display = SoC->Display_setup();


    hw_info.gnss = GNSS_setup();
    ThisAircraft.aircraft_type = settings->aircraft_type;

  ThisAircraft.protocol = settings->rf_protocol;
  ThisAircraft.stealth  = settings->stealth;
  ThisAircraft.no_track = settings->no_track;

  Battery_setup();
 
  SoC->swSer_enableRx(false);

  LED_setup();

  WiFi_setup();

  
  OTA_setup();
  Web_setup();
  NMEA_setup();


  delay(1000);

  /* expedite restart on WDT reset */
  if (resetInfo->reason != REASON_WDT_RST) {
    LED_test();
  }

 
  switch (settings->mode)
  {
  case SOFTRF_MODE_TXRX_TEST1:
      Time_setup();
      set_air = 1;
      break;
  case SOFTRF_MODE_TXRX_TEST2:
      set_air = 2;
      Time_setup();
      break;
  case SOFTRF_MODE_WATCHOUT:
    Time_setup();
    break;
  case SOFTRF_MODE_BRIDGE:
    break;
  case SOFTRF_MODE_NORMAL:
  case SOFTRF_MODE_UAV:
  default:
    SoC->swSer_enableRx(true);
    set_air = 0;
    break;
  }


  SoC->post_init();
  tftModule.Setup();
  MainScreen->saveVer(ver_soft);  // Сохранить строку с текущей версией.

  SoC->WDT_setup();

}

int thisByte = 33;


void loop()
{

  // Сначала займитесь обычными радиочастотными делами
  RF_loop();
  esp_task_wdt_reset();
  switch (settings->mode)
  {
#if !defined(EXCLUDE_TEST_MODE)
  case SOFTRF_MODE_TXRX_TEST1:
  case SOFTRF_MODE_TXRX_TEST2:
    txrx_test();
    break;
#endif /* EXCLUDE_TEST_MODE */
  case SOFTRF_MODE_NORMAL:
  default:
    normal();
    break;
  }

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

  
  if (SoC->UART_ops) {
     SoC->UART_ops->loop();
  }

  Battery_loop();

  SoC->Button_loop();
  tftModule.Update();
 
  Time_loop();

  yield();
}


void normal()
{
  bool success;

  Baro_loop();

#if defined(ENABLE_AHRS)
  AHRS_loop();
#endif /* ENABLE_AHRS */

  GNSS_loop();

  ThisAircraft.timestamp = now();
  if (isValidFix()) {
    ThisAircraft.latitude  = gnss.location.lat();
    ThisAircraft.longitude = gnss.location.lng();
    ThisAircraft.altitude  = gnss.altitude.meters();
    ThisAircraft.course    = gnss.course.deg();
    ThisAircraft.speed     = gnss.speed.knots();
    ThisAircraft.hdop      = (uint16_t) gnss.hdop.value();
    ThisAircraft.geoid_separation = gnss.separation.meters();

#if !defined(EXCLUDE_EGM96)
    /*
     * When geoidal separation is zero or not available - use approx. EGM96 value
     */
    if (ThisAircraft.geoid_separation == 0.0) {
      ThisAircraft.geoid_separation = (float) LookupSeparation(
                                                ThisAircraft.latitude,
                                                ThisAircraft.longitude
                                              );
      /* we can assume the GPS unit is giving ellipsoid height */
      ThisAircraft.altitude -= ThisAircraft.geoid_separation;
    }
#endif /* EXCLUDE_EGM96 */

    RF_Transmit(RF_Encode(&ThisAircraft), true);
  }

  success = RF_Receive();

#if DEBUG
  success = true;
#endif

 // if (success && isValidFix()) ParseData();



  if (isTimeToDisplay()) {
    if (isValidFix()) { 
      LED_DisplayTraffic();
    } else {
      LED_Clear();
    }
    LEDTimeMarker = millis();
  }

   if (isTimeToExport()) {
    NMEA_Export();
    GDL90_Export();
 
    ExportTimeMarker = millis();
  }

  // Handle Air Connect
  NMEA_loop();

}



#if !defined(EXCLUDE_TEST_MODE)

unsigned int pos_ndx = 0;
unsigned long TxPosUpdMarker = 0;

float altitude1 = 100.0;
float speed1 = 300.0;
bool alt_high = false;
bool alien_dist = false;

//
////55.945148, 37.188258 Деревня Рузино
//float alien_lat1 = 55.945148;
//float alien_lon1 = 37.188258;
//
//// 55.976033, 37.306534 Деревня Чёрная Грязь
//float alien_lat2 = 55.976033;
//float alien_lon2 = 37.306534;

// 55.980740, 37.409649 //  Шереметьево аэропорт
//float alien_lat0 = 55.980740;
//float alien_lon0 = 37.409649;


// деревня Верескино
float alien_lat2 = 55.932420; 
float alien_lon2 = 37.353540;

//Точка 1 посёлок Андреевка
float alien_lat1 = 55.980951;
float alien_lon1 = 37.130935;

// Точка 2 деревня Верескино 55.932420, 37.353540
float alien_lat = 55.932420;
float alien_lon = 37.353540;

// Точка центр
//float alien_lat0 = 55.955023;  // Дом
//float alien_lon0 = 37.231561;
//

//float alien_lat = 55.945148;
//float alien_lon = 37.188258;



//float alien_lat0 = 55.951577;  // микрорайон Сходня
//float alien_lon0 = 37.295610;

float alien_lat0 = 55.955982;  // улица Чкалова, 20А
float alien_lon0 = 37.249766;


float test_curse = 0.0;
float Aircraft_latitude_old = 0.0;
float Aircraft_longitude_old = 0.0;


//int set_air = 1;   //  


void txrx_test()
{
  bool success = false;
#if DEBUG_TIMING
  unsigned long baro_start_ms, baro_end_ms;
  unsigned long tx_start_ms, tx_end_ms, rx_start_ms, rx_end_ms;
  unsigned long parse_start_ms, parse_end_ms, led_start_ms, led_end_ms;
  unsigned long export_start_ms, export_end_ms;
  unsigned long oled_start_ms, oled_end_ms;
#endif
  ThisAircraft.timestamp = now();

  if (TxPosUpdMarker == 0 || (millis() - TxPosUpdMarker) > 2000 ) {
   // ThisAircraft.latitude  = pgm_read_float( &txrx_test_positions[pos_ndx][0]);
   // ThisAircraft.longitude = pgm_read_float( &txrx_test_positions[pos_ndx][1]);
    pos_ndx = (pos_ndx + 1) % TXRX_TEST_NUM_POSITIONS;

        switch (set_air)
        {
        case 0:
            ThisAircraft.latitude = pgm_read_float(&txrx_test_positions[pos_ndx][0]);
            ThisAircraft.longitude = pgm_read_float(&txrx_test_positions[pos_ndx][1]);
            
            if (!alt_high)
            {
                altitude1 += 100.0;
                if (altitude1 > 4000.0)
                {
                    altitude1 = 4000.0;
                    alt_high = true;
                }
            }
            if (alt_high)
            {
                altitude1 -= 100.0;
                if (altitude1 < 100.0)
                {
                    altitude1 = 100.0;
                    alt_high = false;
                }
            }
            
  
            break;
        case 1:
        
            ThisAircraft.latitude = alien_lat0 ;    // 
            ThisAircraft.longitude = alien_lon0 ;   // 
            
    //        ThisAircraft.latitude = alien_lat2 + (0.001213275 * 20);   //55.996177;  //
    //        ThisAircraft.longitude = alien_lon1 + (0.005565125 * 20);   //38.345584; //
    //
    
    /*        
            test_curse = test_curse + 2.0;
            if (test_curse >= 360.0)
                test_curse = 0.0;
 
            speed1 = speed1-2.0;
            if (speed1 <=2.0)
                speed1 = 200.0;*/

            test_curse = 360.0;

            altitude1 = 1000.0;
           // test_curse = 40;
            break;
        case 2:

            if (!alien_dist)
            {
                alien_lat += 0.001213275;
                alien_lon -= 0.005565125;

                if (alien_lat >= alien_lat1)
                {
                    alien_lat = alien_lat1;
                    alien_dist = true;
                }
                test_curse = 291.33;
            }

            if (alien_dist)
            {
                alien_lat -= 0.001213275;
                alien_lon += 0.005565125;

                if (alien_lat <= alien_lat2)
                {
                    alien_lat = alien_lat2;
                    alien_dist = false;
                }
                test_curse = 111.14;
            }
            ThisAircraft.latitude = alien_lat;
            ThisAircraft.longitude = alien_lon;

            /* */
            if (!alt_high)
            {
                altitude1 += 10.0;
                if (altitude1 > 1200.0)
                {
                    altitude1 = 1200.0;
                    alt_high = true;
                }
            }
            if (alt_high)
            {

                altitude1 -= 10.0;
                if (altitude1 < 30.0)
                {
                    altitude1 = 30.0;
                    alt_high = false;
                }
            }
            speed1 = speed1 - 2.0;
            if (speed1 <= 2.0)
                speed1 = 300.0;
             break;
        default:
            break;
        }
        TxPosUpdMarker = millis();
    }


    ThisAircraft.altitude = altitude1; 
    ThisAircraft.course   = test_curse; 
    ThisAircraft.speed    = speed1;    
    ThisAircraft.vs       = TXRX_TEST_VS;


#if DEBUG_TIMING
  baro_start_ms = millis();
#endif
  Baro_loop();
#if DEBUG_TIMING
  baro_end_ms = millis();
#endif

#if defined(ENABLE_AHRS)
  AHRS_loop();
#endif /* ENABLE_AHRS */

#if DEBUG_TIMING
  tx_start_ms = millis();
#endif
  RF_Transmit(RF_Encode(&ThisAircraft), true);
#if DEBUG_TIMING
  tx_end_ms = millis();
  rx_start_ms = millis();
#endif
  success = RF_Receive();
#if DEBUG_TIMING
  rx_end_ms = millis();
#endif

#if DEBUG_TIMING
  parse_start_ms = millis();
#endif
 
#if DEBUG_TIMING
  parse_end_ms = millis();
#endif

 

#if DEBUG_TIMING
  led_start_ms = millis();
#endif
  if (isTimeToDisplay()) 
  {
    LED_DisplayTraffic();
    LEDTimeMarker = millis();
  }
#if DEBUG_TIMING
  led_end_ms = millis();
#endif


#if DEBUG_TIMING
  export_start_ms = millis();
#endif
  if (isTimeToExport()) {
#if defined(USE_NMEALIB)
    NMEA_Position();
#endif
    NMEA_Export();
    GDL90_Export();
 
    ExportTimeMarker = millis();
  }
#if DEBUG_TIMING
  export_end_ms = millis();
#endif

#if DEBUG_TIMING
  oled_start_ms = millis();
#endif
//  SoC->Display_loop();
#if DEBUG_TIMING
  oled_end_ms = millis();
#endif



  // Handle Air Connect
  NMEA_loop();

}

#endif /* EXCLUDE_TEST_MODE */
