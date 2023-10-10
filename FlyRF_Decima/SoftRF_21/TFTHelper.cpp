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

#if defined(EXCLUDE_TFT)
byte TFT_setup()                  {return DISPLAY_NONE;}
void TFT_loop()                   {}
void TFT_fini(const char *msg)    {}
void TFT_Mode_Cycle()             {}
#else

#include <SPI.h>
#include <TFT_eSPI.h>

#include <TimeLib.h>
//#include <FT5206.h>

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

TFT_eSPI *tft_radar = NULL;
TFT_eSprite *sprite = NULL;
//static FT5206_Class *tp = NULL;

static unsigned long TFTTimeMarker = 0;

static int TFT_view_mode = 0;
bool TFT_vmode_updated = true;

//static Gesture_t gesture = { false, {0,0}, {0,0} };

const char EPD_SoftRF_text1[] = "FlyRF";
const char EPD_SoftRF_text2[] = "and";
const char EPD_SoftRF_text3[] = "DECIMA";
const char EPD_SoftRF_text4[] = "Ver. 2023.10.10_21";
const char EPD_SoftRF_text5[] = "Linar Yusupov";
const char EPD_SoftRF_text6[] = "(C) 2016-2023";


const char EPD_Radio_text[] = "RADIO   ";
const char EPD_GNSS_text[] = "GNSS    ";
const char EPD_tft_epd_text[] = "tft_radar ";
const char EPD_RTC_text[] = "RTC     ";
const char EPD_Flash_text[] = "FLASH   ";
const char EPD_Baro_text[] = "BARO  ";
const char EPD_IMU_text[] = "IMU   ";

//void TFT_off()
//{
////#ifndef ST7735_DRIVER
////    tft_radar->writecommand(TFT_DISPOFF);
////    tft_radar->writecommand(TFT_SLPIN);
////#else
////    tft_radar->writecommand(ST7735_DISPOFF);
////    tft_radar->writecommand(ST7735_SLPIN);
////#endif /* ST7735_DRIVER */
////    if (tp) {
////      tp->enterSleepMode();
////    }
//}
//
//void TFT_sleep()
//{
////#ifndef ST7789_DRIVER
////    tft_radar->writecommand(TFT_DISPOFF);
////    tft_radar->writecommand(TFT_SLPIN);
////#else
////    tft_radar->writecommand(ST7735_DISPOFF);
////    tft_radar->writecommand(ST7735_SLPIN);
////#endif /* ST7789_DRIVER */
////    if (tp) {
////      tp->enterMonitorMode();
////    }
//}
//
//void TFT_wakeup()
//{
////#ifndef ST7789_DRIVER
//// /*   tft_radar->writecommand(TFT_SLPOUT);
////    tft_radar->writecommand(TFT_DISPON);*/
////#else
////    tft_radar->writecommand(ST7735_SLPOUT);
////    tft_radar->writecommand(ST7735_DISPON);
////#endif /* ST7735_DRIVER */
//}
//
//void TFT_backlight_init(void)
//{
//    //int bl_pin = (hw_info.model == SOFTRF_MODEL_WEBTOP_USB) ?
//    //             SOC_GPIO_PIN_TDONGLE_TFT_BL : SOC_GPIO_PIN_TWATCH_TFT_BL;
//
//    //ledcAttachPin(bl_pin, BACKLIGHT_CHANNEL);
//    //ledcSetup(BACKLIGHT_CHANNEL, 12000, 8);
//}
//
//uint8_t TFT_backlight_getLevel()
//{
//    return ledcRead(BACKLIGHT_CHANNEL);
//}
//
//void TFT_backlight_adjust(uint8_t level)
//{
//    //ledcWrite(BACKLIGHT_CHANNEL, level);
//}
//
//bool TFT_isBacklightOn()
//{
//    return (bool)ledcRead(BACKLIGHT_CHANNEL);
//}
//
//void TFT_backlight_off()
//{
//    //ledcWrite(BACKLIGHT_CHANNEL, 0);
//}
//
//void TFT_backlight_on()
//{
//    //ledcWrite(BACKLIGHT_CHANNEL, 250);
//}

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
    //   //!!tft_radar->getTextBounds(EPD_SoftRF_text2, 0, 0, &tbx2, &tby2, &tbw2, &tbh2);

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

 //  TFT_view_mode = settings->m.vmode;


    sprite = new TFT_eSprite(tft_radar);
    sprite->setColorDepth(1);

//    tft_radar->setTextFont(4);
//    tft_radar->setTextSize(2);
//    tft_radar->setTextColor(TFT_WHITE, TFT_NAVY);
//
//    uint16_t tbw = tft_radar->textWidth(SoftRF_text1);
//    uint16_t tbh = tft_radar->fontHeight();
//    tft_radar->setCursor((tft_radar->width() - tbw)/2, (tft_radar->height() - tbh)/2);
//    tft_radar->println(SoftRF_text1);

//    rval = DISPLAY_TFT_TTGO_240;

    //TFT_status_setup();
    TFT_radar_setup();
    //TFT_text_setup();
    //TFT_time_setup();

  //return rval;
}

void TFT_loop()
{
      if (isTimeToDisplay()) 
      {
        //switch (TFT_view_mode)
        //{
        //case VIEW_MODE_RADAR:
        TFT_radar_loop();
        //  break;
        //case VIEW_MODE_TEXT:
        //  TFT_text_loop();
        //  break;
        //case VIEW_MODE_TIME:
        //  TFT_time_loop();
        //  break;
        //case VIEW_MODE_STATUS:
        //default:
        //  TFT_status_loop();
        //  break;
        //}

        TFTTimeMarker = millis();
      }


 //     //  if (tp->touched()) {
 //     //      TP_Point p =  tp->getPoint();
 //     //      p.x = map(p.x, 0, TP_MAX_X, 0, LV_HOR_RES);
 //     //      p.y = map(p.y, 0, TP_MAX_Y, 0, LV_VER_RES);

 //     //      if (gesture.touched) {
 //     //        gesture.d_loc = p;
 //     //      } else {
 //     //        gesture.t_loc = p; gesture.d_loc = p;
 //     //        gesture.touched = true;
 //     //      }
 //     //  } else {
 //     //      if (gesture.touched) {
 //     //        int16_t threshold_x = tft_radar->width() / 10;
 //     //        int16_t threshold_y = tft_radar->height() / 10;
 //     //        int16_t limit_xl = tft_radar->width()/2 - threshold_x;
 //     //        int16_t limit_xr = tft_radar->width()/2 + threshold_x;
 //     //        int16_t limit_yt = tft_radar->height()/2 - threshold_y;
 //     //        int16_t limit_yb = tft_radar->height()/2 + threshold_y;

 //     //        if (gesture.d_loc.x < limit_xl && gesture.t_loc.x > limit_xr) {
 //     //          tp_action = SWIPE_LEFT;
 //     //        } else if (gesture.d_loc.x > limit_xr && gesture.t_loc.x < limit_xl) {
 //     //          tp_action = SWIPE_RIGHT;
 //     //        } else if (gesture.d_loc.y > limit_yb && gesture.t_loc.y < limit_yt) {
 //     //          tp_action = SWIPE_DOWN;
 //     //        } else if (gesture.d_loc.y < limit_yt && gesture.t_loc.y > limit_yb) {
 //     //          tp_action = SWIPE_UP;
 //     //        }

 //     //        gesture.touched = false;
 //     //        gesture.t_loc = gesture.d_loc = {0,0};
 //     //      } else {
 //     //         /* TBD */
 //     //      }
 //     //  }

 //     //  switch (tp_action)
 //     //  {
 //     //  case SWIPE_LEFT:
 //     //    if (TFT_view_mode < VIEW_MODE_TIME) {
 //     //      TFT_view_mode++;
 //     //      TFT_vmode_updated = true;
 //     //    }
 //     //    break;
 //     //  case SWIPE_RIGHT:
 //     //    if (TFT_view_mode > VIEW_MODE_STATUS) {
 //     //      TFT_view_mode--;
 //     //      TFT_vmode_updated = true;
 //     //    }
 //     //    break;
 //     //  case SWIPE_DOWN:
 //     //    TFT_Up();
 //     //    break;
 //     //  case SWIPE_UP:
 //     //    TFT_Down();
 //     //    break;
 //     //  case NO_GESTURE:
 //     //  default:
 //     //    break;
 //     //  }
 //     //}
 //   }

 //   break;

 // case DISPLAY_NONE:
 // default:
 //   break;
 // }
}

void TFT_fini(const char *msg)
{
  /*switch (hw_info.display)
  {
  case DISPLAY_TFT_TTGO_240:
  case DISPLAY_TFT_TTGO_135:
    if (tft_radar) {
        int level;

        for (level = 250; level >= 0; level -= 25) {
          TFT_backlight_adjust(level);
          delay(100);
        }

        tft_radar->fillScreen(TFT_NAVY);

        tft_radar->setTextFont(4);
        tft_radar->setTextSize(2);
        tft_radar->setTextColor(TFT_WHITE, TFT_NAVY);

        uint16_t tbw = tft_radar->textWidth(msg);
        uint16_t tbh = tft_radar->fontHeight();

        tft_radar->setCursor((tft_radar->width() - tbw)/2, (tft_radar->height() - tbh)/2);
        tft_radar->print(msg);

        for (level = 0; level <= 250; level += 25) {
          TFT_backlight_adjust(level);
          delay(100);
        }

        delay(2000);

        for (level = 250; level >= 0; level -= 25) {
          TFT_backlight_adjust(level);
          delay(100);
        }

        TFT_backlight_off();
        TFT_off();
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
 /* if (hw_info.display == DISPLAY_TFT_TTGO_240) {
    switch (TFT_view_mode)
    {
    case VIEW_MODE_RADAR:
      TFT_radar_unzoom();
      break;
    case VIEW_MODE_TEXT:
      TFT_text_prev();
      break;
    case VIEW_MODE_TIME:
      TFT_time_prev();
      break;
    case VIEW_MODE_STATUS:
    default:
      TFT_status_prev();
      break;
    }
  }*/
}

void TFT_Down()
{
  /*if (hw_info.display == DISPLAY_TFT_TTGO_240) {
    switch (TFT_view_mode)
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
    }
  }*/
}

void TFT_Message(const char *msg1, const char *msg2)
{
  int16_t  tbx, tby;
  uint16_t tbw, tbh;
  uint16_t x, y;

 /* if (msg1 != NULL && strlen(msg1) != 0) {
    tft_radar->setTextFont(4);
    tft_radar->setTextSize(2);

    tft_radar->fillScreen(TFT_NAVY);

    tbw = tft_radar->textWidth(msg1);
    tbh = tft_radar->fontHeight();
    x = (tft_radar->width() - tbw) / 2;
    y = msg2 == NULL ? (tft_radar->height() - tbh) / 2 : tft_radar->height() / 2 - tbh;
    tft_radar->setCursor(x, y);
    tft_radar->print(msg1);

    if (msg2 != NULL && strlen(msg2) != 0) {
      tbw = tft_radar->textWidth(msg2);
      x = (tft_radar->width() - tbw) / 2;
      y = tft_radar->height() / 2;
      tft_radar->setCursor(x, y);
      tft_radar->print(msg2);
    }
  }*/
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

#endif /* EXCLUDE_TFT */


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

    /* divider is a half of full scale */
    int32_t divider = 2000;

    sprite->createSprite(tft_radar->width(), tft_radar->height());

    sprite->fillSprite(TFT_BLACK);
    sprite->setTextColor(TFT_WHITE);

    sprite->setTextFont(4);
    sprite->setTextSize(1);

    tbw = sprite->textWidth("N");
    tbh = sprite->fontHeight();

    uint16_t radar_x = 0;
    uint16_t radar_y = 0;
    uint16_t radar_w = sprite->width();

    uint16_t radar_center_x = radar_w / 2;
    uint16_t radar_center_y = radar_y + radar_w / 2;
    uint16_t radius = radar_w / 2 - 1;

    //if (settings->m.units == UNITS_METRIC || settings->m.units == UNITS_MIXED) {
    //    switch (TFT_zoom)
    //    {
    //    case ZOOM_LOWEST:
    //        divider = 10000; /* 20 KM */
    //        break;
    //    case ZOOM_LOW:
    //        divider = 5000; /* 10 KM */
    //        break;
    //    case ZOOM_HIGH:
    //        divider = 1000; /*  2 KM */
    //        break;
    //    case ZOOM_MEDIUM:
    //    default:
    //        divider = 2000;  /* 4 KM */
    //        break;
    //    }
    //}
    //else {
    //    switch (TFT_zoom)
    //    {
    //    case ZOOM_LOWEST:
    //        divider = 9260;  /* 10 NM */
    //        break;
    //    case ZOOM_LOW:
    //        divider = 4630;  /*  5 NM */
    //        break;
    //    case ZOOM_HIGH:
    //        divider = 926;  /*  1 NM */
    //        break;
    //    case ZOOM_MEDIUM:  /*  2 NM */
    //    default:
    //        divider = 1852;
    //        break;
    //    }
    //}

    sprite->drawCircle(radar_center_x, radar_center_y,
        radius, TFT_WHITE);
    sprite->drawCircle(radar_center_x, radar_center_y,
        radius / 2, TFT_WHITE);

#if 0
    /* arrow tip */
    sprite->fillTriangle(radar_center_x - 7, radar_center_y + 5,
        radar_center_x, radar_center_y - 5,
        radar_center_x + 7, radar_center_y + 5,
        TFT_WHITE);
    sprite->fillTriangle(radar_center_x - 7, radar_center_y + 5,
        radar_center_x, radar_center_y + 2,
        radar_center_x + 7, radar_center_y + 5,
        TFT_NAVY);
#else
    /* little airplane */
    sprite->drawFastVLine(radar_center_x, radar_center_y - 4, 14, TFT_WHITE);
    sprite->drawFastVLine(radar_center_x + 1, radar_center_y - 4, 14, TFT_WHITE);

    sprite->drawFastHLine(radar_center_x - 8, radar_center_y, 18, TFT_WHITE);
    sprite->drawFastHLine(radar_center_x - 10, radar_center_y + 1, 22, TFT_WHITE);

    sprite->drawFastHLine(radar_center_x - 3, radar_center_y + 8, 8, TFT_WHITE);
    sprite->drawFastHLine(radar_center_x - 2, radar_center_y + 9, 6, TFT_WHITE);
#endif

    switch (0/*settings->m.orientation*/)
    {
    case DIRECTION_NORTH_UP:
        x = radar_x + radar_w / 2 - radius + tbw / 2;
        y = radar_y + (radar_w - tbh) / 2;
        sprite->setCursor(x, y);
        sprite->print("W");
        x = radar_x + radar_w / 2 + radius - (3 * tbw) / 2;
        y = radar_y + (radar_w - tbh) / 2;
        sprite->setCursor(x, y);
        sprite->print("E");
        x = radar_x + (radar_w - tbw) / 2;
        y = radar_y + radar_w / 2 - radius + tbh / 2;
        sprite->setCursor(x, y);
        sprite->print("N");
        x = radar_x + (radar_w - tbw) / 2;
        y = radar_y + radar_w / 2 + radius - tbh;
        sprite->setCursor(x, y);
        sprite->print("S");
        break;
    case DIRECTION_TRACK_UP:
        x = radar_x + radar_w / 2 - radius + tbw / 2;
        y = radar_y + (radar_w - tbh) / 2;
        sprite->setCursor(x, y);
        sprite->print("L");
        x = radar_x + radar_w / 2 + radius - (3 * tbw) / 2;
        y = radar_y + (radar_w - tbh) / 2;
        sprite->setCursor(x, y);
        sprite->print("R");
        x = radar_x + (radar_w - tbw) / 2;
        y = radar_y + radar_w / 2 + radius - tbh;
        sprite->setCursor(x, y);
        sprite->print("B");

       //!! snprintf(cog_text, sizeof(cog_text), "%03d", ThisAircraft.Track);
        tbw = sprite->textWidth(cog_text);
        tbh = sprite->fontHeight();
        x = radar_x + (radar_w - tbw) / 2;
        y = radar_y + radar_w / 2 - radius + tbh / 2;
        sprite->setCursor(x, y);
        sprite->print(cog_text);
#if 0
        sprite->drawRoundRect(x - 2, y - tbh - 2,
            tbw + 8, tbh + 6,
            4, TFT_WHITE);
#endif
        break;
    default:
        /* TBD */
        break;
    }

    sprite->setTextColor(TFT_WHITE, TFT_BLACK);
    x = radar_x;
    y = radar_y + radar_w - tbh;
    sprite->setCursor(x, y);

    sprite->print(" 4 KM");
 /*   if (settings->m.units == UNITS_METRIC || settings->m.units == UNITS_MIXED) {
        sprite->print(TFT_zoom == ZOOM_LOWEST ? "20 KM" :
            TFT_zoom == ZOOM_LOW ? "10 KM" :
            TFT_zoom == ZOOM_MEDIUM ? " 4 KM" :
            TFT_zoom == ZOOM_HIGH ? " 2 KM" : "");
    }
    else {
        sprite->print(TFT_zoom == ZOOM_LOWEST ? "10 NM" :
            TFT_zoom == ZOOM_LOW ? " 5 NM" :
            TFT_zoom == ZOOM_MEDIUM ? " 2 NM" :
            TFT_zoom == ZOOM_HIGH ? " 1 NM" : "");
    }*/

    tft_radar->setBitmapColor(TFT_WHITE, TFT_NAVY);
    sprite->pushSprite(0, 0);
    sprite->deleteSprite();

   // for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) 
   // {
   //     if (Container[i].ID && (now() - Container[i].timestamp) <= TFT_EXPIRATION_TIME) 
   //     {

   //         int16_t rel_x;
   //         int16_t rel_y;
   //         float distance;
   //         float bearing;

   //         switch (0/*settings->m.orientation*/)
   //         {
   //         case DIRECTION_NORTH_UP:
   //             rel_x = Container[i].RelativeEast;
   //             rel_y = Container[i].RelativeNorth;
   //             break;
   //         case DIRECTION_TRACK_UP:
   //             distance = sqrtf(Container[i].RelativeNorth * Container[i].RelativeNorth +
   //                 Container[i].RelativeEast * Container[i].RelativeEast);

   //             bearing = atan2f(Container[i].RelativeNorth,
   //                 Container[i].RelativeEast) * 180.0 / PI;  /* -180 ... 180 */

   ///* convert from math angle into course relative to north */
   //             bearing = (bearing <= 90.0 ? 90.0 - bearing :
   //                 450.0 - bearing);

   //             bearing -= ThisAircraft.Track;

   //             rel_x = constrain(distance * sin(radians(bearing)),
   //                 -32768, 32767);
   //             rel_y = constrain(distance * cos(radians(bearing)),
   //                 -32768, 32767);
   //             break;
   //         default:
   //             /* TBD */
   //             break;
   //         }

   //         int16_t x = ((int32_t)rel_x * (int32_t)radius) / divider;
   //         int16_t y = ((int32_t)rel_y * (int32_t)radius) / divider;

   //         uint32_t color = Container[i].AlarmLevel == ALARM_LEVEL_URGENT ? TFT_RED :
   //             (Container[i].AlarmLevel == ALARM_LEVEL_IMPORTANT ?
   //                 TFT_YELLOW : TFT_GREEN);

   //         if (Container[i].RelativeVertical > TFT_RADAR_V_THRESHOLD) {
   //             tft_radar->fillTriangle(radar_center_x + x - 4, radar_center_y - y + 3,
   //                 radar_center_x + x, radar_center_y - y - 5,
   //                 radar_center_x + x + 4, radar_center_y - y + 3,
   //                 color);
   //         }
   //         else if (Container[i].RelativeVertical < -TFT_RADAR_V_THRESHOLD) {
   //             tft_radar->fillTriangle(radar_center_x + x - 4, radar_center_y - y - 3,
   //                 radar_center_x + x, radar_center_y - y + 5,
   //                 radar_center_x + x + 4, radar_center_y - y - 3,
   //                 color);
   //         }
   //         else {
   //             tft_radar->fillCircle(radar_center_x + x,
   //                 radar_center_y - y,
   //                 5, color);
   //         }
   //     }
   // }
}

void TFT_radar_setup()
{
    //TFT_zoom = settings->m.zoom;
    uint16_t radar_x = 0;
    uint16_t radar_y = 0;
    uint16_t radar_w = tft_radar->width();
}

void TFT_radar_loop()
{
   /*  bool hasData = settings->m.protocol == PROTOCOL_NMEA  ? NMEA_isConnected()  :
                    settings->m.protocol == PROTOCOL_GDL90 ? GDL90_isConnected() :
                    false;

     if (hasData) {

       bool hasFix = settings->m.protocol == PROTOCOL_NMEA  ? isValidNMEAFix()   :
                     settings->m.protocol == PROTOCOL_GDL90 ? GDL90_hasOwnShip() :
                     false;

       if (hasFix) {
         view_state_curr = STATE_RVIEW_RADAR;
       } else {
         view_state_curr = STATE_RVIEW_NOFIX;
       }
     } else {
       view_state_curr = STATE_RVIEW_NODATA;
     }

     if (TFT_vmode_updated) {
       view_state_prev = STATE_RVIEW_NONE;
       TFT_vmode_updated = false;
     }

     if (view_state_curr != view_state_prev &&
         view_state_curr == STATE_RVIEW_NOFIX) {
       TFT_Clear_Screen();
       TFT_Message(NO_FIX_TEXT, NULL);
       view_state_prev = view_state_curr;
     }

     if (view_state_curr != view_state_prev &&
         view_state_curr == STATE_RVIEW_NODATA) {
       TFT_Clear_Screen();
       TFT_Message(NO_DATA_TEXT, NULL);
       view_state_prev = view_state_curr;
     }

     if (view_state_curr == STATE_RVIEW_RADAR) {
       if (view_state_curr != view_state_prev) {
          TFT_Clear_Screen();
          view_state_prev = view_state_curr;
       }
       TFT_Draw_Radar();
     }*/

     TFT_Draw_Radar(); //!!
}

void TFT_radar_zoom()
{
   // if (TFT_zoom < ZOOM_HIGH) TFT_zoom++;
}

void TFT_radar_unzoom()
{
   // if (TFT_zoom > ZOOM_LOWEST) TFT_zoom--;
}