
#ifndef WEBHELPER_H
#define WEBHELPER_H

#if defined(ARDUINO) && !defined(EXCLUDE_WIFI)
#include <WiFiClient.h>

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#endif /* ARDUINO */

#define BOOL_STR(x) (x ? "true":"false")
#define JS_MAX_CHUNK_SIZE 4096

//WebServer server(80);

void Web_setup(void);
void Web_loop(void);
void Web_fini(void);

#if defined(ARDUINO) && !defined(EXCLUDE_WIFI)
#ifndef Serial_setDebugOutput
#define Serial_setDebugOutput(x) Serial.setDebugOutput(x)
#endif /* Serial_setDebugOutput */

extern WiFiClient client;
#endif /* ARDUINO */


#endif /* WEBHELPER_H */
