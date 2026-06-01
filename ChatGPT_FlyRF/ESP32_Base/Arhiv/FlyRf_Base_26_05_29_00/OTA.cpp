/*
  Модуль OTA.cpp
  Назначение:
  - Подсистема обновления прошивки по сети OTA.

  Основные задачи модуля:
  - При наличии соответствующей конфигурации поднять обработчики OTA-обновления.
  - Обслуживать прием прошивки без использования проводного интерфейса.
  - В текущем проекте сохранить совместимость с исходной структурой системы.
*/

/*
 * OTAHelper.cpp
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

#if defined(EXCLUDE_WIFI)
//------------------------------------------------------------------------------
// Назначение функции: Инициализирует OTA-обновление, подготавливает связанные объекты и включает работу соответствующего узла.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
void OTA_setup()    {}
//------------------------------------------------------------------------------
// Назначение функции: Обслуживает OTA-обновление в основном цикле: проверяет события, обновляет состояние и запускает нужные действия.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
void OTA_loop()     {}
#else

#include <ArduinoOTA.h>

#include "OTA.h"
#include "WiFiRF.h"

static bool g_otaStarted = false;

//------------------------------------------------------------------------------
// Назначение функции: Инициализирует OTA-обновление, подготавливает связанные объекты и включает работу соответствующего узла.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
void OTA_setup()
{
  if (!WiFi_ready() || g_otaStarted)
  {
    return;
  }

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
  g_otaStarted = true;
}

//------------------------------------------------------------------------------
// Назначение функции: Обслуживает OTA-обновление в основном цикле: проверяет события, обновляет состояние и запускает нужные действия.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
void OTA_loop()
{
  if (!WiFi_ready())
  {
    return;
  }
  if (!g_otaStarted)
  {
    OTA_setup();
  }
  // Handle OTA server.
  ArduinoOTA.handle();
}

#endif /* EXCLUDE_WIFI */
