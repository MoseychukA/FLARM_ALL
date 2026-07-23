/*
  Main Arduino entry point for FlyRf_Base_26_06_02_00.
  The displayed firmware version is derived from this file name.
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
