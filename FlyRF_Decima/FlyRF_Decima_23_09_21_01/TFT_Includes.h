#pragma once
//--------------------------------------------------------------------------------------------------------------------------------------
#include "Configuration_ESP32.h"
#include <SPI.h>
#include <Adafruit_GFX.h>    // подключаем графическую библиотеку
#include <Adafruit_ST7735.h> // подключаем библиотеку для управления дисплеем


#define TFT_CS    14
#define TFT_RST   15  // этот контакт можно подключить к RESET-
#define TFT_DC     2
#define TFT_SCLK   5   // здесь можно задать любой контакт
#define TFT_MOSI  27   // здесь можно задать любой контакт
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);



//--------------------------------------------------------------------------------------------------------------------------------------
//#include "Fonts/GFXFF/gfxfont.h"
#include "BigRusFont.h"
#include "SmallRusFont.h"
#include "IconsFont.h"
#include "SensorFont.h"
#include "SensorFont2.h"
#include "Flarm_Font.h"
#include "RusFont.h"
#include "SevenSegNumFontMDS.h"
#include "SevenSegNumFontPlus.h"
#include "Various_Symbols_32x32.h"

#define FONTTYPE const GFXfont*
//--------------------------------------------------------------------------------------------------------------------------------------
#define TFT_Class tft               // класс поддержки TFT

#define TFT_FONT (&BigRusFont)           // какой шрифт юзаем
#define TFT_SMALL_FONT (&SmallRusFont)   // какой шрифт юзаем
#define SENSOR_FONT (&SensorFont)
#define SENSOR_FONT2 (&SensorFont2)
#define SIGN_FONT (&Flarm_Font)
#define SEVEN_SEG_NUM_FONT_MDS (&SevenSegNumFontMDS)
#define SEVEN_SEG_NUM_FONT_PLUS (&SevenSegNumFontPlus)
#define VARIOUS_SYMBOLS_32x32 (&Various_Symbols_32x32)
#define ICONS_FONT (&IconsFont)

