/*
  FlyRf_Disp_26_07_19_02
  Проект внешнего дисплея FlyRF для RGB TFT GL050001C0-40.
  Полный перенос прикладной обработки дисплея из FlyRf_Disp_26_05_18_01.
  TFT_eSPI полностью исключена; графика выводится драйвером RGB-панели.
  
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
