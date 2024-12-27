/*
 * OLEDHelper.cpp
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

#if defined(USE_TFT)


#include "TFT_RF.h"
#include "Configuration_ESP32.h"
#include "TFT_eSPI.h"
#include "RF.h"
#include "GNSS.h"
#include "Baro.h"
#include "TrafficHelper.h"
#include "TimeRF.h"
#include "EEPROMRF.h"
#include "NotoSansMonoSCB20.h"
#include "NotoSansBold15.h"
#include <esp_task_wdt.h>

 /* Спрайты вывода изображений и информации на экран дисплея */

 // create a new sprite
TFT_eSPI tft = TFT_eSPI();

TFT_eSprite back = TFT_eSprite(&tft);         // Спрайт фона
TFT_eSprite backsprite = TFT_eSprite(&tft);         // Спрайт отображения вращающегося поля воздушной обстановки
TFT_eSprite rows_Message = TFT_eSprite(&tft);    // Спрайт отображения текстов сообщений
TFT_eSprite DUMP1090_info = TFT_eSprite(&tft);      // Этот спрайт, площадка в котором будет располагатся информация о приеме данных с приемника 1090
TFT_eSprite time_info = TFT_eSprite(&tft);          // Этот спрайт, площадка в котором будет располагатся информация о времени с GPS
TFT_eSprite rssi_info = TFT_eSprite(&tft);          // Спрайт окна информации уровня принимаемого сигнала LoRa
TFT_eSprite* arrow[MAX_TRACKING_OBJECTS];           // Спрайт отображения стрелки
TFT_eSprite* Air_txt_Sprite[MAX_TRACKING_OBJECTS];  // Этот спрайт, площадка в котором будет располагатся формуляр стороннего самолета
TFT_eSprite* airplane[MAX_TRACKING_OBJECTS]; // Этот спрайт, площадка в котором будет располагатся изображение стороннего самолета DUMP1090
TFT_eSprite* area_airplane[MAX_TRACKING_OBJECTS];   // Этот спрайт, площадка в котором будет располагатся спрайт airplane стороннего самолета

int alien_altitude_old[MAX_TRACKING_OBJECTS];       // Предыдущее значение высоты стороннего самолета. Нужно для вычисления высоты с учетом гистерезиса
int alien_altitude_actual[MAX_TRACKING_OBJECTS];    // Высота стороннего самолета. Нужно для вычисления высоты с учетом гистерезиса
int this_alien_altitude[MAX_TRACKING_OBJECTS];      // Высота стороннего самолета. Нужно для вычисления 
int old_alien_altitude_arrow[MAX_TRACKING_OBJECTS]; // Предыдущая высота стороннего самолета для отображения стрелок выше/ниже.
int alien_altitude_hysteresis[MAX_TRACKING_OBJECTS];// Обработанная высота стороннего самолета после применения гистерезиса
int height_difference[MAX_TRACKING_OBJECTS];        // Разность высот нашего и стороннего самолета
int alien_speed_tmr[MAX_TRACKING_OBJECTS];          // Скорость стороннего самолета
int alien_speed_view[MAX_TRACKING_OBJECTS];         // Скорость стороннего самолета для вывода на дисплей в виде линии
int alient_course[MAX_TRACKING_OBJECTS];           // Курс стороннего самолета
int Container_alien_X[MAX_TRACKING_OBJECTS];        // Координаты стороннего самолета
int Container_alien_Y[MAX_TRACKING_OBJECTS];        // Координаты стороннего самолета
int Container_logbook_X[MAX_TRACKING_OBJECTS];      // Координаты формуляра стороннего самолета
int Container_logbook_Y[MAX_TRACKING_OBJECTS];      // Координаты формуляра стороннего самолета
int Container_arrow_X[MAX_TRACKING_OBJECTS];        // Координаты стрелки стороннего самолета
int Container_arrow_Y[MAX_TRACKING_OBJECTS];        // Координаты стрелки стороннего самолета
uint8_t arrow_up_down[MAX_TRACKING_OBJECTS];        // флаг стрелки вверх или вниз
uint8_t arrow_up_down_old[MAX_TRACKING_OBJECTS];    // флаг стрелки вверх или вниз
uint8_t DUMP1090_arrow_up_down[MAX_TRACKING_OBJECTS];        // флаг стрелки вверх или вниз
uint8_t DUMP1090_arrow_up_down_old[MAX_TRACKING_OBJECTS];    // флаг стрелки вверх или вниз
bool Air_txt_left[MAX_TRACKING_OBJECTS];            // флаг расположения формуляра слева или справа

word  little_air_color[MAX_TRACKING_OBJECTS];       // Цвет предупреждения столкновения с сторонним самолетом 

int alien_speed_filtre[MAX_TRACKING_OBJECTS][speed_array_size];    // Фильтр скорости стороннего самолета
int alien_altitude_filtre[MAX_TRACKING_OBJECTS][speed_array_size]; // Фильтр высоты стороннего самолета

bool alien_speed_array_countMax[MAX_TRACKING_OBJECTS];             // Флаг заполнения массиво фильтра скорости стороннего самолета
int alien_speed_sum[MAX_TRACKING_OBJECTS];                         // = 0;
uint8_t alien_speed_array_count[MAX_TRACKING_OBJECTS];             // Счетчик фильтра скорости стороннего самолета

bool alien_altitude_array_countMax[MAX_TRACKING_OBJECTS];          // Флаг заполнения массиво фильтра высоты стороннего самолета
int alien_altitude_sum[MAX_TRACKING_OBJECTS];                      // = 0;
uint8_t alien_altitude_array_count[MAX_TRACKING_OBJECTS];          // Счетчик фильтра высоты стороннего самолета

static uint32_t tmr_array[MAX_TRACKING_OBJECTS];                   // Задержка по времени контроля движения чужого самолета

int16_t new_angle[MAX_TRACKING_OBJECTS];                           // Для вычисления курса стороннего самолета

int dump1090_speed[MAX_TRACKING_OBJECTS];                          // 
String dump1090_info_txt[MAX_TRACKING_OBJECTS];                    //

bool isTeam_all[MAX_TRACKING_OBJECTS] = { false };              // Удалить данные по самолету



//......................................colors
#define backColor  0x0026




uint8_t TFT_flip                   = 0;
static bool TFT_display_titles     = false;
static bool TFT_display_frontpage  = false;
static uint32_t prev_tx_packets_counter = (uint32_t) -1;
static uint32_t prev_rx_packets_counter = (uint32_t) -1;
extern uint32_t tx_packets_counter, rx_packets_counter;

static TFT_eSPI* tftDC = NULL;


unsigned long TFTTimeMarker = 0;


int cx = 160;
int cy = 160;
int rx = 158;
float fx[360]; //outer points of Speed gaouges
float fy[360];
float px[360]; //ineer point of Speed gaouges
float py[360];
float px1[360]; //ineer point of Speed gaouges
float py1[360];
float lx[360]; //text of Speed gaouges
float ly[360];
float nx[360]; //needle low of Speed gaouges
float ny[360];
double rad = 0.01745;




void TFT_setup() 
{

  bool tft_probe = false;

#if defined(USE_TFT)

  tftDC = new TFT_eSPI();

  tftDC->init();

  tftDC->setRotation(3);

  tftDC->fillScreen(TFT_NAVY);

  tftDC->setTextColor(TFT_WHITE); //TFT_WHITE TFT_BLACK
  tftDC->setTextWrap(false);

  uint16_t tbw1;
  uint16_t x_tft, y_tft;

  const char EPD_SoftRF_text1[] = "FlyRF";
  const char EPD_SoftRF_text2[] = "www.decima.ru";
  const char EPD_SoftRF_text3[] = "DECIMA";
  const char EPD_SoftRF_text6[] = "(C) 2024";


  tftDC->fillScreen(TFT_NAVY);

  tftDC->setTextColor(TFT_WHITE); //TFT_WHITE TFT_BLACK
  tftDC->setTextWrap(false);

  tftDC->setFreeFont(&FreeMonoBold24pt7b);

  x_tft = 90;
  y_tft = 80;
  tftDC->setCursor(x_tft, y_tft);
  tftDC->print(EPD_SoftRF_text1);

  x_tft = 80;
  y_tft = 150;
  tftDC->setCursor(x_tft, y_tft);
  tftDC->print(EPD_SoftRF_text3);

  tftDC->setFreeFont(&FreeSerif9pt7b);

  x_tft = 10;
  y_tft = 205;
  tftDC->setCursor(x_tft, y_tft);
  tftDC->print(EPD_SoftRF_text2);

  x_tft = 10;
  y_tft = tftDC->height() - tftDC->fontHeight() + 10;
  tftDC->setCursor(x_tft, y_tft);
  tftDC->print(EPD_SoftRF_text6);

  /*String Current_version = SettingsMain.getVer();
  tbw1 = tft_radar->textWidth(Current_version);
  x_tft = (tft_radar->width() - tbw1) - 4;
  y_tft = tft_radar->height() - tft_radar->fontHeight() + 10;
  tft_radar->setCursor(x_tft, y_tft);
  tft_radar->print(Current_version);*/

  esp_task_wdt_reset();
  vTaskDelay(1000);

  esp_task_wdt_reset();





  for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
  {
      Air_txt_Sprite[i] = new TFT_eSprite(&tft);     // Спрайт информации стороннего воздушного объекта
      Air_txt_Sprite[i]->createSprite(55, 15);
      Air_txt_Sprite[i]->setPivot(27, 7);
      //Air_txt_Sprite[i]->setColorDepth(8);

      arrow[i] = new TFT_eSprite(&tft);              // Спрайт информации стороннего воздушного объекта
      arrow[i]->createSprite(10, 10);                // Спрайт отображения стрелка вверх/вниз
     // arrow[i]->setColorDepth(8);

      airplane[i] = new TFT_eSprite(&tft);          // Спрайт информации стороннего воздушного объекта
      airplane[i]->createSprite(100, 100);          // Спрайт отображения объекта, полученного из DUMP1090. 
      airplane[i]->setPivot(50, 50);
      //airplane[i]->setColorDepth(8);

      area_airplane[i] = new TFT_eSprite(&tft);      // Этот спрайт, площадка в котором будет располагатся сторонний самолет
      area_airplane[i]->createSprite(100, 100);
      area_airplane[i]->setPivot(50, 50);
      //area_airplane[i]->setColorDepth(8);

      alien_speed_array_countMax[i] = false;
      alien_speed_sum[i] = 0;
      alien_speed_array_count[i] = 0;


      alien_altitude_array_countMax[i] = false;
      alien_altitude_sum[i] = 0;
      alien_altitude_array_count[i] = 0;

      old_alien_altitude_arrow[i] = 0;                // Массив хранения предыдущих значений высоты, для формирования стрелок направления перемещения самолета вврх/вниз

      tmr_array[i] = millis();
      esp_task_wdt_reset();
  }

  back.createSprite(320, 320);
  back.setColorDepth(8);

  DUMP1090_info.createSprite(30, 30);
  DUMP1090_info.setPivot(15, 15);
  DUMP1090_info.fillSprite(TFT_BLACK);      // Закрасим поле самолетика

  time_info.createSprite(60, 25);
  time_info.setColorDepth(8);

  backsprite.createSprite(320, 320);
  backsprite.setColorDepth(8);
  backsprite.loadFont(NotoSansMonoSCB20);          // Загружаем шрифты символов направления света
  backsprite.setSwapBytes(true);
  backsprite.setTextColor(TFT_WHITE, TFT_BLACK);
  backsprite.setTextDatum(4);

  rssi_info.createSprite(80, 25);
  rssi_info.setColorDepth(8);
  rssi_info.loadFont(NotoSansMonoSCB20);
  rssi_info.setTextDatum(TC_DATUM);
  rssi_info.setTextColor(TFT_WHITE, TFT_BLACK);

 
  int a = 270;
  for (int i = 0; i < 360; i++)
  {
      fx[i] = ((rx - 5) * cos(rad * a)) + cx;    //Длина линии внешняя точка
      fy[i] = ((rx - 5) * sin(rad * a)) + cy;    //Длина линии внешняя точка
      px[i] = ((rx - 14) * cos(rad * a)) + cx;  //Длина линии внутрення точка
      py[i] = ((rx - 14) * sin(rad * a)) + cy;  //Длина линии внешняя точка
      px[i] = ((rx - 14) * cos(rad * a)) + cx;  //Длина линии внешняя точка
      py[i] = ((rx - 14) * sin(rad * a)) + cy;  //Длина линии внутрення точка
      px1[i] = ((rx - 5) * cos(rad * a)) + cx;  //Длина линии внутрення точка
      py1[i] = ((rx - 5) * sin(rad * a)) + cy;  //Длина линии внутрення точка
      lx[i] = ((rx - 6) * cos(rad * a)) + cx;   //Положение символов по кругу
      ly[i] = ((rx - 6) * sin(rad * a)) + cy;   //Положение символов по кругу
      nx[i] = ((rx - 36) * cos(rad * a)) + cx;
      ny[i] = ((rx - 36) * sin(rad * a)) + cy;

      a++;
      if (a == 360)
          a = 0;
  }



#endif /* USE_TFT */

 

  TFTTimeMarker = millis();

 
}

void TFT_loop()
{

    char buf[16];
    uint32_t disp_value;

    uint16_t tbw;
    uint16_t tbh;

    if (!TFT_display_frontpage)
    {

  
        //esp_task_wdt_reset();

        tftDC->fillScreen(TFT_NAVY);
        back.fillSprite(backColor);                   // Закрасим поле 
        backsprite.fillSprite(backColor);             // 
        backsprite.setPivot(160, 160);                // Назначаем центр вращения спрайта воздушной обстановки

       /* Draw_circular_scale();


        angle = (360 - (int)ThisAircraft.course) % 360;*/

        ///*Рисуем малый серый круг*/
        //backsprite.drawCircle(cx, 160, 80, TFT_DARKGREY);

        /*Выполняем поворот по азимуту*/
       // backsprite.pushRotated(&back, angle, TFT_BLACK);
        /***************    TFT_шкала дистанции    *******************/

        rssi_info.fillSprite(TFT_BLACK);
        rssi_info.drawRect(0, 0, 80, 25, TFT_DARKGREY);
        rssi_info.drawString("--- db", 40, 4);


        /*Формируем картинку самолета*/
        /* Рисуем фюзеляж*/

        int width_air = 148;
        int height_air = 150;
        back.drawLine(12 + width_air, 0 + height_air, 12 + width_air, 18 + height_air, TFT_DARKGREY);

        /*Рисуем передние крылья*/
        back.drawLine(3 + width_air, 7 + height_air, 20 + width_air, 7 + height_air, TFT_DARKGREY);
        back.drawLine(0 + width_air, 8 + height_air, 23 + width_air, 8 + height_air, TFT_DARKGREY);

        /*Рисуем задние крылья*/
        back.drawLine(7 + width_air, 17 + height_air, 17 + width_air, 17 + height_air, TFT_DARKGREY);
        back.pushSprite(0, 0);
        //   /* Определение местоположения при старте */
        uint8_t fix_tmp = (uint8_t)isValidGNSSFix();

        /*if (!fix_tmp && (settings->mode != SOFTRF_MODE_TXRX_TEST1) && (settings->mode != SOFTRF_MODE_TXRX_TEST2) && (settings->mode != SOFTRF_MODE_TXRX_TEST3) && (settings->mode != SOFTRF_MODE_TXRX_TEST4) && (settings->mode != SOFTRF_MODE_TXRX_TEST5))
        {
            if (!text_call)
            {
                waiting_txt(menuManager);
            }

            vTaskDelay(2000);
            esp_task_wdt_reset();
        }*/

        esp_task_wdt_reset();





        TFT_display_frontpage = true;
    }
    else
    {


    }


}




static void TFT_radio()
{
  char buf[16];
  uint32_t disp_value;

  //if (!TFT_display_titles)
  //{

  //  u8x8->clear();

  //  u8x8->drawString(1, 1, ID_text);

  //  snprintf (buf, sizeof(buf), "%06X", ThisAircraft.addr);
  //  u8x8->draw2x2String(0, 2, buf);

  //  u8x8->drawString(8, 1, PROTOCOL_text);

  //  u8x8->draw2x2Glyph(14, 2, Protocol_ID[ThisAircraft.protocol][0]);

  //  u8x8->drawString(1, 5, RX_text);

  //  u8x8->drawString(9, 5, TX_text);

  //  if ((settings->power_save & POWER_SAVE_NORECEIVE) &&
  //      (hw_info.rf == RF_IC_SX1276 ||
  //       hw_info.rf == RF_IC_SX1262))
  //  {
  //    u8x8->draw2x2String(0, 6, "OFF");
  //    prev_rx_packets_counter = rx_packets_counter;
  //  } else {
  //    prev_rx_packets_counter = (uint32_t) -1;
  //  }

  //  if (settings->mode  == FLYRF_MODE_RECEIVER || settings->txpower     == RF_TX_POWER_OFF) 
  //  {
  //    u8x8->draw2x2String(8, 6, "OFF");
  //    prev_tx_packets_counter = tx_packets_counter;
  //  } 
  //  else 
  //  {
  //    prev_tx_packets_counter = (uint32_t) -1;
  //  }

  //  OLED_display_titles = true;
  //}

  //if (rx_packets_counter != prev_rx_packets_counter) 
  //{
  //  disp_value = rx_packets_counter % 1000;
  //  itoa(disp_value, buf, 10);

  //  if (disp_value < 10) {
  //    strcat_P(buf,PSTR("  "));
  //  } else {
  //    if (disp_value < 100) {
  //      strcat_P(buf,PSTR(" "));
  //    };
  //  }

  //  u8x8->draw2x2String(0, 6, buf);
  //  prev_rx_packets_counter = rx_packets_counter;
  //}

  //if (tx_packets_counter != prev_tx_packets_counter) 
  //{
  //  disp_value = tx_packets_counter % 1000;
  //  itoa(disp_value, buf, 10);

  //  if (disp_value < 10) {
  //    strcat_P(buf,PSTR("  "));
  //  } else {
  //    if (disp_value < 100) {
  //      strcat_P(buf,PSTR(" "));
  //    };
  //  }

  //  u8x8->draw2x2String(8, 6, buf);
  //  prev_tx_packets_counter = tx_packets_counter;
  //}
}

static void TFT_other()
{
  char buf[16];
  uint32_t disp_value;

  if (!TFT_display_titles) 
  {

 /*   u8x8->clear();

    u8x8->drawString( 1, 1, ACFTS_text);

    u8x8->drawString( 7, 1, SATS_text);

    u8x8->drawString(12, 1, FIX_text);

    u8x8->drawString( 1, 5, UPTIME_text);

    u8x8->drawString(12, 5, BAT_text);

    u8x8->drawTile  (4, 6, 1, (uint8_t *) Dot_Tile);
    u8x8->drawTile  (4, 7, 1, (uint8_t *) Dot_Tile);

    u8x8->drawGlyph (13, 7, '.');

    prev_acrfts_counter = (uint32_t) -1;
    prev_sats_counter   = (uint32_t) -1;
    prev_fix            = (uint8_t)  -1;
    prev_uptime_minutes = (uint32_t) -1;
    prev_voltage        = (uint32_t) -1;*/

    TFT_display_titles = true;
  }

  uint32_t acrfts_counter = Traffic_Count();
  uint32_t sats_counter   = gnss.satellites.value();
  uint8_t  fix            = (uint8_t) isValidGNSSFix();
  uint32_t uptime_minutes = UpTime.minutes;
 
  //if (prev_acrfts_counter != acrfts_counter) 
  //{
  //  disp_value = acrfts_counter > 99 ? 99 : acrfts_counter;
  //  itoa(disp_value, buf, 10);

  //  if (disp_value < 10) 
  //  {
  //    strcat_P(buf,PSTR(" "));
  //  }

  //  u8x8->draw2x2String(1, 2, buf);
  //  prev_acrfts_counter = acrfts_counter;
  //}

  //if (prev_sats_counter != sats_counter) 
  //{
  //  disp_value = sats_counter > 99 ? 99 : sats_counter;
  //  itoa(disp_value, buf, 10);

  //  if (disp_value < 10) 
  //  {
  //    strcat_P(buf,PSTR(" "));
  //  }

  //  u8x8->draw2x2String(7, 2, buf);
  //  prev_sats_counter = sats_counter;
  //}

  //if (prev_fix != fix) 
  //{
  //  u8x8->draw2x2Glyph(12, 2, fix > 0 ? '+' : '-');
  //  prev_fix = fix;
  //}

  //if (prev_uptime_minutes != uptime_minutes) 
  //{
  //  disp_value = UpTime.hours; /* 0-23 */
  //  if (disp_value < 10) {
  //    buf[0] = '0';
  //    itoa(disp_value, buf+1, 10);
  //  } else {
  //    itoa(disp_value, buf, 10);
  //  }

  //  u8x8->draw2x2String(0, 6, buf);

  //  disp_value = uptime_minutes;
  //  if (disp_value < 10) {
  //    buf[0] = '0';
  //    itoa(disp_value, buf+1, 10);
  //  } else {
  //    itoa(disp_value, buf, 10);
  //  }

  //  u8x8->draw2x2String(5, 6, buf);

  //  prev_uptime_minutes = uptime_minutes;
  //}

 
}





void TFT_fini(int reason)
{
 
}



#endif /* USE_TFT */
