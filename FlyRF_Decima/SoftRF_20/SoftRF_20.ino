



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
#include <egm96s.h>
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
//#include <egm96s.h>
//#include <BLEDevice.h>
#include "Adafruit_GFX.h"
//#include <GxEPD2_BW.h>



#define  XPOWERS_CHIP_AXP2102
#include <XPowersLib.h>

#include <esp_task_wdt.h>

#define WDT_TIMEOUT 8

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
//#include "EPD.h"
#include "TFTHelper.h"

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

//extern ui_settings_t ui_settings;
//extern ui_settings_t* ui;

unsigned long LEDTimeMarker = 0;
unsigned long ExportTimeMarker = 0;


//void  Radar_Task(void* pvParameters)
//{
//    //  unsigned long LockTime = millis();
//
//    for (;; )
//    {
//        if (EPD_update_in_progress != EPD_UPDATE_NONE) 
//        {
//            //    if (SoC->Display_lock()) {
//            Serial.println("EPD_Task: lock"); Serial.flush();
//
//            //      LockTime = millis();
//            //!!display->display(EPD_update_in_progress == EPD_UPDATE_FAST ? true : false);
//            Serial.println("EPD_Task: display"); Serial.flush();
//            yield();
//
//            /*
//             * SYX 1942 revision of D67 display can use power_off() after partial update,
//             * SYX 1948 revision - can not.
//             */
//            if (EPD_update_in_progress == EPD_UPDATE_FAST) { /* EPD_POWEROFF; */ }
//
//            EPD_update_in_progress = EPD_UPDATE_NONE;
//            //      SoC->Display_unlock();
//            Serial.println("EPD_Task: unlock"); Serial.flush();
//            //      delay(100);
//            //    } else {
//            //      if (millis() - LockTime > 4000) {
//            //        SoC->Display_unlock();
//            //Serial.println("EPD_Task: reseet lock"); Serial.flush();
//            //      }
//
//        }
//
//        yield();
//    }
//}


void setup()
{
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
  if (resetInfo->reason != REASON_WDT_RST) 
  {
    LED_test();
  }

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

 // EPD_setup();

  TFT_setup();

  SoC->post_init();
  

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

 // xTaskCreatePinnedToCore(Radar_Task, "Radar_Task", 4096, NULL, 1, NULL, 0);


  SoC->WDT_setup();
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
  LED_loop();
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

 /* if (SoC->Bluetooth_ops) 
  {
    SoC->Bluetooth_ops->loop();
  }*/

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

  //EPD_loop();

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

  if (isValidMAVFix()) {
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

  if (TxPosUpdMarker == 0 || (millis() - TxPosUpdMarker) > 4000 ) {
    ThisAircraft.latitude  = pgm_read_float( &txrx_test_positions[pos_ndx][0]);
    ThisAircraft.longitude = pgm_read_float( &txrx_test_positions[pos_ndx][1]);
    pos_ndx = (pos_ndx + 1) % TXRX_TEST_NUM_POSITIONS;
    TxPosUpdMarker = millis();
  }
  ThisAircraft.altitude = TXRX_TEST_ALTITUDE;
  ThisAircraft.course   = TXRX_TEST_COURSE;
  ThisAircraft.speed    = TXRX_TEST_SPEED;
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
  if (isTimeToDisplay()) {
    LED_DisplayTraffic();
    LEDTimeMarker = millis();
  }
#if DEBUG_TIMING
  led_end_ms = millis();
#endif

  Sound_loop();

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

