/*
  FlyRf_Disp_26_07_22_09

  Настройки компиляции:
  Board: ESP32S3 Dev Module (esp32_esp32s3)
  ESP32 Arduino Core: 3.3.10
  Flash Size: 16 MB
  PSRAM: OPI PSRAM

  Основа проекта: FlyRf_Disp_26_07_20_06.
  Дисплей и назначения выводов: FlyRf_Disp_26_07_20_00.
  Состояние WiFi при запуске определяется сохранённым флагом настройки.
  Включение и выключение WiFi выполняется командами Bluetooth.
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
