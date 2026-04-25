/*
  Главный файл проекта FlyRf_Base_26_04_21_02
  Назначение:
  - Точка входа Arduino-приложения.

  Основные задачи файла:
  - Запустить основной Serial для логов.
  - Определить строку версии по имени файла проекта.
  - Передать управление в общий модуль System для setup/loop всей системы.
*/

#include <Arduino.h>
#include "System.h"
#include "DeviceInfo.h"

void setup()
{
    Serial.begin(115200);
    delay(200);
    DeviceInfo_setProgramVersion(DeviceInfo_programVersionFromFile(__FILE__));
    SystemSetup();
}

void loop()
{
    SystemLoop();
}
