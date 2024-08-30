#pragma once
#include <Arduino.h>
#include "Configuration_ESP32.h"
#include <TFT_eSPI.h>                 // Include the graphics library (this includes the sprite functions)

//--------------------------------------------------------------------------------------------------------------------------------
class TFTModuleClass
{
public:
    TFTModuleClass();

    void setup();
    void update();                                                // обновить данные
  

private:

    void build_banner(String msg, int xpos);
    void numberBox(int num, int x, int y);
    unsigned int rainbow(byte value);
 
};
//--------------------------------------------------------------------------------------------------------------------------------
extern TFTModuleClass TFTModuleMain;
