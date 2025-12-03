#include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <esp_task_wdt.h>
#include <iostream>
#include <locale.h>
#include <math.h>

#include "OTA.h"
#include "TimeRF.h"
#include "GNSS.h"
#include "RF.h"
#include "EEPROMRF.h"
#include "NMEA.h"
#include "SoC.h"
#include "WiFiRF.h"
#include "WebRF.h"
#include "Baro.h"
#include "TrafficHelper.h"
#include "ESP32RF.h"
#include <TimeLib.h>
#include <TinyGPS++.h>
#include "ServiceMain.h"
#include "Configuration_ESP32.h"
#include "Button.h"
#include <HardwareSerial.h>
#include "SoftRF.h"


//=================================================================
#define RS485_SERIAL   Serial2
#define RS485_TX_PIN   18
#define RS485_RX_PIN   17
#define RS485_DE_PIN   21
#define RS485_BAUD     115200// 256000 //921600
#define RS485_CONFIG   SERIAL_8N1
#define LED            4  

#if !defined(SERIAL_FLUSH)
#define SERIAL_FLUSH() Serial.flush()
#endif

#define DEBUG 0
#define DEBUG_TIMING 0

#define isTimeToDisplay() (millis() - LEDTimeMarker     > 1000)
#define isTimeToExport()  (millis() - ExportTimeMarker  > 1000)

int threshold_level_tmp = 300;
int set_air = 0;   //  
bool set_test_coordinate = false; // Признак тестовых ввода текущих координат 
bool set_test_coordinate5 = false; // Признак тестовых ввода текущих координат 

static void rs485SetTX(bool enable)
{
    digitalWrite(RS485_DE_PIN, enable ? HIGH : LOW);
    if (enable) delayMicroseconds(50);
}

void setupRS485()
{
    RS485_SERIAL.setRxBufferSize(1024);
    RS485_SERIAL.setTxBufferSize(1024);
    RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
    pinMode(RS485_DE_PIN, OUTPUT);
    digitalWrite(RS485_DE_PIN, LOW);
    rs485SetTX(false);
}



ufo_t ThisAircraft;
extern ufo_t fo, Container[MAX_TRACKING_OBJECTS];

//===============================================================
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

static void onButtonPressDownCb(void* button_handle, void* usr_data) 
{
   service.set_num_button(1);
}

static void onButtonDoubleClickEventCb(void* button_handle, void* usr_data)
{
   service.set_num_button(2);
}

static void onButtonLongPressStartEventCb(void* button_handle, void* usr_data)
{
   service.set_num_button(3);
}


void setup()
{
    rst_info* resetInfo;

    hw_info.soc = SoC_setup(); // Has to be very first procedure in the execution order

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
/*
  if (settings->default_settings == SETTINGS_ON)
  {
      EEPROM_clear();
  }
*/
  EEPROM_setup();

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

  if (settings->input_coordinates == IMPUT_COORD_MANUAL)
  {
      ThisAircraft.test_latitude = settings->test_latitude;
      ThisAircraft.test_longitude = settings->test_longitude;
  }


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
      set_air = 0;
      break;
  }

  SoC->post_init();
   

  if (psramInit() == false)
      Serial.println("PSRAM failed to initialize");
  else
      Serial.println("PSRAM initialized");

  Serial.printf("PSRAM Size available (bytes): %d\r\n", ESP.getFreePsram());

  heap_caps_malloc_extmem_enable(8000); //Use PSRAM for memory requests larger than 1,000 bytes
 // CommandHandler.setup();

  //------------------------------------------------------------------------------

  setupRS485();

  // initializing a button
  Button* btn = new Button(GPIO_NUM_48, false);

  btn->attachPressDownEventCb(&onButtonPressDownCb, NULL);
  btn->attachDoubleClickEventCb(&onButtonDoubleClickEventCb, NULL);
  btn->attachLongPressStartEventCb(onButtonLongPressStartEventCb, NULL);
 

  Serial.print("Sizeof full_packet_net_t: "); Serial.println(sizeof(full_packet_net_t));

  if (settings->threshold_level != threshold_level_tmp)
  {
      threshold_level_tmp = settings->threshold_level;
      //values[0] = settings->threshold_level;
      //sendPacketToRP2040(values, 1);
  }

 
  SoC->WDT_setup();

  Serial.println("================ Setup End =======================");
}

void loop()
{

  esp_task_wdt_reset();
 
  // Show status info on tiny OLED display
  SoC->Display_loop();

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

    SoC_fini(reason);
}



