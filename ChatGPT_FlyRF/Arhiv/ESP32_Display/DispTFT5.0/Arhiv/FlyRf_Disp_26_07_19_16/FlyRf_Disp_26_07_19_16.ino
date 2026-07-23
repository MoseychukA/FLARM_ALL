/*
  FlyRf_Disp_26_07_19_16
  Проект внешнего дисплея FlyRF для RGB TFT GL050001C0-40.
  RS485 автоматически определяет порядок маркеров, CRC и размер time_t.
  INA219 использует кольцевое усреднение напряжения и тока.
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
