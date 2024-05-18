

#include <Arduino.h>

#include "WebRF.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "Settings.h"

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
<p>Эта программа часть проекта FlyRF</p>\
<p>URL: https://t.me/flyrf_Support</p>\
<p>URL: https://www.decima.ru/contacts/</p>\
<p>Author: <b>Александр Мосейчук</b></p>\
<p>E-mail: Aleksandr.Moseychuk@decima.ru</p>\
<p>E-mail: decima@decima.ru</p>\
<table width=100%%>\
</table>\
<hr>\
Copyright (C) 2024 &nbsp;&nbsp;&nbsp; ООО Децима\
</body>\
</html>";

void handleRoot() 
{

  unsigned int sats = Settings.getNumSat(); // Number of satellites in use (u32)

  char str_lat[16];
  char str_lon[16];
  char str_alt[16];
  char str_ver[32];

  
  size_t size = 3920;
  char *offset;
  size_t len = 0;

  char *Root_temp = (char *) malloc(size);
  if (Root_temp == NULL) {
    return;
  }
  offset = Root_temp;


    long latitude_t = Settings.getCurrentLatitude();
    float latitude_f = latitude_t / 1000000.0;

    long longitude_t = Settings.getLCurrentLongitude();
    float longitude_f = longitude_t / 1000000.0;



  dtostrf(latitude_f,  8, 6, str_lat);
  dtostrf(longitude_f, 8, 6, str_lon);
  dtostrf(Settings.getAltitude() ,  7, 1, str_alt);
   


 String ver = Settings.getVer();
  ver.toCharArray(str_ver, 32);

   snprintf_P(offset, size,
      PSTR("<html>\
  <head>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
    <title>FlytRF status</title>\
  </head>\
<body>\
 <table width=100%%>\
  <tr><!-- <td align=left><h1>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</h1></td> -->\
  <td align=center><h1>FlyRF тест GPS</h1></td>\
  </table>\
 <table width=100%%>\
  <tr><th align=left>Идентификатор устройства</th><td align=right>%06X</td></tr>\
  <tr><th align=left>Версия ПО</th><td align=right>%s</td></tr>"
 "<h2 align=center>Последние данные GNSS</h2>\
 <table width=100%%>\
   <tr><th align=left>Спутников</th><td align=right>%d</td></tr>\
  <tr><th align=left>Latitude</th><td align=right>%s</td></tr>\
  <tr><th align=left>Longitude</th><td align=right>%s</td></tr>\
  <tr><td align=left><b>Высота над уровнем моря</b></td><td align=right>%s</td></tr>\
 </table>\
 <hr>\
 <table width=100%%>\
  <tr>\
     <td align=center><input type=button onClick=\"location.href='/about'\" value='Информация'></td>"),
      Settings.ESP32_getChipId() & 0x00FFFFFF, str_ver,// FLYRF_FIRMWARE_VERSION,
      sats, str_lat, str_lon, str_alt
  );

  len = strlen(offset);
  offset += len;
  size -= len;


  /* SoC specific part 1 */
  snprintf_P(offset, size, PSTR("\
    <td align=right><input type=button onClick=\"location.href='/firmware'\" value='Обновление программы'></td>"));
  len = strlen(offset);
  offset += len;
  size -= len;
 
  snprintf_P(offset, size, PSTR("\
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

