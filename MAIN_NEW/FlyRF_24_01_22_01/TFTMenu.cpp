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


#include "Configuration_ESP32.h"
#include "TFT_eSPI.h"
#include <SPI.h>
#include "SoftRF.h"
#include <TimeLib.h>
#include <esp_task_wdt.h>
#include "src/driver/LED.h"
#include "src/driver/EEPROM.h"
#include "src/TrafficHelper.h"
#include "src/driver/GNSS.h"
#include "NotoSansMonoSCB20.h"
#include "NotoSansBold15.h"

#ifdef USE_TFT_MODULE

#include "TFTMenu.h"

TFT_eSPI tft = TFT_eSPI();

TFT_eSprite back       = TFT_eSprite(&tft);         // Спрайт фона
TFT_eSprite backsprite = TFT_eSprite(&tft);         // Спрайт отображения вращающегося поля воздушной обстановки
TFT_eSprite dist_info  = TFT_eSprite(&tft);         // Спрайт окна информации расстояние до ближайшего стороннего воздушного объекта
TFT_eSprite Airplane   = TFT_eSprite(&tft);         // Спрайт нашего самолета
TFT_eSprite data_az    = TFT_eSprite(&tft);         // Информационный спрайт.Азимут (угол направления нашего самолета)
TFT_eSprite data_KM    = TFT_eSprite(&tft);         // Информационный спрайт. Дипазон расстояний всего поля 
TFT_eSprite power1     = TFT_eSprite(&tft);         // Спрайт отображения заряда аккумулятора 

TFT_eSprite* arrow[MAX_TRACKING_OBJECTS];           // Спрайт отображения стрелки
TFT_eSprite* arrow_old[MAX_TRACKING_OBJECTS];       // Спрайт отображения стрелка 

TFT_eSprite* Air_txt_Sprite[MAX_TRACKING_OBJECTS];  // Этот спрайт, площадка в котором будет располагатся формуляр стороннего самолета
TFT_eSprite* little_airplane[MAX_TRACKING_OBJECTS]; // Этот спрайт, площадка в котором будет располагатся изображение стороннего самолета
TFT_eSprite* area_airplane[MAX_TRACKING_OBJECTS];   // Этот спрайт, площадка в котором будет располагатся спрайт little_airplane стороннего самолета

int alien_altitude_old[MAX_TRACKING_OBJECTS];       // Предыдущее значение высоты стороннего самолета
int alien_altitude_actual[MAX_TRACKING_OBJECTS];    // Высота стороннего самолета
int alien_altitude_arrow[MAX_TRACKING_OBJECTS];     // Предыдущая высота стороннего самолета для отображения стрелок выше/ниже.
int alien_altitude_hysteresis[MAX_TRACKING_OBJECTS];// Обработанная высота стороннего самолета
int height_difference[MAX_TRACKING_OBJECTS];        // Разность высот нашего и стороннего самолета
int alien_speed_tmr[MAX_TRACKING_OBJECTS];          // Скорость стороннего самолета
int bearing_tmr[MAX_TRACKING_OBJECTS];              // Угол в градусах между нашим самолетом и сторонним
int distance_tmr[MAX_TRACKING_OBJECTS];             // Дистанция между нашим и сторонним самолетом
int alien_curse[MAX_TRACKING_OBJECTS];              // Курс стороннего самолета
int Container_alien_X[MAX_TRACKING_OBJECTS];        // Координаты стороннего самолета
int Container_alien_Y[MAX_TRACKING_OBJECTS];        // Координаты стороннего самолета
int Container_logbook_X[MAX_TRACKING_OBJECTS];      // Координаты формуляра стороннего самолета
int Container_logbook_Y[MAX_TRACKING_OBJECTS];      // Координаты формуляра стороннего самолета
uint8_t arrow_up_down[MAX_TRACKING_OBJECTS];        // флаг стрелки вверх или вниз
uint8_t arrow_up_down_old[MAX_TRACKING_OBJECTS];    // флаг стрелки вверх или вниз
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

int this_speed_filtre[speed_array_size];                           // Фильтр скорости нашего самолета
int this_altitude_filtre[speed_array_size];                        // Фильтр высоты нашего самолета


//......................................colors
#define backColor     0x0026
#define gaugeColor    0x055D
#define dataColor     0x0311
#define purple        0xEA16
#define Air_infoColor 0xF811

static int TFT_zoom = ZOOM_MEDIUM;

bool isTeam_all[MAX_TRACKING_OBJECTS]    = { false };
bool isThere_plane[MAX_TRACKING_OBJECTS] = { false };

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#define FONT_HEIGHT(dc) dc->fontHeight(1)


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
unsigned int utf8GetCharSize(unsigned char bt) 
{ 
  if (bt < 128) 
  return 1; 
  else if ((bt & 0xE0) == 0xC0) 
  return 2; 
  else if ((bt & 0xF0) == 0xE0) 
  return 3; 
  else if ((bt & 0xF8) == 0xF0) 
  return 4; 
  else if ((bt & 0xFC) == 0xF8) 
  return 5; 
  else if ((bt & 0xFE) == 0xFC) 
  return 6; 
 
  return 1; 
} 

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
AbstractTFTScreen::AbstractTFTScreen()
{ 
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
AbstractTFTScreen::~AbstractTFTScreen()
{ 
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTMenu
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTMenu* TFTScreen = NULL;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTMenu::TFTMenu()
{
  TFTScreen = this;
  currentScreenIndex = -1;
  flags.isLCDOn = true;
  switchTo = NULL;
  switchToIndex = -1;
  tftDC = NULL;
  on_action = NULL;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::setup()
{
    int rot = 3;
    int dRot = 3;
  
    tftDC = new TFT_eSPI();

    tftDC->init();
    tftDC->setRotation(dRot);
    tftDC->fillScreen(TFT_BACK_COLOR);

    tftDC->setTextColor(TFT_RED, TFT_BACK_COLOR);

    delay(200);
   
    rusPrint.init(tftDC);

    resetIdleTimer();

    // добавляем служебные экраны
    // окно сообщения
    TFTScreenInfo mbscrif;
 
    //TFTMenuScreen
    mbscrif.screen = new TFTMenuScreen();
    mbscrif.screen->setup(this);
    mbscrif.screenName = "MENU";
    screens.push_back(mbscrif);

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::onButtonPressed(int button)
{
  if(currentScreenIndex == -1)
    return;

  resetIdleTimer();
  TFTScreenInfo* currentScreenInfo = &(screens[currentScreenIndex]);
  currentScreenInfo->screen->onButtonPressed(this, button);

  if(on_action != NULL)
  {
    on_action(currentScreenInfo->screen);
  }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::onButtonReleased(int button)
{
  if(currentScreenIndex == -1)
    return;

  resetIdleTimer();
  TFTScreenInfo* currentScreenInfo = &(screens[currentScreenIndex]);
  currentScreenInfo->screen->onButtonReleased(this, button);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::update()
{
  if(!tftDC)
  {
    return;
  }
	
  if(currentScreenIndex == -1 && !switchTo)                         // ни разу не рисовали ещё ничего, исправляемся
  {
    
    switchToScreen("MENU");
  
  }

  if(switchTo != NULL)
  {
      tftDC->fillScreen(TFT_BACK_COLOR); // clear screen first      
      yield();
      currentScreenIndex = switchToIndex;
      switchTo->onActivate(this);
      switchTo->update(this);
      yield();
      switchTo->draw(this);
      yield();
      resetIdleTimer(); // сбрасываем таймер ничегонеделанья

      switchTo = NULL;
      switchToIndex = -1;
    return;
  }



  // обновляем текущий экран
  TFTScreenInfo* currentScreenInfo = &(screens[currentScreenIndex]);
  currentScreenInfo->screen->update(this);
  yield();
  
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
AbstractTFTScreen* TFTMenu::getScreen(const char* screenName)
{
  for(size_t i=0;i<screens.size();i++)
  {
    TFTScreenInfo* si = &(screens[i]);
    if(!strcmp(si->screenName,screenName))
    {
      return si->screen;
    }
  }

  return NULL;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::switchToScreen(AbstractTFTScreen* to)
{
	if(!tftDC)
	{
		return;
	}
   // переключаемся на запрошенный экран
  for(size_t i=0;i<screens.size();i++)
  {
    TFTScreenInfo* si = &(screens[i]);
    if(si->screen == to)
    {
      switchTo = si->screen;
      switchToIndex = i;
      break;

    }
  } 
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::switchToScreen(const char* screenName)
{
	if(!tftDC)
	{
		return;
	}
  
  // переключаемся на запрошенный экран
  for(size_t i=0;i<screens.size();i++)
  {
    TFTScreenInfo* si = &(screens[i]);
    if(!strcmp(si->screenName,screenName))
    {
      switchTo = si->screen;
      switchToIndex = i;
      break;

    }
  }

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
AbstractTFTScreen* TFTMenu::getActiveScreen()
{
  if(currentScreenIndex > -1 && screens.size())
  {
    TFTScreenInfo* currentScreenInfo = &(screens[currentScreenIndex]);
     return (currentScreenInfo->screen);
  }  
  
  return NULL;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::resetIdleTimer()
{
  idleTimer = millis();

  if(currentScreenIndex > -1 && screens.size() && on_action != NULL)
  {
    TFTScreenInfo* currentScreenInfo = &(screens[currentScreenIndex]);
    on_action(currentScreenInfo->screen);
  }
}


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTMenuScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

 TFTMenuScreen* MainScreen = NULL;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTMenuScreen::TFTMenuScreen()
 {
 
  MainScreen = this;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTMenuScreen::~TFTMenuScreen()
 {
	
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::onActivate(TFTMenu* menuManager)
 {
	 if (!menuManager->getDC())
	 {
		 return;
	 }
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::setup(TFTMenu* menuManager)
 {
    TFT_Class* dc = menuManager->getDC();

    if (!dc)
    {
	    return;
    }

    tft.setRotation(3);
    
   
   for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
   {
       Air_txt_Sprite[i] = new TFT_eSprite(&tft);     // Спрайт информации стороннего воздушного объекта
       Air_txt_Sprite[i]->createSprite(76, 30);
       Air_txt_Sprite[i]->setPivot(38, 15);

       arrow[i] = new TFT_eSprite(&tft);              // Спрайт информации стороннего воздушного объекта
       arrow[i]->createSprite(8, 10);                 // Спрайт отображения стрелка вверх

       little_airplane [i] = new TFT_eSprite(&tft);   // Спрайт информации стороннего воздушного объекта
       little_airplane [i]->createSprite(24, 18);
       little_airplane[i]->setPivot(12,9); 

       area_airplane[i] = new TFT_eSprite(&tft);      // Этот спрайт, площадка в котором будет располагатся сторонний самолет
       area_airplane[i]->createSprite(30, 30);
       area_airplane[i]->setPivot(15, 15);

       alien_speed_array_countMax[i] = false;
       alien_speed_sum[i] = 0;
       alien_speed_array_count[i] = 0;

       alien_altitude_array_countMax[i] = false;
       alien_altitude_sum[i] = 0;
       alien_altitude_array_count[i] = 0;

   }
 
  
    Airplane.createSprite(24, 20);
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


    data_az.createSprite(50, 25);
    data_az.setTextColor(TFT_GREEN, backColor);

    data_KM.createSprite(70, 20);
    data_KM.setTextColor(TFT_DARKGREY, TFT_BLACK);

    dist_info.createSprite(80, 25);
  
    power1.createSprite(40, 20);
    power1.setTextColor(TFT_GREEN, TFT_BLACK);


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

    pinMode(right_button, INPUT);
    pinMode(left_button, INPUT);
    pinMode(distance_button, INPUT);
 }

 
 //----------------------------------------------------------------------------------------------------------------------------------------------------------------------
 
 void TFTMenuScreen::update(TFTMenu* menuManager)
 {
	
      TFT_Class* dc = menuManager->getDC();

      if (!dc)
      {
          return;
      }

   static uint32_t tmr = millis();

    /* Проверяем наличие новой информации */ 
    if (millis() - tmr > DATA_MEASURE_THRESHOLD)
    {
        // drawVoltage(menuManager);
        // drawWiFi(menuManager);
        tmr = millis();
        int16_t new_angle;
        int arr_min = 32767;             // первоначально будем сравнивать
        int Air_txt_x = 30;              // Расположение текста в формуляре стороннего самолета 
         
           

        /* Проверяем есть ли данные GPS. Ждем 20 секунд */
        fix = (uint8_t)isValidGNSSFix();

        if(!fix && (settings->mode != SOFTRF_MODE_TXRX_TEST))
        {
            static uint32_t tmr_GNSS = millis();
            if (millis() - tmr_GNSS > 20000)
            {
                tmr_GNSS = millis();
                if (!text_call)
                {
                    waiting_txt(menuManager);  // Вывод сообщения о том что нет данных GPS
                    text_call = true;          // Запретить повторный вывод нового сообщения 
                }
            }
        }
        else
        {
            text_call = false;                  // Готов к выводу нового сообщения 

            /* Определяем какие пакеты приняты */
            for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
            {
                if (Container[i].addr && (now() - Container[i].timestamp) <= TFT_EXPIRATION_TIME)
                {
                    isThere_plane[i] = true;
                    isTeam_all[i] = true;

                    /* вычисляем минимальное значение дистанции для переключения диапазона просмотра */
                    arr_min = min(arr_min, (int)Container[i].distance);   // функция min выдает меньшее из двух значений,

                }
                else
                {
                    /* нет данных за длительный период  */
                    isThere_plane[i] = false;
                }
                esp_task_wdt_reset();
            }


            /* ================ Подпрограмма корректировки вывода формуляров (пока отложена) ================*/
            /* Смотрим сколько сторонних самолетов зафиксировано */

            Serial.println("view_alien_count.. ");
            view_alien_count = alien_count();

            /* Записываем в базу обнаруженные самолеты */
            /*
             uint32_t addr;                           // Адрес самолета
             uint8_t Container_i;                     // Номер самолета в контейнере
             uint8_t screen_side_width;               // Сторона экрана лево/право
             uint8_t screen_side_height;              // Сторона экрана верх/низ
             uint8_t base_alien[alien_count_base];    // Перечень в базе
             uint8_t base_index;                      // Порядковый номер в базе
             uint16_t alien_X;                        // Координата X
             uint16_t alien_Y;                        // Координата Y
            */

            if (view_alien_count >= 1)
            {
                Serial.print("alien_count >= 1 ");
                Serial.println(view_alien_count);
                int base_index_tmp = 0;
                for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
                {

                    if (Container[i].addr)
                    {
                        set_table_alien[base_index_tmp].addr = Container[i].addr;
                        set_table_alien[base_index_tmp].Container_i = i;
                        set_table_alien[base_index_tmp].base_index = base_index_tmp;

                        base_index_tmp++;
                    }
                }
                for (int i = 0; i < view_alien_count; i++)
                {
                    //Serial.print("Container_i ");
                    //Serial.print(set_table_alien[i].Container_i);
                    Serial.print(" addr ");
                    Serial.print(set_table_alien[i].addr, HEX);
                    //Serial.print(" base_index ");
                    //Serial.print(set_table_alien[i].base_index, HEX);
                    ///*Serial.print("alien_count");
                    //Serial.print(view_alien_count);*/
                    Serial.print("alien_X - ");
                    Serial.print(set_table_alien[i].alien_X, 5);
                    Serial.print(" | alien_Y - ");
                    Serial.print(set_table_alien[i].alien_Y, 5);
                    Serial.println("");

                }
            }
            else
            {


            }

            /*==================================================================*/

             /* Автоматический выбор диапазона отображения */
            divider = arr_min * 2.2;
            int divider_num = 1;

            if (arr_min > 10100)
            {
                divider = 32000;  // 16000
                divider_num = 1;
            }
            if (arr_min <= 10100 && arr_min > 5100)
            {
                divider = 21500;  // 10000
                divider_num = 2;
            }
            else if (arr_min <= 5100 && arr_min > 2100)
            {
                divider = 10500;  // 5000
                divider_num = 3;
            }
            else if (arr_min <= 2100 && arr_min > 1050)
            {
                divider = 4340; // 2000
                divider_num = 4;
            }
            else if (arr_min <= 1050 && arr_min > 510)
            {
                divider = 2150;  //1000
                divider_num = 5;
            }
            else if (arr_min <= 510 && arr_min > 210)
            {
                divider = 1100;  //500m
                divider_num = 6;
            }
            else if (arr_min <= 210 && arr_min > 110)
            {
                divider = 440;  // 200 m
                divider_num = 7;
            }
            else if (arr_min <= 110 && arr_min > 50)
            {
                divider = 220;  // 100 m
                divider_num = 8;
            }
            else if (arr_min <= 50)
            {
                divider = 110; // 50 m
                divider_num = 9;
            }



            //if (divider > 16000)
            //{
            //    divider = 32000;  // 16000
            //    divider_num = 1;
            //}
            //if (divider <= 24000 && divider > 13000)
            //{
            //     divider = 16000;  // 10000
            //     divider_num = 2;
            //}
            //else if (divider <= 13000 && divider > 5300)
            //{
            //    divider = 11500;  // 5000
            //    divider_num = 3;
            //}
            //else if (divider <= 5300 && divider > 3000)
            //{
            //    divider = 4500; // 2000
            //    divider_num = 4;
            //}
            //else if (divider <= 3000 && divider > 1500)
            //{
            //    divider = 2300;  //1000
            //    divider_num = 5;
            //}
            //else if (divider <= 1500 && divider > 600)
            //{
            //    divider = 1250;  //500m
            //    divider_num = 6;
            //}
            //else if (divider <= 600 && divider > 350)
            //{
            //    divider = 500;  // 200 m
            //    divider_num = 7;
            //}
            //else if (divider <= 350 && divider > 150)
            //{
            //    divider = 250;  // 100 m
            //    divider_num = 8;
            //}
            //else if (divider <= 150)
            //{
            //    divider = 150; // 50 m
            //    divider_num = 9;
            //}

            /* Фильтр скорости нашего самолета */
            this_speed_filtre[this_speed_array_count] = (int)ThisAircraft.speed;

            int this_val_speed = 0;

            if (this_speed_array_countMax)                                   // формируем данные о величине скорости
            {
                for (int k = 0; k < speed_array_size; k++)
                {
                    this_speed_sum += this_speed_filtre[this_speed_array_count];
                }
                this_val_speed = this_speed_sum / speed_array_size;
                this_speed_sum = 0;
            }

            this_speed_array_count++;
            if (this_speed_array_count > speed_array_size - 1)                // проверка заполнения массива первичными данными о скорости
            {
                this_speed_array_count = 0;
                this_speed_array_countMax = true;                             //Разрешить выдавать данные о величине скорости
            }

            this_speed_tmr = this_val_speed * 1.852;

            /* При малой скорости нашего самолета поворачиваем экран на отметку 360 */
            if (this_speed_tmr >= 0 && this_speed_tmr < 4)
            {
                ThisAircraft.course = 0;
            }

            angle = (360 - (int)ThisAircraft.course) % 360;            // 

                // /*Рисуем новую картинку*/
            for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
            {
                if (Container[i].addr)
                {

                    //================================================
                    bearing_tmr[i] = (int)Container[i].bearing;      // угол в градусах между нашим самолетом и сторонним
                    distance_tmr[i] = (int)Container[i].distance;    // дистанция между нашим самолетом и сторонним

                    Serial.print("distance_tmr - ");
                    Serial.println(distance_tmr[i]);


                    /* фильтруем показания скорости стороннего самолета */
                    /* Сначала заполняем массив фильтра данными по скорости */
                    alien_speed_filtre[i][alien_speed_array_count[i]] = (int)Container[i].speed;
                    int alien_val_speed = 0;

                    if (alien_speed_array_countMax[i])                     // формируем данные о величине скорости
                    {
                        for (int k = 0; k < speed_array_size; k++)
                        {
                            alien_speed_sum[i] += alien_speed_filtre[i][k];
                        }
                        alien_val_speed = alien_speed_sum[i] / speed_array_size;
                        alien_speed_sum[i] = 0;
                    }

                    alien_speed_array_count[i]++;
                    if (alien_speed_array_count[i] > speed_array_size - 1)   // проверка заполнения массива первичными данными о скорости
                    {
                        alien_speed_array_count[i] = 0;
                        alien_speed_array_countMax[i] = true;                //Разрешить выдавать данные о величине скорости
                    }

                    alien_speed_tmr[i] = alien_val_speed * 1.852;            // Скорость стороннего самолета после фильтра

                   // Serial.print("alien_val_speed ");
                   // Serial.print(alien_val_speed);
                   // Serial.print(" alien_speed_tmr ");
                   // Serial.println(alien_speed_tmr[i]);

                    // --------------------------------------------------------------------------------
                     /* При малой скорости смотрим в центр экрана на наш самолет. Это означает что самолет не летит (на земле) */
                    if (alien_speed_tmr[i] >= 0 && alien_speed_tmr[i] < 4)
                    {
                        Container[i].course = (180 + bearing_tmr[i]) % 360;
                    }

                    /* курс стороннего самолета с учетом поворота экрана */
                    alien_curse[i] = (angle + (int)Container[i].course) % 360;

                    /*Расчет координат сторонних самолетов на неподвижном экране с поправкой на вращение*/
                    new_angle = (angle + bearing_tmr[i]) % 360;

                    /*Функция проверяет и если надо задает новое значение, так чтобы оно была в области допустимых значений, заданной параметрами.*/
                    new_rel_x = constrain(distance_tmr[i] * sin(radians(new_angle)), -32768, 32767);
                    new_rel_y = constrain(distance_tmr[i] * cos(radians(new_angle)), -32768, 32767);

                    new_x = ((int32_t)new_rel_x * (int32_t)radius) / divider;
                    new_y = ((int32_t)new_rel_y * (int32_t)radius) / divider;

                    Container_alien_X[i] = new_x;  // Сохранить координаты стороннего самолета
                    Container_alien_Y[i] = new_y;

                    /* Расчет координат формуляра стороннего самолета */
                    if (new_x >= 0)  // Зона левая сторона
                    {
                        Air_txt_x = 41;
                        Air_txt_left[i] = false;
                        if (new_y <= -62) //
                        {
                            form_x = new_x + 9; // ok
                            form_y = new_y + 22; //
                        }
                        else
                        {
                            form_x = new_x + 9; //1
                            form_y = new_y + 15;     //
                        }

                    }
                    else
                    {
                        Air_txt_x = 30;
                        Air_txt_left[i] = true;
                        if (new_y <= -62) //
                        {
                            form_x = new_x - 85; // ok
                            form_y = new_y + 22; //
                        }
                        else
                        {
                            form_x = new_x - 85; //
                            form_y = new_y + 15; //1
                        }

                    }

                    Container_logbook_X[i] = form_x;  // Сохранить координаты формуляра стороннего самолета
                    Container_logbook_Y[i] = form_y;

                    esp_task_wdt_reset();

                    /* Фильтр высоты стороннего самолета */
                    alien_altitude_filtre[i][alien_altitude_array_count[i]] = (int)Container[i].altitude;

                    int alien_val_altitude = 0;

                    if (alien_altitude_array_countMax[i])                                   // формируем данные о высоте
                    {
                        for (int k = 0; k < altitude_array_size; k++)
                        {
                            alien_altitude_sum[i] += alien_altitude_filtre[i][k];
                        }
                        alien_val_altitude = alien_altitude_sum[i] / altitude_array_size;
                        alien_altitude_sum[i] = 0;
                    }

                    alien_altitude_array_count[i]++;
                    if (alien_altitude_array_count[i] > altitude_array_size - 1)   // проверка заполнения массива первичными данными высоте
                    {
                        alien_altitude_array_count[i] = 0;
                        alien_altitude_array_countMax[i] = true;                   // Флаг готовности данные о высоте стороннего самолета
                    }

                    alien_altitude_actual[i] = alien_val_altitude;                    // Данные высоты стороннего самолета после фильтра            
  
                    /* Устанавливаем ограничение высоты стороннего самолета с применением гистерезиса */
                    int diff_altitude = 5; // Не реагировать если изменение меньше

                    if ((alien_altitude_actual[i] - alien_altitude_old[i] > diff_altitude) || alien_altitude_old[i] - alien_altitude_actual[i] > diff_altitude)
                    {
                        alien_altitude_old[i] = alien_altitude_actual[i]; // Окончательные данные по высоте стороннего самолета с учетом гистерезиса.
                        alien_altitude_hysteresis[i] = alien_altitude_actual[i]; // Окончательные данные по высоте стороннего самолета с учетом гистерезиса.

                    }

 
                    /* Фильтр высоты нашего самолета */
                    this_altitude_filtre[this_altitude_array_count] = (int)ThisAircraft.altitude;

                    int this_val_altitude = 0;

                    if (this_altitude_array_countMax)                                   // формируем данные о высоте
                    {
                        for (int k = 0; k < altitude_array_size; k++)
                        {
                            this_altitude_sum += this_altitude_filtre[this_altitude_array_count];
                        }
                        this_val_altitude = this_altitude_sum / altitude_array_size;
                        this_altitude_sum = 0;
                    }

                    this_altitude_array_count++;
                    if (this_altitude_array_count > altitude_array_size - 1)           // проверка заполнения массива первичными данными о скорости
                    {
                        this_altitude_array_count = 0;
                        this_altitude_array_countMax = true;                           //Разрешить выдавать данные о величине скорости
                    }

                    this_altitude_tmr = this_val_altitude;                             // Данные по высоте нашего самолета после фильтра
   

                    //----------------------------------------------------------------------
                     /* Определяем разность высот и устанавливаем цвет предупреждения*/
                    
                    int RelativeVertical = alien_altitude_hysteresis[i] - this_altitude_tmr;     // Разность высот со знаком +-
                    int VerticalSet = 0;

                    if (RelativeVertical >= 0)
                    {
                        VerticalSet = alien_altitude_hysteresis[i] - this_altitude_tmr;
                    }
                    else if (RelativeVertical < 0)
                    {
                        VerticalSet = this_altitude_tmr - alien_altitude_hysteresis[i];
                    }
                    /* данные VerticalSet со знаком плюс */


                    /* Получить установки определения уровней предупреждения */
                    int alarm_attention_set = settings->alarm_attention;
                    int alarm_warning_set = settings->alarm_warning;
                    int alarm_danger_set = settings->alarm_danger;
                    int alarm_height_set = settings->alarm_height;

                    if (arr_min >= alarm_attention_set)   // Чужой самолет очень далеко
                    {
                        little_air_color[i] = TFT_WHITE;
                        txt_color = TFT_GREEN;
                    }
                    else if (arr_min <= alarm_attention_set && arr_min > alarm_warning_set) // Чужой самолет на расстоянии информирования
                    {
                        if (VerticalSet > alarm_height_set)  //  Чужой самолет выше расстояния информирования
                        {
                            little_air_color[i] = TFT_WHITE;
                            txt_color = TFT_GREEN;
                        }
                        else 
                        {
                            //  Чужой самолет на расстоянии информирования
                            little_air_color[i] = TFT_YELLOW;
                            txt_color = TFT_YELLOW;
                        }
                    }
                    else if (arr_min <= alarm_warning_set && arr_min > alarm_danger_set)  // Чужой самолет на расстоянии предупреждения
                    {
                        if (VerticalSet > alarm_height_set)                              // Чужой самолет выше расстояния предупреждения
                        {
                            little_air_color[i] = TFT_WHITE;
                            txt_color = TFT_GREEN;
                        }
                        else if (VerticalSet <= alarm_height_set)
                        {
                            // Чужой самолет на расстоянии предупреждения
                            little_air_color[i] = TFT_ORANGE;
                            txt_color = TFT_ORANGE;
                        }
                    }
                    else if (arr_min <= alarm_danger_set && VerticalSet <= alarm_height_set) // Чужой самолет на расстоянии и высоте опасен
                    {
                        little_air_color[i] = TFT_RED;
                        txt_color = TFT_RED;
                    }
                    esp_task_wdt_reset();


                    /* Вычисляем разность высот между нашим самолетом и сторонним. Данне со знаком + или -*/
                    height_difference[i] = alien_altitude_hysteresis[i] - this_altitude_tmr;

                    /* Пишем в формуляр скорость стороннего самолета */
                    Air_txt_Sprite[i]->fillSprite(backColor);                        // Закрасим поле соообщений
                    Air_txt_Sprite[i]->setTextColor(little_air_color[i], backColor); // Установить цвет согласно программе предупреждения опастности
                    Air_txt_Sprite[i]->setTextDatum(TC_DATUM);                       // Определим как будет выводится текс
                    Air_txt_Sprite[i]->loadFont(NotoSansBold15);                     // Установить шрифт формуляра
                    /**/
                    if (height_difference[i] > 0) // Чужой самолет выше нашего
                    {
                        Air_txt_Sprite[i]->drawString("+" + String(int(height_difference[i])), Air_txt_x, 2, 1);
                    }
                    else
                    {
                        // Чужой самолет ниже нашего
                        Air_txt_Sprite[i]->drawString(String(int(height_difference[i])), Air_txt_x, 2, 1);
                    }

                    /* Записать скорость в формуляр */
                    Air_txt_Sprite[i]->drawString(String(alien_speed_tmr[i]), Air_txt_x, 14, 1); 

                    /* Определяем наличие и направление вывода стрелок */
                   
                    if (alien_altitude_array_countMax[i]&& ((int)Container[i].distance < 3000)) //Разрешить выдавать данные о высоте если дистанция ближе 3000 м.
                    {
                        static uint32_t tmr_array = millis();
                     
                        if (millis() - tmr_array > 300)
                        {
                            tmr_array = millis();
                            array_ok = true;

                            if (alien_altitude_hysteresis[i] > alien_altitude_arrow[i])
                            {
                                arrow_up_down[i] = 1;
                                alien_altitude_arrow[i] = alien_altitude_hysteresis[i];
                            }
                            else if (alien_altitude_hysteresis[i] < alien_altitude_arrow[i])
                            {
                                arrow_up_down[i] = 2;
                                alien_altitude_arrow[i] = alien_altitude_hysteresis[i];
                            }
                            else
                            {
                                arrow_up_down[i] = 0;
                            }

                           // Serial.print("array_ok ");
                           // Serial.print(array_ok);
                           // Serial.print("arrow_up_down");
                           // Serial.println(arrow_up_down[i]);

                        }
                    }
                    else
                    {
                        array_ok = true;
                        arrow_up_down[i] = 0;
                    }

                    /* Определение стрелки вверх вниз. Подем или снижение самолета*/

                    if (array_ok)
                    {
                        array_ok = false;

                      /*  if (arrow_up_down[i] != arrow_up_down_old[i])
                        {
                            arrow_up_down_old[i] = arrow_up_down[i];*/

                            switch (arrow_up_down[i])
                            {
                            case 0:

                                arrow[i]->fillSprite(backColor);             // Закрасим поле стрелок вверх
                                break;
                            case 1:
                                /*Рисуем стрелку вверх */
                                arrow[i]->fillSprite(backColor);                  // Закрасим поле стрелок вверх
                                arrow[i]->drawLine(4, 0, 4, 10, little_air_color[i]);
                                arrow[i]->drawLine(0, 4, 4, 0, little_air_color[i]);
                                arrow[i]->drawLine(4, 0, 8, 4, little_air_color[i]);
                                //arrow[i]->pushToSprite(Air_txt_Sprite[i], Air_txt_x - 26, 6, TFT_BLACK);
                                break;
                            case 2:

                                /*Рисуем стрелку вниз */
                                arrow[i]->fillSprite(backColor);             // Закрасим поле стрелок вниз
                                arrow[i]->drawLine(4, 0, 4, 10, little_air_color[i]);
                                arrow[i]->drawLine(0, 6, 4, 10, little_air_color[i]);
                                arrow[i]->drawLine(4, 10, 8, 6, little_air_color[i]);
                              /*  arrow[i]->pushToSprite(Air_txt_Sprite[i], Air_txt_x - 26, 6, TFT_BLACK);*/
                                break;
                            default:
                                break;
                            }
                        //}
                    }

                    little_airplane[i]->fillSprite(TFT_BLACK);      // Закрасим поле самолетика

                    /*Рисуем маленький самолетик */
                    little_airplane[i]->drawLine(10, 1, 10, 16, little_air_color[i]);
                    little_airplane[i]->drawLine(11, 0, 11, 16, little_air_color[i]);
                    little_airplane[i]->drawLine(12, 1, 12, 16, little_air_color[i]);

                    little_airplane[i]->drawLine(6, 5, 15, 5, little_air_color[i]);
                    little_airplane[i]->drawLine(3, 6, 18, 6, little_air_color[i]);
                    little_airplane[i]->drawLine(0, 7, 21, 7, little_air_color[i]);


                    little_airplane[i]->drawLine(6, 15, 14, 15, little_air_color[i]);
                    little_airplane[i]->drawLine(7, 16, 15, 16, little_air_color[i]);

                    area_airplane[i]->fillSprite(TFT_BLACK);      // Закрасим поле 

                    esp_task_wdt_reset();
                }
            }

            back.fillSprite(backColor);                   // Закрасим поле 
            backsprite.fillSprite(backColor);             // 
            backsprite.setPivot(160, 160);                // Назначаем центр вращения спрайта воздушной обстановки

            /* Рисуем круглую шкалу серым цветом и символы сторон света белым*/
            for (int i = 0; i < 36; i++)
            {
                color2 = TFT_DARKGREY;
                if (i % 3 == 0)
                {
                    backsprite.drawWedgeLine(x[i * 10], y[i * 10], px[i * 10], py[i * 10], 1, 1, color2);
                    backsprite.setTextColor(TFT_DARKGREY, TFT_BLACK);
                    if (i == 0)
                    {
                        backsprite.drawString("N", lx[i * 10] + 1, ly[i * 10]);
                    }
                    if (i == 9)
                    {
                        backsprite.drawString("E", lx[i * 10], ly[i * 10]);
                    }
                    if (i == 18)
                    {
                        backsprite.drawString("S", lx[i * 10], ly[i * 10]);
                    }
                    if (i == 27)
                    {
                        backsprite.drawString("W", lx[i * 10], ly[i * 10]);
                    }
                }
                else
                {
                    backsprite.drawWedgeLine(x[i * 10], y[i * 10], px1[i * 10], py1[i * 10], 1, 1, color2);
                }
            }

            //      data_KM.fillSprite(backColor);
                  /*Рисуем малый серый круг*/
            backsprite.drawCircle(cx, 160, 80, TFT_DARKGREY);

            ///* Вычисляем направление полета нашего самолета*/
            if (ThisAircraft.latitude != latitude_old)
            {
                test_curse = bearing_calc(latitude_old, longitude_old, ThisAircraft.latitude, ThisAircraft.longitude);

                latitude_old = ThisAircraft.latitude;
                longitude_old = ThisAircraft.longitude;
            }

            esp_task_wdt_reset();

            /*Выполняем поворот нашего самолета по азимуту*/
            backsprite.pushRotated(&back, angle, TFT_BLACK);

            if (divider <= 32767)
            {
                data_KM.loadFont(NotoSansBold15);
                data_KM.fillSprite(backColor);
                data_KM.setTextDatum(TC_DATUM);
                int data_KM_x = 35;

                switch (divider_num)
                {
                case 1:
                    data_KM.drawString("16000 m", data_KM_x, 1);
                    break;
                case 2:
                    data_KM.drawString("10000 m", data_KM_x, 1);
                    break;
                case 3:
                    data_KM.drawString("5000 m", data_KM_x, 1);
                    break;
                case 4:
                    data_KM.drawString("2000 m", data_KM_x, 1);
                    break;
                case 5:
                    data_KM.drawString("1000 m", data_KM_x, 1);
                    break;
                case 6:
                    data_KM.drawString("500 m", data_KM_x, 1);
                    break;
                case 7:
                    data_KM.drawString("200 m", data_KM_x, 1);
                    break;
                case 8:
                    data_KM.drawString("100 m", data_KM_x, 1);
                    break;
                case 9:
                    data_KM.drawString("50 m", data_KM_x, 1);
                    break;
                default:
                    data_KM.drawString("16000 m", data_KM_x, 1);
                    break;
                    // выполняется, если не выбрана ни одна альтернатива
                    // default необязателен
                }

                data_KM.pushToSprite(&back, 120, 225, TFT_BLACK); // Выводим надпись дистанции внизу малого круга
            }



            /* Рисум градусы азимута*/
            /*  отображаем на табло курс в градусах*/
            esp_task_wdt_reset();
            data_az.loadFont(NotoSansMonoSCB20);
            data_az.setTextDatum(CR_DATUM);
            data_az.fillSprite(TFT_BLACK);

            // data_az.drawSmoothRoundRect(0, 0, 5, 5, 50, 25, TFT_DARKGREY);

            data_az.drawRect(0, 0, 50, 25, TFT_DARKGREY);
            data_az.drawCircle(42, 8, 3, TFT_GREEN);    // Рисуем кружок символа градуса
            data_az.setTextColor(TFT_GREEN, backColor);
            data_az.drawString(String(360 - angle), 37, 14);
            data_az.pushToSprite(&back, 138, 1);
            esp_task_wdt_reset();

            /*Рисуем заряд аккумулятора*/
            power1.fillSprite(TFT_BLACK);
            power1.fillRect(2, 2, 26, 12, TFT_GREEN);
            power1.drawRect(0, 0, 30, 16, TFT_WHITE);
            power1.fillRect(30, 4, 3, 8, TFT_WHITE);
            power1.pushToSprite(&back, 282, 4, TFT_BLACK);

            /* настройки сообщения о дистанции вверху слева*/
            dist_info.loadFont(NotoSansMonoSCB20);
            dist_info.fillSprite(TFT_BLACK);
            dist_info.setTextDatum(TC_DATUM);
            String arr_min_txt = String(arr_min);

            if (arr_min <= 5)
            {
                arr_min_txt = "0";
            }
            else
            {
                int len = arr_min_txt.length();
                arr_min_txt.setCharAt(len - 1, '0');
            }

            if (arr_min != 32767)
            {
                // dist_info.drawSmoothRoundRect(0, 0, 5, 1, 80, 25, txt_color);
                dist_info.drawRect(0, 0, 80, 25, TFT_DARKGREY);
                dist_info.setTextColor(txt_color, backColor);
                dist_info.drawString(arr_min_txt + " m", 40, 4);
                dist_info.pushToSprite(&back, 1, 2);
            }
            else
            {
              
                // dist_info.drawSmoothRoundRect(0, 0, 5, 1, 80, 25, TFT_DARKGREY);
                dist_info.drawRect(0, 0, 80, 25, TFT_DARKGREY);
                dist_info.setTextColor(TFT_WHITE, backColor);
                dist_info.drawString("-----", 40, 4);
                dist_info.pushToSprite(&back, 1, 2);
                
            }

            /*Формируем картинку самолета*/
                /* Рисуем фюзеляж*/
            Airplane.drawLine(12, 0, 12, 18, TFT_DARKGREY);

            /*Рисуем передние крылья*/
            Airplane.drawLine(3, 7, 20, 7, TFT_DARKGREY);
            Airplane.drawLine(0, 8, 23, 8, TFT_DARKGREY);

            /*Рисуем задние крылья*/
            Airplane.drawLine(7, 17, 17, 17, TFT_DARKGREY);
            Airplane.pushToSprite(&back, 148, 150, TFT_BLACK);

            /*отображаем спрайт с информацией по объектам*/
            for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
            {
                if (isTeam_all[i] == true)
                {

                    little_airplane[i]->pushRotated(area_airplane[i], alien_curse[i], TFT_BLACK);
                    area_airplane[i]->pushToSprite(&back, radar_center_x + Container_alien_X[i] - 15, radar_center_y - Container_alien_Y[i] - 15, TFT_BLACK);
                    arrow[i]->pushToSprite(Air_txt_Sprite[i], Air_txt_x - 26, 6, TFT_BLACK);

                    if (Air_txt_left[i])
                    {
                        Air_txt_Sprite[i]->drawSmoothRoundRect(0, 0, 1, 1, 65, 29, TFT_DARKCYAN);
                        Air_txt_Sprite[i]->fillTriangle(76, 15, 67, 7, 67, 23, TFT_DARKCYAN);

                    }
                    else
                    {
                        Air_txt_Sprite[i]->drawSmoothRoundRect(11, 0, 1, 1, 64, 29, TFT_DARKCYAN);
                        Air_txt_Sprite[i]->fillTriangle(0, 15, 9, 7, 9, 23, TFT_DARKCYAN);
                    }

                    Air_txt_Sprite[i]->pushToSprite(&back, radar_center_x + Container_logbook_X[i], radar_center_y - Container_logbook_Y[i], TFT_BLACK);

                    isTeam_all[i] = false;
                    esp_task_wdt_reset();
                }
            }

            esp_task_wdt_reset();

            /*рисуем все спрайты*/
            back.pushSprite(0, 0);
        }
    }
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::draw(TFTMenu* menuManager)
 {
     TFT_Class* tft_radar = menuManager->getDC();
     if (!tft_radar)
     {
         return;
     }
     TFTRus* rusPrinter = menuManager->getRusPrinter();


     uint16_t tbw1;
     uint16_t x_tft, y_tft;

     const char EPD_SoftRF_text1[] = "FlyRF";
     const char EPD_SoftRF_text2[] = "www.decima.ru";
     const char EPD_SoftRF_text3[] = "DECIMA";
     const char EPD_SoftRF_text5[] = "Linar Yusupov";
     const char EPD_SoftRF_text6[] = "(C) 2023";


     tft_radar->fillScreen(TFT_NAVY);

     tft_radar->setTextColor(TFT_WHITE); //TFT_WHITE TFT_BLACK
     tft_radar->setTextWrap(false);

     tft_radar->setFreeFont(&FreeMonoBold24pt7b);

     x_tft = 90;
     y_tft = 80;
     tft_radar->setCursor(x_tft, y_tft);
     tft_radar->print(EPD_SoftRF_text1);

     x_tft = 80;
     y_tft = 150;
     tft_radar->setCursor(x_tft, y_tft);
     tft_radar->print(EPD_SoftRF_text3);

     tft_radar->setFreeFont(&FreeSerif9pt7b);

     x_tft = 10;
     y_tft = 205;
     tft_radar->setCursor(x_tft, y_tft);
     tft_radar->print(EPD_SoftRF_text2);

     x_tft = 10;
     y_tft = tft_radar->height() - tft_radar->fontHeight() + 10;
     tft_radar->setCursor(x_tft, y_tft);
     tft_radar->print(EPD_SoftRF_text6);

     //Current_version


     tbw1 = tft_radar->textWidth(Current_version);
     x_tft = (tft_radar->width() - tbw1) - 4;
     y_tft = tft_radar->height() - tft_radar->fontHeight() + 10;
     tft_radar->setCursor(x_tft, y_tft);
     tft_radar->print(Current_version);

     esp_task_wdt_reset();
     vTaskDelay(4000);

/*
     for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) {
         if (Container[i].addr && (ThisAircraft.timestamp - Container[i].timestamp) > ENTRY_EXPIRATION_TIME) {
             Container[i] = EmptyFO;
         }
     }

*/


     esp_task_wdt_reset();

     back.fillSprite(backColor);                   // Закрасим поле 
     backsprite.fillSprite(backColor);             // 
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

    dist_info.loadFont(NotoSansMonoSCB20);
    dist_info.setTextDatum(TC_DATUM);
    dist_info.fillSprite(TFT_BLACK);
    dist_info.drawRect(0, 0, 80, 25, TFT_DARKGREY);
    dist_info.setTextColor(TFT_WHITE, backColor);
    dist_info.drawString("-----", 40, 4);
    dist_info.pushToSprite(&back, 1, 2);


         /* Рисум градусы азимута*/
    /*  отображаем на табло курс в градусах*/

   data_az.loadFont(NotoSansMonoSCB20);
   data_az.setTextDatum(CR_DATUM);
   data_az.fillSprite(TFT_BLACK);
   data_az.drawRect(0, 0, 48, 25, TFT_WHITE);
   data_az.drawCircle(40, 8, 3, TFT_GREEN);    // Рисуем кружок символа градуса
   data_az.setTextColor(TFT_GREEN, backColor);
   data_az.drawString(String(0), 35, 14);
   data_az.pushToSprite(&back, 138, 1);


            /*Рисуем заряд аккумулятора*/
    power1.fillSprite(TFT_BLACK);
    power1.fillRect(2, 2, 26, 12, TFT_GREEN);
    power1.drawRect(0, 0, 30, 16, TFT_WHITE);
    power1.fillRect(30, 4, 3, 8, TFT_WHITE);
    power1.pushToSprite(&back, 282, 4, TFT_BLACK);

    /* настройки сообщения о дистанции внизу слева*/
                                                              /*Формируем картинку самолета*/
     /* Рисуем фюзеляж*/

   Airplane.drawLine(12, 0, 12, 18, TFT_DARKGREY);

   /*Рисуем пердние крылья*/
   Airplane.drawLine(3, 7, 20, 7, TFT_DARKGREY);
   Airplane.drawLine(0, 8, 23, 8, TFT_DARKGREY);

   /*Рисуем зажние крылья*/
   Airplane.drawLine(7, 17, 17, 17, TFT_DARKGREY);
   // Airplane.drawLine(7, 20, 15, 20, TFT_DARKGREY);
   Airplane.pushToSprite(&back, 148, 150, TFT_BLACK);
     /*рисуем все неподвижные спрайты*/
    back.pushSprite(0, 0);
 
    //   /* Определение местоположения при старте */
    fix = (uint8_t)isValidGNSSFix();

    if (!fix && (settings->mode != SOFTRF_MODE_TXRX_TEST))
    {
            if (!text_call)
            {
                waiting_txt(menuManager);
               // text_call = true;
            }
    }

    esp_task_wdt_reset();
    vTaskDelay(4000);
    esp_task_wdt_reset();
    //vTaskDelay(4000);
 

    //while (((int)ThisAircraft.latitude==0) && ((int)ThisAircraft.longitude == 0))
    //{
 
    //}


 }

 void TFTMenuScreen::saveVer(String ver)
 {

     Current_version = ver;

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 // Контроль внутреннего источника питания (аккумуляторов)
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::drawVoltage(TFTMenu* menuManager)
 {
	// TFT_Class* dc = menuManager->getDC();
	// if (!dc)
	// {
	//	 return;
	// }
	// TFTRus* rusPrinter = menuManager->getRusPrinter();


	// int screenWidth = dc->width();
	// int screenHeight = dc->height();

	//// dc->setFreeFont(TFT_SMALL_FONT);
	// int textFontHeight = FONT_HEIGHT(dc);

	// String data = SOFTWARE_VERSION;

	// int textFontWidth = dc->textWidth(data);              // Returns pixel width of string in current font
	// uint16_t curX = screenWidth - textFontWidth - 10;     // Координаты вывода 
	// uint16_t curY = 5;// 305;                             // Координаты вывода версии



	// rusPrinter->print(data.c_str(), curX, curY, TFT_WHITE, TFT_BLACK); // Отображаем версию программы

	// //dc->setFreeFont(TFT_FONT);

	//
	// VoltageData vData5 = Settings.voltage5V;     // Контроль источника питания +5.0в

	// if (last5Vvoltage != vData5.raw)
	// {
	//	 last5Vvoltage = vData5.raw;
 //    
	//	 int y_val = 37;
	//	 int x_val = map(vData5.voltage5, 10, 230, 0, 100);

	//	 if (x_val < 20)
	//	 {
	//		 dc->fillRect(10, y_val, vData5.voltage5, 7, TFT_RED);
	//	 }
	//	 else if ((x_val >= 20) && (x_val < 60))
	//	 {
	//		 dc->fillRect(10, y_val, vData5.voltage5, 7, TFT_YELLOW);
	//	 }
	//	 else if (x_val >= 60)
	//	 {
	//		 dc->fillRect(10, y_val, vData5.voltage5, 7, TFT_GREEN);
	//	 }
	//	 dc->fillRect(vData5.voltage5, y_val-1, 230, 7+1, TFT_WHITE);

	// }

 }

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Отображение заряда источника питания (аккумуляторов)
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::chargeControl(TFTMenu* menuManager)
 {

	 TFT_Class* dc = menuManager->getDC();
	 if (!dc)
	 {
		 return;
	 }
	 //int y_val = 45;
	 //int chargeControl_on = digitalRead(BATTERY_CHARGE);
	 //if (chargeControl_on == 0 )
	 //{
		// if (millis() - tmr > 30)
		// {
		//	 tmr = millis();
		//	 control_X++;
		//	 if (control_X > 220) control_X = 10;
		//	 dc->fillRect(control_X, y_val-1, 230, 7+1, TFT_WHITE);
		//	 dc->fillRect(10, y_val, control_X, 7, TFT_DARKBLUE);
		//	// dc->fillRect(control_X-10, y_val, 230, 7, TFT_WHITE);
		//	 charge_on = true;
		// }
	 //}
	 //else if(charge_on)
	 //{
		// control_X = 10;
		// dc->fillRect(10, y_val, 230, 7, TFT_WHITE);
		// charge_on = false;
	 //}

 }


 ////------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 //// Вывод текущей даты и времени
 ////------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 //void TFTMenuScreen::Rotate_and_Draw_Bitmap(TFTMenu* menuManager, const uint8_t* bitmap, int winkel, uint8_t x, uint8_t y, uint8_t color)
 //{

	// TFT_Class* dc = menuManager->getDC();
	// if (!dc)
	// {
	//	 return;
	// }
	// TFTRus* rusPrinter = menuManager->getRusPrinter();

	// dc->setFreeFont(TFT_FONT);

 //    uint8_t width, height;
 //    width = 32;                           // Read the image width from the array in PROGMEM
 //    height = 32;                          // Read the height width from the array in PROGMEM

 //    int altes_x, altes_y, neues_x, neues_y; // old and new (rotated) Pixel-Coordinates

 //    int drehpunkt_x = width / 2;          // Calculate the (rotation) center of the image (x fraction)
 //    int drehpunkt_y = height / 2;         // Calculate the (rotation) center of the image (y fraction)

 //    float winkel_rad = winkel / 57.3;

 //    float sin_winkel = sin(winkel_rad);   // Lookup the sinus
 //    float cos_winkel = cos(winkel_rad);   // Lookup the cosinus

 //    uint8_t gedrehtes_bild[height / 8 * width + 2]; // Image array in RAM (will contain the rotated image)
 //    memset(gedrehtes_bild, 0, sizeof(gedrehtes_bild)); // Clear the array with 0

 //    int i, j, counter = 0;

 //    gedrehtes_bild[0] = width;                // First byte of the rotated image contains (as the original) the width
 //    gedrehtes_bild[1] = height;               // Second byte of the rotated image contains (as the original) the height

 //    dc->fillRect(x+16, y - 13, width+4, height+4, TFT_BLACK);


 //    for (i = 0; i < height * width / 8; i++) { // i goes through all the Bytes of the image
 //        uint8_t displayData = 0x0f;// bmp;  // Read the image data from PROGMEM
 //        for (j = 0; j < 8; j++) {           // j goes through all the Bits of a Byte
 //            if (displayData & (1 << j)) { // if a Bit is set, rotate it
 //                altes_x = ((i % width) + 1) - drehpunkt_x;                     // Calculate the x-position of the Pixel to be rotated
 //                altes_y = drehpunkt_y - (((int)(i / width)) * 8 + j + 1);              // Calculate the y-position of the Pixel to be rotated
 //                neues_x = (int)(altes_x * cos_winkel - altes_y * sin_winkel); // Calculate the x-position of the rotated Pixel
 //                neues_y = (int)(altes_y * cos_winkel + altes_x * sin_winkel); // Calculate the y-position of the rotated Pixel

 //                // Check if the rotated pixel is withing the image (important if non-square images are used). If not, continue with the next pixel.
 //                if (neues_x <= (drehpunkt_x - 1) && neues_x >= (1 - drehpunkt_x) && neues_y <= (drehpunkt_y - 1) && neues_y >= (1 - drehpunkt_y)) {
 //                    // Write the rotated bit to the array (gedrehtes_bild[]) in RAM
 //                    gedrehtes_bild[(neues_x + drehpunkt_x) % width + ((int)((drehpunkt_y - neues_y - 1) / 8) * width) + 2] |= (1 << (drehpunkt_y - neues_y - 1) % 8);
 //                }
 //            }
 //        }
 //    }

 //    dc->drawBitmap(50, 20, gedrehtes_bild, x, y, TFT_WHITE);
 //}


 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 // Вывод параметров WiFi
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::drawWiFi(TFTMenu* menuManager)
 {
     TFT_Class* dc = menuManager->getDC();
     if (!dc)
     {
         return;
     }
     TFTRus* rusPrinter = menuManager->getRusPrinter();

     //dc->setTextFont(1);
     //dc->setFreeFont(TFT_FONT);
     //dc->setFreeFont(TFT_SMALL_FONT);
      uint16_t curX = 5;     // Координаты вывода WiFi
      uint16_t curY = 2;
      //bool WiFi_On = Settings.GetWiFiState();
      //bool WiFi_Connect = Settings.GetWiFiConnect();   // Признак подключения к роутеру

      //WiFi_On = true;                                // Признак подключения модуля в работу
     // WiFi_Connect = true;                           // Признак подключения к роутеру

 //     if (WiFi_On)
 //     {
 //         if (WiFi_Connect)                           // Признак подключения к роутеру
 //         {
 //             rusPrinter->print("           ", curX, curY, TFT_BLACK, TFT_BLACK);
 //             rusPrinter->print("WiFi on    ", curX, curY, TFT_BLACK, 0x16C2);

 //         }
 //         else
 //         {
 //             rusPrinter->print("WiFi off", curX, curY, TFT_BLACK, TFT_RED);

 //         }

 //        // rusPrinter->print("           ", curX, curY, TFT_BLACK, TFT_WHITE);
 //     }
 ///*     else
 //     {
 //         rusPrinter->print("           ", curX, curY, TFT_BLACK, TFT_WHITE);

 //     }*/


 }




 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 // Вывод направления движения
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------


 float TFTMenuScreen::bearing_calc(float lat, float lon, float lat2, float lon2)
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

 double TFTMenuScreen::distance_form(double lat1, double long1, double lat2, double long2)
 {
     // возвращает расстояние в метрах между двумя указанными позициями
     // как десятичные градусы со знаком широты и долготы. Использует большой круг
     // расчет расстояния для гипотетической сферы радиусом 6372795 метров.
     // Поскольку Земля не является точной сферой, ошибки округления могут достигать 0,5%.
     // С разрешения Маартена Ламерса


     double delta = radians(long1 - long2);
     double sdlong = sin(delta);
     double cdlong = cos(delta);
     lat1 = radians(lat1);
     lat2 = radians(lat2);
     double slat1 = sin(lat1);
     double clat1 = cos(lat1);
     double slat2 = sin(lat2);
     double clat2 = cos(lat2);
     delta = (clat1 * slat2) - (slat1 * clat2 * cdlong);
     delta = sq(delta);
     delta += sq(clat2 * sdlong);
     delta = sqrt(delta);
     double denom = (slat1 * slat2) + (clat1 * clat2 * cdlong);
     delta = atan2(delta, denom);
     return delta * 6372795;
 }


 uint16_t TFTMenuScreen::getSpeed(uint16_t speed) // 
 {

     //float ina_voltage = ina.readBusVoltage();
     //voltageAkk1 = ina_voltage * 100;

     //dimension_array[array_count] = voltageAkk1;
     //array_count++;
     int val_voltage = 0;
     //if (array_count > array_size)                    // проверка заполнения массива первичными данными о уровне напряжения аккумулятора
     //{
     //    array_count = 0;
     //    array_countMax = true;                       //Разрешить выдавать данные об уровне напряжения аккумулятора
     //}

     //sum = 0;                                         //

     //if (array_countMax)                              // формируем данные об уровне напряжения аккумулятора
     //{
     //    for (int i = 0; i < array_size; i++)
     //    {
     //        sum += dimension_array[i];
     //    }
     //    val_voltage = sum / array_size;
     //}
     //else
     //{
     //    for (int i = 0; i < array_count; i++)       //формируем первичные (заполняем массив) данные об уровне напряжения аккумулятора
     //    {
     //        sum += dimension_array[array_count - 1];
     //    }
     //    val_voltage = sum / array_count;
     //}

     //sum = 0;
     return val_voltage;                                 //Напряжение питания аккумулятора
 }

 uint16_t TFTMenuScreen::getPowerVoltageAkk(uint16_t pin) // Контроль напряжения питания внутренних источников (аккумуляторов).
 {


     //float ina_voltage = ina.readBusVoltage();
     //voltageAkk1 = ina_voltage * 100;

     ////float BusPower = ina.readBusPower();

     ////float ShuntVoltage = ina.readShuntVoltage();

     ////float ShuntCurrent = ina.readShuntCurrent();

     //////DBG("Bus voltage:   ");
     //////DBG(ina_voltage);
     ////////DBGLN(" V");

     //////DBG("Bus power:     ");
     //////DBG(BusPower);
     ////////DBGLN(" W");

     /////*
     //////DBG("Shunt voltage: ");
     ////Serial.print(ina.readShuntVoltage(), 5); 
     ////////DBGLN(" V");*/

     //////DBG("Shunt current: ");
     ////Serial.print(ina.readShuntCurrent(), 5);
     ////////DBGLN(" A");

     ////////DBGLN("");

     //dimension_array[array_count] = voltageAkk1;
     //array_count++;
     int val_voltage = 0;
     //if (array_count > array_size)                    // проверка заполнения массива первичными данными о уровне напряжения аккумулятора
     //{
     //    array_count = 0;
     //    array_countMax = true;                       //Разрешить выдавать данные об уровне напряжения аккумулятора
     //}

     //sum = 0;                                         //

     //if (array_countMax)                              // формируем данные об уровне напряжения аккумулятора
     //{
     //    for (int i = 0; i < array_size; i++)
     //    {
     //        sum += dimension_array[i];
     //    }
     //    val_voltage = sum / array_size;
     //}
     //else
     //{
     //    for (int i = 0; i < array_count; i++)       //формируем первичные (заполняем массив) данные об уровне напряжения аккумулятора
     //    {
     //        sum += dimension_array[array_count - 1];
     //    }
     //    val_voltage = sum / array_count;
     //}

     //sum = 0;
     return val_voltage;                                 //Напряжение питания аккумулятора
 }

 int TFTMenuScreen::alien_count()
 {
     int count = 0;

     for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) 
     {
         //Serial.print("Container[i].addr - ");
         //Serial.println(Container[i].addr, HEX);
         if (Container[i].addr) 
         {
           /*  Serial.print("Container[i].addr - ");
             Serial.println(Container[i].addr,HEX);*/
             count++;
         }
     }

     return count;
 }

 bool TFTMenuScreen::coordinates_waiting()
 {
     bool coord = false;



     return coord;
 }

 void TFTMenuScreen::waiting_txt(TFTMenu* menuManager) // Вывод текста "ОПРЕДЕЛЕНИЕ МЕСТОПОЛОЖЕНИЯ"
 {
     /* Определение местоположения нашего самолета при старте */

     TFT_Class* tft_radar = menuManager->getDC();
     if (!tft_radar)
     {
         return;
     }

     TFTRus* rusPrinter = menuManager->getRusPrinter();

     int screenWidth = tft_radar->width();
     int screenHeight = tft_radar->height();

    // Serial.print("screenWidth ");
    // Serial.print(screenWidth);

   //  Serial.print(" | screenHeight ");
   //  Serial.print(screenHeight);


     tft_radar->setFreeFont(TFT_FONT);
     int textFontHeight = FONT_HEIGHT(tft_radar);

     String data = data_txt; //"ОПРЕДЕЛЕНИЕ"
     String data1 = data_txt1; //"МЕСТОПОЛОЖЕНИЯ"
     int textFontWidth = tft_radar->textWidth(data, 2);                   // Returns pixel width of string in current font

   //  Serial.print("textFontWidth ");
   //  Serial.print(textFontWidth);

     uint16_t curX = (screenWidth / 2) - (textFontWidth / 2) - 12;        // Координаты вывода 
     uint16_t curY = 110;                                                 // Координаты вывода текста
     rusPrinter->print(data.c_str(), curX, curY, backColor, TFT_YELLOW);  // Отображаем 

     textFontWidth = tft_radar->textWidth(data1, 2);                      // Returns pixel width of string in current font
    // Serial.print(" | textFontWidth2 ");
    // Serial.print(textFontWidth);

     curX = (screenWidth / 2) - (textFontWidth / 2) - 18;                  // Координаты вывода 
     curY = 140;                                                           // Координаты вывода текста
     rusPrinter->print(data1.c_str(), curX, curY, backColor, TFT_YELLOW);  // Отображаем 


 }



//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_TFT_MODULE
