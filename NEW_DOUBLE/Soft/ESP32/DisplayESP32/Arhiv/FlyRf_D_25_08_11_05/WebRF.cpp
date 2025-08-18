/*
 * WebHelper.cpp
 * Copyright (C) 2016-2023 Linar Yusupov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SoC.h"

#if defined(EXCLUDE_WIFI)
void Web_setup()    {}
void Web_loop()     {}
void Web_fini()     {}
#else

#include <Arduino.h>

#include "RF.h"
#include "WebRF.h"
#include "TrafficHelper.h"
#include "NMEA.h"
#include "TimeRF.h"
#include "ServiceMain.h"

static uint32_t prev_rx_pkt_cnt = 0;


#include "jquery_min_js.h"

byte getVal(char c)
{
   if(c >= '0' && c <= '9')
     return (byte)(c - '0');
   else
     return (byte)(toupper(c)-'A'+10);
}


void hardwareSettings()
{
    //hardware_settings
    size_t size = 5400;
    char* offset;
    size_t len = 0;
    char* Settings_temp = (char*)malloc(size);

    if (Settings_temp == NULL) {
        return;
    }

    offset = Settings_temp;

    /* Common part 1 */

snprintf_P(offset, size,
PSTR("<html>\
<head>\
<meta name='viewport' content='width=device-width, initial-scale=1'>\
<title>hardwareSettings</title>\
</head>\
<body>\
<h1 align=center>Аппаратные настройки</h1>\
<form action='/hardware_input' method='GET'>\
<table width=100%%>\
<tr>\
<th align=left>Отобразить уровень сигнала LoRa</th>\
<td align=right>\
<select name='rssi_view'>\
<option % s value = '%d'>Выключен</option>\
<option % s value = '%d'>Включен</option>\
</select>\
</td>\
</tr>\
<tr>\
<th align=left>Отобразить размер памяти RAM</th>\
<td align=right>\
<select name='ram_view'>\
<option % s value = '%d'>Выключен</option>\
<option % s value = '%d'>Включен</option>\
</select>\
</td>\
</tr>\
<tr>\
<th align=left>Отобразить заряд аккумулятора</th>\
<td align=right>\
<select name='akk_view'>\
<option % s value = '%d'>Выключен</option>\
<option % s value = '%d'>Включен</option>\
</select>\
</td>\
</tr>\
<tr>\
<th align=left>Отобразить кнопку SOS</th>\
<td align=right>\
<select name='sos_view'>\
<option % s value = '%d'>Выключен</option>\
<option % s value = '%d'>Включен</option>\
</select>\
</td>\
</tr>"),
(settings->rssi_view == VIEW_RSSI_OFF ? "selected" : ""), VIEW_RSSI_OFF,
(settings->rssi_view == VIEW_RSSI_ON ? "selected" : ""), VIEW_RSSI_ON,
(settings->ram_view == VIEW_RAM_OFF ? "selected" : ""), VIEW_RAM_OFF,
(settings->ram_view == VIEW_RAM_ON ? "selected" : ""), VIEW_RAM_ON,
(settings->akk_view == VIEW_AKK_OFF ? "selected" : ""), VIEW_AKK_OFF,
(settings->akk_view == VIEW_AKK_ON ? "selected" : ""), VIEW_AKK_ON, 
(settings->sos_view == VIEW_SOS_OFF ? "selected" : ""), VIEW_SOS_OFF,
(settings->sos_view == VIEW_SOS_ON ? "selected" : ""), VIEW_SOS_ON
);

    len = strlen(offset);
    offset += len;
    size -= len;

     /* Common part 8 */
    snprintf_P(offset, size,
        PSTR("\
</table>\
<p align=center><INPUT type='submit' value='Сохранить и обновить'></p>\
</form>\
</body>\
</html>")
);

    SoC->swSer_enableRx(false);
    server.sendHeader(String(F("Cache-Control")), String(F("no-cache, no-store, must-revalidate")));
    server.sendHeader(String(F("Pragma")), String(F("no-cache")));
    server.sendHeader(String(F("Expires")), String(F("-1")));
    server.send(200, "text/html", Settings_temp);
    SoC->swSer_enableRx(true);
    free(Settings_temp);
}



void handleRoot() 
{

    char str_ver[32];

    String ver = service.getVer();
     ver.toCharArray(str_ver, 32);

    size_t size = 3050; //2420;3920
    char* offset;
    size_t len = 0;

    char* Root_temp = (char*)malloc(size);
    if (Root_temp == NULL) {
        return;
    }
    offset = Root_temp;

    snprintf_P(offset, size,
        PSTR("<html>\
  <head>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
    <title>About</title>\
  </head>\
<body>\
<h1 align=center>Информация</h1>\
<p>Эта программа часть проекта FlyRF.</p>\
<p>URL: https://t.me/flyrf_Support</p>\
<p>URL: https://www.decima.ru/contacts/</p>\
<p>E-mail: decima@decima.ru</p>\
<table width=100%%>\
</table>\
<hr>\
Copyright (C) 2023-2024 &nbsp;&nbsp;&nbsp; OOO Децима\
</body>\
</html>\
<head>\
  <meta name='viewport' content='width=device-width, initial-scale=1'>\
  <title>FlytRF status</title>\
  </head>\
  <body>\
  <table width=100%%>\
  <td align=center><h1>FlyRF статус</h1></td>\
  </table>\
  <table width=100%%>\
  <tr><th align=left>Идентификатор устройства</th><td align=right>%06X</td></tr>\
  <tr><th align=left>Блокировать адрес</th><td align=right>%06X</td></tr>\
  <tr><th align=left>Версия ПО</th><td align=right>%s</td></tr>"
  "</table>\
  <table width=100%%>\
  <tr>\
  "),
        ThisAircraft.addr, settings->block_addr, str_ver   
    );

    len = strlen(offset);
    offset += len;
    size -= len;

    /* SoC specific part 1 */

    snprintf_P(offset, size, PSTR("\
 <td align=left><input type=button onClick=\"location.href='/hardware_settings'\" value='Аппаратные настройки'></td>\
  <td align=center><input type=button onClick=\"location.href='/firmware'\" value='Обновление программы'></td>\
 "));
    len = strlen(offset);
    offset += len;
    size -= len;

    snprintf_P(offset, size, PSTR("\
  </tr>\
 </table>\
</body>\
</html>")
);

    SoC->swSer_enableRx(false);
    server.sendHeader(String(F("Cache-Control")), String(F("no-cache, no-store, must-revalidate")));
    server.sendHeader(String(F("Pragma")), String(F("no-cache")));
    server.sendHeader(String(F("Expires")), String(F("-1")));
    server.send(200, "text/html", Root_temp);
    SoC->swSer_enableRx(true);
    free(Root_temp);
}


//void handleInput() {
//
//  size_t size = 2120; //1700;3100
//
//  char *Input_temp = (char *) malloc(size);
//  if (Input_temp == NULL) {
//    return;
//  }
//
//  for ( uint8_t i = 0; i < server.args(); i++ ) 
//  {
//    if (server.argName(i).equals("mode")) {
//      settings->mode = server.arg(i).toInt();
//    } else if (server.argName(i).equals("acft_type")) {
//      settings->aircraft_type = server.arg(i).toInt();
//    } else if (server.argName(i).equals("gsm_send")) {
//      settings->gsm_send = server.arg(i).toInt();
//    } else if (server.argName(i).equals("bluetooth")) {
//      settings->bluetooth = server.arg(i).toInt();
//    } else if (server.argName(i).equals("nmea_g")) {
//      settings->nmea_g = server.arg(i).toInt();
//    } else if (server.argName(i).equals("nmea_p")) {
//      settings->nmea_p = server.arg(i).toInt();
//    } else if (server.argName(i).equals("nmea_l")) {
//      settings->nmea_l = server.arg(i).toInt();
//    } else if (server.argName(i).equals("nmea_s")) {
//      settings->nmea_s = server.arg(i).toInt();
//    } else if (server.argName(i).equals("nmea_out")) {
//      settings->nmea_out = server.arg(i).toInt();
//    } else if (server.argName(i).equals("d1090")) {
//      settings->d1090 = server.arg(i).toInt();
//    } else if (server.argName(i).equals("attention")) {
//      settings->alarm_attention = server.arg(i).toInt();
//    } else if (server.argName(i).equals("warning")) {
//      settings->alarm_warning = server.arg(i).toInt();
//    } else if (server.argName(i).equals("danger")) {
//      settings->alarm_danger = server.arg(i).toInt();
//    } else if (server.argName(i).equals("height")) {
//      settings->alarm_height = server.arg(i).toInt();
//    }
//  }
//  snprintf_P ( Input_temp, size,
//PSTR("<html>\
//<head>\
//<meta http-equiv='refresh' content='15; url=/'>\
//<meta name='viewport' content='width=device-width, initial-scale=1'>\
//<title>FLYRF Settings</title>\
//</head>\
//<body>\
//<h1 align=center>Новые настройки:</h1>\
//<table width=100%%>\
//<tr><th align=left>Mode</th><td align=right>%d</td></tr>\
//<tr><th align=left>Aircraft type</th><td align=right>%d</td></tr>\
//<tr><th align=left>GSM send</th><td align=right>%d</td></tr>\
//<tr><th align=left>Bluetooth</th><td align=right>%d</td></tr>\
//<tr><th align=left>NMEA GNSS</th><td align=right>%s</td></tr>\
//<tr><th align=left>NMEA Private</th><td align=right>%s</td></tr>\
//<tr><th align=left>NMEA Legacy</th><td align=right>%s</td></tr>\
//<tr><th align=left>NMEA Sensors</th><td align=right>%s</td></tr>\
//<tr><th align=left>NMEA Out</th><td align=right>%d</td></tr>\
//<tr><th align=left>DUMP1090</th><td align=right>%d</td></tr>\
//<tr><th align=left>Тревога внимание</th><td align=right>%d</td></tr>\
//<tr><th align=left>Тревога предупреждение</th><td align=right>%d</td></tr>\
//<tr><th align=left>Тревога опасность</th><td align=right>%d</td></tr>\
//<tr><th align=left>Тревога высота</th><td align=right>%d</td></tr>\
//</table>\
//<hr>\
// <p align=center><h2 align=center>Выполняется перезагрузка... Пожалуйста, подождите!</h2></p>\
//</body>\
//</html>"),
//  settings->mode,
//  settings->aircraft_type, settings->gsm_send, settings->bluetooth,
//  BOOL_STR(settings->nmea_g), BOOL_STR(settings->nmea_p),
//  BOOL_STR(settings->nmea_l), BOOL_STR(settings->nmea_s),
//  settings->nmea_out, settings->d1090,
//  settings->alarm_attention, settings->alarm_warning,
//  settings->alarm_danger, settings->alarm_height
//  );
//  SoC->swSer_enableRx(false);
//  server.send ( 200, "text/html", Input_temp );
//  delay(1000);
//  free(Input_temp);
//  EEPROM_store();
//  RF_Shutdown();
//  delay(1000);
//  SoC->reset();
//}

//==
void hardwareInput() 
{
    size_t size = 3580; //1700;3100

    char* Input_temp = (char*)malloc(size);
    if (Input_temp == NULL) {
        return;
    }

    for (uint8_t i = 0; i < server.args(); i++) 
    {
        if (server.argName(i).equals("rssi_view")) {
            settings->rssi_view = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("ram_view")) {
            settings->ram_view = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("akk_view")) {
            settings->akk_view = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("sos_view")) {
            settings->sos_view = server.arg(i).toInt();
        }
    }

    snprintf_P(Input_temp, size,
        PSTR("<html>\
<head>\
<meta http-equiv='refresh' content='15; url=/'>\
<meta name='viewport' content='width=device-width, initial-scale=1'>\
<title>hardware Settings</title>\
</head>\
<body>\
<h1 align=center>Новые аппаратные настройки:</h1>\
<table width=100%%>\
<tr><th align=left>Отобразить уровень сигнала LoRa</th><td align=right>%d</td></tr>\
<tr><th align=left>Отобразить размер памяти RAM</th><td align=right>%d</td></tr>\
<tr><th align=left>Отобразить заряд аккумулятора</th><td align=right>%d</td></tr>\
<tr><th align=left>Отобразить кнопку SOS</th><td align=right>%d</td></tr>\
<hr>\
 <p align=center><h2 align=center>Выполняется перезагрузка... Пожалуйста, подождите!</h2></p>\
</body>\
</html>"),
settings->rssi_view, settings->ram_view,
settings->akk_view, settings->sos_view
);
    SoC->swSer_enableRx(false);
    server.send(200, "text/html", Input_temp);
    delay(1000);
    free(Input_temp);
    EEPROM_store();
    RF_Shutdown();
    delay(1000);
    SoC->reset();
}

void handleNotFound() 
{
  {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";

    for ( uint8_t i = 0; i < server.args(); i++ ) {
      message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
    }

    server.send ( 404, "text/plain", message );
  }
}

void Web_setup()
{
  server.on ( "/", handleRoot );
  //server.on ( "/settings", handleSettings );
  server.on ("/hardware_settings", hardwareSettings);
  server.on ( "/about", []() {
    SoC->swSer_enableRx(false);
    server.sendHeader(String(F("Cache-Control")), String(F("no-cache, no-store, must-revalidate")));
    server.sendHeader(String(F("Pragma")), String(F("no-cache")));
    server.sendHeader(String(F("Expires")), String(F("-1")));
 /*   server.send_P ( 200, PSTR("text/html"), about_html);*/
    SoC->swSer_enableRx(true);
  } );

 // server.on ( "/input", handleInput );
  server.on ("/hardware_input", hardwareInput);
  server.on ( "/inline", []() {
    server.send ( 200, "text/plain", "this works as well" );
  } );
  server.on("/firmware", HTTP_GET, [](){
    SoC->swSer_enableRx(false);
    server.sendHeader(String(F("Connection")), String(F("close")));
    server.sendHeader(String(F("Access-Control-Allow-Origin")), String(F("*")));
    server.send_P(200,
      PSTR("text/html"),
      PSTR("\
<html>\
  <head>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
    <title>Firmware update</title>\
  </head>\
<body>\
<body>\
 <h1 align=center>Обновление программы</h1>\
 <hr>\
 <table width=100%%>\
  <tr>\
    <td align=left>\
<script src='/jquery.min.js'></script>\
<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>\
    <input type='file' name='update'>\
    <input type='submit' value='Обновление'>\
</form>\
<div id='prg'>progress: 0%</div>\
<script>\
$('form').submit(function(e){\
    e.preventDefault();\
      var form = $('#upload_form')[0];\
      var data = new FormData(form);\
       $.ajax({\
            url: '/update',\
            type: 'POST',\
            data: data,\
            contentType: false,\
            processData:false,\
            xhr: function() {\
                var xhr = new window.XMLHttpRequest();\
                xhr.upload.addEventListener('progress', function(evt) {\
                    if (evt.lengthComputable) {\
                        var per = evt.loaded / evt.total;\
                        $('#prg').html('progress: ' + Math.round(per*100) + '%');\
                    }\
               }, false);\
               return xhr;\
            },\
            success:function(d, s) {\
                console.log('success!')\
           },\
            error: function (a, b, c) {\
            }\
          });\
});\
</script>\
    </td>\
  </tr>\
 </table>\
 <p align=center><h2 align=center>После обновления программы устройство перезагрузится !</h2></p>\
</body>\
</html>")
    );
  SoC->swSer_enableRx(true);
  });
  server.onNotFound ( handleNotFound );

  server.on("/update", HTTP_POST, [](){
    SoC->swSer_enableRx(false);
    server.sendHeader(String(F("Connection")), String(F("close")));
    server.sendHeader(String(F("Access-Control-Allow-Origin")), "*");
    server.send(200, String(F("text/plain")), (Update.hasError())?"FAIL":"OK");
//    SoC->swSer_enableRx(true);
    RF_Shutdown();
    delay(1000);
    SoC->reset();
  },[](){
    HTTPUpload& upload = server.upload();
    if(upload.status == UPLOAD_FILE_START){
      Serial_setDebugOutput(true);
      SoC->WiFiUDP_stopAll();
      SoC->WDT_fini();
      Serial.printf("Update: %s\r\n", upload.filename.c_str());
      uint32_t maxSketchSpace = SoC->maxSketchSpace();
      if(!Update.begin(maxSketchSpace)){//start with max available size
        Update.printError(Serial);
      }
    } else if(upload.status == UPLOAD_FILE_WRITE){
      if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
        Update.printError(Serial);
      }
    } else if(upload.status == UPLOAD_FILE_END){
      if(Update.end(true)){ //true to set the size to the current progress
        Serial.printf("Update Success: %u\r\nRebooting...\r\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
      Serial_setDebugOutput(false);
    }
    yield();
  });


  server.on ( "/jquery.min.js", []() {

    PGM_P content = jquery_min_js_gz;
    size_t bytes_left = jquery_min_js_gz_len;
    size_t chunk_size;

    server.setContentLength(bytes_left);
    server.sendHeader(String(F("Content-Encoding")),String(F("gzip")));
    server.send(200, String(F("application/javascript")), "");

    do {
      chunk_size = bytes_left > JS_MAX_CHUNK_SIZE ? JS_MAX_CHUNK_SIZE : bytes_left;
      server.sendContent_P(content, chunk_size);
      content += chunk_size;
      bytes_left -= chunk_size;
    } while (bytes_left > 0) ;

  } );

#if defined(ENABLE_RECORDER)
  server.on("/flights", HTTP_GET, Handle_Flight_Download);
#endif /* ENABLE_RECORDER */

  server.begin();
  Serial.println (F("HTTP server has started at port: 80"));

  delay(1000);
}

void Web_loop()
{
  server.handleClient();
}

void Web_fini()
{
  server.stop();
}

#endif /* EXCLUDE_WIFI */
