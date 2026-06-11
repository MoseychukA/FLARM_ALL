#pragma once
//--------------------------------------------------------------------------------------------------------------------------------------
#include "TFT_eSPI.h"
#include <SPI.h>
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
