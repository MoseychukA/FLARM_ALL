 #include <stdio.h>                // define I/O functions//#include "MAVLinkRF.h"
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <esp_task_wdt.h>
#include <iostream>
#include <locale.h>

#include "OTA.h"
#include "TimeRF.h"
#include "GNSS.h"
#include "RF.h"
#include "EEPROMRF.h"
#include "NMEA.h"
#include "D1090.h"
#include "SoC.h"
#include "WiFiRF.h"
#include "WebRF.h"
#include "Baro.h"
#include "TrafficHelper.h"
#include "ESP32RF.h"
#include <TimeLib.h>
#include <TinyGPS++.h>
#include "ServiceMain.h"

int set_air = 0;   //  
 
void txrx_test();
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
  .model    = DEFAULT_FLYRF_MODEL,
  .revision = 0,
  .soc      = SOC_NONE,
  .rf       = RF_IC_NONE,
  .gnss     = GNSS_MODULE_NONE,
  .baro     = BARO_MODULE_NONE,
  .display  = DISPLAY_NONE,
};

unsigned long LEDTimeMarker = 0;
unsigned long ExportTimeMarker = 0;


void setup()
{
    setlocale(LC_ALL, "Russian");
    rst_info* resetInfo;

    hw_info.soc = SoC_setup(); // Has to be very first procedure in the execution order
   // SERIAL_TRACKER.begin(SERIAL_TRACKER_SPEED, SERIAL_IN_BITS, 39, 15);
    resetInfo = (rst_info*)SoC->getResetInfoPtr();

    Serial.println();
    Serial.print(F(FLYRF_IDENT "-"));
    Serial.print(SoC->name);
    Serial.print(F(" FW.REV: " FLYRF_FIRMWARE_VERSION " DEV.ID: "));
    Serial.println(String(SoC->getChipId(), HEX));

    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    ver_soft.remove(val_srt);
    Serial.println(ver_soft);
    service.saveVer(ver_soft);  // Сохранить строку с текущей версией.

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

  //SoC->Button_setup();

  ThisAircraft.addr = SoC->getChipId() & 0x00FFFFFF;

  hw_info.rf = RF_setup();

  delay(100);

  hw_info.baro = Baro_setup();

  hw_info.display = SoC->Display_setup();

  hw_info.gnss = GNSS_setup();
  ThisAircraft.aircraft_type = settings->aircraft_type;
 
  ThisAircraft.protocol = settings->rf_protocol;
  ThisAircraft.stealth  = settings->stealth;
  ThisAircraft.no_track = settings->no_track;

  Traffic_setup();

  SoC->swSer_enableRx(false);

  WiFi_setup();

 
  if (SoC->Bluetooth_ops) 
  {
     SoC->Bluetooth_ops->setup();
  }

  OTA_setup();
  Web_setup();
  NMEA_setup();


  delay(1000);

  switch (settings->mode)
  {
  case FLYRF_MODE_TXRX_TEST0:
      Time_setup();
      set_air = 0;
      break;
  case FLYRF_MODE_TXRX_TEST1:
      Time_setup();
      set_air = 1;
      break;
  case FLYRF_MODE_TXRX_TEST2:
      set_air = 2;
      Time_setup();
      break;
  case FLYRF_MODE_TXRX_TEST3:
      set_air = 3;
      Time_setup();
      break;
  case FLYRF_MODE_TXRX_TEST4:
      set_air = 4;
      Time_setup();
      break;
  case FLYRF_MODE_TXRX_TEST5:
      set_air = 5;
      Time_setup();
      break;
  case FLYRF_MODE_NORMAL:
  default:
      SoC->swSer_enableRx(true);
      set_air = 0;
      break;
  }
  //service.saveVer(ver_soft);  // Сохранить строку с текущей версией.
  SoC->post_init();

  if (psramInit() == false)
      Serial.println("PSRAM failed to initialize");
  else
      Serial.println("PSRAM initialized");

  Serial.printf("PSRAM Size available (bytes): %d\r\n", ESP.getFreePsram());

  heap_caps_malloc_extmem_enable(8000); //Use PSRAM for memory requests larger than 1,000 bytes

  Serial.printf("PSRAM Size available (bytes): %d\r\n", ESP.getFreePsram());

  SoC->WDT_setup();
}

void loop()
{
   RF_loop();  // Сначала выполните общие действия с радиочастотами

  switch (settings->mode)
  {
#if !defined(EXCLUDE_TEST_MODE)
  case FLYRF_MODE_TXRX_TEST0:
  case FLYRF_MODE_TXRX_TEST1:
  case FLYRF_MODE_TXRX_TEST2:
  case FLYRF_MODE_TXRX_TEST3:
  case FLYRF_MODE_TXRX_TEST4:
  case FLYRF_MODE_TXRX_TEST5:
    txrx_test();
    break;
#endif /* EXCLUDE_TEST_MODE */
  case FLYRF_MODE_NORMAL:
  default:
    normal();
    break;
  }

  // Show status info on tiny OLED display
  SoC->Display_loop();

  // Handle DNS
  WiFi_loop();

  // Handle Web
  Web_loop();

  // Handle OTA update.
  OTA_loop();

  SoC->loop();

  if (SoC->Bluetooth_ops) 
  {
    SoC->Bluetooth_ops->loop();
  }

//  if (SoC->USB_ops) {
//    SoC->USB_ops->loop();
//  }
//
//  if (SoC->UART_ops) {
//     SoC->UART_ops->loop();
//  }

//  SoC->Button_loop();
//
  Time_loop();

  yield();
}

void shutdown(int reason)
{
  SoC->WDT_fini();

  SoC->swSer_enableRx(false);

   NMEA_fini();

  Web_fini();

  if (SoC->Bluetooth_ops) {
     SoC->Bluetooth_ops->fini();
  }

  if (SoC->USB_ops) {
     SoC->USB_ops->fini();
  }

  WiFi_fini();


   GNSS_fini();
  

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

  if (isTimeToDisplay()) 
  {
 /*   if (isValidFix()) 
    {
      LED_DisplayTraffic();
    } else {
      LED_Clear();
    }*/
    LEDTimeMarker = millis();
  }

  if (isTimeToExport()) {
    NMEA_Export();
    D1090_Export();

    ExportTimeMarker = millis();
  }

  // Handle Air Connect
  NMEA_loop();

  ClearExpired();
}

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

  if (TxPosUpdMarker == 0 || (millis() - TxPosUpdMarker) > 2000 ) 
  {
 
    ThisAircraft.latitude  = 56.026725;
    ThisAircraft.longitude = 38.291524;   
    pos_ndx = (pos_ndx + 1) % TXRX_TEST_NUM_POSITIONS;
    TxPosUpdMarker = millis();
  }
  ThisAircraft.altitude = 120;//TXRX_TEST_ALTITUDE;
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
 /* if (isTimeToDisplay()) {
    LED_DisplayTraffic();
    LEDTimeMarker = millis();
  }*/
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
    D1090_Export();
    ExportTimeMarker = millis();
  }
#if DEBUG_TIMING
  export_end_ms = millis();
#endif

#if DEBUG_TIMING
  oled_start_ms = millis();
#endif
  SoC->Display_loop();
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
