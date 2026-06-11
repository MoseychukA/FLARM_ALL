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
#include "ServiceMain.h"

 /* Спрайты вывода изображений и информации на экран дисплея */

 // create a new sprite
TFT_eSPI tft = TFT_eSPI();

TFT_eSprite back = TFT_eSprite(&tft);         // Спрайт фона
TFT_eSprite backsprite = TFT_eSprite(&tft);         // Спрайт отображения вращающегося поля воздушной обстановки
TFT_eSprite rows_Message = TFT_eSprite(&tft);    // Спрайт отображения текстов сообщений
TFT_eSprite DUMP1090_info = TFT_eSprite(&tft);      // Этот спрайт, площадка в котором будет располагатся информация о приеме данных с приемника 1090
TFT_eSprite time_info = TFT_eSprite(&tft);          // Этот спрайт, площадка в котором будет располагатся информация о времени с GPS
TFT_eSprite rssi_info = TFT_eSprite(&tft);          // Спрайт окна информации уровня принимаемого сигнала LoRa
TFT_eSprite our_plane = TFT_eSprite(&tft);          // Спрайт окна нашего самолета
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

int angle = 0;
int angle_old = 0;

bool text_call = false;

//TFT_Class* getDC() { return tftDC; };
//TFT_Class* tftDC;

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

  String version = service.getVer();
  tbw1 = tftDC->textWidth(version);
  x_tft = (tftDC->width() - tbw1) - 4;
  y_tft = tftDC->height() - tftDC->fontHeight() + 10;
  tftDC->setCursor(x_tft, y_tft);
  tftDC->print(version);
  
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

   // back.setColorDepth(8);

  DUMP1090_info.createSprite(30, 30);
  DUMP1090_info.setPivot(15, 15);
  DUMP1090_info.fillSprite(TFT_BLACK);      // Закрасим поле самолетика

  time_info.createSprite(60, 25);
  time_info.setColorDepth(8);
  time_info.loadFont(NotoSansMonoSCB20);
  time_info.setPivot(-100, 158);

 // backsprite.setColorDepth(8);
  backsprite.createSprite(320, 320);
  backsprite.loadFont(NotoSansMonoSCB20);          // Загружаем шрифты символов направления света
  backsprite.setSwapBytes(true);
  backsprite.setTextColor(TFT_WHITE, TFT_BLACK);
  backsprite.setTextDatum(4);

  rssi_info.createSprite(80, 25);
  rssi_info.loadFont(NotoSansMonoSCB20);
  rssi_info.setTextDatum(TC_DATUM);
  rssi_info.setTextColor(TFT_WHITE, TFT_BLACK);
  rssi_info.setPivot(158, 158);
  
  our_plane.createSprite(30, 30);
  our_plane.setPivot(10, 10);
  our_plane.fillSprite(TFT_BLACK);      // Закрасим поле самолетика
 
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

        tftDC->setRotation(0);

        back.fillSprite(backColor);                   // Закрасим поле 
        backsprite.setPivot(160, 160);                // Назначаем центр вращения спрайта воздушной обстановки
        backsprite.fillSprite(backColor);             // 
        backsprite.pushRotated(&back, -90, TFT_BLACK);
        backsprite.pushSprite(0, 0);

        Draw_circular_scale();


        angle = -90;//(360 - (int)ThisAircraft.course) % 360;

   
        /*Выполняем поворот по азимуту*/
        backsprite.pushRotated(&back, angle, TFT_BLACK);
        /***************    TFT_шкала дистанции    *******************/

        rssi_info.fillSprite(TFT_BLACK);
        rssi_info.drawRect(0, 0, 80, 25, TFT_DARKGREY);
        rssi_info.drawString("--- db", 40, 4);
        rssi_info.pushRotated(&back, 270, TFT_BLACK);

       // time_info
        time_info.fillSprite(TFT_BLACK);
        time_info.setTextDatum(TC_DATUM);
        time_info.drawRect(0, 0, 59, 24, TFT_DARKGREY);
        time_info.setTextColor(TFT_GREEN, backColor);
        time_info.drawString("--:--", 29, 4);
        time_info.pushRotated(&back, 270, TFT_BLACK);
  
        /*Формируем картинку самолета*/
        /* Рисуем фюзеляж*/

        //int width_air = 145;
        //int height_air = 145;

        int width_air = 5;
        int height_air = 5;

        //our_plane
        our_plane.drawLine(12, 0, 12, 18, TFT_DARKGREY);

        /*Рисуем передние крылья*/
        our_plane.drawLine(3, 7, 20, 7, TFT_DARKGREY);
        our_plane.drawLine(0, 8, 23, 8, TFT_DARKGREY);

        /*Рисуем задние крылья*/
        our_plane.drawLine(7, 17, 17, 17, TFT_DARKGREY);
        our_plane.pushRotated(&back,  270, TFT_BLACK);
 
        back.pushSprite(0, 0);
        //   /* Определение местоположения при старте */
        uint8_t fix_tmp = (uint8_t)isValidGNSSFix();

        if (!fix_tmp && (settings->mode != FLYRF_MODE_TXRX_TEST1) && (settings->mode != FLYRF_MODE_TXRX_TEST2) && (settings->mode != FLYRF_MODE_TXRX_TEST3) && (settings->mode != FLYRF_MODE_TXRX_TEST4) && (settings->mode != FLYRF_MODE_TXRX_TEST5))
        {
            if (!text_call)
            {
               // tftDC->setRotation(3);
                waiting_txt();
               // tftDC->setRotation(0);
            }

            //vTaskDelay(2000);
            //esp_task_wdt_reset();
        }

        esp_task_wdt_reset();





        TFT_display_frontpage = true;
    }
    else
    {


    }


}


void Draw_circular_scale()
{

    /* Рисуем круглую шкалу серым цветом и символы сторон света белым*/
    for (int i = 0; i < 36; i++)
    {
        // unsigned short color2 = TFT_DARKGREY;
        if (i % 3 == 0)
        {
            backsprite.drawWedgeLine(fx[i * 10], fy[i * 10], px[i * 10], py[i * 10], 1, 1, TFT_DARKGREY);
            backsprite.setTextColor(TFT_DARKGREY, TFT_BLACK);
            if (i == 0)
            {
                backsprite.drawString("N", lx[i * 10] + 1, ly[i * 10], TFT_DARKGREY);
            }
            if (i == 9)
            {
                backsprite.drawString("E", lx[i * 10], ly[i * 10], TFT_DARKGREY);
            }
            if (i == 18)
            {
                backsprite.drawString("S", lx[i * 10], ly[i * 10], TFT_DARKGREY);
            }
            if (i == 27)
            {
                backsprite.drawString("W", lx[i * 10], ly[i * 10], TFT_DARKGREY);
            }
        }
        else
        {
            backsprite.drawWedgeLine(fx[i * 10], fy[i * 10], px1[i * 10], py1[i * 10], 1, 1, TFT_DARKGREY);
        }
    }
    esp_task_wdt_reset();

    /*Рисуем малый серый круг*/
    backsprite.drawCircle(cx, 160, 80, TFT_DARKGREY);
}

////=========================================================================================================================================================================
//void drawMessage()
//{
//
//    // TFTRus* rusPrinter = menuManager->getRusPrinter();
//
//    char msg_mem[Number_of_bytes_block] = "";
//    char time_msg[Number_of_bytes_time] = "";
//
//    /*Вывести на дисплей сообщение*/
//
//    //!!strncpy(msg_mem, CommandHandler.msg_tmp_all, strlen(CommandHandler.msg_tmp_all));
//    //!!strncpy(msg_mem, msg_mem_tmp, strlen(msg_mem_tmp));
//
//    rows_Message.createSprite(320, 52);
//    rows_Message.fillSprite(TFT_BLACK);
//    rows_Message.setTextColor(TFT_YELLOW, backColor);
//    rows_Message.setTextDatum(TL_DATUM);
//    rows_Message.setTextSize(2);
//
//    // Необходимо вычислить количество символов в каждой строке.
//
//    int lette_num = mb_strlen(msg_mem, 21);  //
//
//    char str1[50] = { 0 };
//
//    strncpy(str1, msg_mem, lette_num);
//    str1[lette_num + 1] = 0;                // записать 0 в конес первой строчки
//    rows_Message.drawString(str1, 0, 0);    // Запишем первую строчку в спрайт
//
//    char str_tmp[80] = { 0 };
//    strcpy(str_tmp, msg_mem + lette_num);   // Записать в str_tmp текст начиная со второй строчки
//    lette_num = mb_strlen(str_tmp, 21);     // Определить указатель конца второй строчки
//    char str2[50] = { 0 };                  // Назначить вторую строчку
//    strncpy(str2, str_tmp, lette_num);      // Копируем вторую строчку с ограничением  
//    str2[lette_num + 1] = 0;                // записать 0 в конес второй строчки
//    rows_Message.drawString(str2, 0, 17);
//
//    char str_tmp1[50] = { 0 };
//    lette_num = mb_strlen(msg_mem, 42);     // Ищем конец второй строчки
//    strcpy(str_tmp1, msg_mem + lette_num);  // Копируем текст третьей строчки
//
//    lette_num = mb_strlen(str_tmp1, 26);    // Ищем конец третьей строчки
//    char str3[50] = { 0 };
//
//    strncpy(str3, str_tmp1, lette_num);
//    str3[lette_num + 1] = 0;
//    rows_Message.drawString(str3, 0, 35);
//
//
//    rows_Message.pushToSprite(&back, 0, 0, TFT_BLACK);
//    back.pushSprite(0, 0);
//
//
//}
////------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//// Функция подсчёта количества символов в строке utf8,
//// состоящей из букв английского и русского алфавитов, цифр, общепринятых символов...
//
//int mb_strlen(char* source, int letter_n)  // как в php :)
//{
//    int i, k;
//    int target = 0;
//    unsigned char n;
//    char m[2] = { '0', '\0' };
//    k = strlen(source);
//    i = 0;
//
//    while (i < k) {
//        n = source[i]; i++;
//
//        if (n >= 0xBF)
//        {
//            switch (n)
//            {
//            case 0xD0:
//            {
//                n = source[i]; i++;
//                if (n == 0x81) { n = 0xA8; break; }
//                if (n >= 0x90 && n <= 0xBF) n = n + 0x2F;
//                break;
//            }
//            case 0xD1:
//            {
//                n = source[i]; i++;
//                if (n == 0x91) { n = 0xB7; break; }
//                if (n >= 0x80 && n <= 0x8F) n = n + 0x6F;
//                break;
//            }
//            }
//        }
//        m[0] = n; target = target + 1;
//        if (target == letter_n)
//            break;
//    }
//    return i;// target;
//}
//
//void clearMSG()
//{
//
////    flags.MailOn = false;                                                   // Отключить отсчет времени удаления сообщения через 10 минут
////    Allow_flashing = false;  // Запретить мигание сообщения
/////*    for (int i = 0; i < (Max_Count_Block_Message * Number_of_bytes_block) + 400; i++)
////    {
////        MemWrite(i, 0x00);
////    }
////    MemCommit();*/
////
////    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
////    {
////        if (Container[i].signal_source == 2)
////        {
////            Container_msg[i] = EmptyFO;
////            Container[i] = EmptyFO;
////        }
////    }
////
////    rows_Message.fillSprite(TFT_BLACK);
////    rows_Message.pushToSprite(&back, 0, 0, TFT_BLACK);
////    rows_Message.deleteSprite();
////    back.pushSprite(0, 0);
////
////    int screenWidth = dc->width();
////    int screenHeight = dc->height();
////
////    dc->setFreeFont(&FreeSerif12pt7b);
////    dc->setTextColor(TFT_YELLOW);                   // Set character (glyph) color only (background not over-written)
////
////    const char msg_txt[] = "MESSAGES DELETED";
////
////    uint16_t curX = 40;                                      // Координаты вывода 
////    uint16_t curY = 120;                                     // Координаты вывода текста
////    dc->setCursor(curX, curY),                               // Set cursor for tft.print()
////
////        dc->print(msg_txt);  // Отображаем 
////    delay(3000);
//
//}
//
//
////------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//// Вывод направления движения
////------------------------------------------------------------------------------------------------------------------------------------------------------------------------


float bearing_calc(float lat, float lon, float lat2, float lon2)
{

    float teta1 = radians(lat);
    float teta2 = radians(lat2);
    float delta1 = radians(lat2 - lat);
    float delta2 = radians(lon2 - lon);

    //==================Heading Formula Calculation================//

    float y = sin(delta2) * cos(teta2);
    float x = cos(teta1) * sin(teta2) - sin(teta1) * cos(teta2) * cos(delta2);
    float brng = atan2(y, x);
    brng = degrees(brng);// radians to degrees
    brng = (((int)brng + 360) % 360);
    return brng;

    /*
    *  // возвращает курс в градусах (Север=0, Запад=270) из позиции 1 в позицию 2,
 // оба указаны как широта и долгота в десятичных градусах со знаком.
 // Поскольку Земля не является точной сферой, расчетный курс может немного отклоняться.
 // С разрешения Маартена Ламерса

 double dlon = radians(long2-long1);
 lat1 = radians(lat1);
 lat2 = radians(lat2);
 double a1 = sin(dlon) * cos(lat2);
 double a2 = sin(lat1) * cos(lat2) * cos(dlon);
 a2 = cos(lat1) * sin(lat2) - a2;
 a2 = atan2(a1, a2);
 if (a2 < 0.0)
 {
   a2 += TWO_PI;
 }
 return degrees(a2);
    */
}

//double distance_form(double lat1, double long1, double lat2, double long2)
//{
//    // возвращает расстояние в метрах между двумя указанными позициями
//    // как десятичные градусы со знаком широты и долготы. Использует большой круг
//    // расчет расстояния для гипотетической сферы радиусом 6372795 метров.
//    // Поскольку Земля не является точной сферой, ошибки округления могут достигать 0,5%.
//    // С разрешения Маартена Ламерса
//
//
//    double delta = radians(long1 - long2);
//    double sdlong = sin(delta);
//    double cdlong = cos(delta);
//    lat1 = radians(lat1);
//    lat2 = radians(lat2);
//    double slat1 = sin(lat1);
//    double clat1 = cos(lat1);
//    double slat2 = sin(lat2);
//    double clat2 = cos(lat2);
//    delta = (clat1 * slat2) - (slat1 * clat2 * cdlong);
//    delta = sq(delta);
//    delta += sq(clat2 * sdlong);
//    delta = sqrt(delta);
//    double denom = (slat1 * slat2) + (clat1 * clat2 * cdlong);
//    delta = atan2(delta, denom);
//    return delta * 6372795;
//}
//
//
//int alien_count()
//{
//    int count = 0;
//
//    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
//    {
//        if (Container[i].addr)
//        {
//            count++;
//        }
//    }
//
//    return count;
//}
//
bool coordinates_waiting()
{
    bool coord = false;

    return coord;
}

void waiting_txt() // Вывод текста "ОПРЕДЕЛЕНИЕ МЕСТОПОЛОЖЕНИЯ"
{
    /* Определение местоположения нашего самолета при старте */
    tftDC = new TFT_eSPI();
    tftDC->setRotation(3);
    int screenWidth = tftDC->width();
    int screenHeight = tftDC->height();

    tftDC->setFreeFont(&FreeSerif12pt7b);
    tftDC->setTextColor(TFT_YELLOW);                           // Set character (glyph) color only (background not over-written)

    const char data_txt1[] = "LOCATION";
    const char data_txt2[] = "DETERMINATION";
    const char data_txt3[] = "WAIT...";

    int textFontWidth = tftDC->textWidth(data_txt1, 2);

    uint16_t curX = (screenWidth / 2) - (textFontWidth / 2) - 30; // Координаты вывода 
    uint16_t curY = 80;                                           // Координаты вывода текста
    tftDC->setCursor(curX, curY),                               // Set cursor for tft.print()
    tftDC->print(data_txt1);                                    // Отображаем 

    textFontWidth = tftDC->textWidth(data_txt2, 2);
    curX = (screenWidth / 2) - (textFontWidth / 2) - 50;          // Координаты вывода 
    curY += 30;                                                   // Координаты вывода текста
    tftDC->setCursor(curX, curY),                               // Set cursor for tft.print()
    tftDC->print(data_txt2);                                    // Отображаем 

    textFontWidth = tftDC->textWidth(data_txt3, 2);
    curX = (screenWidth / 2) - (textFontWidth / 2) - 12;          // Координаты вывода 
    curY += 30;                                                   // Координаты вывода текста
    tftDC->setCursor(curX, curY),                               // Set cursor for tft.print()
    tftDC->print(data_txt3);                                    // Отображаем 
    tftDC->setRotation(0);
}


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------








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


/*
  Enable PSRAM on the ESP32-S3
  By: Nathan Seidle
  SparkFun Electronics
  Date: January 13, 2024
  License: MIT. Please see LICENSE.md for more information.

  This example shows how to enable PSRAM on ESP32-S3 modules that have it, and use it for
  RAM requests above a certain byte threshold.

  Note: Not all ESP32 modules have PSRAM built-in. The SparkFun ESP32-S3 uses the 
  ESP32-S3 Mini N8R2 with 8MB flash and 2MB PSRAM. 

  Feel like supporting open source hardware?
  Buy a board from SparkFun!
  SparkFun ESP32-S3 Thing Plus (DEV-24408) https://www.sparkfun.com/products/24408

  Select the following in the Arduino IDE:
  Board: ESP32S3 Dev Module
  USB Mode: Hardware CDC and JTAG
  USB CDC on Boot: Enabled
  Upload Mode: UART0 / Hardware CDC
  PSRAM: QSPI PSRAM
  Port: Select the COM port that the device shows up on under device manager
*/

/*

unsigned long lastHeapReport;

void setup()
{
    Serial.begin(115200);
    while (Serial == false); //Wait for serial monitor to connect before printing anything

    if (psramInit() == false)
        Serial.println("PSRAM failed to initialize");
    else
        Serial.println("PSRAM initialized");

    Serial.printf("PSRAM Size available (bytes): %d\r\n", ESP.getFreePsram());

    heap_caps_malloc_extmem_enable(1000); //Use PSRAM for memory requests larger than 1,000 bytes
}

void loop()
{
    delay(20);

    if (millis() - lastHeapReport > 1000)
    {
        lastHeapReport = millis();
        reportHeap();
    }

    if (Serial.available()) ESP.restart();
}

void reportHeap()
{
    Serial.printf("FreeHeap: %d / HeapLowestPoint: %d / LargestBlock: %d / Used PSRAM: %d\r\n", ESP.getFreeHeap(),
        xPortGetMinimumEverFreeHeapSize(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), ESP.getPsramSize() - ESP.getFreePsram());
}*/