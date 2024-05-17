

#include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <TFT_eSPI.h>             // Поддержка TFT дисплея  
#include "TFTModule.h" 
#include "Configuration_ESP32.h"
#include <esp_task_wdt.h>


#include "OTA.h"
//#include "EEPROMRF.h"
//#include "WiFiRF.h"
//#include "WebRF.h"
#include <TimeLib.h>

#include <WiFi.h>
#include <WebServer.h>
/* Установите здесь свои SSID и пароль */
const char* ssid = "ESP32";
const char* password = "12345678";
/* Настройки IP адреса */
IPAddress local_ip(192, 168, 2, 1);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);
WebServer server(80);



TFTModule tftModule;
uint32_t screenIdleTimer = 0;
 
#if !defined(SERIAL_FLUSH)
#define SERIAL_FLUSH() Serial.flush()
#endif


void setup()
{
 
  String ver_soft = __FILE__;
  int val_srt = ver_soft.lastIndexOf('\\');
  ver_soft.remove(0, val_srt+1);
  val_srt = ver_soft.lastIndexOf('.');
  ver_soft.remove(val_srt);
  Serial.println(ver_soft);

  SERIAL_FLUSH();

  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);
 // server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("HTTP server started");


  /*
  EEPROM_setup();

  WiFi_setup();*/

  
  OTA_setup();
 // Web_setup();
  
  tftModule.Setup();
  MainScreen->saveVer(ver_soft);  // Сохранить строку с текущей версией.

}

int thisByte = 33;


void loop()
{

  esp_task_wdt_reset();
  
  // Handle DNS
 // WiFi_loop();

  // Handle Web
  //Web_loop();

  // Handle OTA update.
  OTA_loop();

  tftModule.Update();

  yield();
}

