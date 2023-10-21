/*
 * TFTHelper.cpp
 * Copyright (C) 2019-2023 Linar Yusupov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SoC.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include <TimeLib.h>

#include "TFTHelper.h"
#include "LED.h"
#include "RF.h"
#include "Baro.h"
#include "TrafficHelper.h"
#include "TimeHELPER.h"
#include <TFT_eSPI.h>             // Поддержка TFT дисплея 
#include "Adafruit_GFX.h"
#include "SoftRF.h"
#include "BatteryHelper.h"
#include "EEPROMHELPER.h"
#include "NMEA.h"
#include "GDL90.h"
//#include "Adafruit_GFX.h"

#include "Configuration_ESP32.h"
#include "Airplane.h"
#include "NotoSansMonoSCB20.h"
#include "NotoSansBold15.h"

 //#include "Fonts/GFXFF/FreeMonoBold24pt7b.h"
 //#include "Fonts/GFXFF/FreeMonoBold18pt7b.h"
 //#include "Fonts/GFXFF/FreeMonoBold12pt7b.h"
 //#include "Fonts/GFXFF/FreeMono18pt7b.h"
 //#include <Fonts/GFXFF/FreeMonoBold9pt7b.h>
 #include "Picopixel.h"
 ////#include "Fonts/Org_01.h"
 //#include "Fonts/GFXFF/FreeMonoBoldOblique9pt7b.h"
 //#include "FreeSerif9pt7b.h"


//
TFT_eSPI *tft_radar = NULL;
TFT_eSprite *sprite = NULL;

static unsigned long TFTTimeMarker = 0;

static int TFT_view_mode = 0;
bool TFT_vmode_updated = true;

const char EPD_SoftRF_text1[] = "FlyRF";
const char EPD_SoftRF_text2[] = "and";
const char EPD_SoftRF_text3[] = "DECIMA";
const char EPD_SoftRF_text4[] = "Ver.SoftRF_21_SkyWatch_01";
const char EPD_SoftRF_text5[] = "Linar Yusupov";
const char EPD_SoftRF_text6[] = "(C) 2016-2023";

const char EPD_Radio_text[] = "RADIO   ";
const char EPD_GNSS_text[] = "GNSS    ";
const char EPD_tft_epd_text[] = "tft_radar ";
const char EPD_RTC_text[] = "RTC     ";
const char EPD_Flash_text[] = "FLASH   ";
const char EPD_Baro_text[] = "BARO  ";
const char EPD_IMU_text[] = "IMU   ";


void TFT_Clear_Screen()
{
  tft_radar->fillScreen(TFT_NAVY);
}

void TFT_setup()
{

    bool rval = false;

    int16_t  tbx1, tby1;
    uint16_t tbw1, tbh1;
    int16_t  tbx2, tby2;
    uint16_t tbw2, tbh2;
    int16_t  tbx3, tby3;
    uint16_t tbw3, tbh3;
    int16_t  tbx4, tby4;
    uint16_t tbw4, tbh4;
    uint16_t x, y;


    tft_radar = new TFT_eSPI(LV_HOR_RES, LV_VER_RES);
    tft_radar->init();
    tft_radar->setRotation(3);

    tft_radar->fillScreen(TFT_NAVY);

    tft_radar->setTextColor(TFT_WHITE); //TFT_WHITE TFT_BLACK
    tft_radar->setTextWrap(false);

    tft_radar->setFreeFont(&FreeMonoBold24pt7b);
    //tft_radar->fillScreen(TFT_WHITE);
    
    tbw1 = sizeof(EPD_SoftRF_text1);

    x = 90;// (tft_radar->width() - tbw1) / 2;
    y = 80;// (tft_radar->height() + tbh1) / 2 - tbh3;
    tft_radar->setCursor(x, y);
    tft_radar->print(EPD_SoftRF_text1);

    tft_radar->setFreeFont(&FreeMono18pt7b);
    
    x = 130;// (tft_radar->width() - tbw2) / 2;
    y = 125;// (tft_radar->height() + tbh2) / 2;
    tft_radar->setCursor(x, y);
    tft_radar->print(EPD_SoftRF_text2);

    tft_radar->setFreeFont(&FreeMonoBold24pt7b);

    x = 80;// (tft_radar->width() - tbw3) / 2;
    y = 180;// tft_radar->height() + tbh3) / 2 + tbh3;
    tft_radar->setCursor(x, y);
    tft_radar->print(EPD_SoftRF_text3);

    tft_radar->setFreeFont(&FreeSerif9pt7b);
    tbw4 = tft_radar->textWidth(EPD_SoftRF_text4);
    //tft_radar->fontHeight();

    x = (tft_radar->width() - tbw4) - 4;
    y = tft_radar->height() - tft_radar->fontHeight() + 10;
    tft_radar->setCursor(x, y);
    tft_radar->print(EPD_SoftRF_text4);

    //sprite = new TFT_eSprite(tft_radar);
    //sprite->setColorDepth(1);

    TFT_radar_setup();

}

void TFT_loop()
{
      if (isTimeToDisplay()) 
      {
        TFT_radar_loop();
        TFTTimeMarker = millis();
      }



  /*      switch (tp_action)
        {
            case SWIPE_LEFT:
              if (TFT_view_mode < VIEW_MODE_TIME) 
              {
                TFT_view_mode++;
                TFT_vmode_updated = true;
              }
              break;
            case SWIPE_RIGHT:
              if (TFT_view_mode > VIEW_MODE_STATUS) 
              {
                TFT_view_mode--;
                TFT_vmode_updated = true;
              }
              break;
            case SWIPE_DOWN:
              TFT_Up();
              break;
            case SWIPE_UP:
              TFT_Down();
              break;
            case NO_GESTURE:
            default:
              break;
        }*/
  
}

void TFT_fini(const char *msg)
{
  /*switch (hw_info.display)
  {
  case DISPLAY_TFT_TTGO_240:
  case DISPLAY_TFT_TTGO_135:
    if (tft_radar) {
        int level;

  
        tft_radar->fillScreen(TFT_NAVY);

        tft_radar->setTextFont(4);
        tft_radar->setTextSize(2);
        tft_radar->setTextColor(TFT_WHITE, TFT_NAVY);

        uint16_t tbw = tft_radar->textWidth(msg);
        uint16_t tbh = tft_radar->fontHeight();

        tft_radar->setCursor((tft_radar->width() - tbw)/2, (tft_radar->height() - tbh)/2);
        tft_radar->print(msg);

        delay(2000);

        SPI.end();
    }
    break;

  case DISPLAY_NONE:
  default:
    break;
  }*/
}

void TFT_Up()
{
    //switch (TFT_view_mode)
    //{
    //case VIEW_MODE_RADAR:
    //  TFT_radar_unzoom();
    //  break;
    //case VIEW_MODE_TEXT:
    //  TFT_text_prev();
    //  break;
    //case VIEW_MODE_TIME:
    //  TFT_time_prev();
    //  break;
    //case VIEW_MODE_STATUS:
    //default:
    //  TFT_status_prev();
    //  break;
    //}
}

void TFT_Down()
{
 /*   switch (TFT_view_mode)
    {
    case VIEW_MODE_RADAR:
      TFT_radar_zoom();
      break;
    case VIEW_MODE_TEXT:
      TFT_text_next();
      break;
    case VIEW_MODE_TIME:
      TFT_time_next();
      break;
    case VIEW_MODE_STATUS:
    default:
      TFT_status_next();
      break;
    }*/
}

void TFT_Message(const char *msg1, const char *msg2)
{
  int16_t  tbx, tby;
  uint16_t tbw, tbh;
  uint16_t x, y;

  if (msg1 != NULL && strlen(msg1) != 0) 
  {
    tft_radar->setTextFont(4);
    tft_radar->setTextSize(2);

    tft_radar->fillScreen(TFT_NAVY);

    tbw = tft_radar->textWidth(msg1);
    tbh = tft_radar->fontHeight();
    x = (tft_radar->width() - tbw) / 2;
    y = msg2 == NULL ? (tft_radar->height() - tbh) / 2 : tft_radar->height() / 2 - tbh;
    tft_radar->setCursor(x, y);
    tft_radar->print(msg1);

    if (msg2 != NULL && strlen(msg2) != 0) 
    {
      tbw = tft_radar->textWidth(msg2);
      x = (tft_radar->width() - tbw) / 2;
      y = tft_radar->height() / 2;
      tft_radar->setCursor(x, y);
      tft_radar->print(msg2);
    }
  }
}

void TFT_Mode_Cycle()
{
 /* TFT_view_mode++;

  if (TFT_view_mode > VIEW_MODE_TIME) {
    TFT_view_mode = VIEW_MODE_STATUS;
  }

  TFT_vmode_updated = true;*/
}

void TFT_info1()
{

}



//======================================= View_Radar_TFT ===============================================================

static int TFT_zoom = ZOOM_MEDIUM;

enum {
    STATE_RVIEW_NONE,
    STATE_RVIEW_RADAR,
    STATE_RVIEW_NOFIX,
    STATE_RVIEW_NODATA
};

static int view_state_curr = STATE_RVIEW_NONE;
static int view_state_prev = STATE_RVIEW_NONE;

static void TFT_Draw_Radar()
{

    int16_t  tbx, tby;
    uint16_t tbw, tbh;
    uint16_t x;
    uint16_t y;
    char cog_text[6];

    
    int32_t divider = 2000;  //делитель равен половине полной шкалы

    uint16_t radar_x = 0;
    uint16_t radar_y = 0; //(tft_radar->width() - tft_radar->height()) / 2;
    uint16_t radar_w = tft_radar->width();

    uint16_t radar_center_x = radar_w / 2;
    uint16_t radar_center_y = radar_y + radar_w / 2;
    uint16_t radius = radar_w / 2 - 2;

    Serial.print("radar_y - ");
    Serial.print(radar_y);
    Serial.print(" radar_w - ");
    Serial.print(radar_w);
    Serial.print(" radar_center_x - ");
    Serial.print(radar_center_x);
    Serial.print(" radar_center_y - ");
    Serial.print(radar_center_y);
    Serial.print(" radius - ");
    Serial.println(radius);



    if (settings->m.units == UNITS_METRIC || settings->m.units == UNITS_MIXED)
    {
        switch (TFT_zoom)
        {
        case ZOOM_LOWEST:
            divider = 30000; /* 60 KM */
            break;
        case ZOOM_LOW:
            divider = 5000; /* 10 KM */
            break;
        case ZOOM_HIGH:
            divider = 1000; /*  2 KM */
            break;
        case ZOOM_MEDIUM:
        default:
            divider = 2000;  /* 4 KM */
            break;
        }
    }
    else 
    {
        switch (TFT_zoom)
        {
        case ZOOM_LOWEST:
            divider = 27780;  /* 30 NM */
            break;
        case ZOOM_LOW:
            divider = 4630;  /*  5 NM */
            break;
        case ZOOM_HIGH:
            divider = 926;  /*  1 NM */
            break;
        case ZOOM_MEDIUM:  /*  2 NM */
        default:
            divider = 1852;
            break;
        }
    }


    tft_radar->setFreeFont(&FreeMonoBold12pt7b);

    tft_radar->fillScreen(TFT_WHITE);
    tft_radar->setTextColor(TFT_BLACK);

    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
    {
        if (Container[i].addr && (now() - Container[i].timestamp) <= TFT_EXPIRATION_TIME) 
        {

            int16_t rel_x;
            int16_t rel_y;
            float distance;
            float bearing;

            settings->m.orientation = 0;

            bool isTeam = (Container[i].addr == settings->m.team);  

            distance = Container[i].distance;
            bearing = Container[i].bearing;     // Азимут

            Serial.print("distance - ");
            Serial.print(distance);
            Serial.print(" dbearing - ");
            Serial.print(bearing);
            Serial.print(" orientation - ");
            Serial.print(settings->m.orientation);
            Serial.print(" ThisAircraft.course - ");
            Serial.println(ThisAircraft.course);

            switch (settings->m.orientation)
            {
            case DIRECTION_NORTH_UP:
                break;
            case DIRECTION_TRACK_UP:
                bearing -= ThisAircraft.course;
                break;
            default:
                /* TBD */
                break;
            }

            rel_x = constrain(distance * sin(radians(bearing)),
                -32768, 32767);
            rel_y = constrain(distance * cos(radians(bearing)),
                -32768, 32767);

            int16_t x = ((int32_t)rel_x * (int32_t)radius) / divider;
            int16_t y = ((int32_t)rel_y * (int32_t)radius) / divider;

            float RelativeVertical = Container[i].altitude - ThisAircraft.altitude;  // Разность высот

            if (RelativeVertical > TFT_RADAR_V_THRESHOLD) 
            {
                if (isTeam) 
                {
                    tft_radar->drawTriangle(radar_center_x + x - 5, radar_center_y - y + 4,
                        radar_center_x + x, radar_center_y - y - 6,
                        radar_center_x + x + 5, radar_center_y - y + 4,
                        TFT_BLACK);
                    tft_radar->drawTriangle(radar_center_x + x - 6, radar_center_y - y + 5,
                        radar_center_x + x, radar_center_y - y - 7,
                        radar_center_x + x + 6, radar_center_y - y + 5,
                        TFT_BLACK);
                }
                else 
                {
                    tft_radar->fillTriangle(radar_center_x + x - 4, radar_center_y - y + 3,
                        radar_center_x + x, radar_center_y - y - 5,
                        radar_center_x + x + 4, radar_center_y - y + 3,
                        TFT_BLACK);
                }
            }
            else if (RelativeVertical < -TFT_RADAR_V_THRESHOLD) 
            {
                if (isTeam) 
                {
                    tft_radar->drawTriangle(radar_center_x + x - 5, radar_center_y - y - 4,
                        radar_center_x + x, radar_center_y - y + 6,
                        radar_center_x + x + 5, radar_center_y - y - 4,
                        TFT_BLACK);
                    tft_radar->drawTriangle(radar_center_x + x - 6, radar_center_y - y - 5,
                        radar_center_x + x, radar_center_y - y + 7,
                        radar_center_x + x + 6, radar_center_y - y - 5,
                        TFT_BLACK);
                }
                else 
                {
                    tft_radar->fillTriangle(radar_center_x + x - 4, radar_center_y - y - 3,
                        radar_center_x + x, radar_center_y - y + 5,
                        radar_center_x + x + 4, radar_center_y - y - 3,
                        TFT_BLACK);
                }
            }
            else 
            {
                if (isTeam) 
                {
                    tft_radar->drawCircle(radar_center_x + x,
                        radar_center_y - y,
                        6, TFT_BLACK);
                    tft_radar->drawCircle(radar_center_x + x,
                        radar_center_y - y,
                        7, TFT_BLACK);
                }
                else 
                            
                {
                    tft_radar->fillCircle(radar_center_x + x,
                        radar_center_y - y,
                        5, TFT_BLACK);
                }
            }
        }
    }

    tft_radar->drawCircle(radar_center_x, radar_center_y, radius, TFT_BLACK);
    tft_radar->drawCircle(radar_center_x, radar_center_y, radius / 2, TFT_BLACK);

                 /* Тип нашего воздушного судна*/
    if (ThisAircraft.aircraft_type == AIRCRAFT_TYPE_GLIDER ||      // планер
        ThisAircraft.aircraft_type == AIRCRAFT_TYPE_TOWPLANE ||    // БУКСИРОВКА САМОЛЕТА
        ThisAircraft.aircraft_type == AIRCRAFT_TYPE_HELICOPTER ||  // ВЕРТОЛЕТ
        ThisAircraft.aircraft_type == AIRCRAFT_TYPE_DROPPLANE ||   // десантируемый самолет
        ThisAircraft.aircraft_type == AIRCRAFT_TYPE_POWERED ||
        ThisAircraft.aircraft_type == AIRCRAFT_TYPE_JET)           //  
    {

        /* little airplane */
        tft_radar->drawFastVLine(radar_center_x, radar_center_y - 4, 14, TFT_BLACK);
        tft_radar->drawFastVLine(radar_center_x + 1, radar_center_y - 4, 14, TFT_BLACK);

        tft_radar->drawFastHLine(radar_center_x - 8, radar_center_y, 18, TFT_BLACK);
        tft_radar->drawFastHLine(radar_center_x - 10, radar_center_y + 1, 22, TFT_BLACK);

        tft_radar->drawFastHLine(radar_center_x - 3, radar_center_y + 8, 8, TFT_BLACK);
        tft_radar->drawFastHLine(radar_center_x - 2, radar_center_y + 9, 6, TFT_BLACK);

    }
    else 
    {

        /* arrow tip */
        tft_radar->fillTriangle(radar_center_x - 7, radar_center_y + 5,
            radar_center_x, radar_center_y - 5,
            radar_center_x + 7, radar_center_y + 5,
            TFT_BLACK);
        tft_radar->fillTriangle(radar_center_x - 7, radar_center_y + 5,
            radar_center_x, radar_center_y + 2,
            radar_center_x + 7, radar_center_y + 5,
            TFT_WHITE);
    }

    switch (settings->m.orientation)
    {
    case DIRECTION_NORTH_UP:
        x = radar_x + radar_w / 2 - radius + tbw / 2;
        y = radar_y + (radar_w + tbh) / 2;
        tft_radar->setCursor(x, y);
        tft_radar->print("W");
        x = radar_x + radar_w / 2 + radius - (3 * tbw) / 2;
        y = radar_y + (radar_w + tbh) / 2;
        tft_radar->setCursor(x, y);
        tft_radar->print("E");
        x = radar_x + (radar_w - tbw) / 2;
        y = radar_y + radar_w / 2 - radius + (3 * tbh) / 2;
        tft_radar->setCursor(x, y);
        tft_radar->print("N");
        x = radar_x + (radar_w - tbw) / 2;
        y = radar_y + radar_w / 2 + radius - tbh / 2;
        tft_radar->setCursor(x, y);
        tft_radar->print("S");
        break;
    case DIRECTION_TRACK_UP:
        x = radar_x + radar_w / 2 - radius + tbw / 2;
        y = radar_y + (radar_w + tbh) / 2;
        tft_radar->setCursor(x, y);
        tft_radar->print("L");
        x = radar_x + radar_w / 2 + radius - (3 * tbw) / 2;
        y = radar_y + (radar_w + tbh) / 2;
        tft_radar->setCursor(x-15, y);
        tft_radar->print("R");
        x = radar_x + (radar_w - tbw) / 2;
        y = radar_y + radar_w / 2 + radius - tbh / 2;
        tft_radar->setCursor(x, y);
        tft_radar->print("B");

        tft_radar->setFreeFont(&FreeMonoBold12pt7b);
        snprintf(cog_text, sizeof(cog_text), "%d", (int)ThisAircraft.course);
                    
        x = radar_x + (radar_w - tbw) / 2;
        y = radar_y + radar_w / 2 - radius + (3 * tbh) / 2;
        tft_radar->setCursor(x-10, y+20);
        tft_radar->print(cog_text);
        tft_radar->drawRoundRect(x - 2+20, y - tbh - 2+8, tbw + 10, tbh + 10, 5, TFT_BLACK); //Знак градуса
        break;
    default:
        /* TBD */
        break;
    }

     tft_radar->setFreeFont(&FreeMonoBold12pt7b);
              
    x = radar_x + tbw / 2;
    y = radar_y + radar_w - tbh;
    tft_radar->setCursor(x, y);

    int tr_count = Traffic_Count();
    tft_radar->print(tr_count);   // Количество воздушных объектов?
    Serial.print("Aircrafts - ");
    Serial.println(tr_count);

    tft_radar->setFreeFont(&FreeMonoBold12pt7b);
    y += tbh; y += tbh;
    tft_radar->setCursor(x, y);
    tft_radar->print("ACFTS");     // Aircraft

    tft_radar->setFreeFont(&FreeMonoBold12pt7b);
               
    x = 0;// radar_x + radar_w - tbw;
    y = 10;// radar_y + radar_w - tbh;
    tft_radar->setCursor(x, y);

    if (settings->m.units == UNITS_METRIC || settings->m.units == UNITS_MIXED) 
    {
        tft_radar->print(TFT_zoom == ZOOM_LOWEST ? "60" :
            TFT_zoom == ZOOM_LOW ? "10" :
            TFT_zoom == ZOOM_MEDIUM ? "4 " :
            TFT_zoom == ZOOM_HIGH ? "2 " : "");
    }
    else 
    {
        tft_radar->print(TFT_zoom == ZOOM_LOWEST ? "30" :
            TFT_zoom == ZOOM_LOW ? "5 " :
            TFT_zoom == ZOOM_MEDIUM ? "2 " :
            TFT_zoom == ZOOM_HIGH ? "1 " : "");
    }

    tft_radar->setFreeFont(&FreeMonoBold12pt7b);
            
    x = 10;// += tbw;
    y = 10;// += tbh; y += tbh;
    tft_radar->setCursor(x, y);

    tft_radar->print(settings->m.units == UNITS_METRIC || settings->m.units == UNITS_MIXED ? "KM" : "NM");
  

  
//    int16_t  tbx, tby;
//    uint16_t tbw, tbh;
//    uint16_t x;
//    uint16_t y;
//    char cog_text[6];
//    char rssi_text[10];
//
//    /* divider is a half of full scale */
//    int32_t divider = 2000;
//
//    sprite->createSprite(tft_radar->width(), tft_radar->height());
//
//    sprite->fillSprite(TFT_BLACK);
//    sprite->setTextColor(TFT_WHITE);
//
//    sprite->setTextFont(4);
//    sprite->setTextSize(1);
//
//    tbw = sprite->textWidth("N");
//    tbh = sprite->fontHeight();
//
//    uint16_t radar_x = 0;
//    uint16_t radar_y = 0;
//    uint16_t radar_w = sprite->width();
//
//    uint16_t radar_center_x = radar_w / 2;
//    uint16_t radar_center_y = radar_y + radar_w / 2;
//    uint16_t radius = radar_w / 2 - 1;
//
//    settings->m.units = UNITS_METRIC;
//    TFT_zoom = ZOOM_LOWEST;
//
//
//    if (settings->m.units == UNITS_METRIC || settings->m.units == UNITS_MIXED) 
//    {
//        switch (TFT_zoom)
//        {
//        case ZOOM_LOWEST:
//            divider = 10000; /* 20 KM */
//            break;
//        case ZOOM_LOW:
//            divider = 5000; /* 10 KM */
//            break;
//        case ZOOM_HIGH:
//            divider = 1000; /*  2 KM */
//            break;
//        case ZOOM_MEDIUM:
//        default:
//            divider = 2000;  /* 4 KM */
//            break;
//        }
//    }
//    else 
//    {
//        switch (TFT_zoom)
//        {
//        case ZOOM_LOWEST:
//            divider = 9260;  /* 10 NM */
//            break;
//        case ZOOM_LOW:
//            divider = 4630;  /*  5 NM */
//            break;
//        case ZOOM_HIGH:
//            divider = 926;  /*  1 NM */
//            break;
//        case ZOOM_MEDIUM:  /*  2 NM */
//        default:
//            divider = 1852;
//            break;
//        }
//    }
//
//    sprite->drawCircle(radar_center_x, radar_center_y, radius, TFT_WHITE);
//    sprite->drawCircle(radar_center_x, radar_center_y, radius / 2, TFT_WHITE);
//
//#if 0
//    /* arrow tip */
//    sprite->fillTriangle(radar_center_x - 7, radar_center_y + 5,
//        radar_center_x, radar_center_y - 5,
//        radar_center_x + 7, radar_center_y + 5,
//        TFT_WHITE);
//    sprite->fillTriangle(radar_center_x - 7, radar_center_y + 5,
//        radar_center_x, radar_center_y + 2,
//        radar_center_x + 7, radar_center_y + 5,
//        TFT_NAVY);
//#else
//    /* little airplane */
//    sprite->drawFastVLine(radar_center_x, radar_center_y - 4, 14, TFT_WHITE);
//    sprite->drawFastVLine(radar_center_x + 1, radar_center_y - 4, 14, TFT_WHITE);
//
//    sprite->drawFastHLine(radar_center_x - 8, radar_center_y, 18, TFT_WHITE);
//    sprite->drawFastHLine(radar_center_x - 10, radar_center_y + 1, 22, TFT_WHITE);
//
//    sprite->drawFastHLine(radar_center_x - 3, radar_center_y + 8, 8, TFT_WHITE);
//    sprite->drawFastHLine(radar_center_x - 2, radar_center_y + 9, 6, TFT_WHITE);
//#endif
//
//    switch (settings->m.orientation)
//    {
//    case DIRECTION_NORTH_UP:
//        x = radar_x + radar_w / 2 - radius + tbw / 2;
//        y = radar_y + (radar_w - tbh) / 2;
//        sprite->setCursor(x, y);
//        sprite->print("W");
//        x = radar_x + radar_w / 2 + radius - (3 * tbw) / 2;
//        y = radar_y + (radar_w - tbh) / 2;
//        sprite->setCursor(x, y);
//        sprite->print("E");
//        x = radar_x + (radar_w - tbw) / 2;
//        y = radar_y + radar_w / 2 - radius + tbh / 2;
//        sprite->setCursor(x, y);
//        sprite->print("N");
//        x = radar_x + (radar_w - tbw) / 2;
//        y = radar_y + radar_w / 2 + radius - tbh;
//        sprite->setCursor(x, y);
//        sprite->print("S");
//        break;
//    case DIRECTION_TRACK_UP:
//        x = radar_x + radar_w / 2 - radius + tbw / 2;
//        y = radar_y + (radar_w - tbh) / 2;
//        sprite->setCursor(x, y);
//        sprite->print("L");
//        x = radar_x + radar_w / 2 + radius - (3 * tbw) / 2;
//        y = radar_y + (radar_w - tbh) / 2;
//        sprite->setCursor(x, y);
//        sprite->print("R");
//        x = radar_x + (radar_w - tbw) / 2;
//        y = radar_y + radar_w / 2 + radius - tbh;
//        sprite->setCursor(x, y);
//        sprite->print("B");
//
//        snprintf(cog_text, sizeof(cog_text), "%03d", ThisAircraft.Track);
//        tbw = sprite->textWidth(cog_text);
//        tbh = sprite->fontHeight();
//        x = radar_x + (radar_w - tbw) / 2;
//        y = radar_y + radar_w / 2 - radius + tbh / 2;
//        sprite->setCursor(x, y);
//        sprite->print(cog_text);
//#if 0
//        sprite->drawRoundRect(x - 2, y - tbh - 2,
//            tbw + 8, tbh + 6,
//            4, TFT_WHITE);
//#endif
//        break;
//    default:
//        /* TBD */
//        break;
//    }
//
//    sprite->setTextColor(TFT_WHITE, TFT_BLACK);
//    x = 0;// radar_x;
//    y = 5;// radar_y + radar_w - tbh;
//    sprite->setCursor(x, y);
//
//    //Serial.print("radar_x - ");
//    //Serial.println(x);
//    //Serial.print("radar_y - ");
//    //Serial.println(y);
//    // 
//    //!!
//
//    if (settings->m.units == UNITS_METRIC || settings->m.units == UNITS_MIXED) 
//    {
//        sprite->print(TFT_zoom == ZOOM_LOWEST ? "20 KM" :
//            TFT_zoom == ZOOM_LOW ? "10 KM" :
//            TFT_zoom == ZOOM_MEDIUM ? " 4 KM" :
//            TFT_zoom == ZOOM_HIGH ? " 2 KM" : "");
//    }
//    else 
//    {
//        sprite->print(TFT_zoom == ZOOM_LOWEST ? "10 NM" :
//            TFT_zoom == ZOOM_LOW ? " 5 NM" :
//            TFT_zoom == ZOOM_MEDIUM ? " 2 NM" :
//            TFT_zoom == ZOOM_HIGH ? " 1 NM" : "");
//    }
//
//    tft_radar->setBitmapColor(TFT_WHITE, TFT_NAVY);
//
//    snprintf(rssi_text, sizeof(rssi_text), "rssi %0d", LMIC.rssi);
//
//    //sprite->fontHeight();
//   // sprite->loadFont(FreeSerif9pt7b);
//    sprite->setTextSize(1);
//    x = (sprite->width() - sprite->textWidth(rssi_text));
//    sprite->setCursor(x, 2);
//    sprite->print(rssi_text);
//
//   // x = (sprite->width() - sprite->textWidth(EPD_SoftRF_text4));
//
//    /*
//    tft_epd->setFreeFont(&FreeSerif9pt7b);
//     tbw4 = tft_epd->textWidth(EPD_SoftRF_text4);
//     //tft_epd->fontHeight();
// 
//     x = (tft_epd->width() - tbw4)-4;
//     y = tft_epd->height() - tft_epd->fontHeight()+10;
//    tft_epd->setCursor(x, y);
//    tft_epd->print(EPD_SoftRF_text4);
//    
//    */
//
//   
//
//    sprite->pushSprite(0, 0);
//    sprite->deleteSprite();
//
//  
//  // Добавил для проверки. Убрать по окончании
//  /* if (isValidFix())
//    {
//        ThisAircraft.latitude = gnss.location.lat();
//        ThisAircraft.longitude = gnss.location.lng();
//        ThisAircraft.altitude = gnss.altitude.meters();
//        ThisAircraft.course = gnss.course.deg();
//        ThisAircraft.speed = gnss.speed.knots();
//        ThisAircraft.hdop = (uint16_t)gnss.hdop.value();
//        ThisAircraft.geoid_separation = gnss.separation.meters();
//    }
//  */
//  
//
//    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) 
//    {
//        if (i == 0)
//        {
//            Serial.print("ID - ");
//            Serial.print(Container[i].addr, HEX);
//            Serial.print(" latitude - ");
//            Serial.print(Container[i].latitude,5);
//            Serial.print(" longitude - ");
//            Serial.print(Container[i].longitude,5);
//            Serial.print(" altitude - ");
//            Serial.print(Container[i].altitude, 0);
//            Serial.print(" distance - ");
//            Serial.print(Container[i].distance,0);
//            Serial.print(" m ");
//            Serial.print(" RelativeNorth - ");
//            Serial.print(Container[i].RelativeNorth, 0);
//            Serial.println("");
//
//
//
//        }
//
//        if (Container[i].addr && (now() - Container[i].timestamp) <= TFT_EXPIRATION_TIME)
//        {
//
//            int16_t rel_x;
//            int16_t rel_y;
//            float distance;
//            float bearing;
//
//            distance = Container[i].distance;
//            bearing = Container[i].bearing;
//
//
//            switch (settings->m.orientation)
//            {
//            case DIRECTION_NORTH_UP:
//                rel_x = Container[i].RelativeEast;  // Относительно Восток
//                rel_y = Container[i].RelativeNorth; // Относительно Север
//                break;
//            case DIRECTION_TRACK_UP:
//                distance = sqrtf(Container[i].RelativeNorth * Container[i].RelativeNorth + Container[i].RelativeEast * Container[i].RelativeEast);
//
//                bearing = atan2f(Container[i].RelativeNorth,
//                    Container[i].RelativeEast) * 180.0 / PI;  /* -180 ... 180 */
//
//                 /* преобразовать математический угол в курс относительно севера */
//                bearing = (bearing <= 90.0 ? 90.0 - bearing :
//                    450.0 - bearing);
//
//               // bearing -= ThisAircraft.Track;
//                bearing -= ThisAircraft.course;
//
//                rel_x = constrain(distance * sin(radians(bearing)), -32768, 32767);
//                rel_y = constrain(distance * cos(radians(bearing)), -32768, 32767);
//                break;
//            default:
//                /* TBD */
//                break;
//            }
//
//            int16_t x = ((int32_t)rel_x * (int32_t)radius) / divider;
//            int16_t y = ((int32_t)rel_y * (int32_t)radius) / divider;
//
//            float RelativeVertical = Container[i].altitude - ThisAircraft.altitude;
//            uint32_t color = Container[i].AlarmLevel == ALARM_LEVEL_URGENT ? TFT_RED : (Container[i].AlarmLevel == ALARM_LEVEL_IMPORTANT ? TFT_YELLOW : TFT_GREEN);
//
//            if (Container[i].RelativeVertical > TFT_RADAR_V_THRESHOLD) 
//            {
//                tft_radar->fillTriangle(radar_center_x + x - 4, radar_center_y - y + 3,
//                    radar_center_x + x, radar_center_y - y - 5,
//                    radar_center_x + x + 4, radar_center_y - y + 3,
//                    color);
//            }
//            else if (Container[i].RelativeVertical < -TFT_RADAR_V_THRESHOLD) 
//            {
//                tft_radar->fillTriangle(radar_center_x + x - 4, radar_center_y - y - 3,
//                    radar_center_x + x, radar_center_y - y + 5,
//                    radar_center_x + x + 4, radar_center_y - y - 3,
//                    color);
//            }
//            else 
//            {
//                tft_radar->fillCircle(radar_center_x + x, radar_center_y - y, 5, color);
//                Serial.print("radar_center_x - ");
//                Serial.print(radar_center_x + x);
//                Serial.print("  radar_center_y - ");
//                Serial.println(radar_center_y);
//                Serial.println("");
//             }
//        }
//    }
}

void TFT_radar_setup()
{
    TFT_zoom = settings->m.zoom;
    uint16_t radar_x = 0;
    uint16_t radar_y = 0;
    uint16_t radar_w = tft_radar->width();
}

void TFT_radar_loop()
{

    bool hasData = settings->m.protocol = true; /*= PROTOCOL_NMEA ? NMEA_isConnected() :
        settings->m.protocol == PROTOCOL_GDL90 ? GDL90_isConnected() :
        false;*/

    /*bool hasData = settings->m.protocol == PROTOCOL_NMEA  ? NMEA_isConnected()  :
                    settings->m.protocol == PROTOCOL_GDL90 ? GDL90_isConnected() :
                    false;*/

    // if (hasData) 
    // {

    //     bool hasFix = settings->m.protocol = true;
    ///*         false;

    //   bool hasFix = settings->m.protocol == PROTOCOL_NMEA  ? isValidNMEAFix()   :
    //                 settings->m.protocol == PROTOCOL_GDL90 ? GDL90_hasOwnShip() :
    //                 false;*/

    //   if (hasFix) 
    //   {
    //     view_state_curr = STATE_RVIEW_RADAR;
    //   } else 
    //   {
    //     view_state_curr = STATE_RVIEW_NOFIX;
    //   }
    // }
    // else 
    // {
    //   view_state_curr = STATE_RVIEW_NODATA;
    // }

    view_state_curr = STATE_RVIEW_RADAR;

     //if (TFT_vmode_updated) 
     //{
     //  view_state_prev = STATE_RVIEW_NONE;
     //  TFT_vmode_updated = false;
     //}

     //if (view_state_curr != view_state_prev && view_state_curr == STATE_RVIEW_NOFIX) 
     //{
     //  TFT_Clear_Screen();
     //  TFT_Message(NO_FIX_TEXT, NULL);
     //  view_state_prev = view_state_curr;
     //}

     //if (view_state_curr != view_state_prev && view_state_curr == STATE_RVIEW_NODATA) 
     //{
     //  TFT_Clear_Screen();
     //  TFT_Message(NO_DATA_TEXT, NULL);
     //  view_state_prev = view_state_curr;
     //}

     if (view_state_curr == STATE_RVIEW_RADAR) 
     {
       if (view_state_curr != view_state_prev)
       {
          TFT_Clear_Screen();
          view_state_prev = view_state_curr;
       }
       TFT_Draw_Radar();
     }
}

void TFT_radar_zoom()
{
   // if (TFT_zoom < ZOOM_HIGH) TFT_zoom++;
}

void TFT_radar_unzoom()
{
   // if (TFT_zoom > ZOOM_LOWEST) TFT_zoom--;
}