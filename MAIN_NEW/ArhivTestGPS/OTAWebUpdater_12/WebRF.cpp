
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



void handleRoot() 
{
 
  size_t size = 3920;
  char *offset;
  size_t len = 0;

  char *Root_temp = (char *) malloc(size);
  if (Root_temp == NULL) {
    return;
  }
  offset = Root_temp;

  snprintf_P ( offset, size,
    PSTR("<html>\
  <head>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
    <title>SoftRF status</title>\
  </head>\
<body>\
 <table width=100%%>\
  <tr><!-- <td align=left><h1>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</h1></td> -->\
  <td align=center><h1>FlyRF тест</h1></td>\
  </table>\
  </td>")
  );

  len = strlen(offset);
  offset += len;
  size -= len;

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

 // SoC->swSer_enableRx(false);
  server.sendHeader(String(F("Cache-Control")), String(F("no-cache, no-store, must-revalidate")));
  server.sendHeader(String(F("Pragma")), String(F("no-cache")));
  server.sendHeader(String(F("Expires")), String(F("-1")));
  server.send ( 200, "text/html", Root_temp );
 // SoC->swSer_enableRx(true);
  free(Root_temp);
}

void handleNotFound() {
#if defined(ENABLE_RECORDER)
  if (!handleFileRead(server.uri()))
#endif /* ENABLE_RECORDER */
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
  server.on("/firmware", HTTP_GET, [](){
    //SoC->swSer_enableRx(false);
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
</body>\
</html>")
);
 
  });
  server.onNotFound ( handleNotFound );

  server.on("/update", HTTP_POST, []()
  {
    server.sendHeader(String(F("Connection")), String(F("close")));
    server.sendHeader(String(F("Access-Control-Allow-Origin")), "*");
    server.send(200, String(F("text/plain")), (Update.hasError())?"FAIL":"OK");
   // RF_Shutdown();
    delay(1000);
    ESP.restart();
  },[](){
    HTTPUpload& upload = server.upload();
    if(upload.status == UPLOAD_FILE_START)
    {
      Serial_setDebugOutput(true);
      Serial.printf("Update: %s\r\n", upload.filename.c_str());
      uint32_t maxSketchSpace = 0x200000; //
      if(!Update.begin(maxSketchSpace)) //start with max available size
      {
        Update.printError(Serial);
      }
    }
    else if(upload.status == UPLOAD_FILE_WRITE)
    {
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

void Web_loop()
{
  server.handleClient();
}

void Web_fini()
{
  server.stop();
}
