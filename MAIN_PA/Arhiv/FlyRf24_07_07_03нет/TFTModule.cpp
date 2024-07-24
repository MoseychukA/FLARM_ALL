/* Вычисление координат и курса стороннего самолета.
* Есть следующие данные:
* Координаты нашего самолета
* Координаты стороннего самолета
* Дистанция между нашим и сторонним самолетом
* distance_tmr[i] = (int)Container[i].distance;    // дистанция между нашим самолетом и сторонним
* Угол в полярных координатах между нашим и сторонним самолетом.
* bearing_tmr[i] = (int)Container[i].bearing;      // угол в градусах между нашим самолетом и сторонним
*
 Порядок вычисления

  bearing_tmr[i] = (int)Container[i].bearing;      // угол в градусах между нашим самолетом и сторонним
  distance_tmr[i] = (int)Container[i].distance;    // дистанция между нашим самолетом и сторонним
 Зная радиус (дистанцию) можно вычислить координаты по формуле
 rel_x = constrain(distance_tmr[i] * sin(radians(bearing_tmr[i])), -32768, 32767);
 rel_y = constrain(distance_tmr[i] * cos(radians(bearing_tmr[i])), -32768, 32767);
 */



#include "TFTModule.h"
#include "TFT_eSPI.h"
#include <SPI.h>
#include "SettingsMain.h"
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <Arduino.h>
#include "Configuration_ESP32.h"
#include "Memory.h"               // Работа с энергонезависимой памятью

#include "SoftRF.h"
#include <TimeLib.h>
#include <esp_task_wdt.h>
#include "EEPROMRF.h"
#include "TrafficHelper.h"
#include "GNSS.h"
#include "NotoSansMonoSCB20.h"
#include "NotoSansBold15.h"
#include "RF.h"
#include "CoreButton.h"
#include <stdio.h>
#include "Memory.h"               // Работа с энергонезависимой памятью
#include "CoreCommandBuffer.h"


 /* Спрайты вывода изображений и информации на экран дисплея */

TFT_eSPI tft = TFT_eSPI();

TFT_eSprite back = TFT_eSprite(&tft);               // Спрайт фона
TFT_eSprite backsprite = TFT_eSprite(&tft);         // Спрайт отображения вращающегося поля воздушной обстановки
TFT_eSprite data_KM = TFT_eSprite(&tft);            // Информационный спрайт. Дипазон расстояний всего поля 
TFT_eSprite rows_mail = TFT_eSprite(&tft);          // Спрайт отображения текстов почтового ящика

TFT_eSprite* arrow[MAX_TRACKING_OBJECTS];           // Спрайт отображения стрелки
TFT_eSprite* arrow_old[MAX_TRACKING_OBJECTS];       // Спрайт отображения стрелка 

TFT_eSprite* Air_txt_Sprite[MAX_TRACKING_OBJECTS];  // Этот спрайт, площадка в котором будет располагатся формуляр стороннего самолета
TFT_eSprite* little_airplane[MAX_TRACKING_OBJECTS]; // Этот спрайт, площадка в котором будет располагатся изображение стороннего самолета DUMP1090
TFT_eSprite* msg_airplane[MAX_TRACKING_OBJECTS];    // Этот спрайт, площадка в котором будет располагатся изображение стороннего самолета с текстовой строки
TFT_eSprite* LoRa_airplane[MAX_TRACKING_OBJECTS];   // Этот спрайт, площадка в котором будет располагатся изображение стороннего самолета c LoRa
TFT_eSprite* area_airplane[MAX_TRACKING_OBJECTS];   // Этот спрайт, площадка в котором будет располагатся спрайт little_airplane стороннего самолета
TFT_eSprite* DUMP1090_Sprite[MAX_TRACKING_OBJECTS]; // Этот спрайт, площадка в котором будет располагатся данные 1090 стороннего самолета

int alien_altitude_old[MAX_TRACKING_OBJECTS];       // Предыдущее значение высоты стороннего самолета. Нужно для вычисления высоты с учетом гистерезиса
int alien_altitude_actual[MAX_TRACKING_OBJECTS];    // Высота стороннего самолета. Нужно для вычисления высоты с учетом гистерезиса
int this_alien_altitude[MAX_TRACKING_OBJECTS];      // Высота стороннего самолета. Нужно для вычисления 
int old_alien_altitude_arrow[MAX_TRACKING_OBJECTS]; // Предыдущая высота стороннего самолета для отображения стрелок выше/ниже.
int alien_altitude_hysteresis[MAX_TRACKING_OBJECTS];// Обработанная высота стороннего самолета после применения гистерезиса
int height_difference[MAX_TRACKING_OBJECTS];        // Разность высот нашего и стороннего самолета
int alien_speed_tmr[MAX_TRACKING_OBJECTS];          // Скорость стороннего самолета
int alien_speed_view[MAX_TRACKING_OBJECTS];         // Скорость стороннего самолета для вывода на дисплей в виде линии
int bearing_tmr[MAX_TRACKING_OBJECTS];              // Угол в градусах между нашим самолетом и сторонним
int distance_tmr[MAX_TRACKING_OBJECTS];             // Дистанция между нашим и сторонним самолетом
int alient_course[MAX_TRACKING_OBJECTS];            // Курс стороннего самолета
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
word  DUMP1090_air_color[MAX_TRACKING_OBJECTS];     // Цвет предупреждения столкновения с сторонним самолетом без координат

int alien_speed_filtre[MAX_TRACKING_OBJECTS][speed_array_size];    // Фильтр скорости стороннего самолета
int alien_altitude_filtre[MAX_TRACKING_OBJECTS][speed_array_size]; // Фильтр высоты стороннего самолета

bool alien_speed_array_countMax[MAX_TRACKING_OBJECTS];             // Флаг заполнения массиво фильтра скорости стороннего самолета
int alien_speed_sum[MAX_TRACKING_OBJECTS];                         // = 0;
uint8_t alien_speed_array_count[MAX_TRACKING_OBJECTS];             // Счетчик фильтра скорости стороннего самолета

bool alien_altitude_array_countMax[MAX_TRACKING_OBJECTS];          // Флаг заполнения массиво фильтра высоты стороннего самолета
int alien_altitude_sum[MAX_TRACKING_OBJECTS];                      // = 0;
uint8_t alien_altitude_array_count[MAX_TRACKING_OBJECTS];          // Счетчик фильтра высоты стороннего самолета

static uint32_t tmr_array[MAX_TRACKING_OBJECTS];                   // Задержка по времени контроля движения чужого самолета

int Aircraft_speed_filtre[speed_array_size];                       // Фильтр скорости нашего самолета
int Aircraft_altitude_tmr[speed_array_size];                       // Фильтр высоты нашего самолета
int16_t new_angle[MAX_TRACKING_OBJECTS];                           // Для вычисления курса стороннего самолета

int dump1090_speed[MAX_TRACKING_OBJECTS];                          // 
String dump1090_info_txt[MAX_TRACKING_OBJECTS];                    //

//......................................colors
#define backColor     0x0026

static int TFT_zoom = ZOOM_MEDIUM;

bool isTeam_all[MAX_TRACKING_OBJECTS] = { false };
bool isThere_plane[MAX_TRACKING_OBJECTS] = { false };


//--------------------------------------------------------------------------------------------------------------------------------
TFTClass TFTMain;
//--------------------------------------------------------------------------------------------------------------------------------
TFTClass::TFTClass()
{

}

//--------------------------------------------------------------------------------------------------------------------------------
TFTClass::~TFTClass()
{

}
//--------------------------------------------------------------------------------------------------------------------------------
void TFTClass::setup()
{
    tftDC = new TFT_eSPI();
     
    tftDC->init();
    tftDC->setRotation(3);
    tftDC->fillScreen(TFT_BACK_COLOR);
    tftDC->setTextColor(TFT_RED, TFT_BACK_COLOR);
    delay(200);
    yield();
  /*  tftDC->setRotation(3);*/


    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
    {
        Air_txt_Sprite[i] = new TFT_eSprite(&tft);     // Спрайт информации стороннего воздушного объекта
        Air_txt_Sprite[i]->createSprite(45, 14);
        Air_txt_Sprite[i]->setPivot(20, 7);

        DUMP1090_Sprite[i] = new TFT_eSprite(&tft);     // Спрайт информации DUMP1090 стороннего воздушного объекта
        DUMP1090_Sprite[i]->createSprite(100, 20);
        DUMP1090_Sprite[i]->setPivot(50, 10);

        arrow[i] = new TFT_eSprite(&tft);              // Спрайт информации стороннего воздушного объекта
        arrow[i]->createSprite(10, 10);                // Спрайт отображения стрелка вверх/вниз

        little_airplane[i] = new TFT_eSprite(&tft);   // Спрайт информации стороннего воздушного объекта
        little_airplane[i]->createSprite(100, 100);
        little_airplane[i]->setPivot(50, 50);

        LoRa_airplane[i] = new TFT_eSprite(&tft);     // Спрайт информации стороннего воздушного объекта LoRa
        LoRa_airplane[i]->createSprite(100, 100);
        LoRa_airplane[i]->setPivot(50, 50);

        msg_airplane[i] = new TFT_eSprite(&tft);   // Спрайт информации стороннего воздушного объекта
        msg_airplane[i]->createSprite(100, 100);
        msg_airplane[i]->setPivot(50, 50);

        area_airplane[i] = new TFT_eSprite(&tft);      // Этот спрайт, площадка в котором будет располагатся сторонний самолет
        area_airplane[i]->createSprite(100, 100);
        area_airplane[i]->setPivot(50, 50);

        alien_speed_array_countMax[i] = false;
        alien_speed_sum[i] = 0;
        alien_speed_array_count[i] = 0;

        alien_altitude_array_countMax[i] = false;
        alien_altitude_sum[i] = 0;
        alien_altitude_array_count[i] = 0;

        old_alien_altitude_arrow[i] = 0;                // Массив хранения предыдущих значений высоты, для формирования стрелок направления перемещения самолета вврх/вниз

        tmr_array[i] = millis();
    }


    // Airplane.createSprite(24, 20);                   // Изображение нашего самолета в центре экрана
    back.createSprite(320, 320);

    backsprite.createSprite(320, 320);
    backsprite.loadFont(NotoSansMonoSCB20);          // Загружаем шрифты символов направления света
    backsprite.setSwapBytes(true);
    backsprite.setTextColor(TFT_WHITE, TFT_BLACK);
    backsprite.setTextDatum(4);

    /***************************************************************************************
    **                         Section 5: Font datum enumeration
    ***************************************************************************************/
    //These enumerate the text plotting alignment (reference datum point)
    //#define TL_DATUM 0 // Top left (default)
    //#define TC_DATUM 1 // Top centre
    //#define TR_DATUM 2 // Top right
    //#define ML_DATUM 3 // Middle left
    //#define CL_DATUM 3 // Centre left, same as above
    //#define MC_DATUM 4 // Middle centre
    //#define CC_DATUM 4 // Centre centre, same as above
    //#define MR_DATUM 5 // Middle right
    //#define CR_DATUM 5 // Centre right, same as above
    //#define BL_DATUM 6 // Bottom left
    //#define BC_DATUM 7 // Bottom centre
    //#define BR_DATUM 8 // Bottom right
    //#define L_BASELINE  9 // Left character baseline (Line the 'A' character would sit on)
    //#define C_BASELINE 10 // Centre character baseline
    //#define R_BASELINE 11 // Right character baseline

    //rows_mail.createSprite(320, 36);
    //rows_mail.setTextColor(TFT_WHITE, backColor);

    data_KM.createSprite(70, 20);
    data_KM.setTextColor(TFT_DARKGREY, TFT_BLACK);

    int a = 270;
    for (int i = 0; i < 360; i++)
    {
        x[i] = ((r - 5) * cos(rad * a)) + cx;    //Длина линии внешняя точка
        y[i] = ((r - 5) * sin(rad * a)) + cy;    //Длина линии внешняя точка
        px[i] = ((r - 14) * cos(rad * a)) + cx;  //Длина линии внутрення точка
        py[i] = ((r - 14) * sin(rad * a)) + cy;  //Длина линии внешняя точка
        px[i] = ((r - 14) * cos(rad * a)) + cx;  //Длина линии внешняя точка
        py[i] = ((r - 14) * sin(rad * a)) + cy;  //Длина линии внутрення точка
        px1[i] = ((r - 5) * cos(rad * a)) + cx;  //Длина линии внутрення точка
        py1[i] = ((r - 5) * sin(rad * a)) + cy;  //Длина линии внутрення точка
        lx[i] = ((r - 6) * cos(rad * a)) + cx;   //Положение символов по кругу
        ly[i] = ((r - 6) * sin(rad * a)) + cy;   //Положение символов по кругу
        nx[i] = ((r - 36) * cos(rad * a)) + cx;
        ny[i] = ((r - 36) * sin(rad * a)) + cy;

        a++;
        if (a == 360)
            a = 0;
    }

    yield();
}

void TFTClass::update()
{

}

void TFTClass::draw()
{
    TFT_Class* tft_draw = getDC();
    if (!tft_draw)
    {
        return;
    }
   // TFTRus* rusPrinter = menuManager->getRusPrinter();



    uint16_t tbw1;
    uint16_t x_tft, y_tft;

    const char EPD_SoftRF_text1[] = "FlyRF";
    const char EPD_SoftRF_text2[] = "www.decima.ru";
    const char EPD_SoftRF_text3[] = "DECIMA";
    const char EPD_SoftRF_text6[] = "(C) 2024";


    tft_draw->fillScreen(TFT_NAVY);

    tft_draw->setTextColor(TFT_WHITE); //TFT_WHITE TFT_BLACK
    tft_draw->setTextWrap(false);

    tft_draw->setFreeFont(&FreeMonoBold24pt7b);

    x_tft = 90;
    y_tft = 80;
    tft_draw->setCursor(x_tft, y_tft);
    tft_draw->print(EPD_SoftRF_text1);

    x_tft = 80;
    y_tft = 150;
    tft_draw->setCursor(x_tft, y_tft);
    tft_draw->print(EPD_SoftRF_text3);

    tft_draw->setFreeFont(&FreeSerif9pt7b);

    x_tft = 10;
    y_tft = 205;
    tft_draw->setCursor(x_tft, y_tft);
    tft_draw->print(EPD_SoftRF_text2);

    x_tft = 10;
    y_tft = tft_draw->height() - tft_draw->fontHeight() + 10;
    tft_draw->setCursor(x_tft, y_tft);
    tft_draw->print(EPD_SoftRF_text6);

    //Current_version

    String Current_version = SettingsMain.getVer();
    tbw1 = tft_draw->textWidth(Current_version);
    x_tft = (tft_draw->width() - tbw1) - 4;
    y_tft = tft_draw->height() - tft_draw->fontHeight() + 10;
    tft_draw->setCursor(x_tft, y_tft);
    tft_draw->print(Current_version);

    esp_task_wdt_reset();
    vTaskDelay(3000);

    esp_task_wdt_reset();

    back.fillSprite(backColor);                   // Закрасим поле 
    backsprite.fillSprite(TFT_BLACK);             // 
    backsprite.setPivot(160, 160);                // Назначаем центр вращения спрайта воздушной обстановки

    /* Рисуем круглую шкалу серым цветом и символы сторон света белым*/
    for (int i = 0; i < 36; i++)
    {
        color2 = TFT_DARKGREY;
        if (i % 3 == 0)
        {
            backsprite.drawWedgeLine(x[i * 10], y[i * 10], px[i * 10], py[i * 10], 1, 1, color2);
            backsprite.setTextColor(TFT_WHITE, TFT_BLACK);
            if (i == 0)
            {
                backsprite.drawString("N", lx[i * 10] + 1, ly[i * 10], color2);
            }
            if (i == 9)
            {
                backsprite.drawString("E", lx[i * 10], ly[i * 10], color2);
            }
            if (i == 18)
            {
                backsprite.drawString("S", lx[i * 10], ly[i * 10], color2);
            }
            if (i == 27)
            {
                backsprite.drawString("W", lx[i * 10], ly[i * 10], color2);
            }
        }
        else
        {
            backsprite.drawWedgeLine(x[i * 10], y[i * 10], px1[i * 10], py1[i * 10], 1, 1, color2);
        }
    }

    angle = (360 - (int)ThisAircraft.course) % 360;

    /*Рисуем малый серый круг*/
    backsprite.drawCircle(cx, 160, 80, TFT_DARKGREY);

    /*Выполняем поворот по азимуту*/
    backsprite.pushRotated(&back, angle, TFT_BLACK);
    /***************    TFT_шкала дистанции    *******************/



   /* настройки сообщения о дистанции внизу слева*/
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
    /*рисуем все неподвижные спрайты*/
    back.pushSprite(0, 0);


    //   /* Определение местоположения при старте */
    //fix = (uint8_t)isValidGNSSFix();

    //if (!fix && (settings->mode != SOFTRF_MODE_TXRX_TEST1) && (settings->mode != SOFTRF_MODE_TXRX_TEST2) && (settings->mode != SOFTRF_MODE_TXRX_TEST3) && (settings->mode != SOFTRF_MODE_TXRX_TEST4) && (settings->mode != SOFTRF_MODE_TXRX_TEST5))
    //{
    //    if (!text_call)
    //    {
    //        waiting_txt(menuManager);
    //        // text_call = true;
    //    }
    //}

    esp_task_wdt_reset();
    //esp_task_wdt_reset();
    //vTaskDelay(4000);
    //esp_task_wdt_reset();

}

