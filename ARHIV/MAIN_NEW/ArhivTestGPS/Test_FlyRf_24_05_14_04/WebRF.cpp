

#include <Arduino.h>

#include "WebRF.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

WebServer server(80);

static uint32_t prev_rx_pkt_cnt = 0;


#include "jquery_min_js.h"

byte getVal(char c)
{
   if(c >= '0' && c <= '9')
     return (byte)(c - '0');
   else
     return (byte)(toupper(c)-'A'+10);
}



static const char about_html[] PROGMEM = "<html>\
  <head>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
    <title>About</title>\
  </head>\
<body>\
<h1 align=center>Информация</h1>\
<p>This firmware is a part of FlyRF project</p>\
<p>URL: https://t.me/flyrf_Support</p>\
<p>Author: <b>Александр Мосейчук</b></p>\
<p>E-mail: Aleksandr.Moseychuk@decima.ru</p>\
<h2 align=center>Параметры системы</h2>\
<p align=center>(in historical order)</p>\
<table width=100%%>\
</table>\
<hr>\
Copyright (C) 2023-2024 &nbsp;&nbsp;&nbsp; OOO Decima\
</body>\
</html>";




//void handleSettings() {
//
//  size_t size = 6990;
//  char *offset;
//  size_t len = 0;
//  char *Settings_temp = (char *) malloc(size);
//
//  if (Settings_temp == NULL) {
//    return;
//  }
//
//  offset = Settings_temp;
//
////  /* Common part 1 */
////  snprintf_P(offset, size,
////      PSTR("<html>\
////<head>\
////<meta name='viewport' content='width=device-width, initial-scale=1'>\
////<title>Settings</title>\
////</head>\
////<body>\
////<h1 align=center>Настройки</h1>\
////<form action='/input' method='GET'>\
////<table width=100%%>\
////<tr>\
////<th align=left>Режим</th>\
////<td align=right>\
////</td>\
////</tr>"
//////)
//////  (settings->mode == SOFTRF_MODE_NORMAL ? "selected" : "") , SOFTRF_MODE_NORMAL,
//////  (settings->mode == SOFTRF_MODE_TXRX_TEST1 ? "selected" : ""), SOFTRF_MODE_TXRX_TEST1,
//////  (settings->mode == SOFTRF_MODE_TXRX_TEST2 ? "selected" : ""), SOFTRF_MODE_TXRX_TEST2,
//////  (settings->mode == SOFTRF_MODE_BRIDGE ? "selected" : ""), SOFTRF_MODE_BRIDGE,
//////  (settings->mode == SOFTRF_MODE_UAV ? "selected" : ""), SOFTRF_MODE_UAV
//// // );
//
//  len = strlen(offset);
//  offset += len;
//  size -= len;
//
//
//
//  /* Common part 8 */
//  snprintf_P ( offset, size,
//    PSTR("\
//</table>\
//<p align=center>\
//<INPUT type='submit' value='Сохранить и обновить'></p>\
//</form>\
//</body>\
//</html>")
//  );
//
//  server.sendHeader(String(F("Cache-Control")), String(F("no-cache, no-store, must-revalidate")));
//  server.sendHeader(String(F("Pragma")), String(F("no-cache")));
//  server.sendHeader(String(F("Expires")), String(F("-1")));
//  server.send ( 200, "text/html", Settings_temp );
//  free(Settings_temp);
//}


//void handleRoot()
//{
//
//    size_t size = 3920;
//    char* offset;
//    size_t len = 0;
//
//    char* Root_temp = (char*)malloc(size);
//    if (Root_temp == NULL) {
//        return;
//    }
//    offset = Root_temp;
//
//    snprintf_P(offset, size,
//        PSTR("<html>\
//  <head>\
//    <meta name='viewport' content='width=device-width, initial-scale=1'>\
//    <title>SoftRF status</title>\
//  </head>\
//<body>\
// <table width=100%%>\
 //  <tr><!-- <td align=left><h1>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</h1></td> -->\
//  <td align=center><h1>FlyRF тест</h1></td>\
//  </table>\
//  </td>")
//    );
//
//    len = strlen(offset);
//    offset += len;
//    size -= len;
//
//    snprintf_P(offset, size, PSTR("\
//    <td align=right><input type=button onClick=\"location.href='/firmware'\" value='Обновление программы'></td>"));
//    len = strlen(offset);
//    offset += len;
//    size -= len;
//
//    snprintf_P(offset, size, PSTR("\
//  </tr>\
// </table>\
//</body>\
//</html>")
//);
//
//    // SoC->swSer_enableRx(false);
//    server.sendHeader(String(F("Cache-Control")), String(F("no-cache, no-store, must-revalidate")));
//    server.sendHeader(String(F("Pragma")), String(F("no-cache")));
//    server.sendHeader(String(F("Expires")), String(F("-1")));
//    server.send(200, "text/html", Root_temp);
//    // SoC->swSer_enableRx(true);
//    free(Root_temp);
//}



void handleRoot() 
{

  //float vdd = Battery_voltage() ;
  //bool low_voltage = (Battery_voltage() <= Battery_threshold());

  //unsigned int timestamp = (unsigned int) ThisAircraft.timestamp;
  //unsigned int sats = gnss.satellites.value(); // Number of satellites in use (u32)

  char str_lat[16];
  char str_lon[16];
  char str_alt[16];
  char str_Vcc[8];

  size_t size = 3920;
  char *offset;
  size_t len = 0;

  char *Root_temp = (char *) malloc(size);
  if (Root_temp == NULL) {
    return;
  }
  offset = Root_temp;

 /* dtostrf(ThisAircraft.latitude,  8, 4, str_lat);
  dtostrf(ThisAircraft.longitude, 8, 4, str_lon);
  dtostrf(ThisAircraft.altitude,  7, 1, str_alt);
  dtostrf(vdd, 4, 2, str_Vcc);*/

  snprintf_P ( offset, size,
    PSTR("<html>\
  <head>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
    <title>FlytRF status</title>\
  </head>\
<body>\
 <table width=100%%>\
  <tr><!-- <td align=left><h1>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</h1></td> -->\
  <td align=center><h1>FlyRF статус</h1></td>\
  </table>\
 <table width=100%%>\
 <tr><th align=left>Версия ПО</th><td align=right>%s</td></tr>"
 "<table width=100%%>\
  <tr>\
    <!--td align=left><input type=button onClick=\"location.href='/settings'\" value='Настройки'></td>-->\
    <td align=center><input type=button onClick=\"location.href='/about'\" value='Информация'></td>"), 
     FLYRF_FIRMWARE_VERSION
 /*   GNSS_name[hw_info.gnss],
    (rf_chip   == NULL ? "NONE" : rf_chip->name),
    (baro_chip == NULL ? "NONE" : baro_chip->name),
    UpTime.hours, UpTime.minutes, UpTime.seconds, SoC->getFreeHeap(),
    low_voltage ? "red" : "green", str_Vcc,
    tx_packets_counter, rx_packets_counter,
    timestamp, sats, str_lat, str_lon, str_alt*/
  );

  len = strlen(offset);
  offset += len;
  size -= len;


  /* SoC specific part 1 */

    snprintf_P ( offset, size, PSTR("\
    <td align=right><input type=button onClick=\"location.href='/firmware'\" value='Обновление программы'></td>"));
    len = strlen(offset);
    offset += len;
    size -= len;
 

  snprintf_P ( offset, size, PSTR("\
  </tr>\
 </table>\
</body>\
</html>")
  );

  server.sendHeader(String(F("Cache-Control")), String(F("no-cache, no-store, must-revalidate")));
  server.sendHeader(String(F("Pragma")), String(F("no-cache")));
  server.sendHeader(String(F("Expires")), String(F("-1")));
  server.send ( 200, "text/html", Root_temp );
  free(Root_temp);
}

/*
 "</table><table width=100%%>\
  <tr><!--td align=left><table><tr><th align=left>GNSS&nbsp;&nbsp;</th><td align=right>%s</td></tr></table></td>-->\
  <!--td align=center><table><tr><th align=left>Radio&nbsp;&nbsp;</th><td align=right>%s</td></tr></table></td>-->\
  <!--td align=right><table><tr><th align=left>Baro&nbsp;&nbsp;</th><td align=right>%s</td></tr></table></td></tr>-->\
  </table><table width=100%%>"
 "<!--tr><th align=left>Время работы</th><td align=right>%02d:%02d:%02d</td></tr>-->\
  <tr><!--th align=left>Свободная память</th><td align=right>%u</td></tr>-->\
  <tr><!--th align=left>Напряжение батареи</th><td align=right><font color=%s>%s</font></td></tr>-->"
 "</table>\
 <table width=100%%>\
   <tr><!--th align=left>Пакетов</th>-->\
    <td align=right><table><tr>\
     <!--th align=left>Tx&nbsp;&nbsp;</th><td align=right>%u</td>-->\
     <!--th align=left>&nbsp;&nbsp;&nbsp;&nbsp;Rx&nbsp;&nbsp;</th><td align=right>%u</td>-->\
   </tr></table></td></tr>\
 </table>\
 <h2 align=center>Последние данные GNSS</h2>\
 <table width=100%%>\
  <tr><!--th align=left>Время</th><td align=right>%u</td></tr>-->\
  <tr><!--th align=left>Спутников</th><td align=right>%d</td></tr>-->\
  <tr><!--th align=left>Latitude</th><td align=right>%s</td></tr>-->\
  <tr><!--th align=left>Longitude</th><td align=right>%s</td></tr>-->\
  <tr><!--td align=left><b>Высота над уровнем моря</b></td><td align=right>%s</td></tr>-->\
 </table>\
 <hr>\
*/




//void handleInput() 
//{
//
//  size_t size = 3100;
//
//  char *Input_temp = (char *) malloc(size);
//  if (Input_temp == NULL) {
//    return;
//  }
//
//  /*for ( uint8_t i = 0; i < server.args(); i++ ) {
//    if (server.argName(i).equals("mode")) {
//      settings->mode = server.arg(i).toInt();
//    } else if (server.argName(i).equals("protocol")) {
//      settings->rf_protocol = server.arg(i).toInt();
//    } else if (server.argName(i).equals("band")) {
//      settings->band = server.arg(i).toInt();
//    } else if (server.argName(i).equals("acft_type")) {
//      settings->aircraft_type = server.arg(i).toInt();
//    } else if (server.argName(i).equals("alarm")) {
//      settings->alarm = server.arg(i).toInt();
//    } else if (server.argName(i).equals("txpower")) {
//      settings->txpower = server.arg(i).toInt();
//    } else if (server.argName(i).equals("volume")) {
//      settings->volume = server.arg(i).toInt();
//    } else if (server.argName(i).equals("pointer")) {
//      settings->pointer = server.arg(i).toInt();
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
//    } else if (server.argName(i).equals("gdl90")) {
//      settings->gdl90 = server.arg(i).toInt();
//    } else if (server.argName(i).equals("d1090")) {
//      settings->d1090 = server.arg(i).toInt();
//    } else if (server.argName(i).equals("stealth")) {
//      settings->stealth = server.arg(i).toInt();
//    } else if (server.argName(i).equals("no_track")) {
//      settings->no_track = server.arg(i).toInt();
//    } else if (server.argName(i).equals("power_save")) {
//      settings->power_save = server.arg(i).toInt();
//    } else if (server.argName(i).equals("rfc")) {
//      settings->freq_corr = server.arg(i).toInt();
//	} else if (server.argName(i).equals("attention")) {
//      settings->alarm_attention = server.arg(i).toInt();
//    } else if (server.argName(i).equals("warning")) {
//      settings->alarm_warning = server.arg(i).toInt();
//    } else if (server.argName(i).equals("danger")) {
//      settings->alarm_danger = server.arg(i).toInt();
//    } else if (server.argName(i).equals("height")) {
//      settings->alarm_height = server.arg(i).toInt();
//#if defined(USE_OGN_ENCRYPTION)
//    } else if (server.argName(i).equals("igc_key")) {
//      char buf[32 + 1];
//      server.arg(i).toCharArray(buf, sizeof(buf));
//      settings->igc_key[3] = strtoul(buf + 24, NULL, 16);
//      buf[24] = 0;
//      settings->igc_key[2] = strtoul(buf + 16, NULL, 16);
//      buf[16] = 0;
//      settings->igc_key[1] = strtoul(buf +  8, NULL, 16);
//      buf[ 8] = 0;
//      settings->igc_key[0] = strtoul(buf +  0, NULL, 16);
//#endif
//    }
//  }*/
//  snprintf_P ( Input_temp, size,
//PSTR("<html>\
//<head>\
//<meta http-equiv='refresh' content='15; url=/'>\
//<meta name='viewport' content='width=device-width, initial-scale=1'>\
//<title>FlyRF Settings</title>\
//</head>\
//<body>\
//<h1 align=center>Новые настройки:</h1>\
//<table width=100%%>\
//<tr><th align=left>Mode</th><td align=right>%d</td></tr>\
//<tr><th align=left>Protocol</th><td align=right>%d</td></tr>\
//<tr><th align=left>Band</th><td align=right>%d</td></tr>\
//<tr><th align=left>Aircraft type</th><td align=right>%d</td></tr>\
//<tr><th align=left>Alarm trigger</th><td align=right>%d</td></tr>\
//<tr><th align=left>Tx Power</th><td align=right>%d</td></tr>\
//<tr><th align=left>Volume</th><td align=right>%d</td></tr>\
//<tr><th align=left>LED pointer</th><td align=right>%d</td></tr>\
//<tr><th align=left>Bluetooth</th><td align=right>%d</td></tr>\
//<tr><th align=left>NMEA GNSS</th><td align=right>%s</td></tr>\
//<tr><th align=left>NMEA Private</th><td align=right>%s</td></tr>\
//<tr><th align=left>NMEA Legacy</th><td align=right>%s</td></tr>\
//<tr><th align=left>NMEA Sensors</th><td align=right>%s</td></tr>\
//<tr><th align=left>NMEA Out</th><td align=right>%d</td></tr>\
//<tr><th align=left>GDL90</th><td align=right>%d</td></tr>\
//<tr><th align=left>DUMP1090</th><td align=right>%d</td></tr>\
//<tr><th align=left>Stealth</th><td align=right>%s</td></tr>\
//<tr><th align=left>No track</th><td align=right>%s</td></tr>\
//<tr><th align=left>Power save</th><td align=right>%d</td></tr>\
//<tr><th align=left>Freq. correction</th><td align=right>%d</td></tr>\
//<tr><th align=left>Тревога внимание</th><td align=right>%d</td></tr>\
//<tr><th align=left>Тревога предупреждение</th><td align=right>%d</td></tr>\
//<tr><th align=left>Тревога опасность</th><td align=right>%d</td></tr>\
//<tr><th align=left>Тревога высота</th><td align=right>%d</td></tr>\
//<tr><th align=left>IGC key</th><td align=right>%08X%08X%08X%08X</td></tr>\
//</table>\
//<hr>\
//  <p align=center><h1 align=center>Выполняется перезагрузка... Пожалуйста, подождите!</h1></p>\
//</body>\
//</html>")
//  //settings->mode, settings->rf_protocol, settings->band,
//  //settings->aircraft_type, settings->alarm, settings->txpower,
//  //settings->volume, settings->pointer, settings->bluetooth,
//  //BOOL_STR(settings->nmea_g), BOOL_STR(settings->nmea_p),
//  //BOOL_STR(settings->nmea_l), BOOL_STR(settings->nmea_s),
//  //settings->nmea_out, settings->gdl90, settings->d1090,
//  //BOOL_STR(settings->stealth), BOOL_STR(settings->no_track),
//  //settings->power_save, settings->alarm_attention, settings->alarm_warning,
//  //settings->alarm_danger, settings->alarm_height, settings->freq_corr,
//  //settings->igc_key[0], settings->igc_key[1], settings->igc_key[2], settings->igc_key[3]
//  );
//  server.send ( 200, "text/html", Input_temp );
//  delay(1000);
//  free(Input_temp);
//  //!!EEPROM_store();
//  //!!RF_Shutdown();
//  delay(1000);
//  ESP.restart();
//}


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
 // server.on ( "/settings", handleSettings );
  server.on ( "/about", []() {
    server.sendHeader(String(F("Cache-Control")), String(F("no-cache, no-store, must-revalidate")));
    server.sendHeader(String(F("Pragma")), String(F("no-cache")));
    server.sendHeader(String(F("Expires")), String(F("-1")));
    server.send_P ( 200, PSTR("text/html"), about_html);
  } );

 /*   server.on ( "/input", handleInput );
    server.on ( "/inline", []() {
    server.send ( 200, "text/plain", "this works as well" );
  } );*/
    server.on("/firmware", HTTP_GET, [](){
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
 
  });
    server.onNotFound ( handleNotFound );
    server.on("/update", HTTP_POST, [](){
    server.sendHeader(String(F("Connection")), String(F("close")));
    server.sendHeader(String(F("Access-Control-Allow-Origin")), "*");
    server.send(200, String(F("text/plain")), (Update.hasError())?"FAIL":"OK");
    //RF_Shutdown();
    delay(1000);
    ESP.restart();
  },[](){
    HTTPUpload& upload = server.upload();
    if(upload.status == UPLOAD_FILE_START){
      Serial_setDebugOutput(true);
      //SoC->WiFiUDP_stopAll();
      //SoC->WDT_fini();
      Serial.printf("Update: %s\r\n", upload.filename.c_str());
      uint32_t maxSketchSpace = 0x200000;
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

 /* server.on ( "/logo.png", []() 
      {
    server.send_P ( 200, "image/png", Logo, sizeof(Logo) );
  } );*/

/* FLASH memory usage optimization */

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

  server.begin();
  Serial.println (F("HTTP server has started at port: 80"));

  delay(1000);
}


//<p align=center><h1 align=center>Выполняется перезагрузка... Пожалуйста, подождите!</h1></p>\

void Web_loop()
{
  server.handleClient();
}

void Web_fini()
{
  server.stop();
}

