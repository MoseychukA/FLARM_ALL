/*
 * EPDHelper.cpp
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

#include "EPD.h"
#include "LED.h"
#include "RF.h"
#include "Baro.h"
#include "TrafficHelper.h"
#include "TimeHELPER.h"
#include <TFT_eSPI.h>             // Поддержка TFT дисплея 
#include "Adafruit_GFX.h"
#include <TimeLib.h>
#include "EEPROMHELPER.h"
#include "NMEA.h"
#include "GDL90.h"
#include "PLATFORM_ESP32.h"


static TFT_eSPI* tft_epd = NULL;

//#include "Fonts/FreeMonoBold24pt7b.h"
//#include "Fonts/FreeMonoBold18pt7b.h"
//#include "Fonts/FreeMonoBold12pt7b.h"
//#include "Fonts/FreeMono18pt7b.h"
//#include <Fonts/FreeMonoBold9pt7b.h>
//#include <Fonts/Picopixel.h>
//#include "Fonts/Org_01.h"
//#include "Fonts/FreeMonoBoldOblique9pt7b.h"
//#include "Fonts/FreeSerif9pt7b.h"


const char EPD_SoftRF_text1[] = "FlyRF";
const char EPD_SoftRF_text2[] =  "and"  ;
const char EPD_SoftRF_text3[] = "DECIMA";
const char EPD_SoftRF_text4[] = "Ver.SoftRF_20_EDP_04";
const char EPD_SoftRF_text5[] = "Linar Yusupov";
const char EPD_SoftRF_text6[] = "(C) 2016-2023";


const char EPD_Radio_text[]   = "RADIO   ";
const char EPD_GNSS_text[]    = "GNSS    ";
const char EPD_tft_epd_text[] = "tft_epd ";
const char EPD_RTC_text[]     = "RTC     ";
const char EPD_Flash_text[]   = "FLASH   ";
const char EPD_Baro_text[]    = "BARO  ";
const char EPD_IMU_text[]     = "IMU   ";

unsigned long EPDTimeMarker = 0;
static unsigned long EPD_anti_ghosting_timer = 0;
static uint8_t anti_ghosting_minutes = 0;

static int EPD_view_mode = 0;
bool EPD_vmode_updated = true;
uint16_t EPD_pages_mask = (1 << VIEW_MODE_STATUS) |
                          (1 << VIEW_MODE_RADAR ) |
                          (1 << VIEW_MODE_TEXT  ) |
                          (1 << VIEW_MODE_TIME  );

volatile uint8_t EPD_update_in_progress = EPD_UPDATE_NONE;

void EPD_setup()
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


  tft_epd = new TFT_eSPI(LV_HOR_RES, LV_VER_RES);
  tft_epd->init();
  tft_epd->setRotation(3);

  tft_epd->fillScreen(TFT_NAVY);

  
  tft_epd->setTextColor(TFT_WHITE); //TFT_WHITE TFT_BLACK
  tft_epd->setTextWrap(false);

  tft_epd->setFreeFont(&FreeMonoBold24pt7b);
  //tft_epd->fillScreen(TFT_WHITE);

 // if (hw_info.model == SOFTRF_MODEL_BADGE) {
  tbw1 = sizeof(EPD_SoftRF_text1);

  x = 90;// (tft_epd->width() - tbw1) / 2;
  y = 80;// (tft_epd->height() + tbh1) / 2 - tbh3;
    tft_epd->setCursor(x, y);
    tft_epd->print(EPD_SoftRF_text1);

    tft_epd->setFreeFont(&FreeMono18pt7b);
 
    x = 130;// (tft_epd->width() - tbw2) / 2;
    y = 125;// (tft_epd->height() + tbh2) / 2;
    tft_epd->setCursor(x, y);
    tft_epd->print(EPD_SoftRF_text2);

    tft_epd->setFreeFont(&FreeMonoBold24pt7b);

    x = 80;// (tft_epd->width() - tbw3) / 2;
    y = 180;// tft_epd->height() + tbh3) / 2 + tbh3;
    tft_epd->setCursor(x, y);
    tft_epd->print(EPD_SoftRF_text3);

     tft_epd->setFreeFont(&FreeSerif9pt7b);
     tbw4 = tft_epd->textWidth(EPD_SoftRF_text4);
     //tft_epd->fontHeight();
 
     x = (tft_epd->width() - tbw4)-4;
     y = tft_epd->height() - tft_epd->fontHeight()+10;
    tft_epd->setCursor(x, y);
    tft_epd->print(EPD_SoftRF_text4);
    

 // rval = tft_epd_TFT_TTGO_240;//tft_epd->probe();

 // EPD_status_setup();
    EPD_radar_setup();
 // EPD_text_setup();
 // EPD_baro_setup();
 // EPD_time_setup();
 // EPD_imu_setup();

  EPD_view_mode = ui->vmode;
  if (EPD_pages_mask & (1 << EPD_view_mode) == 0) {
    for (int i=0; i < VIEW_MODES_COUNT; i++) {
      int next_view_mode = (EPD_view_mode + i) % VIEW_MODES_COUNT;
      if ((next_view_mode != EPD_view_mode) &&
          (EPD_pages_mask & (1 << next_view_mode))) {
        EPD_view_mode = next_view_mode;
        break;
      }
    }
  }

  switch (ui->aghost)
  {
    case ANTI_GHOSTING_2MIN:
      anti_ghosting_minutes = 2;
      break;
    case ANTI_GHOSTING_5MIN:
    case ANTI_GHOSTING_AUTO:
      anti_ghosting_minutes = 5;
      break;
    case ANTI_GHOSTING_10MIN:
      anti_ghosting_minutes = 10;
      break;
    case ANTI_GHOSTING_OFF:
    default:
      break;
  }

  SoC->ADB_ops && SoC->ADB_ops->setup();

  EPDTimeMarker = millis();
  EPD_anti_ghosting_timer = millis();

}

void EPD_info1()
{
//  switch (hw_info.tft_epd)
//  {
//  case tft_epd_EPD_1_54:
//  case tft_epd_EPD_2_7:
//    int16_t  tbx, tby;
//    uint16_t tbw, tbh;
//
//    uint16_t x, y;
//
//#if defined(USE_EPD_TASK)
//    while (EPD_update_in_progress != EPD_UPDATE_NONE) delay(100);
//
////    while (!SoC->tft_epd_lock()) { delay(10); }
//#endif
//    tft_epd->setFreeFont(&FreeMonoBold18pt7b);
//    //!!tft_epd->getTextBounds(EPD_Radio_text, 0, 0, &tbx, &tby, &tbw, &tbh);
//
//    tft_epd->fillScreen(TFT_WHITE);
//
//    x = 5;
//    y = (tbh + INFO_1_LINE_SPACING - 2);
//
//    tft_epd->setCursor(x, y);
//    tft_epd->print(EPD_Radio_text);
//    tft_epd->print(hw_info.rf != RF_IC_NONE ? "+" : "-");
//
//    y += (tbh + INFO_1_LINE_SPACING);
//
//    tft_epd->setCursor(x, y);
//    tft_epd->print(EPD_GNSS_text);
//    tft_epd->print(hw_info.gnss != GNSS_MODULE_NONE ? "+" : "-");
//
//    y += (tbh + INFO_1_LINE_SPACING);
//
//    tft_epd->setCursor(x, y);
//    tft_epd->print(EPD_tft_epd_text);
//    tft_epd->print(hw_info.tft_epd != tft_epd_NONE ? "+" : "-");
//
//    if (hw_info.model == SOFTRF_MODEL_BADGE) {
//      y += (tbh + INFO_1_LINE_SPACING);
//
//      tft_epd->setCursor(x, y);
//      tft_epd->print(EPD_RTC_text);
//      tft_epd->print(hw_info.rtc != RTC_NONE ? "+" : "-");
//
//      y += (tbh + INFO_1_LINE_SPACING);
//
//      tft_epd->setCursor(x, y);
//      tft_epd->print(EPD_Flash_text);
//      tft_epd->print(hw_info.storage == STORAGE_FLASH ? "+" : "-");
//
//      y += (tbh + INFO_1_LINE_SPACING);
//
//      if (hw_info.baro == BARO_MODULE_NONE) {
//        tft_epd->setFreeFont(&FreeMono18pt7b);
//      }
//
//      tft_epd->setCursor(x, y);
//      tft_epd->print(EPD_Baro_text);
//      tft_epd->print(hw_info.baro != BARO_MODULE_NONE ? "  +" : "N/A");
//
//      y += (tbh + INFO_1_LINE_SPACING);
//
//      if (hw_info.imu == IMU_NONE) {
//        tft_epd->setFreeFont(&FreeMono18pt7b);
//      } else {
//        tft_epd->setFreeFont(&FreeMonoBold18pt7b);
//      }
//
//      tft_epd->setCursor(x, y);
//      tft_epd->print(EPD_IMU_text);
//      tft_epd->print(hw_info.imu != IMU_NONE ? "  +" : "N/A");
//    }
//
//#if defined(USE_EPD_TASK)
//    EPD_update_in_progress = EPD_UPDATE_SLOW;
//    while (EPD_update_in_progress != EPD_UPDATE_NONE) { delay(100); }
////    SoC->tft_epd_unlock();
//#else
//    //tft_epd->tft_epd(false);
//#endif
//
//    delay(4000);
//
//#if 0
//    tft_epd->fillScreen(TFT_WHITE);
//
//    EPD_update_in_progress = EPD_UPDATE_SLOW;
//    while (EPD_update_in_progress != EPD_UPDATE_NONE) { delay(100); }
//#endif
//
//    break;
//
//  case tft_epd_NONE:
//  default:
//    break;
//  }
}

void EPD_info2(int acfts, char *reg, char *mam, char *cn)
{
  const char *msg_line;

 /* switch (hw_info.tft_epd)
  {
  case tft_epd_EPD_1_54:
  case tft_epd_EPD_2_7:*/
    int16_t  tbx, tby;
    uint16_t tbw, tbh;

    uint16_t x, y;

#if defined(USE_EPD_TASK)
    while (EPD_update_in_progress != EPD_UPDATE_NONE) delay(100);

//    while (!SoC->tft_epd_lock()) { delay(10); }
#endif

    tft_epd->fillScreen(TFT_WHITE);

    if (acfts == -1) {

      tft_epd->setFreeFont(&FreeMonoBold18pt7b);

      msg_line = "WARNING";

      //!!tft_epd->getTextBounds(msg_line, 0, 0, &tbx, &tby, &tbw, &tbh);
      x = (tft_epd->width() - tbw) / 2;
      y = tft_epd->height() / 3;
      tft_epd->setCursor(x, y);
      tft_epd->print(msg_line);

      tft_epd->setFreeFont(&FreeMono18pt7b);

      msg_line = "NO";

     //!!tft_epd->getTextBounds(msg_line, 0, 0, &tbx, &tby, &tbw, &tbh);
      x = (tft_epd->width() - tbw) / 2;
      y = (2 * tft_epd->height()) / 3 - tbh;
      tft_epd->setCursor(x, y);
      tft_epd->print(msg_line);

      y += (tbh + INFO_1_LINE_SPACING);

      msg_line = "AIRCRAFTS";

      //!!tft_epd->getTextBounds(msg_line, 0, 0, &tbx, &tby, &tbw, &tbh);
      x = (tft_epd->width() - tbw) / 2;
      tft_epd->setCursor(x, y);
      tft_epd->print(msg_line);

      y += (tbh + INFO_1_LINE_SPACING);

      msg_line = "DATA";

      //!!tft_epd->getTextBounds(msg_line, 0, 0, &tbx, &tby, &tbw, &tbh);
      x = (tft_epd->width() - tbw) / 2;
      tft_epd->setCursor(x, y);
      tft_epd->print(msg_line);

    } else {

      char buf[TEXT_VIEW_LINE_LENGTH+1];

      snprintf(buf, sizeof(buf), "%d", acfts);

      tft_epd->setFreeFont(&FreeMonoBold12pt7b);

      //!!tft_epd->getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
      x = (tft_epd->width() - tbw) / 2;
      y = tft_epd->height() / 4 - tbh;
      tft_epd->setCursor(x, y);
      tft_epd->print(buf);

      y += (tbh + INFO_1_LINE_SPACING);

      msg_line = "ACFTS LOADED";

      //!!tft_epd->getTextBounds(msg_line, 0, 0, &tbx, &tby, &tbw, &tbh);
      x = (tft_epd->width() - tbw) / 2;
      tft_epd->setCursor(x, y);
      tft_epd->print(msg_line);

      y += (tbh + INFO_1_LINE_SPACING);
      y += (tbh + INFO_1_LINE_SPACING);
      y += (tbh + INFO_1_LINE_SPACING);

      msg_line = "THIS AIRCRAFT:";

      //!!tft_epd->getTextBounds(msg_line, 0, 0, &tbx, &tby, &tbw, &tbh);
      x = (tft_epd->width() - tbw) / 2;
      tft_epd->setCursor(x, y);
      tft_epd->print(msg_line);

      x = 10;
      y += (tbh + INFO_1_LINE_SPACING);

      snprintf(buf, sizeof(buf), "%s", reg);
      tft_epd->setCursor(x, y);
      tft_epd->print(buf);

      y += (tbh + INFO_1_LINE_SPACING);

      snprintf(buf, sizeof(buf), "%s", mam);
      tft_epd->setCursor(x, y);
      tft_epd->print(buf);

      y += (tbh + INFO_1_LINE_SPACING);

      snprintf(buf, sizeof(buf), " CN: %s", cn);
      tft_epd->setCursor(x, y);
      tft_epd->print(buf);
    }

#if defined(USE_EPD_TASK)
    EPD_update_in_progress = EPD_UPDATE_SLOW;
    while (EPD_update_in_progress != EPD_UPDATE_NONE) { delay(100); }
//    SoC->tft_epd_unlock();
#else
    //tft_epd->tft_epd(false);
#endif

    delay(3000);
  //  break;

  //case tft_epd_NONE:
  //default:
  //  break;
  //}
}

void EPD_loop()
{
//  switch (hw_info.tft_epd)
//  {
//  case tft_epd_EPD_1_54:
//
//    if (EPD_vmode_updated) {
//#if defined(USE_EPD_TASK)
//      if (EPD_update_in_progress == EPD_UPDATE_NONE) {
////      while (!SoC->tft_epd_lock()) { delay(10); }
//#else
//      {
//#endif
//        tft_epd->fillScreen(TFT_BLACK /* TFT_WHITE */);
//
//#if defined(USE_EPD_TASK)
//        EPD_update_in_progress = EPD_UPDATE_FAST /* EPD_UPDATE_SLOW */;
//        while (EPD_update_in_progress != EPD_UPDATE_NONE) { delay(100); }
////      SoC->tft_epd_unlock();
//#else
////        tft_epd->tft_epd(false);
//        //tft_epd->tft_epd(true);
//#endif
//        EPD_vmode_updated = false;
//      }
//
//    } else {
//      switch (EPD_view_mode)
//      {
//      case VIEW_MODE_RADAR:
        EPD_radar_loop();
//        break;
//      case VIEW_MODE_TEXT:
//        EPD_text_loop();
//        break;
//      case VIEW_MODE_BARO:
//        EPD_baro_loop();
//        break;
//      case VIEW_MODE_TIME:
//        EPD_time_loop();
//        break;
//      case VIEW_MODE_IMU:
//        EPD_imu_loop();
//        break;
//      case VIEW_MODE_STATUS:
//      default:
//        EPD_status_loop();
//        break;
//      }
//
//      bool auto_ag_condition = ui->aghost == ANTI_GHOSTING_AUTO  &&
//                               (EPD_view_mode == VIEW_MODE_RADAR ||
//                                EPD_view_mode == VIEW_MODE_TEXT) ?
//                               (Traffic_Count() == 0) : true;
//
//      if (anti_ghosting_minutes > 0                                                &&
//          (millis() - EPD_anti_ghosting_timer) > (anti_ghosting_minutes * 60000UL) &&
//          auto_ag_condition) {
//        EPD_vmode_updated = true;
//        EPD_anti_ghosting_timer = millis();
//      }
//    }
//    break;
//
//  case tft_epd_NONE:
//  default:
//    break;
//  }
}

void EPD_fini(int reason, bool screen_saver)
{
  int16_t  tbx, tby;
  uint16_t tbw, tbh;
  uint16_t x, y;

  SoC->ADB_ops && SoC->ADB_ops->fini();

  const char *msg = (reason == SOFTRF_SHUTDOWN_LOWBAT ?
                     "LOW BATTERY" : "NORMAL OFF");

//  switch (hw_info.tft_epd)
//  {
//  case tft_epd_EPD_1_54:
//#if defined(USE_EPD_TASK)
//      while (EPD_update_in_progress != EPD_UPDATE_NONE) delay(100);
////      while (!SoC->tft_epd_lock()) { delay(10); }
//#endif
//    if (screen_saver) {
//      const char *msg_line;
//
//      tft_epd->setFreeFont(&FreeMonoBold12pt7b);
//      tft_epd->fillScreen(TFT_WHITE);
//
//      msg_line = "POWER OFF";
//
//      //!!tft_epd->getTextBounds(msg_line, 0, 0, &tbx, &tby, &tbw, &tbh);
//      x = (tft_epd->width() - tbw) / 2;
//      y = tft_epd->height() / 3;
//      tft_epd->setCursor(x, y);
//      tft_epd->print(msg_line);
//
//      msg_line = "SCREEN SAVER";
//
//      //!!tft_epd->getTextBounds(msg_line, 0, 0, &tbx, &tby, &tbw, &tbh);
//      x = (tft_epd->width() - tbw) / 2;
//      y = (2 * tft_epd->height()) / 3;
//      tft_epd->setCursor(x, y);
//      tft_epd->print(msg_line);
//
//#if defined(USE_EPD_TASK)
//      /* a signal to background EPD update task */
//      EPD_update_in_progress = EPD_UPDATE_SLOW /* EPD_UPDATE_FAST */;
////      SoC->tft_epd_unlock();
//
////    yield();
//
//      while (EPD_update_in_progress != EPD_UPDATE_NONE) delay(100);
////      while (!SoC->tft_epd_lock()) { delay(10); }
//#else
//      //tft_epd->tft_epd(false);
//#endif
//
//      SoC->loop(); /* reload WDT */
//
//      delay(4000);
//
//      SoC->loop(); /* reload WDT */
//
//      tft_epd->fillScreen(TFT_WHITE);
//
//    } else {
//
//      tft_epd->fillScreen(TFT_WHITE);
//
//      tft_epd->setFreeFont(&FreeMonoBold12pt7b);
//     //!!tft_epd->getTextBounds(msg, 0, 0, &tbx, &tby, &tbw, &tbh);
//      x = (tft_epd->width() - tbw) / 2;
//      y = tbh + tbh / 2;
//      tft_epd->setCursor(x, y);
//      tft_epd->print(msg);
//
//      x = (tft_epd->width()  - 128) / 2;
//      y = (tft_epd->height() - 128) / 2 - tbh / 2;
//      tft_epd->drawBitmap(x, y, sleep_icon_128x128, 128, 128, TFT_BLACK);
//
//      //***tft_epd->setFreeFont(&Org_01);
//      //!!tft_epd->getTextBounds(EPD_SoftRF_text4, 0, 0, &tbx, &tby, &tbw, &tbh);
//      x =  5;
//      y += 128 + 17;
//      tft_epd->setCursor(x, y);
//      tft_epd->print(EPD_SoftRF_text4);
//
//      tft_epd->setFreeFont(&FreeMonoBoldOblique9pt7b);
//      tft_epd->print(EPD_SoftRF_text5);
//
//      tft_epd->setFreeFont(&FreeSerif9pt7b);
//      //!!tft_epd->getTextBounds(EPD_SoftRF_text6, 0, 0, &tbx, &tby, &tbw, &tbh);
//      x = (tft_epd->width() - tbw) / 2;
//      y += 21;
//      tft_epd->setCursor(x, y);
//      tft_epd->print(EPD_SoftRF_text6);
//    }
//
//#if defined(USE_EPD_TASK)
//    /* a signal to background EPD update task */
//    EPD_update_in_progress = EPD_UPDATE_SLOW /* EPD_UPDATE_FAST */;
////    SoC->tft_epd_unlock();
//
////    yield();
//
//    while (EPD_update_in_progress != EPD_UPDATE_NONE) delay(100);
////    while (!SoC->tft_epd_lock()) { delay(10); }
//#else
//   // tft_epd->tft_epd(false);
//#endif
//
//   // EPD_HIBERNATE;
//
////    SoC->tft_epd_unlock();
//    break;
//
//  case tft_epd_NONE:
//  default:
//    break;
//  }
}

void EPD_Mode()
{
  /*if (hw_info.tft_epd == tft_epd_EPD_1_54) {
    for (int i=0; i < VIEW_MODES_COUNT; i++) {
      int next_view_mode = (EPD_view_mode + i) % VIEW_MODES_COUNT;
      if ((next_view_mode != EPD_view_mode) &&
          (EPD_pages_mask & (1 << next_view_mode))) {
        EPD_view_mode = next_view_mode;
        EPD_vmode_updated = true;
        break;
      }
    }
  }*/
}

void EPD_Up()
{
  /*if (hw_info.tft_epd == tft_epd_EPD_1_54) {
    switch (EPD_view_mode)
    {
    case VIEW_MODE_RADAR:
      EPD_radar_unzoom();
      break;
    case VIEW_MODE_TEXT:
      EPD_text_next();
      break;
    case VIEW_MODE_BARO:
      EPD_baro_next();
      break;
    case VIEW_MODE_TIME:
      EPD_time_next();
      break;
    case VIEW_MODE_IMU:
      EPD_imu_next();
      break;
    case VIEW_MODE_STATUS:
    default:
      EPD_status_next();
      break;
    }
  }*/
}

void EPD_Down()
{
 /* if (hw_info.tft_epd == tft_epd_EPD_1_54) {
    switch (EPD_view_mode)
    {
    case VIEW_MODE_RADAR:
      EPD_radar_zoom();
      break;
    case VIEW_MODE_TEXT:
      EPD_text_prev();
      break;
    case VIEW_MODE_BARO:
      EPD_baro_prev();
      break;
    case VIEW_MODE_TIME:
      EPD_time_prev();
      break;
    case VIEW_MODE_IMU:
      EPD_imu_prev();
      break;
    case VIEW_MODE_STATUS:
    default:
      EPD_status_prev();
      break;
    }
  }*/
}

void EPD_Message(const char *msg1, const char *msg2)
{
  int16_t  tbx, tby;
  uint16_t tbw, tbh;
  uint16_t x, y;
//
//#if defined(USE_EPD_TASK)
//  if (msg1 != NULL && strlen(msg1) != 0 && EPD_update_in_progress == EPD_UPDATE_NONE) 
//  {
////  if (msg1 != NULL && strlen(msg1) != 0 && SoC->tft_epd_lock()) {
//#else
//  if (msg1 != NULL && strlen(msg1) != 0) {
//#endif
//    tft_epd->setFreeFont(&FreeMonoBold18pt7b);
//
//    tft_epd->fillScreen(TFT_WHITE);
//
//    if (msg2 == NULL) 
//    {
//
//      //!!tft_epd->getTextBounds(msg1, 0, 0, &tbx, &tby, &tbw, &tbh);
//      x = (tft_epd->width() - tbw) / 2;
//      y = (tft_epd->height() + tbh) / 2;
//      tft_epd->setCursor(x, y);
//      tft_epd->print(msg1);
//
//    } else {
//
//      //!!tft_epd->getTextBounds(msg1, 0, 0, &tbx, &tby, &tbw, &tbh);
//      x = (tft_epd->width() - tbw) / 2;
//      y = tft_epd->height() / 2 - tbh;
//      tft_epd->setCursor(x, y);
//      tft_epd->print(msg1);
//
//      //!!tft_epd->getTextBounds(msg2, 0, 0, &tbx, &tby, &tbw, &tbh);
//      x = (tft_epd->width() - tbw) / 2;
//      y = tft_epd->height() / 2 + tbh;
//      tft_epd->setCursor(x, y);
//      tft_epd->print(msg2);
//    }
//
//#if defined(USE_EPD_TASK)
//    /* a signal to background EPD update task */
//    EPD_update_in_progress = EPD_UPDATE_FAST;
////    SoC->tft_epd_unlock();
////    yield();
//#else
//      //tft_epd->tft_epd(true);
//#endif
//  }
}

//EPD_Task_t EPD_Task( void * pvParameters )
//{
//    //  unsigned long LockTime = millis();
//
//    for (;; )
//    {
//        if (EPD_update_in_progress != EPD_UPDATE_NONE) 
//        {
//            //    if (SoC->tft_epd_lock()) {
//            Serial.println("EPD_Task: lock"); Serial.flush();
//
//            //      LockTime = millis();
//            //!!tft_epd->tft_epd(EPD_update_in_progress == EPD_UPDATE_FAST ? true : false);
//            Serial.println("EPD_Task: tft_epd"); Serial.flush();
//            yield();
//
//            /*
//             * SYX 1942 revision of D67 tft_epd can use power_off() after partial update,
//             * SYX 1948 revision - can not.
//             */
//            if (EPD_update_in_progress == EPD_UPDATE_FAST) { /* EPD_POWEROFF; */ }
//
//            EPD_update_in_progress = EPD_UPDATE_NONE;
//            //      SoC->tft_epd_unlock();
//            Serial.println("EPD_Task: unlock"); Serial.flush();
//            //      delay(100);
//            //    } else {
//            //      if (millis() - LockTime > 4000) {
//            //        SoC->tft_epd_unlock();
//            //Serial.println("EPD_Task: reseet lock"); Serial.flush();
//            //      }
//
//        }
//
//        yield();
//    }
//}

//=============================================================================================================
/*static int*/uint8_t EPD_zoom = ZOOM_MEDIUM;

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
        //  if (SoC->tft_epd_lock()) 
          //    {
#else
        {
#endif
            /* divider is a half of full scale */
            int32_t divider = 2000;

            tft_epd->setFreeFont(&FreeMono9pt7b);

            uint16_t radar_x = 0;
            uint16_t radar_y = (tft_epd->height() - tft_epd->width()) / 2;
            uint16_t radar_w = tft_epd->width();

            uint16_t radar_center_x = radar_w / 2;
            uint16_t radar_center_y = radar_y + radar_w / 2;
            uint16_t radius = radar_w / 2 - 2;

            if (ui->units == UNITS_METRIC || ui->units == UNITS_MIXED) 
            {
              switch(EPD_zoom)
              {
              case ZOOM_LOWEST:
                divider = 30000; /* 60 KM */
                break;
              case ZOOM_LOW:
                divider =  5000; /* 10 KM */
                break;
              case ZOOM_HIGH:
                divider =  1000; /*  2 KM */
                break;
              case ZOOM_MEDIUM:
              default:
                divider =  2000;  /* 4 KM */
                break;
              }
            }
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

            tft_epd->fillScreen(TFT_WHITE);

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
            //            tft_epd->drawTriangle(radar_center_x + x - 5, radar_center_y - y + 4,
            //                                radar_center_x + x    , radar_center_y - y - 6,
            //                                radar_center_x + x + 5, radar_center_y - y + 4,
            //                                TFT_BLACK);
            //            tft_epd->drawTriangle(radar_center_x + x - 6, radar_center_y - y + 5,
            //                                radar_center_x + x    , radar_center_y - y - 7,
            //                                radar_center_x + x + 6, radar_center_y - y + 5,
            //                                TFT_BLACK);
            //           }
            //           else 
            //           {
            //             tft_epd->fillTriangle(radar_center_x + x - 4, radar_center_y - y + 3,
            //                                radar_center_x + x    , radar_center_y - y - 5,
            //                                radar_center_x + x + 4, radar_center_y - y + 3,
            //                                TFT_BLACK);
            //           }
            //        }
            //        else if (RelativeVertical < - EPD_RADAR_V_THRESHOLD) 
            //        {
            //        if (isTeam) {
            //            tft_epd->drawTriangle(radar_center_x + x - 5, radar_center_y - y - 4,
            //                                radar_center_x + x    , radar_center_y - y + 6,
            //                                radar_center_x + x + 5, radar_center_y - y - 4,
            //                                TFT_BLACK);
            //            tft_epd->drawTriangle(radar_center_x + x - 6, radar_center_y - y - 5,
            //                                radar_center_x + x    , radar_center_y - y + 7,
            //                                radar_center_x + x + 6, radar_center_y - y - 5,
            //                                TFT_BLACK);
            //        } else {
            //            tft_epd->fillTriangle(radar_center_x + x - 4, radar_center_y - y - 3,
            //                                radar_center_x + x    , radar_center_y - y + 5,
            //                                radar_center_x + x + 4, radar_center_y - y - 3,
            //                                TFT_BLACK);
            //        }
            //        } else {
            //        if (isTeam) {
            //            tft_epd->drawCircle(radar_center_x + x,
            //                                radar_center_y - y,
            //                                6, TFT_BLACK);
            //            tft_epd->drawCircle(radar_center_x + x,
            //                                radar_center_y - y,
            //                                7, TFT_BLACK);
            //            }
            //            else 
            //            {
            //            tft_epd->fillCircle(radar_center_x + x,
            //                                radar_center_y - y,
            //                                5, TFT_BLACK);
            //            }
            //        }
            //    }
            //}

            tft_epd->drawCircle(radar_center_x, radar_center_y,
                radius, TFT_BLACK);
            tft_epd->drawCircle(radar_center_x, radar_center_y,
                radius / 2, TFT_BLACK);

            if (ThisAircraft.aircraft_type == AIRCRAFT_TYPE_GLIDER ||
                ThisAircraft.aircraft_type == AIRCRAFT_TYPE_TOWPLANE ||
                ThisAircraft.aircraft_type == AIRCRAFT_TYPE_HELICOPTER ||
                ThisAircraft.aircraft_type == AIRCRAFT_TYPE_DROPPLANE ||
                ThisAircraft.aircraft_type == AIRCRAFT_TYPE_POWERED ||
                ThisAircraft.aircraft_type == AIRCRAFT_TYPE_JET) {

                /* little airplane */
                tft_epd->drawFastVLine(radar_center_x, radar_center_y - 4, 14, TFT_BLACK);
                tft_epd->drawFastVLine(radar_center_x + 1, radar_center_y - 4, 14, TFT_BLACK);

                tft_epd->drawFastHLine(radar_center_x - 8, radar_center_y, 18, TFT_BLACK);
                tft_epd->drawFastHLine(radar_center_x - 10, radar_center_y + 1, 22, TFT_BLACK);

                tft_epd->drawFastHLine(radar_center_x - 3, radar_center_y + 8, 8, TFT_BLACK);
                tft_epd->drawFastHLine(radar_center_x - 2, radar_center_y + 9, 6, TFT_BLACK);

            }
            else
            {

                /* arrow tip */
                tft_epd->fillTriangle(radar_center_x - 7, radar_center_y + 5,
                    radar_center_x, radar_center_y - 5,
                    radar_center_x + 7, radar_center_y + 5,
                    TFT_BLACK);
                tft_epd->fillTriangle(radar_center_x - 7, radar_center_y + 5,
                    radar_center_x, radar_center_y + 2,
                    radar_center_x + 7, radar_center_y + 5,
                    TFT_WHITE);
            }

            switch (0/*ui->orientation*/)
            {
            case DIRECTION_NORTH_UP:
                x = radar_x + radar_w / 2 - radius + tbw / 2;
                y = radar_y + (radar_w + tbh) / 2;
                tft_epd->setCursor(x, y);
                tft_epd->print("W");
                x = radar_x + radar_w / 2 + radius - (3 * tbw) / 2;
                y = radar_y + (radar_w + tbh) / 2;
                tft_epd->setCursor(x, y);
                tft_epd->print("E");
                x = radar_x + (radar_w - tbw) / 2;
                y = radar_y + radar_w / 2 - radius + (3 * tbh) / 2;
                tft_epd->setCursor(x, y);
                tft_epd->print("N");
                x = radar_x + (radar_w - tbw) / 2;
                y = radar_y + radar_w / 2 + radius - tbh / 2;
                tft_epd->setCursor(x, y);
                tft_epd->print("S");
                break;
            case DIRECTION_TRACK_UP:
                x = radar_x + radar_w / 2 - radius + tbw / 2;
                y = radar_y + (radar_w + tbh) / 2;
                tft_epd->setCursor(x, y);
                tft_epd->print("L");
                x = radar_x + radar_w / 2 + radius - (3 * tbw) / 2;
                y = radar_y + (radar_w + tbh) / 2;
                tft_epd->setCursor(x, y);
                tft_epd->print("R");
                x = radar_x + (radar_w - tbw) / 2;
                y = radar_y + radar_w / 2 + radius - tbh / 2;
                tft_epd->setCursor(x, y);
                tft_epd->print("B");

                tft_epd->setFreeFont(&FreeMonoBold9pt7b);
                snprintf(cog_text, sizeof(cog_text), "%03d", (int)ThisAircraft.course);
                //!!tft_epd->getTextBounds(cog_text, 0, 0, &tbx, &tby, &tbw, &tbh);

                x = radar_x + (radar_w - tbw) / 2;
                y = radar_y + radar_w / 2 - radius + (3 * tbh) / 2;
                tft_epd->setCursor(x, y);
                tft_epd->print(cog_text);
                tft_epd->drawRoundRect(x - 2, y - tbh - 2,
                    tbw + 8, tbh + 6,
                    4, TFT_BLACK);
                break;
            default:
                /* TBD */
                break;
            }

            tft_epd->setFreeFont(&FreeMonoBold12pt7b);
            x = radar_x + tbw / 2;
            y = radar_y + radar_w - tbh;
            tft_epd->setCursor(x, y);

            tft_epd->print(Traffic_Count());

            //!! tft_epd->setFreeFont(&Picopixel);
            y += tbh; y += tbh;
            tft_epd->setCursor(x, y);
            tft_epd->print("ACFTS");

            tft_epd->setFreeFont(&FreeMonoBold12pt7b);

            x = radar_x + radar_w - tbw;
            y = radar_y + radar_w - tbh;
            tft_epd->setCursor(x, y);

            tft_epd->print("10");

       /*     if (ui->units == UNITS_METRIC || ui->units == UNITS_MIXED)
            {
            tft_epd->print(EPD_zoom == ZOOM_LOWEST ? "60" :
                            EPD_zoom == ZOOM_LOW    ? "10" :
                            EPD_zoom == ZOOM_MEDIUM ? "4 " :
                            EPD_zoom == ZOOM_HIGH   ? "2 " : "");
            }
            else
            {
                tft_epd->print(EPD_zoom == ZOOM_LOWEST ? "30" :
                                EPD_zoom == ZOOM_LOW    ? "5 " :
                                EPD_zoom == ZOOM_MEDIUM ? "2 " :
                                EPD_zoom == ZOOM_HIGH   ? "1 " : "");
            }*/

             //tft_epd->setFreeFont(&Picopixel);
            x += tbw;
            y += tbh; y += tbh;
            tft_epd->setCursor(x, y);

            tft_epd->print("KM");

            //tft_epd->print(ui->units == UNITS_METRIC || ui->units == UNITS_MIXED ?
            //                  "KM" : "NM");
       

#if defined(USE_EPD_TASK)
       /* a signal to background EPD update task */
            EPD_update_in_progress = EPD_UPDATE_FAST;
            //    SoC->tft_epd_unlock();
            //    yield();
#else
       // tft_epd->tft_epd(true);
#endif
        }
    }

void EPD_radar_setup()
{
   // EPD_zoom = ui->zoom;
}

void EPD_radar_loop()
{
    if (isTimeToEPD())
    {
        /* bool hasFix = isValidGNSSFix() || (settings->mode == SOFTRF_MODE_TXRX_TEST);

         if (hasFix)
         {*/
          EPD_Draw_Radar();
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

//=================================================================================
