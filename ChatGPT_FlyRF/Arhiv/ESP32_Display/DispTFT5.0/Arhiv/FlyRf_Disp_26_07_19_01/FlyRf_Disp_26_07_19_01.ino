/*
  FlyRf_Disp_26_07_19_01
  Проект внешнего дисплея FlyRF для RGB TFT GL050001C0-40.
  Прикладные функции сохранены из FlyRf_Disp_26_05_18_01.
  
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
