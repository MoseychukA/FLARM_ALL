/*
 * FlyRf(.ino) firmware
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
#include "CoreCommandBuffer.h"    // обработчик входящих по UART команд
#include "SettingsMail.h"

#include "ESP32RF.h"
#include "OTA.h"
#include "TimeRF.h"
#include "GNSS.h"
#include "RF.h"
#include "EEPROMRF.h"
#include "BatteryRF.h"
#include "MAVLinkRF.h"
#include "GDL90.h"
#include "NMEA.h"
#include "D1090.h"
#include "SoC.h"
#include "WiFiRF.h"
#include "WebRF.h"
#include "Baro.h"
#include "TrafficHelper.h"
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

bool start_setup = false;

void  DUMP1090_Task(void* pvParameters)
{
    for (;;)
    {
        if (start_setup)
        { 
            MainScreen->Receive1090();  // Получить пакет от DUMP1090
            esp_task_wdt_reset();
            vTaskDelay(1);
            yield();
        }
    }
}



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

#if !defined(EXCLUDE_MAVLINK)
  if (settings->mode == SOFTRF_MODE_UAV) 
  {
    Serial.begin(57600);
    MAVLink_setup();
    ThisAircraft.aircraft_type = AIRCRAFT_TYPE_UAV;  
  }  else
#endif /* EXCLUDE_MAVLINK */
  {
    hw_info.gnss = GNSS_setup();
    ThisAircraft.aircraft_type = settings->aircraft_type;
  }
  ThisAircraft.protocol = settings->rf_protocol;
  ThisAircraft.stealth  = settings->stealth;
  ThisAircraft.no_track = settings->no_track;

  Battery_setup();
  Traffic_setup();

  SoC->swSer_enableRx(false);
  WiFi_setup();
 
  OTA_setup();
  Web_setup();
  NMEA_setup();

#if defined(ENABLE_TTN)
  TTN_setup();
#endif

  delay(1000);
 
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

  SettingsMail.setup();

  SoC->WDT_setup();

  xTaskCreatePinnedToCore(
      DUMP1090_Task            // 
      , "DUMP1090_Task"
      , 2048                       // Размер стека
      , NULL                       // Когда параметр не используется, просто передайте NULL
      , 3                          // Priority
      , NULL                       // С дескриптором задачи мы сможем манипулировать этой задачей.
      , 0//ARDUINO_RUNNING_CORE    // Ядро, на котором будет выполняться задача
  );


  start_setup = true;            // Настройки завершены, включаем в работу ядро "0"

}


void loop()
{

  // Сначала займитесь обычными радиочастотными делами
  RF_loop();
  esp_task_wdt_reset();
  SettingsMail.update();
  switch (settings->mode)
  {
#if !defined(EXCLUDE_TEST_MODE)
  case SOFTRF_MODE_TXRX_TEST1:
  case SOFTRF_MODE_TXRX_TEST2:
    txrx_test();
    break;
#endif /* EXCLUDE_TEST_MODE */
#if !defined(EXCLUDE_MAVLINK)
  case SOFTRF_MODE_UAV:
    uav();
    break;
#endif /* EXCLUDE_MAVLINK */
#if !defined(EXCLUDE_WIFI)
  case SOFTRF_MODE_BRIDGE:
    bridge();
    break;
#endif /* EXCLUDE_WIFI */
#if !defined(EXCLUDE_WATCHOUT_MODE)
  case SOFTRF_MODE_WATCHOUT:
    watchout();
    break;
#endif /* EXCLUDE_WATCHOUT_MODE */
  case SOFTRF_MODE_NORMAL:
  default:
    normal();
    break;
  }

  // Show status info on tiny OLED display
  SoC->Display_loop();
  esp_task_wdt_reset();
 
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
  // обрабатываем входящие команды с КОМ порта Iridium
  CommandHandler.handleCommands();
  Time_loop();

  yield();
}

void shutdown(int reason)
{
  SoC->WDT_fini();

  SoC->swSer_enableRx(false);

  NMEA_fini();

  Web_fini();

  WiFi_fini();

  if (settings->mode != SOFTRF_MODE_UAV) {
    GNSS_fini();
  }

  SoC->Display_fini(reason);

  Baro_fini();

  RF_Shutdown();

  SoC->Button_fini();

  SoC_fini(reason);
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

  if (success && isValidFix()) ParseData();


  if (isValidFix()) {
    Traffic_loop();
  }

   if (isTimeToExport()) {
    NMEA_Export();
    GDL90_Export();
    D1090_Export();

    ExportTimeMarker = millis();
  }

  // Handle Air Connect
  NMEA_loop();

  ClearExpired();
}

#if !defined(EXCLUDE_MAVLINK)
void uav()
{
  bool success = false;

  PickMAVLinkFix();

  MAVLinkTimeSync();
  MAVLinkSetWiFiPower();

  ThisAircraft.timestamp = now();

  if (isValidMAVFix()) 
  {
    ThisAircraft.latitude  = the_aircraft.location.gps_lat / 1e7;
    ThisAircraft.longitude = the_aircraft.location.gps_lon / 1e7;
    ThisAircraft.altitude  = the_aircraft.location.gps_alt / 1000.0;
    ThisAircraft.course    = the_aircraft.location.gps_cog;
    ThisAircraft.speed     = (the_aircraft.location.gps_vog / 100.0) / _GPS_MPS_PER_KNOT;
    ThisAircraft.hdop      = the_aircraft.location.gps_hdop;
    ThisAircraft.pressure_altitude = the_aircraft.location.baro_alt;

    RF_Transmit(RF_Encode(&ThisAircraft), true);
  }

  success = RF_Receive();

  if (success && isValidMAVFix()) ParseData();

  if (isTimeToExport() && isValidMAVFix()) {
    MAVLinkShareTraffic();
    ExportTimeMarker = millis();
  }

  ClearExpired();
}
#endif /* EXCLUDE_MAVLINK */

#if !defined(EXCLUDE_WIFI)
void bridge()
{
  bool success;

  size_t tx_size = Raw_Receive_UDP(&TxBuffer[0]);

  if (tx_size > 0) {
    RF_Transmit(tx_size, true);
  }

  success = RF_Receive();

  if(success)
  {
    size_t rx_size = RF_Payload_Size(settings->rf_protocol);
    rx_size = rx_size > sizeof(fo.raw) ? sizeof(fo.raw) : rx_size;

    memset(fo.raw, 0, sizeof(fo.raw));
    memcpy(fo.raw, RxBuffer, rx_size);

    if (settings->nmea_p) {
      StdOut.print(F("$PSRFI,"));
      StdOut.print((unsigned long) now());    StdOut.print(F(","));
      StdOut.print(Bin2Hex(fo.raw, rx_size)); StdOut.print(F(","));
      StdOut.println(RF_last_rssi);
    }

    Raw_Transmit_UDP();
  }

  if (isTimeToDisplay()) 
  {
     LEDTimeMarker = millis();
  }
}
#endif /* EXCLUDE_WIFI */

#if !defined(EXCLUDE_WATCHOUT_MODE)
void watchout()
{
  bool success;

  success = RF_Receive();

  if (success) {
    size_t rx_size = RF_Payload_Size(settings->rf_protocol);
    rx_size = rx_size > sizeof(fo.raw) ? sizeof(fo.raw) : rx_size;

    memset(fo.raw, 0, sizeof(fo.raw));
    memcpy(fo.raw, RxBuffer, rx_size);

    if (settings->nmea_p) {
      StdOut.print(F("$PSRFI,"));
      StdOut.print((unsigned long) now());    StdOut.print(F(","));
      StdOut.print(Bin2Hex(fo.raw, rx_size)); StdOut.print(F(","));
      StdOut.println(RF_last_rssi);
    }
  }

  if (isTimeToDisplay()) 
  {
    LEDTimeMarker = millis();
  }
}
#endif /* EXCLUDE_WATCHOUT_MODE */

#if !defined(EXCLUDE_TEST_MODE)

unsigned int pos_ndx = 0;
unsigned long TxPosUpdMarker = 0;

float altitude1 = 100.0;
float speed1 = 300.0;
bool alt_high = false;
bool alien_dist1 = false;
bool alien_dist2 = false;
int alien_route = 1;


//55.945148, 37.188258 Деревня Рузино
float alien_lat3 = 55.945148;
float alien_lon3 = 37.188258;

// 55.976033, 37.306534 Деревня Чёрная Грязь
float alien_lat4 = 55.976033;
float alien_lon4 = 37.306534;

float alien_lat5 = 55.945148;
float alien_lon5 = 37.188258;


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
    /*  
            
            test_curse = test_curse + 2.0;
            if (test_curse >= 360.0)
                test_curse = 0.0;*/
 /*
            speed1 = speed1-2.0;
            if (speed1 <=2.0)
                speed1 = 200.0;*/

           test_curse = 360.0;
           speed1 = 200.0;
           altitude1 = 1000.0;
           // test_curse = 40;
            break;
        case 2:

            if (alien_route== 1)
            {
                if (!alien_dist1)
                {
                    alien_lat += 0.001213275;
                    alien_lon -= 0.005565125;

                    if (alien_lat >= alien_lat1)
                    {
                        alien_lat = alien_lat1;
                        alien_dist1 = true;
                    }
                    test_curse = 291.33;
                }

                if (alien_dist1)
                {
                    alien_lat -= 0.001213275;
                    alien_lon += 0.005565125;

                    if (alien_lat <= alien_lat2)
                    {
                        alien_lat = alien_lat2;
                        alien_dist1 = false;
                        alien_route = 2;
                    }
                    test_curse = 111.14;
                }

                ThisAircraft.latitude = alien_lat;
                ThisAircraft.longitude = alien_lon;
            }

            if (alien_route == 2)
            {

                //55.945148, 37.188258 Деревня Рузино
                //float alien_lat3 = 55.945148;
                //float alien_lon3 = 37.188258;

                //// 55.976033, 37.306534 Деревня Чёрная Грязь
                //float alien_lat4 = 55.976033;
                //float alien_lon4 = 37.306534;


                if (alien_dist2== false)
                {
                    alien_lat5 += 0.001213275;
                    alien_lon5 += 0.005565125;

                    if (alien_lat5 >= alien_lat4) 
                    {
                        alien_lat5 = alien_lat4;
                        alien_dist2 = true;
                    }
                    test_curse = 60;
                }

                if (alien_dist2 == true)
                {
                    alien_lat5 -= 0.001213275;
                    alien_lon5 -= 0.005565125;

                    if (alien_lat5 <= alien_lat3)
                    {
                        alien_lat5 = alien_lat3;
                        alien_dist2 = false;
                        alien_route = 1;
                    }
                    test_curse = 240;
                }

                ThisAircraft.latitude = alien_lat5;
                ThisAircraft.longitude = alien_lon5;
            }

            /* */
            if (!alt_high)
            {
                altitude1 += 50.0;
                if (altitude1 > 1200.0)
                {
                    altitude1 = 1200.0;
                    alt_high = true;
                }
            }
            if (alt_high)
            {

                altitude1 -= 50.0;
                if (altitude1 < 50.0)
                {
                    altitude1 = 50.0;
                    alt_high = false;
                }
            }
            speed1 -= 30.0;
            if (speed1 <= 30.0)
                speed1 = 1020.0;
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
  if (success) ParseData();
#if DEBUG_TIMING
  parse_end_ms = millis();
#endif

#if defined(ENABLE_TTN)
  TTN_loop();
#endif

  Traffic_loop();

#if DEBUG_TIMING
  led_start_ms = millis();
#endif
  if (isTimeToDisplay()) 
  {
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
    D1090_Export();
    ExportTimeMarker = millis();
  }
#if DEBUG_TIMING
  export_end_ms = millis();
#endif

#if DEBUG_TIMING
  oled_start_ms = millis();
#endif

#if DEBUG_TIMING
  oled_end_ms = millis();
#endif

#if DEBUG_TIMING
  if (baro_start_ms - baro_end_ms) {
    Serial.print(F("Baro start: "));
    Serial.print(baro_start_ms);
    Serial.print(F(" Baro stop: "));
    Serial.println(baro_end_ms);
  }
  if (tx_end_ms - tx_start_ms) {
    Serial.print(F("TX start: "));
    Serial.print(tx_start_ms);
    Serial.print(F(" TX stop: "));
    Serial.println(tx_end_ms);
  }
  if (rx_end_ms - rx_start_ms) {
    Serial.print(F("RX start: "));
    Serial.print(rx_start_ms);
    Serial.print(F(" RX stop: "));
    Serial.println(rx_end_ms);
  }
  if (parse_end_ms - parse_start_ms) {
    Serial.print(F("Parse start: "));
    Serial.print(parse_start_ms);
    Serial.print(F(" Parse stop: "));
    Serial.println(parse_end_ms);
  }
  if (led_end_ms - led_start_ms) {
    Serial.print(F("LED start: "));
    Serial.print(led_start_ms);
    Serial.print(F(" LED stop: "));
    Serial.println(led_end_ms);
  }
  if (export_end_ms - export_start_ms) {
    Serial.print(F("Export start: "));
    Serial.print(export_start_ms);
    Serial.print(F(" Export stop: "));
    Serial.println(export_end_ms);
  }
  if (oled_end_ms - oled_start_ms) {
    Serial.print(F("OLED start: "));
    Serial.print(oled_start_ms);
    Serial.print(F(" OLED stop: "));
    Serial.println(oled_end_ms);
  }
#endif

  // Handle Air Connect
  NMEA_loop();

  ClearExpired();
}

#endif /* EXCLUDE_TEST_MODE */
