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
#include "EEPROMRF.h"
#include "TrafficHelper.h"
#include "GNSS.h"
#include "NotoSansMonoSCB20.h"
#include "NotoSansBold15.h"
#include "RF.h"
#include "CoreButton.h"
//#include "mode-s.h"
#include <stdio.h>
#include "Memory.h"               // Работа с энергонезависимой памятью

#define FONT_10 "Arial10"
#define FONT_11 "Arial11"
#define FONT_12 "Arial12"
#define FONT_14 "Arial14"
#define FONT_16 "Arial16"
#define FONT_18 "Arial18"
#define FONT_20 "Arial20"
#define FONT_22 "Arial22"
#define FONT_25 "Arial25"
#define FONT_30 "Arial30"

#ifdef USE_TFT_MODULE

#include "TFTMenu.h"

/* Спрайты вывода изображений и информации на экран дисплея */


// create a new sprite
TFT_eSPI tft = TFT_eSPI();

TFT_eSprite back       = TFT_eSprite(&tft);         // Спрайт фона
TFT_eSprite backsprite = TFT_eSprite(&tft);         // Спрайт отображения вращающегося поля воздушной обстановки
TFT_eSprite dist_info  = TFT_eSprite(&tft);         // Спрайт окна информации расстояние до ближайшего стороннего воздушного объекта
TFT_eSprite Airplane   = TFT_eSprite(&tft);         // Спрайт нашего самолета
TFT_eSprite data_az    = TFT_eSprite(&tft);         // Информационный спрайт.Азимут (угол направления стороннего самолета)
TFT_eSprite data_speed = TFT_eSprite(&tft);         // Информационный спрайт.Скорость стороннего самолета.
TFT_eSprite data_KM    = TFT_eSprite(&tft);         // Информационный спрайт. Дипазон расстояний всего поля 
TFT_eSprite power1     = TFT_eSprite(&tft);         // Спрайт отображения заряда аккумулятора 
TFT_eSprite mail       = TFT_eSprite(&tft);         // Спрайт отображения почтового ящика
TFT_eSprite rows_mail  = TFT_eSprite(&tft);         // Спрайт отображения текстов почтового ящика

TFT_eSprite* arrow[MAX_TRACKING_OBJECTS];           // Спрайт отображения стрелки
TFT_eSprite* arrow_old[MAX_TRACKING_OBJECTS];       // Спрайт отображения стрелка 

TFT_eSprite* Air_txt_Sprite[MAX_TRACKING_OBJECTS];  // Этот спрайт, площадка в котором будет располагатся формуляр стороннего самолета
TFT_eSprite* little_airplane[MAX_TRACKING_OBJECTS]; // Этот спрайт, площадка в котором будет располагатся изображение стороннего самолета DUMP1090
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
int alient_course [MAX_TRACKING_OBJECTS];           // Курс стороннего самолета
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

    // добавляем кнопки
    Button* btn = new Button;

    // кнопка "POINT" 
    btn->begin(BUTTON_MAIL, true, SEND_POINT_TIME_ON); 
    hardwareButtons.push_back(btn);


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

void TFTMenu::onButtonisRetention(int button)
{
    if (currentScreenIndex == -1)
        return;
 
    resetIdleTimer();
    TFTScreenInfo* currentScreenInfo = &(screens[currentScreenIndex]);
    currentScreenInfo->screen->onButtonisRetention(this, button);
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

  for (size_t i = 0; i < hardwareButtons.size(); i++)
  {
      hardwareButtons[i]->update();
  }

  // проверяем состояние кнопок
  for (size_t i = 0; i < hardwareButtons.size(); i++)
  {

      if (hardwareButtons[i]->isPressed())
      {
          // кликнута кнопка на пине pin
          uint8_t pin = hardwareButtons[i]->pinNumber();
          // тут можно вызывать событие для дочернего экрана, например, событие "кнопка нажата"
          onButtonPressed(pin);
          // подробнее по состояниям кнопки см. CoreButton.h
      }

      else if (hardwareButtons[i]->isClicked())
      {
          // кликнута кнопка на пине pin
          uint8_t pin = hardwareButtons[i]->pinNumber();

          // тут можно вызывать событие для дочернего экрана, например, событие "кнопка нажата и отпущена"
          button_ret_Flag = SettingsMail.GetButtonRetention(); // проверить нет ли длительного нажаимя кнопок isRetention()

          if (!button_ret_Flag)
          {
              onButtonReleased(pin);
          }
          else
          {
              SettingsMail.SetButtonRetention(false);
          }
      }

      if (hardwareButtons[i]->isRetention())
      {
          // кликнута кнопка на пине pin
          uint8_t pin = hardwareButtons[i]->pinNumber();

          // тут можно вызывать событие для дочернего экрана, например, событие "кнопка нажата и отпущена"
          SettingsMail.SetButtonRetention(true);     // установить флаг длительного нажатия кнопки
          onButtonisRetention(pin);
      }
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
     pressed_button_Retention = -1;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::onActivate(TFTMenu* menuManager)
 {
	 if (!menuManager->getDC())
	 {
		 return;
	 }
     released_button = -1;
     pressed_button_Retention = -1;

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
       Air_txt_Sprite[i]->createSprite(45, 14);
       Air_txt_Sprite[i]->setPivot(20, 7);
 
       DUMP1090_Sprite[i] = new TFT_eSprite(&tft);     // Спрайт информации DUMP1090 стороннего воздушного объекта
       DUMP1090_Sprite[i]->createSprite(100, 20);
       DUMP1090_Sprite[i]->setPivot(50, 10);

       arrow[i] = new TFT_eSprite(&tft);              // Спрайт информации стороннего воздушного объекта
       arrow[i]->createSprite(10, 10);                // Спрайт отображения стрелка вверх/вниз

       little_airplane [i] = new TFT_eSprite(&tft);   // Спрайт информации стороннего воздушного объекта
       little_airplane [i]->createSprite(100, 100);
       little_airplane[i]->setPivot(50,50); 

       LoRa_airplane[i] = new TFT_eSprite(&tft);     // Спрайт информации стороннего воздушного объекта LoRa
       LoRa_airplane[i]->createSprite(100, 100);
       LoRa_airplane[i]->setPivot(50, 50);

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

    data_speed.createSprite(95, 25);
    data_speed.setTextColor(TFT_GREEN, backColor);

    mail.createSprite(40, 25);
    mail.setTextColor(TFT_GREEN, backColor);

    rows_mail.createSprite(320, 36);
    rows_mail.setTextColor(TFT_WHITE, backColor);
   
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

 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::onButtonPressed(TFTMenu* menuManager, int buttonID)
 {
    // pressed_button = buttonID;
     //DBGLN("LCDMainScreen::onButtonPressed..");
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::onButtonReleased(TFTMenu* menuManager, int buttonID)
 {
     released_button = buttonID;
     //DBGLN("LCDMainScreen::onButtonReleased..");
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::onButtonisRetention(TFTMenu* menuManager, int buttonID)
 {
     //DBGLN("LCDMainScreen::onButtonisRetention..");
     pressed_button_Retention = buttonID;
 }

 
 //----------------------------------------------------------------------------------------------------------------------------------------------------------------------
 
 void TFTMenuScreen::update(TFTMenu* menuManager)
 {
	
      TFT_Class* dc = menuManager->getDC();

      if (!dc)
      {
          return;
      }

      TFTRus* rusPrinter = menuManager->getRusPrinter();

      /* Проверить пришло ли новое сообщение. */
      bool new_flag = SettingsMail.getNewMessageFlag();                             //  Получить признак нового сообщения 
      if (new_flag)                                                                 // если новое сообщение
      {
          SettingsMail.setNewMessageFlag(false);                                    // Сбросить флаг нового сообщения. Программа извещена и приступила к обработке нового сообщения.
          count_message = SettingsMail.getCurrentCountMessage();                    // получить количество всех сообщений
          flipping_count_message = count_message;
          SettingsMail.setFlippingCountMessage(flipping_count_message);             // Установить номер листания на позицию пришедшего сообщения
          View_flipping_count_message = 0;                                          // Номер просмотра переключить в "0"
          drawMessage(menuManager, count_message, View_flipping_count_message);     // вызвать программу отображения информации на дисплее
      }

      //******************** выполнение действий кнопок ******************************

      if (pressed_button_Retention != -1)
      {
          if (pressed_button_Retention == BUTTON_MAIL)
          {
              pressed_button_Retention = -1;
 
              for (int i = 0; i < (Max_Count_Block_Message * Number_of_bytes_block) + 400; i++)
              {
                  MemWrite(i, 0x00);
              }
              MemCommit();
 
              rows_mail.fillSprite(backColor);
              rows_mail.pushToSprite(&back, 1, 29, TFT_BLACK);
              back.pushSprite(0, 0);
              dc->setFreeFont(TFT_FONT);
              String data2 = data_txt2; //Почта удалена
              rusPrinter->print(data2.c_str(), 75, 50, backColor, TFT_YELLOW);  // Отображаем 
              delay(4000);
          }
      }

       else if (released_button != -1)
      {
          if (released_button == BUTTON_MAIL)
          {
              released_button = -1;
              Serial.println("released_button");

              count_message = SettingsMail.getCurrentCountMessage();                 // получить номер текущего сообщения 
              flipping_count_message = SettingsMail.getFlippingCountMessage();       // получить номер листания сообщения 

              /*  вычисляем адрес вызываемого сообщени сообщения. */
              unsigned int cur_adr = (flipping_count_message * Number_of_bytes_block) + Start_Block_Text_ADDRESS - Number_of_bytes_block; // вычисляем адрес вызываемого сообщени сообщения.

              /* определяем было ли отправлено подтверждение прочтения текущего сообщения или нет*/
              confirmation_OK = MemRead(cur_adr + addr_read_NOT_TRANSMITTED);                   // 1 байт - флаг передачи подтверждения "ОК". "1" подтверждение прочтения НЕ ПЕРЕДАНО
              uint8_t  num_receive_in_message = MemRead(cur_adr + addr_number_this_message);    // Получить номер сообщения пришедшего из центра. Листать сообщения максимально ограничиваться этим номером
              uint8_t not_read = SettingsMail.getCoutNotReadMessage();                              // получить показания счетчика не подтвержденного количества сообщений. 
  
              /* проконтролируем в КОМ порту количество неподтвержденных сообщений*/
  
              //----------------------------------------------------------------------------------

              if (confirmation_OK == MESSAGE_CONFIRMED)                                     // Получен специальный код признака "MESSAGE_CONFIRMED" - означает что подтверждение не отправлено
              {
                  char msgOK_Display[60] = "OK->";                                              // Массив для ответного сообщения 
                  char msgOK_Trecker[10] = "|OK";                                               // Формирование строки для ответного сообщения 
                  char msgNum[2] = "";                                                          // массив для записи номера ответного сообщения
                  char msg[60] = "";                                                            // Массив для приема текстовых сообщений
                  char msg_resp[60] = "";                                                       // 
                  char msg_resp_tmp[60] = "";                                                   //

                  itoa(num_receive_in_message, msgNum, 10);                                     // Преобразовать в строку номер сообщения пришедшего из центра

                  /* формируем строку ответа для передачи на треккер */
                  strcat(msgOK_Trecker, msgNum);                                                // Добавили в "|OK" номер ответного сообщения
                  MemReadChars(Response_message_block_ADDRESS, msg_resp, sizeof(msg_resp));     // Получить из памяти уже имеющуюся строку с подтверждеяниями
                  strcat(msg_resp, msgOK_Trecker);                                              // Добавили к текущему ответу новый ответ. Формируем строку с несколькими ответами
                  MemWriteChars(Response_message_block_ADDRESS, msg_resp, sizeof(msg_resp));    // Сохраняем в памяти вновь сформированную строку с подтверждениями о прочтении
                  MemWrite(NEW_CONFIRMATION_MESSAGE, MESSAGE_GENERATED);                         // Сохраняем в памяти признак сформированного нового сообщения

                  /************** формируем строку ответа для вывода на экран ********************/
                  MemReadChars(cur_adr + addr_current_message, msg, sizeof(msg));                // Извлечь пришедшее и записанное сообщение. 
                  /*  Формируем строку о прочтении для вывода на дисплей */
                  strcat(msgOK_Display, msgNum);                                                 // Прибавляем к строке "OK->"+ N (номер входящего сообщения)
                  strcat(msgOK_Display, msg);                                                    // Прибавляем к строке "OK->"+ N + текст входящего сообщения
                  delay(10);
                  MemWriteChars(cur_adr + addr_current_message, msgOK_Display, sizeof(msgOK_Display)); // Записать в память ответное сообщение по текущему адресу для последующего контроля

                  if (not_read > 0)                                                              // Если счетчик неподтвержденных сообщений больше нуля                           
                  {
                      not_read--;                                                                // уменишаем счетчик на один 
                      SettingsMail.setCoutNotReadMessage(not_read);                              // и сохраняем в памяти, обновить показания счетчика не подтвержденного () количества сообщений
                  }

                  MemWrite(cur_adr + addr_read_NOT_TRANSMITTED, MESSAGE_ACKNOWLEDGED);           // устанавливаем флаг ("MESSAGE_ACKNOWLEDGED") о подтверждении прочтения в блок сообщения
                  View_flipping_count_message = count_message - flipping_count_message;
                  drawMessage(menuManager, flipping_count_message, View_flipping_count_message); // вызвать программу отображения информации на дисплее
              }
          }
      }


   static uint32_t tmr = millis();
  
    /* Проверяем наличие новой информации */ 
    if (millis() - tmr > DATA_MEASURE_THRESHOLD)
    {
         tmr = millis();
        int Air_txt_x = 41;              // Расположение текста в формуляре стороннего самолета 
 
        /* Проверяем есть ли данные GPS. Ждем 20 секунд */
        fix = (uint8_t)isValidGNSSFix();                      //Проверяем есть ли данные GPS.

        if(!fix && (settings->mode != SOFTRF_MODE_TXRX_TEST1) && (settings->mode != SOFTRF_MODE_TXRX_TEST2) && (settings->mode != SOFTRF_MODE_TXRX_TEST3)) // Эта проверка не проводится в тестовом режиме
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

            /* Определяем какие пакеты приняты в текущем периоде*/
            /* Определяем минимальную дистанцию между нашим и сторонни самолетом и курс стороннего самолета*/
            int min_distance = 32767;    // Запишем максимальное число для сравнения. Первоначально будем сравнивать
          
            for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
            {
                if (Container[i].addr && (now() - Container[i].timestamp) <= TFT_EXPIRATION_TIME)  // Если есть самолет в базе и подошло время обновления данных
                {
                     isTeam_all[i] = true;     // Сторонние самолеты определены и зарегтстрированы в базе

                    /* вычисляем минимальное значение дистанции для переключения диапазона просмотра */
                    if (Container[i].latitude != 0 && Container[i].longitude != 0) // Расчет возможен если получены координаты стороннего самолета)
                    {
                        if ((int)Container[i].distance < min_distance)//если есть элемент, меньше нашего - делаем его минимальным
                        {
                            min_distance = (int)Container[i].distance;    // Сравниваем дистанции для определения наименшего расстояния
                            index_nearest_aircraft = i;                   // Записываем индекс ближайшего самолета  в базе
                            alient_course0 = Container[i].course;         // Определяем курс в градусах ближайшего чужого самолета
                            alient_speed0 = Container[i].speed;           // Определяем скорость ближайшего чужого самолета alien_speed_tmr[i]
                        }
                    }
                }
                else
                {
                    /* нет данных за длительный период  */
                    isTeam_all[i] = false;     // Сторонние самолеты определены и зарегтстрированы в базе
                }
                esp_task_wdt_reset();
            }


            /* ================ Подпрограмма корректировки вывода формуляров  ================*/

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

            view_alien_count = alien_count();        /* Смотрим сколько сторонних самолетов зафиксировано */

            if (view_alien_count >= 1)
            {
                /* Запишем во временную базу данные по обнаруженным самолетам */

                int base_index_tmp = 0;
                for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
                {
                    if (Container[i].addr)
                    {
                        set_table_alien[base_index_tmp].addr = Container[i].addr;
                        set_table_alien[base_index_tmp].Container_i = i;
                        set_table_alien[base_index_tmp].base_index = base_index_tmp; 
                        set_table_alien[base_index_tmp].altitude = (int)Container[i].altitude;
                        set_table_alien[base_index_tmp].lat = (int)Container[i].latitude;
                        set_table_alien[base_index_tmp].lon = (int)Container[i].longitude;
                        set_table_alien[base_index_tmp].speed = (int)Container[i].speed;
                        set_table_alien[base_index_tmp].signal_source = Container[i].signal_source;
                        set_table_alien[base_index_tmp].heading = Container[i].course;
                        this_alien_altitude[i] = (int)Container[i].altitude;
                        base_index_tmp++;
                    }
                }
                /* Выведем в КОМ порт информацию. Только тля теста */
                /* Смотрим что есть в базе */
                esp_task_wdt_reset();
            }
            else
            {
                /* Нет сторонних самолетов */
                /* Пока не используется */
            }

            /*==================================================================*/

             /* Автоматический выбор диапазона отображения на основании минимальной дистанции от стороннего самолета */
            divider = min_distance * 2.2;
            int divider_num = 1;

            if (min_distance > 10100)
            {
                divider = 32000;  // 16000
                divider_num = 1;
            }
            if (min_distance <= 10100 && min_distance > 5100)
            {
                divider = 21500;  // 10000
                divider_num = 2;
            }
            else if (min_distance <= 5100 && min_distance > 2100)
            {
                divider = 10500;  // 5000
                divider_num = 3;
            }
            else if (min_distance <= 2100 && min_distance > 1050)
            {
                divider = 4340; // 2000
                divider_num = 4;
            }
            else if (min_distance <= 1050 && min_distance > 510)
            {
                divider = 2150;  //1000
                divider_num = 5;
            }
            else if (min_distance <= 510 && min_distance > 210)
            {
                divider = 1100;  //500m
                divider_num = 6;
            }
            else if (min_distance <= 210 && min_distance > 110)
            {
                divider = 440;  // 200 m
                divider_num = 7;
            }
            else if (min_distance <= 110 && min_distance > 50)
            {
                divider = 220;  // 100 m
                divider_num = 8;
            }
            else if (min_distance <= 50)
            {
                divider = 110; // 50 m
                divider_num = 9;
            }
 
           // Получить установки определения уровней предупреждения. Параметры задаются со смартфона и записываются в EEPROM
 
            int alarm_attention_set = settings->alarm_attention;     // Внимание. Параметр - расстояние 
            int alarm_warning_set   = settings->alarm_warning;       // Предупреждение. Параметр - расстояние 
            int alarm_danger_set    = settings->alarm_danger;        // Тревога. Параметр - расстояние        
            int alarm_height_set    = settings->alarm_height;        // Тревога по высоте. Параметр - высота 

            //=========================== Сглаживаем основные показатели скорости и высоты ==================================

            /* Фильтр скорости нашего самолета.    Нужно выяснить откуда этот пример взят Предполагаю, данные со смещением по массиву*/

            Aircraft_speed_filtre[this_speed_array_count] = (int)ThisAircraft.speed;

            int this_val_speed = 0;

            if (this_speed_array_countMax)                                   // формируем данные о величине скорости
            {
                for (int k = 0; k < speed_array_size; k++)
                {
                    this_speed_sum += Aircraft_speed_filtre[this_speed_array_count];
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

            thisAircraft_speed_tmr = this_val_speed;

            /* При малой скорости нашего самолета поворачиваем экран на отметку 360 */
            if (thisAircraft_speed_tmr >= 0 && thisAircraft_speed_tmr < 4)
            {
                ThisAircraft.course = 0;
            }

            angle = (360 - (int)ThisAircraft.course) % 360;            // Курс нашего самолета

            //=================================== Фильтр высоты нашего самолета ====================================

            Aircraft_altitude_tmr[this_altitude_array_count] = (int)ThisAircraft.altitude;

            int this_val_altitude = 0;

            if (this_altitude_array_countMax)                                        // формируем данные о высоте нашего самолета
            {
                for (int k = 0; k < altitude_array_size; k++)
                {
                    this_altitude_sum += Aircraft_altitude_tmr[this_altitude_array_count];
                }
                this_val_altitude = this_altitude_sum / altitude_array_size;
                this_altitude_sum = 0;
            }

            this_altitude_array_count++;
            if (this_altitude_array_count > altitude_array_size - 1)                // проверка заполнения массива первичными данными о скорости
            {
                this_altitude_array_count = 0;
                this_altitude_array_countMax = true;                                //Разрешить выдавать данные о величине скорости
            }

            thisAircraft_altitude_tmr = this_val_altitude;                          // Данные по высоте нашего самолета после фильтра

            //===================================================================================

            for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
            {
                if (Container[i].addr)  // Если есть данные стороннего самолета
                {

                  //=============================== фильтруем показания скорости стороннего самолета ==========================================
 
                     /* Сначала заполняем массив фильтра данными по скорости */

                    alien_speed_filtre[i][alien_speed_array_count[i]] = (int)Container[i].speed; 
                    int alien_val_speed = 0;

                    if (alien_speed_array_countMax[i])                        // формируем данные о величине скорости
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
                    alien_speed_tmr[i] = alien_val_speed;                    // Скорость стороннего самолета после фильтра

                 //================================== Фильтр высоты стороннего самолета =============================================
 
                    if ((int)Container[i].altitude != 0)
                    {
                        alien_altitude_filtre[i][alien_altitude_array_count[i]] = (int)Container[i].altitude;
                    }

                    int alien_val_altitude = 0;

                    if (alien_altitude_array_countMax[i])                            // формируем данные о высоте
                    {
                        for (int k = 0; k < altitude_array_size; k++)
                        {
                            alien_altitude_sum[i] += alien_altitude_filtre[i][k];
                        }
                        alien_val_altitude = alien_altitude_sum[i] / altitude_array_size;
                        alien_altitude_sum[i] = 0;
                    }

                    alien_altitude_array_count[i]++;

                    if (alien_altitude_array_count[i] > altitude_array_size - 1)         // проверка заполнения массива первичными данными высоте
                    {
                        alien_altitude_array_count[i] = 0;
                        alien_altitude_array_countMax[i] = true;                         // Флаг готовности данные о высоте стороннего самолета
                    }

                    if (alien_val_altitude != 0)
                    {
                        alien_altitude_actual[i] = alien_val_altitude;                   // Данные высоты стороннего самолета после фильтра            
                    }

                    /* Устанавливаем ограничение высоты стороннего самолета с применением гистерезиса */
                    int diff_altitude = 5; // Не реагировать если изменение меньше

                    if ((alien_altitude_actual[i] - alien_altitude_old[i] > diff_altitude) || alien_altitude_old[i] - alien_altitude_actual[i] > diff_altitude)
                    {
                        
                        alien_altitude_old[i] = alien_altitude_actual[i];                 // Окончательные данные по высоте стороннего самолета с учетом гистерезиса.
                        alien_altitude_hysteresis[i] = alien_altitude_actual[i];          // Окончательные данные по высоте стороннего самолета с учетом гистерезиса.
                    }
                    //=================================================================================================

                    /* Определяем разность высот между нашим и сторонним самолетом. Нужно для вывода текста в формуляр */
                    int VerticalSet = 0;  // абсолютная величина без учета знака

                    if (alien_altitude_actual[i] != 0)
                    {
                        int RelativeVertical = alien_altitude_hysteresis[i] - thisAircraft_altitude_tmr;   // Определяем разность высот
                     
                        if (RelativeVertical >= 0)
                        {
                            VerticalSet = alien_altitude_hysteresis[i] - thisAircraft_altitude_tmr;        // Данные разности высот со знаком +
                        }
                        else if (RelativeVertical < 0)
                        {
                            VerticalSet = thisAircraft_altitude_tmr - alien_altitude_hysteresis[i];        // Данные разности высот со знаком -
                        }
                    }

 
                    /* Вычисляем разность высот между нашим самолетом и сторонним. Данне со знаком + или -*/
                    height_difference[i] = alien_altitude_hysteresis[i] - thisAircraft_altitude_tmr;

                    //============================  Определение направления стрелок подъем или снижение ==========================================================
                    /*
                    Напоминание: alien_altitude_array_countMax  это флаг готовности данные о высоте стороннего самолета
                    */

                    if (alien_altitude_array_countMax[i] && alien_altitude_hysteresis[i] !=0)  //
                    {
                        if (millis() - tmr_array[i] > 100 + (DATA_MEASURE_THRESHOLD*4))  //Исключить мигание стрелки вверх/вниз. Небходимо немного времени для изменения высоты самолета
                        {
                            tmr_array[i] = millis();

                            /* Напоминание
                                old_alien_altitude_arrow -  Предыдущая высота стороннего самолета для отображения стрелок выше/ниже.
                                alien_altitude_hysteresis - Обработанная высота стороннего самолета
                            */
  
                            if (alien_altitude_hysteresis[i] > old_alien_altitude_arrow[i] && old_alien_altitude_arrow[i] != 0) // При старых нулевых значениях не имеет смысла сравнивать
                            {
                                arrow_up_down[i] = 1;
                            }
                            else if (alien_altitude_hysteresis[i] < old_alien_altitude_arrow[i] && old_alien_altitude_arrow[i] != 0) // При старых нулевых значениях не имеет смысла сравнивать
                            {
                                arrow_up_down[i] = 2;
                            }
                            else
                            {
                               arrow_up_down[i] = 0;
                            }
                            old_alien_altitude_arrow[i] = alien_altitude_hysteresis[i];
                        }
                    }
                    else
                    {
                       arrow_up_down[i] = 0;
                    }

                    //========================== Если координаты стороннего самолета определены ====================================
 
                    if (set_table_alien[i].lat != 0 && set_table_alien[i].lon != 0)   
                    {

                        bearing_tmr[i] = (int)Container[i].bearing;                 // угол в градусах между нашим самолетом и сторонним. Не путать с курсом
                        distance_tmr[i] = (int)Container[i].distance;               // дистанция между нашим самолетом и сторонним

                        // --------------------------------------------------------------------------------
                         /* При малой скорости смотрим в центр экрана на наш самолет. Это означает что самолет не летит (на земле) */
                        if (alien_speed_tmr[i] >= 0 && alien_speed_tmr[i] < 4)
                        {
                            Container[i].course = (180 + bearing_tmr[i]) % 360;
                        }

                        /* курс стороннего самолета с учетом поворота экрана */
                        alient_course[i] = (angle + (int)Container[i].course) % 360;

                        /*Расчет координат сторонних самолетов на неподвижном экране с поправкой на вращение*/
                        new_angle[i] = (angle + bearing_tmr[i]) % 360;

                        /*Функция проверяет и если надо задает новое значение, так чтобы оно была в области допустимых значений, заданной параметрами.*/
                        new_rel_x = constrain(distance_tmr[i] * sin(radians(new_angle[i])), -32768, 32767);
                        new_rel_y = constrain(distance_tmr[i] * cos(radians(new_angle[i])), -32768, 32767);

                        new_x = ((int32_t)new_rel_x * (int32_t)radius) / divider;
                        new_y = ((int32_t)new_rel_y * (int32_t)radius) / divider;

                        Container_alien_X[i] = new_x;  // Сохранить координаты стороннего самолета
                        Container_alien_Y[i] = new_y;

                        /* Расчет координат формуляра стороннего самолета */
                        /* Определяем расположение формуляра на экране слева или справа*/ 

                        if (new_x >= 0)  // Зона правая сторона?
                        {
                            Air_txt_left[i] = false;
                        }
                        else   //Зона левая сторона?
                        {
                            Air_txt_left[i] = true;
                        }

                        if (alient_course[i] >= 90 && alient_course[i] <= 270)
                        {
                         
                            form_x = new_x -23;     // Спрайт текста  находтся ниже самолета
                            form_y = new_y +22;     // 21
                        }
                        else
                        {
                            form_x = new_x -23;    // Спрайт текста  находтся выше самолета
                            form_y = new_y -9;     // -8
                        }
 
                        Container_logbook_X[i] = form_x;  // Сохранить координаты формуляра стороннего самолета
                        Container_logbook_Y[i] = form_y;

                        /* Расчет координат стрелок стороннего самолета */
                        /* Определяем расположение стрелок на экране слева или справа*/
                        if (new_x >= 0)  // Зона правая сторона?
                        {
                            if (alient_course[i] >= 0 && alient_course[i] <= 180)
                            {
                                if (new_y <= -62) //  
                                {
                                    form_arrow_x = new_x - 18; //16 Спрайт стрелки находтся xx от самолета
                                    form_arrow_y = new_y + 4; //
                                }
                                else
                                {
                                    form_arrow_x = new_x - 18; //16 Спрайт стрелки находтся xx от самолета
                                    form_arrow_y = new_y + 4;     //
                                }
                            }
                            else
                            {
                                if (new_y <= -62) //
                                {
                                    form_arrow_x = new_x + 9; //8 Спрайт стрелки  находтся xx самолета
                                    form_arrow_y = new_y + 4; //
                                }
                                else
                                {
                                    form_arrow_x = new_x + 9; //8 Спрайт стрелки  находтся xx самолета
                                    form_arrow_y = new_y + 4;     //
                                }
                            }
                        }
                        else   //Зона левая сторона?
                        {
                            if (alient_course[i] >= 0 && alient_course[i] <= 180)
                            {
                                if (new_y <= -62) // 
                                {
                                    form_arrow_x = new_x - 18; //16 Спрайт стрелки  находтся слева от самолета
                                    form_arrow_y = new_y + 4; //
                                }
                                else
                                {
                                    form_arrow_x = new_x - 18; // 16Спрайт стрелки  находтся слева от самолета
                                    form_arrow_y = new_y + 4; //1
                                }
                            }
                            else
                            {
                                if (new_y <= -62) //
                                {
                                    form_arrow_x = new_x + 9; //8 Спрайт стрелки находтся справа самолета
                                    form_arrow_y = new_y + 4; //
                                }
                                else
                                {
                                    form_arrow_x = new_x + 9; //8 Спрайт стрелки  находтся справа самолета
                                    form_arrow_y = new_y + 4; //1
                                }
                            }
                        }

                        Container_arrow_X[i] = form_arrow_x;  // Сохранить координаты формуляра стороннего самолета
                        Container_arrow_Y[i] = form_arrow_y;

                        esp_task_wdt_reset();

                        /* Определяем цвет текстов предупреждения об опастности */
                        if (min_distance >= alarm_attention_set)   // Чужой самолет очень далеко
                        {
                            little_air_color[i] = TFT_WHITE;    // Цвет для вывода изображения самолетика 
                            txt_color = TFT_GREEN;              // Цвет для вывода текста на табло вверху слева расстояния и курса ближайшего самолетика 
                        }
                        else if (min_distance <= alarm_attention_set && min_distance > alarm_warning_set) // Чужой самолет на расстоянии предупреждения
                        {
                            if (VerticalSet > alarm_height_set)   //  Чужой самолет выше расстояния опасности
                            {
                                little_air_color[i] = TFT_WHITE;
                                txt_color = TFT_GREEN;
                            }
                            else
                            {
                                //  Чужой самолет на расстоянии предупреждения
                                little_air_color[i] = TFT_YELLOW;
                                txt_color = TFT_YELLOW;
                            }
                        }
                        else if (min_distance <= alarm_warning_set && min_distance > alarm_danger_set)   // Чужой самолет на расстоянии предупреждения
                        {
                            if (VerticalSet > alarm_height_set)                                          // Чужой самолет выше расстояния предупреждения
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
                        else if (min_distance <= alarm_danger_set && VerticalSet <= alarm_height_set)  // Чужой самолет на близком расстоянии и по высоте опасен
                        {
                            little_air_color[i] = TFT_RED;
                            txt_color = TFT_RED;
                        }
                        esp_task_wdt_reset();

                        /* Настраиваем вывод текста в формуляр скорость подвижного стороннего самолета */
                        Air_txt_Sprite[i]->fillSprite(backColor);                        // Закрасим поле соообщений 
                       // Air_txt_Sprite[i]->drawSmoothRoundRect(0, 0, 1, 1, 44, 15, TFT_RED); //!! Только для теста
                        Air_txt_Sprite[i]->setTextColor(little_air_color[i], backColor); // Установить цвет согласно программе предупреждения опастности
                        Air_txt_Sprite[i]->setTextDatum(TC_DATUM);                       // Определим как будет выводится текст
                        Air_txt_Sprite[i]->loadFont(NotoSansBold15);                     // Установить шрифт формуляра


                        /*===============  Этот фрагмент для вывода самолета в движении*/
                        /* Запись параметров высоты в формуляр */
                        if (height_difference[i] > 0) // Чужой самолет выше нашего
                        {
                            Air_txt_Sprite[i]->drawString("+" + String(int(height_difference[i])), 22, 1, 0);
                        }
                        else
                        {
                            // Чужой самолет ниже нашего
                            Air_txt_Sprite[i]->drawString(String(int(height_difference[i])), 22, 1, 0);
                        }

                        /* Записать скорость в формуляр  движущегося самолета*/
                        alien_speed_view[i] = 50 - ((int)Container[i].speed / 60 * 3); // Расстояние стороннего самолета для вывода на дисплей
                        //alien_speed_view[i] = 50 - (alien_speed_tmr[i]/17); // Скорость стороннего самолета для вывода на дисплей
                        if (alien_speed_view[i] > 40)
                        {
                            alien_speed_view[i] = 40;
                        }

                        /*  
                            Действительно для самолетов с известными координатами.
                            Определяем наличие и направление вывода стрелок.
                            Источник данных не важен
                        */

                        //arrow_up_down[i] = 1;

                            switch (arrow_up_down[i])
                            {
                            case 0:
                                arrow[i]->fillSprite(backColor);                      // Закрасим поле стрелок вверх
                                break;
                            case 1:
                                /*Рисуем стрелку вверх */
                                arrow[i]->fillSprite(backColor);                       // Закрасим поле стрелок вверх
                                arrow[i]->drawLine(4, 0, 4, 9, little_air_color[i]);   // |
                                arrow[i]->drawLine(0, 4, 3, 1, little_air_color[i]);   // /
                                arrow[i]->drawLine(5, 1, 8, 4, little_air_color[i]);   // 
                                break;
                            case 2:
                                /*Рисуем стрелку вниз */
                                arrow[i]->fillSprite(backColor);                       // Закрасим поле стрелок вниз
                                arrow[i]->drawLine(4, 0, 4, 9, little_air_color[i]);   // |
                                arrow[i]->drawLine(0, 5, 3, 8, little_air_color[i]);   // 
                                arrow[i]->drawLine(5, 8, 8, 5, little_air_color[i]);   //
                                break;
                            default:
                                break;
                            }
 
                            // Формируем изображение летящего объекта с учетом с какого источника были полученыданные о координатах

                        if (set_table_alien[i].signal_source == 1)  // С учетом данных, полученных с приемника DUMP1090
                        {
                            /*Рисуем маленький самолетик */
                            little_airplane[i]->fillSprite(TFT_BLACK);      // Закрасим поле самолетика

                            little_airplane[i]->drawLine(49, 44, 49, 57, little_air_color[i]);
                            little_airplane[i]->drawLine(50, 43, 50, 57, little_air_color[i]);
                            little_airplane[i]->drawLine(51, 43, 51, 57, little_air_color[i]);

                            little_airplane[i]->drawLine(46, 47, 54, 47, little_air_color[i]);
                            little_airplane[i]->drawLine(44, 48, 56, 48, little_air_color[i]);
                            little_airplane[i]->drawLine(42, 49, 58, 49, little_air_color[i]);

                            little_airplane[i]->drawLine(46, 56, 54, 56, little_air_color[i]);
                            little_airplane[i]->drawLine(45, 57, 55, 57, little_air_color[i]);

                            little_airplane[i]->drawLine(50, 41, 50, alien_speed_view[i]-4, little_air_color[i]); // Рисуем прямую линию"скорости" с носа самолета

                            area_airplane[i]->fillSprite(TFT_BLACK);      // Закрасим поле 
                           // area_airplane[i]->drawSmoothRoundRect(0, 0, 1, 1, 99, 99, TFT_YELLOW); //!! Только для теста
                        }

                        else if (set_table_alien[i].signal_source == 2) // С учетом данных, полученных с приемника LoRa868
                        {
                            /*Рисуем маленький самолетик в виде закрашенного круга */
                            LoRa_airplane[i]->fillSprite(TFT_BLACK);                                          // Закрасим поле самолетика
                            LoRa_airplane[i]->fillCircle(50, 50, 5, little_air_color[i]),                     // fillCircle //drawCircle
                            LoRa_airplane[i]->drawLine(50,45,50, alien_speed_view[i]-4, little_air_color[i]); // Рисуем прямую линию"скорости" с носа самолета
                            area_airplane[i]->fillSprite(TFT_BLACK);                                          // Закрасим поле 

                           // area_airplane[i]->drawSmoothRoundRect(0, 0, 1, 1, 99, 99, TFT_YELLOW); //!! Только для теста
                           // LoRa_airplane[i]->drawSmoothRoundRect(0, 0, 1, 1, 99, 99, TFT_PINK); //!! Только для теста
                        }

                        esp_task_wdt_reset();

                    } //Закочить обработку данных самолетов с известными координатами

                    //============================= Конец обработки данных самолетов самолетов  с известными координатами ==========================


                    /* Настраиваем вывод текста в формуляр  слева на экране скорость и высоту стороннего самолета без координат
                       Действительно только для данных, полученных без координат. Выводим слева на экране
                     */

                    if (set_table_alien[i].lat == 0 && set_table_alien[i].lon == 0 && set_table_alien[i].altitude != 0 && set_table_alien[i].signal_source == 1 && set_table_alien[i].signal_source != 2)
                    {
                        /* Определение цвета вывода текста */
                        /*Напоминание: VerticalSet - Абсолютная величина разности высот*/
                        /*alien_altitude_hysteresis[i] - thisAircraft_altitude_tmr;        // Данные разности высот со знаком +
                          height_difference[i] = alien_altitude_hysteresis[i] - thisAircraft_altitude_tmr; 
                        */
 
                        if (VerticalSet >= alarm_height_set)      //  Чужой самолет дальше расстояния информирования
                        {
                            DUMP1090_air_color[i] = TFT_WHITE;
                        }
                        else if (VerticalSet < alarm_height_set)   //  Чужой самолет ближе расстояния информирования 
                        {

                            if (height_difference[i] > 0) // Чужой самолет выше нашего
                            {
                                if (arrow_up_down[i] == 1)
                                {
                                    DUMP1090_air_color[i] = TFT_ORANGE;
                                }
                                else if (arrow_up_down[i] == 2)
                                {
                                    DUMP1090_air_color[i] = TFT_RED;
                                }
                                else
                                {
                                    DUMP1090_air_color[i] = TFT_YELLOW;
                                }
                            }
                            else
                            {
                                // Чужой самолет ниже нашего
                                if (arrow_up_down[i] == 2)
                                {
                                    DUMP1090_air_color[i] = TFT_ORANGE;
                                }
                                else if (arrow_up_down[i] == 1)
                                {
                                    DUMP1090_air_color[i] = TFT_RED;
                                }
                                else
                                {
                                    DUMP1090_air_color[i] = TFT_YELLOW;
                                }
                            }
                        }

                        DUMP1090_Sprite[i]->fillSprite(backColor);                           // Закрасим поле соообщений
                        DUMP1090_Sprite[i]->setTextColor(DUMP1090_air_color[i], backColor);  // Установить цвет согласно программе предупреждения опастности
                        DUMP1090_Sprite[i]->setTextDatum(CC_DATUM);                          // Определим как будет выводится текст
                        DUMP1090_Sprite[i]->loadFont(NotoSansBold15);                        // Установить шрифт формуляра

                        /* Формируем изображения стрелок для боковых сообщений*/
                            switch (arrow_up_down[i])
                            {
                            case 0:
                                arrow[i]->fillSprite(backColor);             // Закрасим поле стрелок вверх
                                arrow[i]->pushToSprite(DUMP1090_Sprite[i], 4, 5, TFT_BLACK);
                                break;
                            case 1:
                                //Рисуем стрелку вверх 
                                arrow[i]->fillSprite(backColor);                  // Закрасим поле стрелок вверх
                                arrow[i]->drawLine(4, 0, 4, 9, DUMP1090_air_color[i]);  // |
                                arrow[i]->drawLine(0, 4, 3, 1, DUMP1090_air_color[i]);   // /
                                arrow[i]->drawLine(5, 1, 8, 4, DUMP1090_air_color[i]);   // 
                                arrow[i]->pushToSprite(DUMP1090_Sprite[i], 4, 5, TFT_BLACK);
                                break;
                            case 2:
                                //Рисуем стрелку вниз 
                                arrow[i]->fillSprite(backColor);             // Закрасим поле стрелок вниз
                                arrow[i]->drawLine(4, 0, 4, 9, DUMP1090_air_color[i]);   // |
                                arrow[i]->drawLine(0, 5, 3, 8, DUMP1090_air_color[i]);   // 
                                arrow[i]->drawLine(5, 8, 8, 5, DUMP1090_air_color[i]);   //
                                arrow[i]->pushToSprite(DUMP1090_Sprite[i], 4, 5, TFT_BLACK);
                                break;
                            default:
                                arrow[i]->fillSprite(backColor);             // Закрасим поле стрелок вверх
                                arrow[i]->pushToSprite(DUMP1090_Sprite[i], 4, 5, TFT_BLACK);
                                break;
                            }

                    //==================================================================================================


                        dump1090_speed[i] = int(set_table_alien[i].speed);                  // Установить скорость стороннего самолета
                        dump1090_info_txt[i] = String(int(height_difference[i]));      
                        dump1090_info_txt[i] += "/";

                        if (dump1090_speed[i] > 0)
                        {
                            dump1090_info_txt[i] += String(dump1090_speed[i]);
                        }
                        else
                        {
                            dump1090_info_txt[i] += "----";
                        }

                        if (height_difference[i] > 0) // Чужой самолет выше нашего
                        {
                            DUMP1090_Sprite[i]->drawString("+" + dump1090_info_txt[i], 55, 12, 1);
                        }
                        else
                        {
                            // Чужой самолет ниже нашего
                            DUMP1090_Sprite[i]->drawString(dump1090_info_txt[i], 55, 12, 1);
                        }
                        dump1090_info_txt[i] = "\0";
                    }

                }
            }


          //============================== Формируем неподвижное базовое изображение на экране =========================================== 

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

                  /*Рисуем малый серый круг*/
            backsprite.drawCircle(cx, 160, 80, TFT_DARKGREY);

            ///* Вычисляем направление полета нашего самолета*/
            if (ThisAircraft.latitude != Aircraft_latitude_old)
            {
                test_curse = bearing_calc(Aircraft_latitude_old, Aircraft_longitude_old, ThisAircraft.latitude, ThisAircraft.longitude);

                Aircraft_latitude_old = ThisAircraft.latitude;
                Aircraft_longitude_old = ThisAircraft.longitude;
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
                  }

                data_KM.pushToSprite(&back, 120, 225, TFT_BLACK); // Выводим надпись дистанции внизу малого круга
            }



            /* Рисум градусы азимута*/
            /*  отображаем на табло вверху курс стороннего самолета в градусах*/
            esp_task_wdt_reset();
            data_az.loadFont(NotoSansMonoSCB20);
            data_az.setTextDatum(CR_DATUM);
            data_az.fillSprite(TFT_BLACK);
            data_az.drawRect(0, 0, 50, 25, TFT_DARKGREY);
            data_az.setTextColor(txt_color, backColor);

            /*  отображаем на табло вверху скорость стороннего самолета в км/час*/
            data_speed.loadFont(NotoSansMonoSCB20);
            data_speed.setTextDatum(CR_DATUM);
            data_speed.fillSprite(TFT_BLACK);
            data_speed.drawRect(0, 0, 95, 25, TFT_DARKGREY);
            data_speed.setTextColor(txt_color, backColor);

            mail.loadFont(NotoSansMonoSCB20);
            mail.setTextDatum(CR_DATUM);
            mail.fillSprite(TFT_BLACK);
            mail.drawRoundRect(0, 0, 40, 25, 3, TFT_DARKGREY);//drawRoundRect
            mail.drawLine(2, 2, 20, 20, TFT_DARKGREY);
            mail.drawLine(20, 20, 38, 2, TFT_DARKGREY);
            mail.drawLine(2, 23, 12, 13, TFT_DARKGREY);
            mail.drawLine(28, 13, 38, 23, TFT_DARKGREY);
            mail.setTextColor(txt_color, backColor);
 
            /*Рисуем заряд аккумулятора*/
            power1.fillSprite(TFT_BLACK);
            power1.fillRect(2, 2, 26, 12, TFT_GREEN);
            power1.drawRect(0, 0, 30, 16, TFT_WHITE);
            power1.fillRect(30, 4, 3, 8, TFT_WHITE);
            power1.pushToSprite(&back, 285, 4, TFT_BLACK);

            /* настройки сообщения о дистанции вверху слева*/
            dist_info.loadFont(NotoSansMonoSCB20);
            dist_info.fillSprite(TFT_BLACK);
            dist_info.setTextDatum(TC_DATUM);
            String min_distance_txt = String(min_distance);

            // Вывод информации почты 
            for (int i = 0; i < 3; i++)
            {
                //rows[i]->drawRect(0, 0, 319, 14, TFT_DARKGREY);
                //rows[i]->pushToSprite(&back, 1, 29 + (16 * i), TFT_BLACK);
            }
 
            if (min_distance <= 5)
            {
                min_distance_txt = "0";
            }
            else
            {
                int len = min_distance_txt.length();
                min_distance_txt.setCharAt(len - 1, '0');
            }

            if (min_distance != 32767)
            {
                dist_info.drawRect(0, 0, 80, 25, TFT_DARKGREY);
                dist_info.setTextColor(txt_color, backColor);
                dist_info.drawString(min_distance_txt + " m", 40, 4);
                data_az.drawCircle(42, 8, 3, txt_color);                                     // Рисуем кружок символа градуса 
                data_az.drawString(String(alient_course0), 37, 14);                          // Рисуем курс стороннего самолета
                data_speed.drawString(String(alient_speed0) + " km/h", txt_loc_speed, 14);   // Рисуем скорость стороннего самолета
            }
            else
            {
                dist_info.drawRect(0, 0, 80, 25, TFT_DARKGREY);
                dist_info.setTextColor(TFT_GREEN, backColor);
                dist_info.drawString("-----", 40, 4);                    // Нет данных о дистанции
                data_az.drawString("----", 43, 14);                      // Нет курса стороннего самолета
                data_speed.drawString("----", txt_loc_speed-17, 14);     // Нет курса стороннего самолета
            }


            if (mail_on) // Обработка входящих писем
            {
                mail_on = false;




            }
            else
            {

            }

            esp_task_wdt_reset();

            dist_info.pushToSprite(&back, 1, 1);        // Отображаем табло дистанции стороннего самолета
            data_az.pushToSprite(&back, 185, 1);        // Отображаем табло курса стороннего самолета
            data_speed.pushToSprite(&back, 85, 1);      // Отображаем табло скорости стороннего самолета
            mail.pushToSprite(&back, 240, 1);           // Отображаем значок письма

            /*Формируем картинку нашего самолета*/
                /* Рисуем фюзеляж*/
            Airplane.drawLine(12, 0, 12, 18, TFT_DARKGREY);

            /*Рисуем передние крылья*/
            Airplane.drawLine(3, 7, 20, 7, TFT_DARKGREY);
            Airplane.drawLine(0, 8, 23, 8, TFT_DARKGREY);

            /*Рисуем задние крылья*/
            Airplane.drawLine(7, 17, 17, 17, TFT_DARKGREY);
            Airplane.pushToSprite(&back, 148, 150, TFT_BLACK);
              
            rows_mail.pushToSprite(&back, 1, 29, TFT_BLACK);
            //rows[0]->pushToSprite(&back, 1, 29 + (18 * 0), TFT_BLACK);

            //============================== Конец формирования неподвижного базовоо изображения на экране =========================================== 

             /*отображаем спрайт формуляра с информацией по объектам*/
            for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
            {

                if (isTeam_all[i] == true)
                {
                    bool screen_side = Air_txt_left[index_nearest_aircraft]; // Определяем сторону экрана с учетом индекса ближайшего стороннего самолета
 
                    /*Сначала выведем сообщения от DUMP1090 слева*/
                    if (set_table_alien[i].lat == 0 && set_table_alien[i].lon == 0)
                    {

                         DUMP1090_Sprite[i]->drawSmoothRoundRect(0, 0, 1, 1, 99, 19, TFT_DARKCYAN);

                        if (set_table_alien[i].altitude != 0)  // Отобразить информацию если есть данные по высоте
                        {
                            /* Определяем место вывода сообщений от DUMP1090  справа или слева в зависимости от вывода стороннего самолета*/
                            if (!screen_side)
                            {
                                DUMP1090_Sprite[i]->pushToSprite(&back, 1, 80 + (22 * i), TFT_BLACK);
                            }
                            else
                            {
                                DUMP1090_Sprite[i]->pushToSprite(&back, 220, 80 + (22 * i), TFT_BLACK);
                            }
                        }
                    }

                    /* Затем формуляры движущихся сторонних самолетов*/
                    if (set_table_alien[i].lat != 0 && set_table_alien[i].lon != 0)
                    {
   
                        if (set_table_alien[i].signal_source == 1 && set_table_alien[i].signal_source != 2) // 
                        {
                            little_airplane[i]->pushRotated(area_airplane[i], alient_course[i], TFT_BLACK); // 
                        }
                        else if (set_table_alien[i].signal_source == 2 && set_table_alien[i].signal_source != 1)
                        {
                            LoRa_airplane[i]->pushRotated(area_airplane[i], alient_course[i], TFT_BLACK);
                        }

                        /*Отобразить после вывода формуляров слева*/
                        area_airplane[i]->pushToSprite(&back, radar_center_x + Container_alien_X[i] - 50, radar_center_y - Container_alien_Y[i] - 50, TFT_BLACK);
                        arrow[i]->pushToSprite(&back, radar_center_x + Container_arrow_X[i], radar_center_y - Container_arrow_Y[i], TFT_BLACK);
                        Air_txt_Sprite[i]->pushToSprite(&back, radar_center_x + Container_logbook_X[i], radar_center_y - Container_logbook_Y[i], TFT_BLACK);

                    }
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
     const char EPD_SoftRF_text6[] = "(C) 2024";


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
    dist_info.setTextColor(TFT_GREEN, backColor);
    dist_info.drawString("-----", 40, 4);
    dist_info.pushToSprite(&back, 1, 0);


         /* Рисум градусы азимута*/
    /*  отображаем на табло курс стороннего самолета в градусах*/

    data_az.loadFont(NotoSansMonoSCB20);
    data_az.setTextDatum(CR_DATUM);
    data_az.fillSprite(TFT_BLACK);
    data_az.drawRect(0, 0, 50, 25, TFT_DARKGREY);
    data_az.drawCircle(40, 8, 3, TFT_GREEN);    // Рисуем кружок символа градуса
    data_az.setTextColor(TFT_GREEN, backColor);
    data_az.drawString("----", 43, 14);
    data_az.pushToSprite(&back, 185, 0);

    data_speed.loadFont(NotoSansMonoSCB20);
    data_speed.setTextDatum(CR_DATUM);
    data_speed.fillSprite(TFT_BLACK);
    data_speed.drawRect(0, 0, 95, 25, TFT_DARKGREY);
    data_speed.setTextColor(TFT_GREEN, backColor);
    data_speed.drawString("----", txt_loc_speed - 17, 14);
    data_speed.pushToSprite(&back, 85, 0);

    mail.loadFont(NotoSansMonoSCB20);
    mail.setTextDatum(CR_DATUM);
    mail.fillSprite(TFT_BLACK);
    mail.drawRoundRect(0, 0, 40, 25, 3, TFT_DARKGREY);//drawRoundRect
    mail.drawLine(2, 2, 20, 20, TFT_DARKGREY);
    mail.drawLine(20, 20, 38, 2, TFT_DARKGREY);
    mail.drawLine(2, 23, 12, 13, TFT_DARKGREY);
    mail.drawLine(28, 13, 38, 23, TFT_DARKGREY);
    mail.pushToSprite(&back, 240, 0);

            /*Рисуем заряд аккумулятора*/
    power1.fillSprite(TFT_BLACK);
    power1.fillRect(2, 2, 26, 12, TFT_GREEN);
    power1.drawRect(0, 0, 30, 16, TFT_WHITE);
    power1.fillRect(30, 4, 3, 8, TFT_WHITE);
    power1.pushToSprite(&back, 285, 4, TFT_BLACK);

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

    if (!fix && (settings->mode != SOFTRF_MODE_TXRX_TEST1) && (settings->mode != SOFTRF_MODE_TXRX_TEST2) && (settings->mode != SOFTRF_MODE_TXRX_TEST3))
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
 
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------

 
 void TFTMenuScreen::drawMessage(TFTMenu* menuManager, int count_message, int count_view)
 {
     TFT_Class* tft_radar = menuManager->getDC();
     if (!tft_radar)
     {
         return;
     }
     TFTRus* rusPrinter = menuManager->getRusPrinter();

     /*
     - Вычисляем адрес сообщения в памяти, применяя номер сообщения
     - Записать в массив msg сообщение, хранящееся в памяти
     - Записать в массив time_msg дату и время из сообщения, хранящееся в памяти
     - Извлекаем флаг 1 байт - флаг передачи подтверждения "ОК". "1" подтверждение прочтения НЕ ПЕРЕДАНО
     - Получить показания счетчика не прочитанного количества сообщений
     - Получить признак нового сообщения
     - получить номер листания сообщения для отображения на дисплее
     - Выводим на дисплей с первой позиции сообщение
     - Выводим на дисплей на центр 3 строки дату и время, полученные из сообщения.
     - Выводим на дисплей номер текущего сообщения
     - Выводим на дисплей символ, обозначающий, было передано подтверждение или нет
     - Выводим на дисплей количество не подтвержденных о прочтении сообщений
     - Если есть неподтвержденные сообщения. выводим значок почтового конверта

     */

     //LCDScreen->resetIdleTimer();                                                    // сбрасываем таймер ничегонеделанья

     unsigned int cur_adr = (count_message * Number_of_bytes_block) + Start_Block_Text_ADDRESS/* - Number_of_bytes_block*/; //  получить  адрес текущего сообщения.
     MemReadChars(cur_adr + addr_current_message, msg, sizeof(msg));                 // Считать из памяти записанное сообщение.
     MemReadChars(cur_adr + addr_time_this_message, time_msg, sizeof(time_msg));     // Считать из памяти время соббщения в память по текущему адресу 
     uint8_t  confirmation_OK = MemRead(cur_adr + addr_read_NOT_TRANSMITTED);        // 1 байт - флаг передачи подтверждения "ОК". "1" подтверждение прочтения НЕ ПЕРЕДАНО
     uint8_t not_read = SettingsMail.getCoutNotReadMessage();                        // получить показания счетчика не подтвержденного количества сообщений
     Serial.println(msg);
     //    /*Вывести на дисплей сообщение*/
  


     char str_count_message[128];
     sprintf(str_count_message, "%d %s ", count_message, msg);
     int n = 0;
     rows_mail.loadFont(FONT_18);
     rows_mail.fillSprite(backColor);
     rows_mail.setTextColor(TFT_WHITE, backColor);
     rows_mail.setTextDatum(TL_DATUM);
    // rows_mail.drawRect(0, 0, 319, 14, TFT_DARKGREY);
     rows_mail.drawString(str_count_message, 0, 0);
  


     // 
     //dc->clear();                                                                    // Стереть экран
     //dc->setCursor(0, 0);                                                            // Установить курсор в начало экрана
     //dc->print(msg);                                                                 // Отобразить новое сообщение

     //dc->setCursor(5, 3);                                                            // Установить курсор для вывода времени сообщения
     //dc->print(time_msg);                                                            // Отобразить время сообщения

     /* отобразить состояние подтверждения прочтения сообщения*/

     char str[1];
     int cursorNum = 0;
 //    dc->setCursor(cursorNum, 3);                                                   // Установить курсор в начало экрана на нижней строке
     if (count_view < 10)
     {
         itoa(count_view, str, 10);                                                  // Преобразуем номер текущего сообщения в строку
        // dc->print(str);                                                            // Отображаем в первой позиции номер сообщения.Выводим на дисплей номер текущего сообщения
     }
     else
     {
        // dc->print("X");                                                           // Если количество больше 10, выводим символ "Х" для этономии знакомест.
     }

     if (confirmation_OK == MESSAGE_CONFIRMED)                                 // Для данного сообщения подтверждение о прочтении не передано
     {
        // dc->print("*");                                                           // Выводим на дисплей символ, обозначающий, было передано подтверждение или нет
     }

     /*  Отобразить количество не прочитанных сообщений */
     //!!if (not_read != 0)                                                           // not_read = показания счетчика не прочитанного количества сообщений
     //{
     //  //  dc->setCursor(cursorNum + 2, 3);                                         // Установить курсор 

     //    if (not_read < 10)
     //    {
     //        itoa(not_read, str, 10);                                            // Записать в строку количество не подтвержденных о прочтении сообщений
     //       // dc->print(str);                                                     // Отобразить количество не подтвержденных о прочтении сообщений
     //    }
     //    else
     //    {
     //      //  dc->print("X");                                                     // Если количество больше 10, выводим символ "Х" для этономии знакомест.

     //    }

     //    /*Отображаем значок конверта*/

     //}
 
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------



 void TFTMenuScreen::saveVer(String ver)
 {
     Current_version = ver;
 }

 String TFTMenuScreen::getVer()
 {
     return Current_version;
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
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 // Вывод направления движения
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------


 float TFTMenuScreen::bearing_calc(float lat, float lon, float lat2, float lon2)
 {

     float teta1  = radians(lat);
     float teta2  = radians(lat2);
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

     int val_voltage = 0;
 
     return val_voltage;                                 //Напряжение питания аккумулятора
 }

 int TFTMenuScreen::alien_count()
 {
     int count = 0;

     for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) 
     {
         if (Container[i].addr) 
         {
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

     tft_radar->setFreeFont(TFT_FONT);
     int textFontHeight = FONT_HEIGHT(tft_radar);

     String data = data_txt; //"ОПРЕДЕЛЕНИЕ"
     String data1 = data_txt1; //"МЕСТОПОЛОЖЕНИЯ"
     int textFontWidth = tft_radar->textWidth(data, 2);                   // Returns pixel width of string in current font

     uint16_t curX = (screenWidth / 2) - (textFontWidth / 2) - 12;        // Координаты вывода 
     uint16_t curY = 110;                                                 // Координаты вывода текста
     rusPrinter->print(data.c_str(), curX, curY, backColor, TFT_YELLOW);  // Отображаем 

     textFontWidth = tft_radar->textWidth(data1, 2);                      // Returns pixel width of string in current font
 
     curX = (screenWidth / 2) - (textFontWidth / 2) - 18;                  // Координаты вывода 
     curY = 140;                                                           // Координаты вывода текста
     rusPrinter->print(data1.c_str(), curX, curY, backColor, TFT_YELLOW);  // Отображаем 
 }

 void TFTMenuScreen::ParsePacket(const byte* packet, int packetSize)
 {
     if (packetSize < sizeof(ToArduino))
     {
         //SerialOutput.print("PACKET TOO SMALL: ");
         //SerialOutput.println(packetSize);
         return;
     }

     ToArduino receivedPacket;
     memcpy(&receivedPacket, packet, packetSize);

     fo.addr = receivedPacket.addr;
     fo.latitude = receivedPacket.lat;
     fo.longitude = receivedPacket.lon;
     fo.altitude = receivedPacket.altitude;
     fo.speed = receivedPacket.speed;
     fo.signal_source = receivedPacket.signal_source;
     fo.timestamp = now(); // 
     fo.seen = receivedPacket.seen;
     fo.course = receivedPacket.track;
     fo.pSignal = receivedPacket.pSignal;
 
     /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
     if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
     {
         Traffic_Update(&fo);   // 
     }

     /* Остальные параметры записываем в базу */
     Traffic_Add(&fo);
 }

 void TFTMenuScreen::Receive1090()
 {
     static byte buff[1024] = { 0 };
     static int bytesReceived = 0;
     static int writeIndex = 0;
     static byte endOfPacketCounter = 0;

     while (Serial.available())
     {
         byte ch = (byte)Serial.read();

         buff[writeIndex++] = ch;
         bytesReceived++;

         if (writeIndex >= sizeof(buff))
         {
             writeIndex = 0;
             bytesReceived = 0;
             memset(buff, 0, sizeof(buff));
         }
         else
         {
             if (ch == 0xFF)
             {
                 if (++endOfPacketCounter >= 3)
                 {
                    ParsePacket(buff, bytesReceived);
                    memset(buff, 0, sizeof(buff));
                    writeIndex = 0;
                    bytesReceived = 0;
                 }
             }
             else
             {
                 endOfPacketCounter = 0;
             }
         }
     }
 }


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_TFT_MODULE
