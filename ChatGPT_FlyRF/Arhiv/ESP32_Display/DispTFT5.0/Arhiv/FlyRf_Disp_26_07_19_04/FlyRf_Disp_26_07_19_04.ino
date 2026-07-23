/*
  FlyRf_Disp_26_07_19_04
  Проект внешнего дисплея FlyRF для RGB TFT GL050001C0-40.
  RS485-приём содержит диагностику размеров, количества байтов и CRC.
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
