#pragma once

#include "Configuration_ESP32.h"
#include "SPI.h"
#include "TFT_eSPI.h"
#include "SettingsMain.h"
#include "CoreButton.h"           // Обработка кнопок
#include "EEPROMRF.h"
#include "SoftRF.h"

//--------------------------------------------------------------------------------------------------------------------------------------
#include "Fonts/GFXFF/gfxfont.h"
#include "BigRusFont.h"
#include "SmallRusFont.h"
#include "RusFont.h"

#define FONTTYPE const GFXfont*
//--------------------------------------------------------------------------------------------------------------------------------------
#define TFT_Class TFT_eSPI               // класс поддержки TFT

#define TFT_FONT (&BigRusFont)           // какой шрифт юзаем
#define TFT_SMALL_FONT (&SmallRusFont)   // какой шрифт юзаем


//--------------------------------------------------------------------------------------------------------------------------------
class TFTClass
{
public:
    TFTClass();
    ~TFTClass();
 
    void setup();
    void update();                                                // обновить данные
    void draw();
    TFT_Class* getDC() { return tftDC; };
     
private:
    TFT_Class* tftDC;

    //............................dont edit this
    int cx = 160;
    int cy = 160;
    int r = 158;
    unsigned short color2;
    double rad = 0.01745;

    float x[360]; //outer points of Speed gaouges
    float y[360];
    float px[360]; //ineer point of Speed gaouges
    float py[360];
    float px1[360]; //ineer point of Speed gaouges
    float py1[360];
    float lx[360]; //text of Speed gaouges
    float ly[360];
    float nx[360]; //needle low of Speed gaouges
    float ny[360];

    int angle = 0;





};
//--------------------------------------------------------------------------------------------------------------------------------
extern TFTClass TFTMain;
