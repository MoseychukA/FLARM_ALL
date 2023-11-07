/*
 * View_Radar_EPD.cpp
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

//#if defined(USE_EPAPER)

#include "EPD.h"

#include <TimeLib.h>

#include "TrafficHelper.h"
#include "EEPROMHELPER.h"
#include "NMEA.h"
#include "GDL90.h"
#include "LED.h"
#include "RF.h"

#include <TFT_eSPI.h>             // Поддержка TFT дисплея 
#include "Adafruit_GFX.h"

static TFT_eSPI* tft_radar = NULL;

//#include <Fonts/FreeMono9pt7b.h>
//#include <Fonts/FreeMonoBold9pt7b.h>
//#include <Fonts/FreeMonoBold12pt7b.h>
//#include <Fonts/Picopixel.h>

extern ui_settings_t ui_settings;
extern ui_settings_t* ui;

static int EPD_zoom = ZOOM_MEDIUM;

enum {
   STATE_RVIEW_NONE,
   STATE_RVIEW_RADAR,
   STATE_RVIEW_NOFIX,
   STATE_RVIEW_NODATA
};

static int view_state_curr = STATE_RVIEW_NONE;
static int view_state_prev = STATE_RVIEW_NONE;

static void EPD_Draw_Radar()
{
  int16_t  tbx, tby;
  uint16_t tbw, tbh;
  uint16_t x;
  uint16_t y;
  char cog_text[6];

#if defined(USE_EPD_TASK)
  if (EPD_update_in_progress == EPD_UPDATE_NONE) 
  {
//  if (SoC->tft_radar_lock()) 
  //    {
#else
  {
#endif
    /* divider is a half of full scale */
    int32_t divider = 2000;

    tft_radar->setFreeFont(&FreeMono9pt7b);
 
    uint16_t radar_x = 0;
    uint16_t radar_y = (tft_radar->height() - tft_radar->width()) / 2;
    uint16_t radar_w = tft_radar->width();

    uint16_t radar_center_x = radar_w / 2;
    uint16_t radar_center_y = radar_y + radar_w / 2;
    uint16_t radius = radar_w / 2 - 2;

    //if (ui->units == UNITS_METRIC || ui->units == UNITS_MIXED) 
    //{
    //  switch(EPD_zoom)
    //  {
    //  case ZOOM_LOWEST:
    //    divider = 30000; /* 60 KM */
    //    break;
    //  case ZOOM_LOW:
    //    divider =  5000; /* 10 KM */
    //    break;
    //  case ZOOM_HIGH:
    //    divider =  1000; /*  2 KM */
    //    break;
    //  case ZOOM_MEDIUM:
    //  default:
    //    divider =  2000;  /* 4 KM */
    //    break;
    //  }
    //}
    //else 
    //{
    //  switch(EPD_zoom)
    //  {
    //  case ZOOM_LOWEST:
    //    divider = 27780;  /* 30 NM */
    //    break;
    //  case ZOOM_LOW:
    //    divider = 4630;  /*  5 NM */
    //    break;
    //  case ZOOM_HIGH:
    //    divider =  926;  /*  1 NM */
    //    break;
    //  case ZOOM_MEDIUM:  /*  2 NM */
    //  default:
    //    divider = 1852;
    //    break;
    //  }
    //}

    tft_radar->fillScreen(TFT_WHITE);
 
    //for (int i=0; i < MAX_TRACKING_OBJECTS; i++) 
    //{
    //    if (Container[i].addr && (now() - Container[i].timestamp) <= EPD_EXPIRATION_TIME) 
    //    {

    //        int16_t rel_x;
    //        int16_t rel_y;
    //        float distance;
    //        float bearing;

    //        bool isTeam = (Container[i].addr == ui->team) ;

    //        distance = Container[i].distance;
    //        bearing  = Container[i].bearing;

    //        switch (ui->orientation)
    //        {
    //            case DIRECTION_NORTH_UP:
    //            break;
    //            case DIRECTION_TRACK_UP:
    //            bearing -= ThisAircraft.course;
    //            break;
    //            default:
    //            /* TBD */
    //            break;
    //        }

    //        rel_x = constrain(distance * sin(radians(bearing)),
    //                                    -32768, 32767);
    //        rel_y = constrain(distance * cos(radians(bearing)),
    //                                    -32768, 32767);

    //        int16_t x = ((int32_t) rel_x * (int32_t) radius) / divider;
    //        int16_t y = ((int32_t) rel_y * (int32_t) radius) / divider;

    //        float RelativeVertical = Container[i].altitude - ThisAircraft.altitude;

    //        if(RelativeVertical >   EPD_RADAR_V_THRESHOLD) 
    //        {
    //           if (isTeam) 
    //           {
    //            tft_radar->drawTriangle(radar_center_x + x - 5, radar_center_y - y + 4,
    //                                radar_center_x + x    , radar_center_y - y - 6,
    //                                radar_center_x + x + 5, radar_center_y - y + 4,
    //                                TFT_BLACK);
    //            tft_radar->drawTriangle(radar_center_x + x - 6, radar_center_y - y + 5,
    //                                radar_center_x + x    , radar_center_y - y - 7,
    //                                radar_center_x + x + 6, radar_center_y - y + 5,
    //                                TFT_BLACK);
    //           }
    //           else 
    //           {
    //             tft_radar->fillTriangle(radar_center_x + x - 4, radar_center_y - y + 3,
    //                                radar_center_x + x    , radar_center_y - y - 5,
    //                                radar_center_x + x + 4, radar_center_y - y + 3,
    //                                TFT_BLACK);
    //           }
    //        }
    //        else if (RelativeVertical < - EPD_RADAR_V_THRESHOLD) 
    //        {
    //        if (isTeam) {
    //            tft_radar->drawTriangle(radar_center_x + x - 5, radar_center_y - y - 4,
    //                                radar_center_x + x    , radar_center_y - y + 6,
    //                                radar_center_x + x + 5, radar_center_y - y - 4,
    //                                TFT_BLACK);
    //            tft_radar->drawTriangle(radar_center_x + x - 6, radar_center_y - y - 5,
    //                                radar_center_x + x    , radar_center_y - y + 7,
    //                                radar_center_x + x + 6, radar_center_y - y - 5,
    //                                TFT_BLACK);
    //        } else {
    //            tft_radar->fillTriangle(radar_center_x + x - 4, radar_center_y - y - 3,
    //                                radar_center_x + x    , radar_center_y - y + 5,
    //                                radar_center_x + x + 4, radar_center_y - y - 3,
    //                                TFT_BLACK);
    //        }
    //        } else {
    //        if (isTeam) {
    //            tft_radar->drawCircle(radar_center_x + x,
    //                                radar_center_y - y,
    //                                6, TFT_BLACK);
    //            tft_radar->drawCircle(radar_center_x + x,
    //                                radar_center_y - y,
    //                                7, TFT_BLACK);
    //            }
    //            else 
    //            {
    //            tft_radar->fillCircle(radar_center_x + x,
    //                                radar_center_y - y,
    //                                5, TFT_BLACK);
    //            }
    //        }
    //    }
    //}

        tft_radar->drawCircle(  radar_center_x, radar_center_y,
                            radius, TFT_BLACK);
        tft_radar->drawCircle(  radar_center_x, radar_center_y,
                            radius / 2, TFT_BLACK);

        if (ThisAircraft.aircraft_type == AIRCRAFT_TYPE_GLIDER     ||
            ThisAircraft.aircraft_type == AIRCRAFT_TYPE_TOWPLANE   ||
            ThisAircraft.aircraft_type == AIRCRAFT_TYPE_HELICOPTER ||
            ThisAircraft.aircraft_type == AIRCRAFT_TYPE_DROPPLANE  ||
            ThisAircraft.aircraft_type == AIRCRAFT_TYPE_POWERED    ||
            ThisAircraft.aircraft_type == AIRCRAFT_TYPE_JET) {

        /* little airplane */
        tft_radar->drawFastVLine(radar_center_x,      radar_center_y - 4, 14, TFT_BLACK);
        tft_radar->drawFastVLine(radar_center_x + 1,  radar_center_y - 4, 14, TFT_BLACK);

        tft_radar->drawFastHLine(radar_center_x - 8,  radar_center_y,     18, TFT_BLACK);
        tft_radar->drawFastHLine(radar_center_x - 10, radar_center_y + 1, 22, TFT_BLACK);

        tft_radar->drawFastHLine(radar_center_x - 3,  radar_center_y + 8,  8, TFT_BLACK);
        tft_radar->drawFastHLine(radar_center_x - 2,  radar_center_y + 9,  6, TFT_BLACK);

        }
        else 
        {

        /* arrow tip */
        tft_radar->fillTriangle(radar_center_x - 7, radar_center_y + 5,
                                radar_center_x    , radar_center_y - 5,
                                radar_center_x + 7, radar_center_y + 5,
                                TFT_BLACK);
        tft_radar->fillTriangle(radar_center_x - 7, radar_center_y + 5,
                                radar_center_x    , radar_center_y + 2,
                                radar_center_x + 7, radar_center_y + 5,
                                TFT_WHITE);
        }

        switch (0/*ui->orientation*/)
        {
            case DIRECTION_NORTH_UP:
            x = radar_x + radar_w / 2 - radius + tbw/2;
            y = radar_y + (radar_w + tbh) / 2;
            tft_radar->setCursor(x , y);
            tft_radar->print("W");
            x = radar_x + radar_w / 2 + radius - (3 * tbw)/2;
            y = radar_y + (radar_w + tbh) / 2;
            tft_radar->setCursor(x , y);
            tft_radar->print("E");
            x = radar_x + (radar_w - tbw) / 2;
            y = radar_y + radar_w/2 - radius + (3 * tbh)/2;
            tft_radar->setCursor(x , y);
            tft_radar->print("N");
            x = radar_x + (radar_w - tbw) / 2;
            y = radar_y + radar_w/2 + radius - tbh/2;
            tft_radar->setCursor(x , y);
            tft_radar->print("S");
            break;
            case DIRECTION_TRACK_UP:
            x = radar_x + radar_w / 2 - radius + tbw/2;
            y = radar_y + (radar_w + tbh) / 2;
            tft_radar->setCursor(x , y);
            tft_radar->print("L");
            x = radar_x + radar_w / 2 + radius - (3 * tbw)/2;
            y = radar_y + (radar_w + tbh) / 2;
            tft_radar->setCursor(x , y);
            tft_radar->print("R");
            x = radar_x + (radar_w - tbw) / 2;
            y = radar_y + radar_w/2 + radius - tbh/2;
            tft_radar->setCursor(x , y);
            tft_radar->print("B");

            tft_radar->setFreeFont(&FreeMonoBold9pt7b);
            snprintf(cog_text, sizeof(cog_text), "%03d", (int) ThisAircraft.course);
            //!!tft_radar->getTextBounds(cog_text, 0, 0, &tbx, &tby, &tbw, &tbh);

            x = radar_x + (radar_w - tbw) / 2;
            y = radar_y + radar_w/2 - radius + (3 * tbh)/2;
            tft_radar->setCursor(x , y);
            tft_radar->print(cog_text);
            tft_radar->drawRoundRect( x - 2, y - tbh - 2,
                                    tbw + 8, tbh + 6,
                                    4, TFT_BLACK);
            break;
            default:
            /* TBD */
            break;
        }

        tft_radar->setFreeFont(&FreeMonoBold12pt7b);
        x = radar_x + tbw / 2;
        y = radar_y + radar_w - tbh;
        tft_radar->setCursor(x, y);

        tft_radar->print(Traffic_Count());

        //!! tft_radar->setFreeFont(&Picopixel);
        y += tbh; y += tbh;
        tft_radar->setCursor(x, y);
        tft_radar->print("ACFTS");

        tft_radar->setFreeFont(&FreeMonoBold12pt7b);
 
        x = radar_x + radar_w - tbw;
        y = radar_y + radar_w - tbh;
        tft_radar->setCursor(x, y);

        tft_radar->print("10");

 /*       if (ui->units == UNITS_METRIC || ui->units == UNITS_MIXED) 
        {
         tft_radar->print(EPD_zoom == ZOOM_LOWEST ? "60" :
                        EPD_zoom == ZOOM_LOW    ? "10" :
                        EPD_zoom == ZOOM_MEDIUM ? "4 " :
                        EPD_zoom == ZOOM_HIGH   ? "2 " : "");
        }
        else 
        {
             tft_radar->print(EPD_zoom == ZOOM_LOWEST ? "30" :
                            EPD_zoom == ZOOM_LOW    ? "5 " :
                            EPD_zoom == ZOOM_MEDIUM ? "2 " :
                            EPD_zoom == ZOOM_HIGH   ? "1 " : "");
        }*/

        //tft_radar->setFreeFont(&Picopixel);
        x += tbw;
        y += tbh; y += tbh;
        tft_radar->setCursor(x, y);

        tft_radar->print("KM");

      /*  tft_radar->print(ui->units == UNITS_METRIC || ui->units == UNITS_MIXED ?
                        "KM" : "NM");
 */

    #if defined(USE_EPD_TASK)
        /* a signal to background EPD update task */
        EPD_update_in_progress = EPD_UPDATE_FAST;
    //    SoC->tft_radar_unlock();
    //    yield();
    #else
       // tft_radar->tft_radar(true);
    #endif
  }
}

void EPD_radar_setup()
{
  //EPD_zoom = ui->zoom;
}

void EPD_radar_loop()
{
  if (isTimeToEPD()) 
  {
   /* bool hasFix = isValidGNSSFix() || (settings->mode == SOFTRF_MODE_TXRX_TEST);

    if (hasFix) 
    {*/
    // EPD_Draw_Radar();
    //}
    //else 
    //{
    // // EPD_Message(NO_FIX_TEXT, NULL);
    //}

    EPDTimeMarker = millis();
  }
}

void EPD_radar_zoom()
{
  if (EPD_zoom < ZOOM_HIGH) EPD_zoom++;
}

void EPD_radar_unzoom()
{
  if (EPD_zoom > ZOOM_LOWEST) EPD_zoom--; else EPD_zoom = ZOOM_HIGH;
}

//#endif /* USE_EPAPER */
