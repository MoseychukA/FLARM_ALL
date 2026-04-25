/*
  Модуль OTA.cpp
  Назначение:
  - Подсистема обновления прошивки по сети OTA.

  Основные задачи модуля:
  - При наличии соответствующей конфигурации поднять обработчики OTA-обновления.
  - Обслуживать прием прошивки без использования проводного интерфейса.
  - В текущем проекте сохранить совместимость с исходной структурой системы.
*/


#if defined(EXCLUDE_WIFI)
//------------------------------------------------------------------------------
// Назначение функции: Инициализирует OTA-обновление, подготавливает связанные объекты и включает работу соответствующего узла.
//
//------------------------------------------------------------------------------
void OTA_setup()    {}
//------------------------------------------------------------------------------
// Назначение функции: Обслуживает OTA-обновление в основном цикле: проверяет события, обновляет состояние и запускает нужные действия.
//
//------------------------------------------------------------------------------
void OTA_loop()     {}
#else

#include <ArduinoOTA.h>

#include "OTA.h"
#include "WiFiRF.h"

//------------------------------------------------------------------------------
// Назначение функции: Инициализирует OTA-обновление, подготавливает связанные объекты и включает работу соответствующего узла.
//
//------------------------------------------------------------------------------
void OTA_setup()
{

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Start OTA server.
  ArduinoOTA.setHostname(host_name.c_str());

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println(F("OTA Start"));
  });
  ArduinoOTA.onEnd([]() {
    Serial.println(F("\nEnd"));
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
  });

  ArduinoOTA.begin();
}

//------------------------------------------------------------------------------
// Назначение функции: Обслуживает OTA-обновление в основном цикле: проверяет события, обновляет состояние и запускает нужные действия.
//
//------------------------------------------------------------------------------
void OTA_loop()
{
  // Handle OTA server.
  ArduinoOTA.handle();
}

#endif /* EXCLUDE_WIFI */
