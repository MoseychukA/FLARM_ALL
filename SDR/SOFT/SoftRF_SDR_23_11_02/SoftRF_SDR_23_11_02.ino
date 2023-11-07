

//-----------------------------------------------------------------------------
#include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <TFT_eSPI.h>             // Поддержка TFT дисплея 
#include <SD.h>                   // Поддержка SD карты
#include "SPIFFS.h"
#include "FS.h"
#include <Wire.h>                 // 
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <aircraft.h>
#include <adsb_encoder.h>
#include <TimeLib.h>
#include <lib_crc.h>
#include <protocol.h>
#include <TinyGPS++.h>
#include <nmealib.h>
#include <fec.h>
#include <flashchips.h>
#include <LibAPRSesp.h>
#include <manchester.h>
#include <U8g2lib.h>
#include <mode-s.h>
#include "egm96s.h"
#include <axp20x.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFiClient.h>
#include <nRF905.h>
#include <NeoPixelBus.h>
#include <WiFiServer.h>
#include <functional>
#include <memory>
#include <WiFi.h>
#include "HTTP_Method.h"
#include "Uri.h"
#include <Adafruit_BMP085.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_MPL3115A2.h>
#include <pcf8563.h>
#include <i2c_bus.h>
#include <AceButton.h>
#include <ArduinoOTA.h>
#include "jquery_min_js.h"
#include <DNSServer.h>
#include <mavlink.h>        // Mavlink interface
#include <battery.h>
#include "Adafruit_GFX.h"
#include "Configuration_ESP32.h"


#define  XPOWERS_CHIP_AXP2102
#include <XPowersLib.h>

#include <esp_task_wdt.h>

#define WDT_TIMEOUT 3

#include "OTA.h"
#include "TimeHELPER.h"
#include "LED.h"
#include "GNSS.h"
#include "RF.h"
#include "Sound.h"
#include "EEPROMHELPER.h"
#include "BatteryHELPER.h"
#include "MAVLinkHELPER.h"
#include "GDL90.h"
#include "NMEA.h"
#include "D1090.h"
#include "SoC.h"
#include "WiFiHELPER.h"
#include "WebHELPER.h"
#include "Baro.h"
#include "TTNHelper.h"
#include "TrafficHelper.h"
#include "Recorder.h"
#include "BluetoothHELPER.h"
#include "PLATFORM_ESP32.h"
#include "TFTModule.h"
#include "SoftRF.h"

TFTModule tftModule;

uint32_t screenIdleTimer = 0;

//--------------------------------------------------------------------------------------------------------------------------------
void screenAction(AbstractTFTScreen* screen)
{
    // какое-то действие на экране произошло.
    // тут просто сбрасываем таймер ничегонеделанья.
    screenIdleTimer = millis();           // Таймер переключения на главный экран
  
}

//#include "Fonts/GFXFF/FreeMonoBold24pt7b.h"
//#include "Fonts/GFXFF/FreeMonoBold18pt7b.h"
//#include "Fonts/GFXFF/FreeMonoBold12pt7b.h"
//#include "Fonts/GFXFF/FreeMono18pt7b.h"
//#include <Fonts/GFXFF/FreeMonoBold9pt7b.h>
////#include <Fonts/Picopixel.h>
////#include "Fonts/Org_01.h"
//#include "Fonts/GFXFF/FreeMonoBoldOblique9pt7b.h"
//#include "Fonts/GFXFF/FreeSerif9pt7b.h"
//#include "FreeSerif9pt7b.h"

//#include "Free_Fonts.h" // Include the header file attached to this sketch


#include "Final_Frontier_28.h"
#include "Latin_Hiragana_24.h"
#include "Unicode_Test_72.h"
#include "MyFont.h" 



#if defined(ENABLE_AHRS)
#include "AHRS.h"
#endif /* ENABLE_AHRS */

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


void  Radar_Task(void* pvParameters)
{
 
    for (;; )
    {
       // TFT_loop();
        //tftModule.Update();
        esp_task_wdt_reset();
        vTaskDelay(10);
        yield();
    }
}


void setup()
{

  Serial.begin(115200);

  rst_info *resetInfo;

  hw_info.soc = SoC_setup();              // Has to be very first procedure in the execution order
  esp_task_wdt_init(WDT_TIMEOUT, false);  //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);                 //add current thread to WDT watch

  resetInfo = (rst_info *) SoC->getResetInfoPtr();

  Serial.println();
  Serial.print(F(SOFTRF_IDENT "-"));
  Serial.print(SoC->name);
  Serial.print(F(" FW.REV: " SOFTRF_FIRMWARE_VERSION " DEV.ID: "));
  Serial.println(String(SoC->getChipId(), HEX));
  Serial.println(F("Copyright (C) 2015-2023 Linar Yusupov. All rights reserved."));
  esp_task_wdt_reset();
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

  hw_info.rf = RF_setup();
  esp_task_wdt_reset();
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
  }
  else
#endif /* EXCLUDE_MAVLINK */
  {
    hw_info.gnss = GNSS_setup();
    Serial.print("hw_info.gnss = ");
    Serial.println(hw_info.gnss);
    ThisAircraft.aircraft_type = settings->aircraft_type;
    Serial.print("ThisAircraft.aircraft_type = ");
    Serial.println(ThisAircraft.aircraft_type);

  }
  ThisAircraft.protocol = settings->rf_protocol;
  ThisAircraft.stealth  = settings->stealth;
  ThisAircraft.no_track = settings->no_track;

  Battery_setup();
  Traffic_setup();
  esp_task_wdt_reset();
  SoC->swSer_enableRx(false);

  LED_setup();

  WiFi_setup();
  esp_task_wdt_reset();
  if (SoC->USB_ops) 
  {
     SoC->USB_ops->setup();
  }

  if (SoC->Bluetooth_ops) 
  {
     SoC->Bluetooth_ops->setup();
  }

  OTA_setup();
  Web_setup();
  NMEA_setup();

#if defined(ENABLE_TTN)
  TTN_setup();
#endif
  esp_task_wdt_reset();
  delay(1000);

  /* expedite restart on WDT reset */
 /* if (resetInfo->reason != REASON_WDT_RST) 
  {
    LED_test();
  }*/

  Sound_setup();
  SoC->Sound_test(resetInfo->reason);

  switch (settings->mode)
  {
  case SOFTRF_MODE_TXRX_TEST:
  case SOFTRF_MODE_WATCHOUT:
    Time_setup();
    break;
  case SOFTRF_MODE_BRIDGE:
    break;
  case SOFTRF_MODE_NORMAL:
  case SOFTRF_MODE_UAV:
  default:
    SoC->swSer_enableRx(true);
    break;
  }
  esp_task_wdt_reset();
  Recorder_setup();

  //TFT_setup();
  tftModule.Setup();
  //screenIdleTimer = millis();

  //TFTScreen->onAction(screenAction);  // 


  SoC->post_init();
  esp_task_wdt_reset();

  //Serial.print("setup() running on core ");
  ////  "Блок setup() выполняется на ядре "
  //Serial.println(xPortGetCoreID());


  // xTaskCreatePinnedToCore(
  //Task1code, /* Функция, содержащая код задачи */
  // "Task1", /* Название задачи */
  //     10000, /* Размер стека в словах */
  //     NULL, /* Параметр создаваемой задачи */
  //     0, /* Приоритет задачи */
  //     & Task1, /* Идентификатор задачи */
  //     0); /* Ядро, на котором будет выполняться задача */
  // 

  xTaskCreatePinnedToCore(Radar_Task, "Radar_Task", 2048, NULL, 10, NULL, 0);


  //SoC->WDT_setup();
  esp_err_t esp_task_wdt_delete(NULL);
  SoC->WDT_fini();
  esp_task_wdt_reset();

}

void loop()
{
  // Do common RF stuff first
  RF_loop();
  esp_task_wdt_reset();


  switch (settings->mode)
  {
#if !defined(EXCLUDE_TEST_MODE)
  case SOFTRF_MODE_TXRX_TEST:
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
  esp_task_wdt_reset();
  // Show status info on tiny OLED display
  SoC->Display_loop();
  esp_task_wdt_reset();
  // battery status LED
 // LED_loop();
  esp_task_wdt_reset();
  // Handle DNS
  WiFi_loop();
  esp_task_wdt_reset();
  //Handle Web
  Web_loop();
  esp_task_wdt_reset();
  // Handle OTA update.
  OTA_loop();
  esp_task_wdt_reset();
  Recorder_loop();
  esp_task_wdt_reset();
  SoC->loop();
  esp_task_wdt_reset();
   if (SoC->USB_ops) {
    SoC->USB_ops->loop();
  }
  esp_task_wdt_reset();
  if (SoC->UART_ops) {
     SoC->UART_ops->loop();
  }
  esp_task_wdt_reset();
  Battery_loop(); 

  SoC->Button_loop();
  esp_task_wdt_reset();
  Time_loop();
  esp_task_wdt_reset();

  tftModule.Update();

  esp_task_wdt_reset();
 
  
   //TFT_loop();

  yield();
}

void shutdown(int reason)
{
  SoC->WDT_fini();

  SoC->swSer_enableRx(false);

  Recorder_fini();

  Sound_fini();

  NMEA_fini();

  Web_fini();

  if (SoC->Bluetooth_ops) {
     SoC->Bluetooth_ops->fini();
  }

  if (SoC->USB_ops) {
     SoC->USB_ops->fini();
  }

  WiFi_fini();

  if (settings->mode != SOFTRF_MODE_UAV) 
  {
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
  if (isValidFix()) 
  {
    ThisAircraft.latitude  = gnss.location.lat();
    ThisAircraft.longitude = gnss.location.lng();
    ThisAircraft.altitude  = gnss.altitude.meters();
    ThisAircraft.course    = gnss.course.deg();
    ThisAircraft.speed     = gnss.speed.knots();
    ThisAircraft.hdop      = (uint16_t) gnss.hdop.value();
    ThisAircraft.geoid_separation = gnss.separation.meters();

#if !defined(EXCLUDE_EGM96)
    /*
     * Если расстояние между геоидами равно нулю или недоступно, используйте прибл. Значение EGM96
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

#if defined(ENABLE_TTN)
  TTN_loop();
#endif

  if (isValidFix()) {
    Traffic_loop();
  }

  if (isTimeToDisplay()) {
    if (isValidFix()) {
      LED_DisplayTraffic();
    } else {
      LED_Clear();
    }
    LEDTimeMarker = millis();
  }

  Sound_loop();

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

  if (isTimeToDisplay()) {
    LED_Clear();
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

  if (isTimeToDisplay()) {
    LED_Clear();
    LEDTimeMarker = millis();
  }
}
#endif /* EXCLUDE_WATCHOUT_MODE */

#if !defined(EXCLUDE_TEST_MODE)

unsigned int pos_ndx = 0;
unsigned long TxPosUpdMarker = 0;

int chorno = 0;

float altitude1 = 100.0;
bool alt_high = false;
bool alien_dist = false;

float alien_lat = 55.950197;
float alien_lon = 38.207984;
float alien_lat1 = 55.927032;
float alien_lon1 = 38.306560;

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
 
  if (TxPosUpdMarker == 0 || (millis() - TxPosUpdMarker) > 4000 ) 
  {
    //ThisAircraft.latitude  = pgm_read_float( &txrx_test_positions[pos_ndx][0]);
    //ThisAircraft.longitude = pgm_read_float( &txrx_test_positions[pos_ndx][1]);
    pos_ndx = (pos_ndx + 1) % TXRX_TEST_NUM_POSITIONS;

    /*
    56.011918, 38.356383 Черноголовка

    56.023279, 38.351918  Афанасово-3

    56.042176, 38.483176 село Стромынь
    55.950197, 38.207984 деревня Мизиново
    19 км.
    56.011524, 38.377847  проезд Строителей, 1Б Средняя точка

    56.026725, 38.291524  село Ивановское
    55.993891, 38.339010  городской округ Черноголовка

    55.927032, 38.306560 село Воскресенское
    56.023282, 38.351907 деревня Афанасово-3

       */

    int set_air = 1;   // Варианты движения самолетов


    switch (set_air)
    {
    case 0:
        ThisAircraft.latitude = pgm_read_float(&txrx_test_positions[pos_ndx][0]);
        ThisAircraft.longitude = pgm_read_float(&txrx_test_positions[pos_ndx][1]);
        /*
                if (!alt_high)
                {
                    altitude1 += 100;
                    if (altitude1 > 4000)
                    {
                        altitude1 = 4000;
                        alt_high = true;
                    }
                }
                if (alt_high)
                {
                    altitude1 -= 100;
                    if (altitude1 < 100)
                    {
                        altitude1 = 100;
                        alt_high = false;
                    }
                }
        */
        altitude1 = 100;

        break;
    case 1:
        ThisAircraft.latitude = 55.996177;  //55.950197 + (0.002299*20);//55.950197
        ThisAircraft.longitude = 38.345584; //38.207984 + (0.006880*20);//
        altitude1 = 100;
        break;
    case 2:

        /* Параметры первого тестового самолета
        56.042176, 38.483176 село Стромынь
        55.950197, 38.207984 деревня Мизиново
        19 км.
        */
        if (!alien_dist)
        {
            alien_lat += 0.002299;
            alien_lon += 0.006880;

            if (alien_lat >= 56.042176)
            {
                alien_lat = 56.042176;
                alien_dist = true;
            }
        }

        if (alien_dist)
        {
            alien_lat -= 0.002299;
            alien_lon -= 0.006880;

            if (alien_lat <= 55.950197)
            {
                alien_lat = 55.950197;
                alien_dist = false;
            }
        }
        ThisAircraft.latitude = alien_lat;
        ThisAircraft.longitude = alien_lon;

        /* Изменяем высоту*/
        if (!alt_high)
        {
            altitude1 += 10;
            if (altitude1 > 200)
            {
                altitude1 = 200;
                alt_high = true;
            }
        }
        if (alt_high)
        {

            altitude1 -= 10;
            if (altitude1 < 30)
            {
                altitude1 = 30;
                alt_high = false;
            }
        }

        break;
    case 3:
        /*Параметры второго тестового самолета
         56.026725, 38.291524  село Ивановское
         55.993891, 38.339010  городской округ Черноголовка
         */
        if (!alien_dist)
        {
            alien_lat1 += 0.004598 / 4;
            alien_lon1 += 0.013759 / 4;

            if (alien_lat1 >= 56.026725)
            {

                alien_lat1 = 56.026725;
                alien_dist = true;
            }
        }

        if (alien_dist)
        {
            alien_lat1 -= (0.004598 / 4);
            alien_lon1 -= (0.013759 / 4);

            if (alien_lat1 <= 55.993891)
            {
                alien_lat1 = 55.993891;
                alien_dist = false;
            }
        }

        ThisAircraft.latitude = alien_lat1;
        ThisAircraft.longitude = alien_lon1;
        /* Изменяем высоту*/
        if (!alt_high)
        {
            altitude1 += 10;
            if (altitude1 > 200)
            {
                altitude1 = 200;
                alt_high = true;
            }
        }
        if (alt_high)
        {

            altitude1 -= 10;
            if (altitude1 < 30)
            {
                altitude1 = 30;
                alt_high = false;
            }
        }
        break;

    case 4:
        /*

       56.023282, 38.351907 деревня Афанасово-3
        */
        if (!alien_dist)
        {
            alien_lat1 += 0.0006774;
            alien_lon1 += 0.000158175;

            if (alien_lat1 >= 56.023282)
            {

                alien_lat1 = 56.023282;
                alien_dist = true;
            }
        }

        if (alien_dist)
        {
            alien_lat1 -= 0.0006774;
            alien_lon1 -= 0.000158175;

            if (alien_lat1 <= 55.996186)
            {
                alien_lat1 = 55.996186;
                alien_dist = false;
            }
        }

        ThisAircraft.latitude = alien_lat1;
        ThisAircraft.longitude = alien_lon1;
        /* Изменяем высоту*/
        if (!alt_high)
        {
            altitude1 += 10;
            if (altitude1 > 200)
            {
                altitude1 = 200;
                alt_high = true;
            }
        }
        if (alt_high)
        {

            altitude1 -= 10;
            if (altitude1 < 30)
            {
                altitude1 = 30;
                alt_high = false;
            }
        }
        break;


    default:
        break;
        }
    TxPosUpdMarker = millis();
    }


  ThisAircraft.altitude = altitude1;// TXRX_TEST_ALTITUDE;
  //ThisAircraft.altitude = TXRX_TEST_ALTITUDE;
  ThisAircraft.course = TXRX_TEST_COURSE;
  ThisAircraft.speed = TXRX_TEST_SPEED;
  ThisAircraft.vs = TXRX_TEST_VS;

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
  if (isTimeToDisplay()) {
    LED_DisplayTraffic();
    LEDTimeMarker = millis();
  }
#if DEBUG_TIMING
  led_end_ms = millis();
#endif

  //Sound_loop();

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
//  SoC->Display_loop();
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

//---------------------------------------------------------------------------------------------------------------

