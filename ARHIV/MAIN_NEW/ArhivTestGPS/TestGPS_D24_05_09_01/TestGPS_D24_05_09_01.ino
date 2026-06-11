

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
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
WebServer server(80);
uint8_t LED1pin = 4;
bool LED1status = LOW;
uint8_t LED2pin = 2;
bool LED2status = LOW;

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

  pinMode(LED1pin, OUTPUT);
  pinMode(LED2pin, OUTPUT);
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);
  server.on("/", handle_OnConnect);
  server.on("/led1on", handle_led1on);
  server.on("/led1off", handle_led1off);
  server.on("/led2on", handle_led2on);
  server.on("/led2off", handle_led2off);
  server.onNotFound(handle_NotFound);
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
  server.handleClient();
  if (LED1status)
  {
      digitalWrite(LED1pin, LOW);
  }
  else
  {
      digitalWrite(LED1pin, HIGH);
  }
  if (LED2status)
  {
      digitalWrite(LED2pin, LOW);
  }
  else
  {
      digitalWrite(LED2pin, HIGH);
  }
  tftModule.Update();

  yield();
}

void handle_OnConnect() {
    LED1status = HIGH;
    LED2status = HIGH;
    Serial.println("GPIO4 Status: OFF | GPIO5 Status: OFF");
    server.send(200, "text/html", SendHTML(LED1status, LED2status));
}
void handle_led1on() {
    LED1status = HIGH;
    Serial.println("GPIO4 Status: ON");
    server.send(200, "text/html", SendHTML(true, LED2status));
}
void handle_led1off() {
    LED1status = LOW;
    Serial.println("GPIO4 Status: OFF");
    server.send(200, "text/html", SendHTML(false, LED2status));
}
void handle_led2on() {
    LED2status = HIGH;
    Serial.println("GPIO5 Status: ON");
    server.send(200, "text/html", SendHTML(LED1status, true));
}
void handle_led2off() {
    LED2status = LOW;
    Serial.println("GPIO5 Status: OFF");
    server.send(200, "text/html", SendHTML(LED1status, false));
}
void handle_NotFound() {
    server.send(404, "text/plain", "Not found");
}
String SendHTML(uint8_t led1stat, uint8_t led2stat) {
    String ptr = "<!DOCTYPE html> <html>\n";
    ptr += "<meta http-equiv=\"Content-type\" content=\"text/html; charset=utf-8\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
    ptr += "<title>Управление светодиодом</title>\n";
    ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
    ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
    ptr += ".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
    ptr += ".button-on {background-color: #3498db;}\n";
    ptr += ".button-on:active {background-color: #2980b9;}\n";
    ptr += ".button-off {background-color: #34495e;}\n";
    ptr += ".button-off:active {background-color: #2c3e50;}\n";
    ptr += "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
    ptr += "</style>\n";
    ptr += "</head>\n";
    ptr += "<body>\n";
    ptr += "<h1>ESP32 Веб сервер</h1>\n";
    ptr += "<h3>Режим точка доступа WiFi (AP)</h3>\n";
    if (led1stat)
    {
        ptr += "<p>Состояние LED1: ВКЛ.</p><a class=\"button button-off\" href=\"/led1off\">ВЫКЛ.</a>\n";
    }
    else
    {
        ptr += "<p>Состояние LED1: ВЫКЛ.</p><a class=\"button button-on\" href=\"/led1on\">ВКЛ.</a>\n";
    }
    if (led2stat)
    {
        ptr += "<p>Состояние LED2: ВКЛ.</p><a class=\"button button-off\" href=\"/led2off\">ВЫКЛ.</a>\n";
    }
    else
    {
        ptr += "<p>Состояние LED2: ВЫКЛ.</p><a class=\"button button-on\" href=\"/led2on\">ВКЛ.</a>\n";
    }
    ptr += "</body>\n";
    ptr += "</html>\n";
    return ptr;
}
