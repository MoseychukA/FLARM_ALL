/*
  FlyRf_Disp_26_07_19_03
  Проект внешнего дисплея FlyRF для RGB TFT GL050001C0-40.
  Прикладные данные сначала принимаются и расшифровываются по RS485,
  затем строятся таблица целей и вращающееся координатное поле.
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
