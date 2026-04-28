/*
  FlyRf_Disp_26_04_27_10
  Чистый проект внешнего TFT-дисплея.
  Экранная часть ESP32RF.* взята из базового модуля FlyRf_Base_26_04_27_01.
  Отличия вынесены в HardwareConfig.h, RS485Display.*, System.* и WEB-настройки дисплея.
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
