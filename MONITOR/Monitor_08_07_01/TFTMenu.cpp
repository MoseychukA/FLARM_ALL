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
#include <stdio.h>
#include "Memory.h"               // Работа с энергонезависимой памятью
#include "CoreCommandBuffer.h"
#include "Module1090.h"

#ifdef USE_TFT_MODULE

#include "TFTMenu.h"

/* Спрайты вывода изображений и информации на экран дисплея */

// create a new sprite
TFT_eSPI tft = TFT_eSPI();

TFT_eSprite back_screen   = TFT_eSprite(&tft);      // Спрайт фона
TFT_eSprite backsprite    = TFT_eSprite(&tft);      // Спрайт отображения вращающегося поля воздушной обстановки
TFT_eSprite rows_mail     = TFT_eSprite(&tft);      // Спрайт отображения текстов почтового ящика
TFT_eSprite DUMP1090_info = TFT_eSprite(&tft);      // Этот спрайт, площадка в котором будет располагатся информация о приеме данных с приемника 1090
TFT_eSprite time_info     = TFT_eSprite(&tft);      // Этот спрайт, площадка в котором будет располагатся информация о времени с GPS
//TFT_eSprite rssi_info     = TFT_eSprite(&tft);      // Спрайт окна информации уровня принимаемого сигнала LoRa

TFT_eSprite* arrow[MAX_TRACKING_OBJECTS];           // Спрайт отображения стрелки

TFT_eSprite* Air_txt_Sprite[MAX_TRACKING_OBJECTS];  // Этот спрайт, площадка в котором будет располагатся формуляр стороннего самолета
TFT_eSprite* little_airplane[MAX_TRACKING_OBJECTS]; // Этот спрайт, площадка в котором будет располагатся изображение стороннего самолета DUMP1090
TFT_eSprite* msg_airplane[MAX_TRACKING_OBJECTS];    // Этот спрайт, площадка в котором будет располагатся изображение стороннего самолета с текстовой строки
TFT_eSprite* LoRa_airplane[MAX_TRACKING_OBJECTS];   // Этот спрайт, площадка в котором будет располагатся изображение стороннего самолета c LoRa
TFT_eSprite* area_airplane[MAX_TRACKING_OBJECTS];   // Этот спрайт, площадка в котором будет располагатся спрайт little_airplane стороннего самолета
TFT_eSprite* rssi_info[MAX_TRACKING_OBJECTS];        // Спрайт окна информации уровня принимаемого сигнала LoRa

int alien_altitude_old[MAX_TRACKING_OBJECTS];       // Предыдущее значение высоты стороннего самолета. Нужно для вычисления высоты с учетом гистерезиса
int alien_altitude_actual[MAX_TRACKING_OBJECTS];    // Высота стороннего самолета. Нужно для вычисления высоты с учетом гистерезиса
int this_alien_altitude[MAX_TRACKING_OBJECTS];      // Высота стороннего самолета. Нужно для вычисления 
int old_alien_altitude_arrow[MAX_TRACKING_OBJECTS]; // Предыдущая высота стороннего самолета для отображения стрелок выше/ниже.
int alien_altitude_hysteresis[MAX_TRACKING_OBJECTS];// Обработанная высота стороннего самолета после применения гистерезиса
int height_difference[MAX_TRACKING_OBJECTS];        // Разность высот нашего и стороннего самолета
int alien_speed_tmr[MAX_TRACKING_OBJECTS];          // Скорость стороннего самолета
int alien_speed_view[MAX_TRACKING_OBJECTS];         // Скорость стороннего самолета для вывода на дисплей в виде линии
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
#define backColor   0x0026

bool isTeam_all[MAX_TRACKING_OBJECTS]    = { false };              // Удалить данные по самолету
//bool isAirDel =  true;                                           // Удалить данные по самолетам полученные по сообщению

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
 // flags.isLCDOn = true;
  switchTo = NULL;
  switchToIndex = -1;
  tftDC = NULL;
  on_action = NULL;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::setup()
{

    /* IC 480(RGB)* 320,*/
   // int rot = 3;
    int dRot = 1;
  
    tftDC = new TFT_eSPI();

    tftDC->init();
    tftDC->setRotation(dRot);
    tftDC->fillScreen(TFT_BLACK);

    tftDC->setTextColor(TFT_RED, TFT_BLACK);

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
    btn->begin(BUTTON_MAIL, true, RETENTION_INTERVAL);
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

void TFTMenu::onButtonisDoubleClicked(int button)
{
    if (currentScreenIndex == -1)
        return;

    resetIdleTimer();
    TFTScreenInfo* currentScreenInfo = &(screens[currentScreenIndex]);
    currentScreenInfo->screen->onButtonisDoubleClicked(this, button);
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
 

        if (hardwareButtons[i]->isDoubleClicked())
        {
            // кликнута кнопка на пине pin
            uint8_t pin = hardwareButtons[i]->pinNumber();

            Serial.println("pressed_button_DoubleClicked");
            // // тут можно вызывать событие для дочернего экрана, например, событие "кнопка нажата и отпущена"
            //button_ret_Flag = SettingsMain.GetButtonRetention(); // проверить нет ли длительного нажаимя кнопок isRetention()
            onButtonisDoubleClicked(pin);
            //if (!button_ret_Flag)
            //{
            //    onButtonisDoubleClicked(pin);
            //}
            //else
            //{
            //    SettingsMain.SetButtonRetention(false);
            //}
        }
        else if (hardwareButtons[i]->isPressed())
        {
            // кликнута кнопка на пине pin
            uint8_t pin = hardwareButtons[i]->pinNumber();

            Serial.println("pressed_button_isPressed");
            // тут можно вызывать событие для дочернего экрана, например, событие "кнопка нажата"
            onButtonPressed(pin);
            // подробнее по состояниям кнопки см. CoreButton.h
        }
        else if (hardwareButtons[i]->isClicked())
        {
            // кликнута кнопка на пине pin
            uint8_t pin = hardwareButtons[i]->pinNumber();
            Serial.println("pressed_button_isClicked");
            // тут можно вызывать событие для дочернего экрана, например, событие "кнопка нажата и отпущена"
            button_ret_Flag = SettingsMain.GetButtonRetention(); // проверить нет ли длительного нажаимя кнопок isRetention()

            if (!button_ret_Flag)
            {
                onButtonReleased(pin);
            }
            else
            {
                SettingsMain.SetButtonRetention(false);
            }
        }
        
        if (hardwareButtons[i]->isRetention())
        {
            // кликнута кнопка на пине pin
            uint8_t pin = hardwareButtons[i]->pinNumber();
           // Serial.println("pressed_button_isRetention");
            // тут можно вызывать событие для дочернего экрана, например, событие "кнопка нажата и отпущена"
            SettingsMain.SetButtonRetention(true);     // установить флаг длительного нажатия кнопки
            onButtonisRetention(pin);
        }

        //if (hardwareButtons[i]->isDoubleClicked())
        //{
        //    // кликнута кнопка на пине pin
        //    uint8_t pin = hardwareButtons[i]->pinNumber();

        //   // Serial.println("pressed_button_DoubleClicked");
        //    // // тут можно вызывать событие для дочернего экрана, например, событие "кнопка нажата и отпущена"
        //    //button_ret_Flag = SettingsMain.GetButtonRetention(); // проверить нет ли длительного нажаимя кнопок isRetention()
        //    onButtonisDoubleClicked(pin);
        //    //if (!button_ret_Flag)
        //    //{
        //    //    onButtonisDoubleClicked(pin);
        //    //}
        //    //else
        //    //{
        //    //    SettingsMain.SetButtonRetention(false);
        //    //}
        //}
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
 
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTMenuScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

 TFTMenuScreen* MainScreen = NULL;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTMenuScreen::TFTMenuScreen()
 {
    flags.MailOn = false;
    MainScreen = this;
    flashing_on_off = true;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTMenuScreen::~TFTMenuScreen()
 {
     pressed_button_Retention = -1;
     pressed_button_DoubleClicked = -1;
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
     pressed_button_DoubleClicked = -1;
     Allow_flashing = false;  // Запретить мигание сообщения
     flags.MailOn   = false;  // Флаг удаления сообщения
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
       Air_txt_Sprite[i]->createSprite(55, 15);
       Air_txt_Sprite[i]->setPivot(27, 7);
 
       arrow[i] = new TFT_eSprite(&tft);              // Спрайт информации стороннего воздушного объекта
       arrow[i]->createSprite(10, 10);                // Спрайт отображения стрелка вверх/вниз

       little_airplane [i] = new TFT_eSprite(&tft);   // Спрайт информации стороннего воздушного объекта
       little_airplane [i]->createSprite(100, 100);
       little_airplane[i]->setPivot(50,50); 

       LoRa_airplane[i] = new TFT_eSprite(&tft);     // Спрайт информации стороннего воздушного объекта LoRa
       LoRa_airplane[i]->createSprite(100, 100);
       LoRa_airplane[i]->setPivot(50, 50);

       msg_airplane[i] = new TFT_eSprite(&tft);   // Спрайт информации стороннего воздушного объекта
       msg_airplane[i]->createSprite(100, 100);
       msg_airplane[i]->setPivot(50, 50);

       area_airplane[i] = new TFT_eSprite(&tft);      // Этот спрайт, площадка в котором будет располагатся сторонний самолет
       area_airplane[i]->createSprite(100, 100);
       area_airplane[i]->setPivot(50, 50);

       rssi_info[i] = new TFT_eSprite(&tft);     // Спрайт информации RSSI LoRa стороннего воздушного объекта
       rssi_info[i]->createSprite(100, 20);
       rssi_info[i]->setPivot(50, 10);
       rssi_info[i]->loadFont(NotoSansMonoSCB20);
       rssi_info[i]->setTextDatum(TC_DATUM);
       rssi_info[i]->setTextColor(TFT_WHITE, TFT_BLACK);


       alien_speed_array_countMax[i] = false;
       alien_speed_sum[i] = 0;
       alien_speed_array_count[i] = 0;

       alien_altitude_array_countMax[i] = false;
       alien_altitude_sum[i] = 0;
       alien_altitude_array_count[i] = 0;

       old_alien_altitude_arrow[i] = 0;                // Массив хранения предыдущих значений высоты, для формирования стрелок направления перемещения самолета вврх/вниз

       tmr_array[i] = millis();
   }
 
    back_screen.createSprite(480, 320);
    time_info.createSprite(60, 25);
    backsprite.createSprite(320, 320);
    backsprite.loadFont(NotoSansMonoSCB20);          // Загружаем шрифты символов направления света
    backsprite.setSwapBytes(true);
    backsprite.setTextColor(TFT_WHITE, TFT_BLACK);
    backsprite.setTextDatum(4);

 /*   rssi_info.createSprite(80, 25);
    rssi_info.loadFont(NotoSansMonoSCB20);
    rssi_info.setTextDatum(TC_DATUM);
    rssi_info.setTextColor(TFT_WHITE, TFT_BLACK);*/

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
    // Serial.println("LCDMainScreen::onButtonPressed..");
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::onButtonReleased(TFTMenu* menuManager, int buttonID)
 {
     released_button = buttonID;
    // Serial.println("LCDMainScreen::onButtonReleased..");
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::onButtonisRetention(TFTMenu* menuManager, int buttonID)
 {
    // Serial.println("LCDMainScreen::onButtonisRetention..");
     pressed_button_Retention = buttonID;
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::onButtonisDoubleClicked(TFTMenu* menuManager, int buttonID)
 {
    // Serial.println("LCDMainScreen::onDoubleClicked..");
     pressed_button_DoubleClicked = buttonID;
 }

 //----------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::update(TFTMenu* menuManager)
 {


    //-------------------- Блок работы с нопкой  -----------------------------------------
    //******************** выполнение действий кнопок ******************************

    if (pressed_button_DoubleClicked != -1) // 
    {
        if (pressed_button_DoubleClicked == BUTTON_MAIL)
        {
            pressed_button_DoubleClicked = -1;
           // Serial.println("pressed_button_DoubleClicked");
            //clearMail(menuManager); // Удалить всю почту
        }
    }
    else if (released_button != -1) // Короткое нажатие кнопки очищаем текущее сообщение и отправляем подтверждение о прочтении
    {
        if (released_button == BUTTON_MAIL)
        {
            released_button = -1;
           // Serial.println("pressed_button_released");
            Allow_flashing = false;                                                // Запретить мигание сообщения
            flags.MailOn = true;                                                   // ??отсчет времени удаления сообщения через 10 минут

            for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
            {
                Container_msg[i] = EmptyFO;
            }
            count_message = SettingsMain.getCurrentCountMessage();                 // получить номер текущего сообщения 
            flipping_count_message = SettingsMain.getFlippingCountMessage();       // получить номер листания сообщения 

            /*  вычисляем адрес вызываемого сообщени сообщения. */
            unsigned int cur_adr = (count_message * Number_of_bytes_block) + Start_Block_Text_ADDRESS; //  получить  адрес текущего сообщения.


            /* определяем было ли отправлено подтверждение прочтения текущего сообщения или нет*/
            confirmation_OK = MemRead(cur_adr + addr_read_NOT_TRANSMITTED);                       // 1 байт - флаг передачи подтверждения "ОК". "1" подтверждение прочтения НЕ ПЕРЕДАНО
            uint8_t  num_receive_in_message = MemRead(cur_adr + addr_number_this_message);        // Получить номер сообщения пришедшего из центра. Листать сообщения максимально ограничиваться этим номером
            uint8_t not_read = SettingsMain.getCoutNotReadMessage();                              // получить показания счетчика не подтвержденного количества сообщений. 

            /* проконтролируем в КОМ порту количество неподтвержденных сообщений*/

            //----------------------------------------------------------------------------------

            if (confirmation_OK == MESSAGE_CONFIRMED)                                         // Получен специальный код признака "MESSAGE_CONFIRMED" - означает что подтверждение не отправлено. Нужно отправить
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
                // MemReadChars(Response_message_block_ADDRESS, msg_resp, sizeof(msg_resp));     // Получить из памяти уже имеющуюся строку с подтверждеяниями
                strcat(msg_resp, msgOK_Trecker);                                              // Добавили к текущему ответу новый ответ. Формируем строку с несколькими ответами
                // MemWriteChars(Response_message_block_ADDRESS, msg_resp, sizeof(msg_resp));    // Сохраняем в памяти вновь сформированную строку с подтверждениями о прочтении
                MemWrite(NEW_CONFIRMATION_MESSAGE, MESSAGE_GENERATED);                         // Сохраняем в памяти признак сформированного нового сообщения
                SERIAL_TRACKER.println(msg_resp);                                              // Передать подтерждение о прочтении сообщения в буфер треккера

                MemWrite(cur_adr + addr_read_NOT_TRANSMITTED, MESSAGE_ACKNOWLEDGED);           // устанавливаем флаг ("MESSAGE_ACKNOWLEDGED") о подтверждении прочтения в блок сообщения
                rows_mail.fillSprite(TFT_BLACK);                                               // Удаляем сообщение с экрана
                rows_mail.pushToSprite(&back_screen, 0, 0, TFT_BLACK);                                // Удаляем сообщение с экрана
                rows_mail.deleteSprite(); // Удаляем сообщение с экрана
            }
        }
    }

   
    if (pressed_button_Retention != -1) // Долгое нажатие кнопки - очищаем все сообщения и настройки
    {
        if (pressed_button_Retention == BUTTON_MAIL)
        {
            pressed_button_Retention = -1;
        // Serial.println("pressed_button_Retention");
            clearMail(menuManager); // Удалить всю почту
        }
    }


  //============================== Конец блока работы с текстовыми сообщениями ===============================================================


  //============================== Основной блок вывода воздушной обстановки на экран ========================================================
     static uint32_t tmr = millis();

     /* Проверяем наличие новой информации */
     if (millis() - tmr > DATA_MEASURE_THRESHOLD)
     {
         tmr = millis();
         int Air_txt_x = 41;              // Расположение текста в формуляре стороннего самолета 

         /* Проверяем есть ли данные GPS. Ждем 20000 миллисекунд */
         fix = (uint8_t)isValidGNSSFix();                      //Проверяем есть ли данные GPS.

         if (!fix && (settings->mode != SOFTRF_MODE_TXRX_TEST1) && (settings->mode != SOFTRF_MODE_TXRX_TEST2) && (settings->mode != SOFTRF_MODE_TXRX_TEST3) && (settings->mode != SOFTRF_MODE_TXRX_TEST4) && (settings->mode != SOFTRF_MODE_TXRX_TEST5)) // Эта проверка не проводится в тестовом режиме
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


            /* =================  Сначала зафиксируем положение нашего самолета =================================*/

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
             if (thisAircraft_speed_tmr >= 0 && thisAircraft_speed_tmr < 5)
             {
                 ThisAircraft.course = 0;
             }

             angle = (360 - (int)ThisAircraft.course) % 360;                   // Курс нашего самолета

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

             //===================================== Закончили ввод данных нашего самолета ==============================================


             /* ======================= Определим наличие сторонних самолетов и отобразим их на экране ==============================*/

             view_alien_count = alien_count();        // Смотрим сколько сторонних самолетов зафиксировано в базе


             if (view_alien_count >= 1)
             {
                  esp_task_wdt_reset();

                 /* Определяем какие пакеты приняты в текущем периоде*/
                 /* Определяем минимальную дистанцию между нашим и сторонни самолетом и курс стороннего самолета*/
                 //unsigned int min_distance = 32767;    // Запишем максимальное число для сравнения. Первоначально будем сравнивать
                 unsigned int min_distance = 65534;      // Запишем максимальное число для сравнения. Первоначально будем сравнивать

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
                                 alient_course0 = Container[i].course;         // Записываем курс в градусах ближайшего чужого самолета
                                 alient_speed0 = Container[i].speed;           // Записываем скорость ближайшего чужого самолета alien_speed_tmr[i]
                             }
                         }

                     }
                     else
                     {
                         /* нет данных по сторонним самолетам за длительный период  */

                         isTeam_all[i] = false;     // Сторонние самолеты определены и зарегтстрированы в базе?
                     }

                     esp_task_wdt_reset();
                 }
                /* isAirDel = true;*/
                 /*==================================================================*/

                  /* Автоматический выбор диапазона отображения на основании минимальной дистанции от стороннего самолета */
                // divider = min_distance * 2.2;

                 min_distance = min_distance / 2;

                 if (min_distance > 8000)
                 {
                     divider = 32000;  // 32000
                     divider_num = 1;
                 }
                 else if (min_distance <= 8000 && min_distance > 4000)  //16000/2
                 {
                     divider = 16000;  // 16000
                     divider_num = 2;
                 }
                 else if (min_distance <= 4000 && min_distance > 2000) // 8000 /2
                 {
                     divider = 8000;  // 8000
                     divider_num = 3;
                 }
                 else if (min_distance <= 2000 && min_distance > 1000) // 4000/2
                 {
                     divider = 4000; // 4000
                     divider_num = 4;
                 }
                 else if (min_distance <= 1000 && min_distance > 500) // 2000/2
                 {
                     divider = 2000;  //2000
                     divider_num = 5;
                 }
                 else if (min_distance <= 500 && min_distance > 250) //1000 /2
                 {
                     divider = 1000;  //1000m
                     divider_num = 6;
                 }
                 else if (min_distance <= 250 && min_distance > 100) // 500/2
                 {
                     divider = 500;  // 500 m
                     divider_num = 7;
                 }
                 else if (min_distance <= 100 && min_distance > 50) //200/2
                 {
                     divider = 200;  // 200 m
                     divider_num = 8;
                 }
                 else if (min_distance <= 50)   // 100/2
                 {
                     divider = 100; // 100 m
                     divider_num = 9;
                 }

 

                 // Установки определения уровней предупреждения. Параметры задаются со смартфона и записываются в EEPROM

                 // settings->alarm_attention;     // Внимание. Параметр - расстояние 
                 // settings->alarm_warning;       // Предупреждение. Параметр - расстояние 
                 // settings->alarm_danger;        // Тревога. Параметр - расстояние        
                 // settings->alarm_height;        // Тревога по высоте. Параметр - высота 


                 //===================================================================================

                 uint8_t rssi_view_count = 0;

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

                         if (alien_altitude_array_countMax[i] && alien_altitude_hysteresis[i] != 0)  //
                         {
                             if (millis() - tmr_array[i] > 100 + (DATA_MEASURE_THRESHOLD * 4))  //Исключить мигание стрелки вверх/вниз. Небходимо немного времени для изменения высоты самолета
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

                         if (Container[i].latitude != 0.0 && Container[i].longitude != 0.0)
                         {
                             // --------------------------------------------------------------------------------
                              /* При малой скорости смотрим в центр экрана на наш самолет. Это означает что самолет не летит (на земле) */
                             if (alien_speed_tmr[i] >= 0 && alien_speed_tmr[i] < 5)
                             {
                                 Container[i].course = (180 + (int)Container[i].bearing) % 360;
                             }

                             /* курс стороннего самолета с учетом поворота экрана */
                             alient_course[i] = (angle + (int)Container[i].course) % 360;

                             /*Расчет координат сторонних самолетов на неподвижном экране с поправкой на вращение*/
                             new_angle[i] = (angle + (int)Container[i].bearing) % 360;

                             /*Функция проверяет и если надо задает новое значение, так чтобы оно была в области допустимых значений, заданной параметрами.*/
                             new_rel_x = constrain(((int)Container[i].distance/2 )* sin(radians(new_angle[i])), -32768, 32767);
                             new_rel_y = constrain(((int)Container[i].distance/2) * cos(radians(new_angle[i])), -32768, 32767);

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

                                 form_x = new_x - 23;     // Спрайт текста  находтся ниже самолета
                                 form_y = new_y + 22;     // 21
                             }
                             else
                             {
                                 form_x = new_x - 23;    // Спрайт текста  находтся выше самолета
                                 form_y = new_y - 9;     // -8
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
                             if (min_distance >= settings->alarm_attention)   // Чужой самолет очень далеко
                             {
                                 little_air_color[i] = TFT_WHITE;    // Цвет для вывода изображения самолетика 
                                 txt_color = TFT_GREEN;              // Цвет для вывода текста на табло вверху слева расстояния и курса ближайшего самолетика 
                             }
                             else if (min_distance <= settings->alarm_attention && min_distance > settings->alarm_warning) // Чужой самолет на расстоянии предупреждения
                             {
                                 if (VerticalSet > settings->alarm_height)   //  Чужой самолет выше расстояния опасности
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
                             else if (min_distance <= settings->alarm_warning && min_distance > settings->alarm_danger)   // Чужой самолет на расстоянии предупреждения
                             {
                                 if (VerticalSet > settings->alarm_height)                                                // Чужой самолет выше расстояния предупреждения
                                 {
                                     little_air_color[i] = TFT_WHITE;
                                     txt_color = TFT_GREEN;
                                 }
                                 else if (VerticalSet <= settings->alarm_height)
                                 {
                                     // Чужой самолет на расстоянии предупреждения
                                     little_air_color[i] = TFT_ORANGE;
                                     txt_color = TFT_ORANGE;
                                 }
                             }
                             else if (min_distance <= settings->alarm_danger && VerticalSet <= settings->alarm_height)  // Чужой самолет на близком расстоянии и по высоте опасен
                             {
                                 little_air_color[i] = TFT_RED;
                                 txt_color = TFT_RED;
                             }
                             esp_task_wdt_reset();

                             /* Настраиваем вывод текста в формуляр скорость подвижного стороннего самолета */
                             Air_txt_Sprite[i]->fillSprite(TFT_BLACK);                        // Закрасим поле соообщений 
                             //Air_txt_Sprite[i]->drawSmoothRoundRect(0, 0, 1, 1, 99, 14, TFT_RED); //!! Только для теста
                             Air_txt_Sprite[i]->setTextColor(little_air_color[i], backColor); // Установить цвет согласно программе предупреждения опастности
                             Air_txt_Sprite[i]->setTextDatum(TC_DATUM);                       // Определим как будет выводится текст
                             Air_txt_Sprite[i]->loadFont(NotoSansBold15);                     // Установить шрифт формуляра


                             /*===============  Этот фрагмент для вывода самолета в движении*/
                             /* Запись параметров высоты в формуляр */
                             if (height_difference[i] >= 0)                    // Чужой самолет выше нашего
                             {
                                 if (Container[i].signal_source == 2)
                                 {
                                     Air_txt_Sprite[i]->drawString("+" + String(int(height_difference[i])), 27, 1, 0);
                                 }
                                 else
                                 {
                                     Air_txt_Sprite[i]->drawString("+" + String(int(height_difference[i])), 27, 1, 0);
                                 }
                             }
                             else
                             {
                                 // Чужой самолет ниже нашего

                                 if (Container[i].signal_source == 2)
                                 {
                                     Air_txt_Sprite[i]->drawString(String(int(height_difference[i])), 27, 1, 0);
                                 }
                                 else
                                 {
                                     Air_txt_Sprite[i]->drawString(String(int(height_difference[i])), 27, 1, 0);
                                 }
                             }

                             /* Записать скорость в формуляр  движущегося самолета*/
                             alien_speed_view[i] = 50 - ((int)Container[i].speed / 60 * 3);  // Расстояние стороннего самолета для вывода на дисплей
                             //alien_speed_view[i] = 50 - (alien_speed_tmr[i]/17);           // Скорость стороннего самолета для вывода на дисплей
                             if (alien_speed_view[i] > 40)
                             {
                                 alien_speed_view[i] = 40;
                             }

                             /*
                                 Действительно для самолетов с известными координатами.
                                 Определяем наличие и направление вывода стрелок.
                                 Источник данных не важен
                             */

                             switch (arrow_up_down[i])
                             {
                             case 0:
                                 arrow[i]->fillSprite(TFT_BLACK);                      // Закрасим поле стрелок вверх
                                 break;
                             case 1:
                                 /*Рисуем стрелку вверх */
                                 arrow[i]->fillSprite(TFT_BLACK);                       // Закрасим поле стрелок вверх
                                 arrow[i]->drawLine(4, 0, 4, 9, little_air_color[i]);   // |
                                 arrow[i]->drawLine(0, 4, 3, 1, little_air_color[i]);   // /
                                 arrow[i]->drawLine(5, 1, 8, 4, little_air_color[i]);   // 
                                 break;
                             case 2:
                                 /*Рисуем стрелку вниз */
                                 arrow[i]->fillSprite(TFT_BLACK);                       // Закрасим поле стрелок вниз
                                 arrow[i]->drawLine(4, 0, 4, 9, little_air_color[i]);   // |
                                 arrow[i]->drawLine(0, 5, 3, 8, little_air_color[i]);   // 
                                 arrow[i]->drawLine(5, 8, 8, 5, little_air_color[i]);   //
                                 break;
                             default:
                                 break;
                             }

                             // Формируем изображение летящего объекта с учетом с какого источника были полученыданные о координатах
                             if (Container[i].signal_source == 0) // С учетом данных, полученных с приемника LoRa868
                             {
                                 /*Рисуем маленький самолетик в виде закрашенного круга */
                                 LoRa_airplane[i]->fillSprite(TFT_BLACK);                                          // Закрасим поле самолетика
                                 LoRa_airplane[i]->fillCircle(50, 50, 5, little_air_color[i]),                     // fillCircle //drawCircle
                                 LoRa_airplane[i]->drawLine(50, 45, 50, alien_speed_view[i] - 4, little_air_color[i]); // Рисуем прямую линию"скорости" с носа самолета
                                 area_airplane[i]->fillSprite(TFT_BLACK);                                          // Закрасим поле 

                                // area_airplane[i]->drawSmoothRoundRect(0, 0, 1, 1, 99, 99, TFT_YELLOW); //!! Только для теста
                                // LoRa_airplane[i]->drawSmoothRoundRect(0, 0, 1, 1, 99, 99, TFT_PINK); //!! Только для теста
                             }
                             else if (Container[i].signal_source == 1)  // С учетом данных, полученных с приемника DUMP1090
                             {
                                 /*Рисуем маленький самолетик */
                                 little_airplane[i]->fillSprite(TFT_BLACK);      // Закрасим поле самолетика

                                 little_airplane[i]->drawLine(49, 44, 49, 57, little_air_color[i]);
                                 little_airplane[i]->drawLine(50, 43, 50, 57, little_air_color[i]);
                                 little_airplane[i]->drawLine(51, 44, 51, 57, little_air_color[i]);

                                 little_airplane[i]->drawLine(46, 47, 54, 47, little_air_color[i]);
                                 little_airplane[i]->drawLine(44, 48, 56, 48, little_air_color[i]);
                                 little_airplane[i]->drawLine(42, 49, 58, 49, little_air_color[i]);

                                 little_airplane[i]->drawLine(46, 56, 54, 56, little_air_color[i]);
                                 little_airplane[i]->drawLine(45, 57, 55, 57, little_air_color[i]);

                                 little_airplane[i]->drawLine(50, 41, 50, alien_speed_view[i] - 4, little_air_color[i]); // Рисуем прямую линию"скорости" с носа самолета
                                 area_airplane[i]->fillSprite(TFT_BLACK);      // Закрасим поле 
                                // area_airplane[i]->drawSmoothRoundRect(0, 0, 1, 1, 99, 99, TFT_YELLOW); //!! Только для теста
                             }
                             else if (Container[i].signal_source == 2)  // С учетом данных, полученных с текстовой строки
                             {
                                 /*Рисуем маленький самолетик */
                                 msg_airplane[i]->fillSprite(TFT_BLACK);      // Закрасим поле самолетика
                                // little_airplane[i]->drawCircle(50, 50, 11, little_air_color[i]);                        // Рисуем кружок вокруг самолета
                                 msg_airplane[i]->drawLine(49, 44, 49, 57, little_air_color[i]);
                                 msg_airplane[i]->drawLine(50, 43, 50, 57, little_air_color[i]);
                                 msg_airplane[i]->drawLine(51, 44, 51, 57, little_air_color[i]);

                                 msg_airplane[i]->drawLine(46, 47, 54, 47, little_air_color[i]);
                                 msg_airplane[i]->drawLine(44, 48, 56, 48, little_air_color[i]);
                                 msg_airplane[i]->drawLine(42, 49, 58, 49, little_air_color[i]);

                                 msg_airplane[i]->drawLine(46, 56, 54, 56, little_air_color[i]);
                                 msg_airplane[i]->drawLine(45, 57, 55, 57, little_air_color[i]);

                                 msg_airplane[i]->drawLine(50, 41, 50, alien_speed_view[i] - 4, little_air_color[i]); // Рисуем прямую линию"скорости" с носа самолета
                                 area_airplane[i]->fillSprite(TFT_BLACK);      // Закрасим поле 

                                // area_airplane[i]->drawSmoothRoundRect(0, 0, 1, 1, 99, 99, TFT_YELLOW); //!! Только для теста
                             }


                             esp_task_wdt_reset();

                             if (Container[i].rssi < 0 && Container[i].addr && Container[i].signal_source == 0)
                             {
                                 rssi_info[i]->drawSmoothRoundRect(0, 0, 1, 1, 99, 19, TFT_DARKCYAN);
                                 rssi_info[i]->fillSprite(TFT_BLACK);
                                 rssi_info[i]->drawRect(0, 0, 80, 25, TFT_DARKGREY);
                                 rssi_adr = String(Container[i].addr);
                                 rssi_txt = String(Container[i].rssi);
                                 rssi_info[i]->drawString(rssi_adr+" "+rssi_txt + " db", 55, 12, 1);

                                 if (settings->rssi_view == VIEW_RSSI_ON)
                                 {
                                     rssi_info[i]->pushToSprite(&back_screen, 1, 1 + (22 * i), TFT_BLACK);         // Отображаем табло уровня сигнала LoRa
                                 }

                                 rssi_view_count = 0;
                             }

                         } //Закочить обработку данных самолетов с известными координатами

                         //============================= Конец обработки данных самолетов самолетов  с известными координатами ==========================
                     }

                    /* if (Container[i].rssi < 0 && Container[i].addr && Container[i].signal_source == 0)
                     {

                         rssi_info.fillSprite(TFT_BLACK);
                         rssi_info.drawRect(0, 0, 80, 25, TFT_DARKGREY);
                         rssi_txt = String(Container[i].rssi);
                         rssi_info.drawString(rssi_txt + " db", 40, 4); 
                         Serial.print("RSSI ");
                         Serial.println(Container[i].rssi);
                         rssi_view_count = 0;
                     }*/
                     rssi_view_count++;
                     Serial.println(rssi_view_count);
                     if(rssi_view_count > MAX_TRACKING_OBJECTS-2)
                     {
                        /* rssi_info.fillSprite(TFT_BLACK);
                         rssi_info.drawRect(0, 0, 80, 25, TFT_DARKGREY);
                         rssi_info.drawString("--- db", 40, 4);*/
                     }

                 }

             }



             //============================== Формируем неподвижное базовое изображение на экране =========================================== 

             back_screen.fillSprite(backColor);            // Закрасим поле 
             back_screen.setPivot(320, 160);               // Назначаем центр вращения спрайта воздушной обстановки
             backsprite.fillSprite(TFT_BLACK);             // 
             backsprite.setPivot(160, 160);                // Назначаем центр вращения спрайта воздушной обстановки


            // time_info.
             /* настройки сообщения о дистанции вверху слева*/
             time_info.loadFont(NotoSansMonoSCB20);
             time_info.fillSprite(TFT_BLACK);
             time_info.setTextDatum(TC_DATUM);
             time_info.drawRect(0, 0, 59, 24, TFT_DARKGREY);
             time_info.setTextColor(TFT_GREEN, backColor);

 
             if (gnss.time.isValid()) //Проверяем есть ли данные GPS.
             {
                 String hour_tmp;

                 if (gnss.time.hour() < 10)
                 {
                     hour_tmp = "0";
                     hour_tmp +=  String (gnss.time.hour());
                 }
                 else
                 {
                     hour_tmp =  String (gnss.time.hour());
                 }
                 String min_tmp;
                 if (gnss.time.minute() < 10)
                 {
                     min_tmp = "0";
                     min_tmp +=  String (gnss.time.minute());
                 }
                 else
                 {
                     min_tmp =  String (gnss.time.minute());
                 }

                 time_info.drawString(hour_tmp +":"+ min_tmp, 29, 4);
             }
             else
             {
                 time_info.drawString("--:--", 29, 4);
             }
   
             time_info.pushToSprite(&back_screen, 259+160, 1);        // Отображаем табло времени

             //if (settings->rssi_view == VIEW_RSSI_ON)
             //{
             //    rssi_info.pushToSprite(&back_screen, 1, 1);         // Отображаем табло уровня сигнала LoRa
             //}

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
             backsprite.pushRotated(&back_screen, angle, TFT_BLACK);

             if (divider <= 32767)
             {
                 back_screen.loadFont(NotoSansBold15);
                 back_screen.setTextColor(TFT_DARKGREY, TFT_BLACK);
                 back_screen.setTextDatum(TC_DATUM);
                 int data_KM_x = 166+160;  // Расположение строки по X
                 int data_KM_y = 223;  // Расположение строки по Y

                 switch (divider_num)
                 {
                 case 1:
                     back_screen.drawString("32000 m", data_KM_x - 3, data_KM_y);
                     break;
                 case 2:
                     back_screen.drawString("16000 m", data_KM_x - 3, data_KM_y);
                     break;
                 case 3:
                     back_screen.drawString("8000 m", data_KM_x - 2, data_KM_y);
                     break;
                 case 4:
                     back_screen.drawString("4000 m", data_KM_x - 2, data_KM_y);
                     break;
                 case 5:
                     back_screen.drawString("2000 m", data_KM_x - 2, data_KM_y);
                     break;
                 case 6:
                     back_screen.drawString("1000 m", data_KM_x - 1, data_KM_y);
                     break;
                 case 7:
                     back_screen.drawString("500 m", data_KM_x - 1, data_KM_y);
                     break;
                 case 8:
                     back_screen.drawString("200 m", data_KM_x - 1, data_KM_y);
                     break;
                 case 9:
                     back_screen.drawString("100 m", data_KM_x, data_KM_y);
                     break;
                 default:
                     back_screen.drawString("32000 m", data_KM_x - 3, data_KM_y);
                     break;
                     // выполняется, если не выбрана ни одна альтернатива
                 }
             }

             esp_task_wdt_reset();

             /*Формируем картинку нашего самолета*/
                 /* Рисуем фюзеляж*/
             int width_air = 148+160;
             int height_air = 150;
             back_screen.drawLine(12 + width_air, 0 + height_air, 12 + width_air, 18 + height_air, TFT_DARKGREY);

             /*Рисуем передние крылья*/
             back_screen.drawLine(3 + width_air, 7 + height_air, 20 + width_air, 7 + height_air, TFT_DARKGREY);
             back_screen.drawLine(0 + width_air, 8 + height_air, 23 + width_air, 8 + height_air, TFT_DARKGREY);

             /*Рисуем задние крылья*/
             back_screen.drawLine(7 + width_air, 17 + height_air, 17 + width_air, 17 + height_air, TFT_DARKGREY);
             //============================== Конец формирования неподвижного базовоо изображения на экране =========================================== 

             /*отображаем спрайт формуляра с информацией по объектам*/
             for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
             {

                 if (isTeam_all[i] == true)
                 {
      
                     /* Формуляры движущихся сторонних самолетов*/
                     if (Container[i].latitude != 0.0 && Container[i].longitude != 0.0)
                     {

                         switch (Container[i].signal_source)
                         {
                         case 0:
                             LoRa_airplane[i]->pushRotated(area_airplane[i], alient_course[i], TFT_BLACK);
                             break;
                         case 1:
                             little_airplane[i]->pushRotated(area_airplane[i], alient_course[i], TFT_BLACK); // 
                             break;
                         case 2:
                             msg_airplane[i]->pushRotated(area_airplane[i], alient_course[i], TFT_BLACK); // 
                             break;
                         default:
                             LoRa_airplane[i]->pushRotated(area_airplane[i], alient_course[i], TFT_BLACK);
                             break;
                         }

                         /*Отобразить после вывода формуляров*/
                         area_airplane[i]->pushToSprite(&back_screen,160 + radar_center_x + Container_alien_X[i] - 50, radar_center_y - Container_alien_Y[i] - 50, TFT_BLACK);
                         arrow[i]->pushToSprite(&back_screen,160+ radar_center_x + Container_arrow_X[i], radar_center_y - Container_arrow_Y[i], TFT_BLACK);
                         Air_txt_Sprite[i]->pushToSprite(&back_screen,160 + radar_center_x + Container_logbook_X[i], radar_center_y - Container_logbook_Y[i], TFT_BLACK);
                     }

  
                     isTeam_all[i] = false;
                     esp_task_wdt_reset();
                 }
             }

             bool DUMP1090_tmp = moduleDump1090.getNewDUMP_0_Flag();
             if (DUMP1090_tmp)
             {

                 DUMP1090_info.createSprite(30, 30);
                 DUMP1090_info.setPivot(15, 15);
                 DUMP1090_info.fillSprite(TFT_BLACK);      // Закрасим поле самолетика

                 DUMP1090_info.drawLine(12, 6, 12, 19, TFT_WHITE);
                 DUMP1090_info.drawLine(13, 5, 13, 19, TFT_WHITE);
                 DUMP1090_info.drawLine(14, 6, 14, 19, TFT_WHITE);

                 DUMP1090_info.drawLine(9, 9, 17, 9, TFT_WHITE);
                 DUMP1090_info.drawLine(7, 10, 19, 10, TFT_WHITE);
                 DUMP1090_info.drawLine(5, 11, 21, 11, TFT_WHITE);

                 DUMP1090_info.drawLine(9, 18, 17, 18, TFT_WHITE);
                 DUMP1090_info.drawLine(8, 19, 18, 19, TFT_WHITE);

                 DUMP1090_info.drawCircle(13, 13, 11, TFT_DARKGREY);

                 DUMP1090_info.pushToSprite(&back_screen, 228, 0, TFT_BLACK);

                 static uint32_t tmr_DUMP1090 = millis();

                 /* Проверяем наличие новой информации */
                 if (millis() - tmr_DUMP1090 > DUMP_FLASHING_OFF)
                 {
                     tmr_DUMP1090 = millis();
                     moduleDump1090.setNewDUMP_0_Flag(false);
                 }
             }


             //============================== Блок работы с текстовыми сообщениями ===============================================================
 
                /* Проверить пришло ли новое сообщение. */
             bool new_flag = SettingsMain.getNewMessageFlag();                             // Получить признак нового сообщения 

             if (new_flag)                                                                 // если новое сообщение
             {
                flags.MailOn = true;                                                     // Начинаем отсчет времени удаления сообщения через 10 минут
                SettingsMain.setNewMessageFlag(false);                                    // Сбросить флаг нового сообщения. Программа извещена и приступила к обработке нового сообщения.
                count_message = SettingsMain.getCurrentCountMessage();                    // получить количество всех сообщений
                TFTScreen->resetIdleTimer();
                //flipping_count_message = count_message;
                //SettingsMain.setFlippingCountMessage(flipping_count_message);           // Установить номер листания на позицию пришедшего сообщения
                View_flipping_count_message = 0;                                          // Номер просмотра переключить в "0"
                Allow_flashing = true;                                                    // Разрешить мигание сообщения
                drawMessage(menuManager, count_message, View_flipping_count_message);     // вызвать программу отображения информации на дисплее
             }
  
             previousMillis_msg = flashing_on_off ? DATA_FLASHING_ON : DATA_FLASHING_OFF;
             if (Allow_flashing)
             {
                 static uint32_t tmr_flashing = millis();
                 if (millis() - tmr_flashing >= previousMillis_msg)
                 {
                     tmr_flashing = millis();
                     if (flashing_on_off == true)
                     {
                         flashing_on_off = false;
                         rows_mail.fillSprite(TFT_BLACK);
                         rows_mail.pushToSprite(&back_screen, 0, 0, TFT_BLACK);
                         rows_mail.deleteSprite();
                     }
                     else
                     {
                         flashing_on_off = true;
                         drawMessage(menuManager, count_message, View_flipping_count_message);     // вызвать программу отображения информации на дисплее
                     }
                 }

                 if (flashing_on_off)
                 {
                    drawMessage(menuManager, count_message, View_flipping_count_message);     // вызвать программу отображения информации на дисплее
                 }
             }

             if (CommandHandler.clear_mail)
             {
                 CommandHandler.clear_mail = false;
                 clearMail(menuManager); // Удалить всю почту
             }



             /* Запустить программу отсчета времени удаления сообщения через 10 минут*/
             if (flags.MailOn)
             {
                 static uint32_t mail_off = millis();
                 if (millis() - mail_off > MAIL_OFF_DELAY/* && flags.MailOn*/)               // Убрать отображение сообщения почты
                 {
                     mail_off = millis();
                     Allow_flashing = false;                                                 // Запретить мигание сообщения
                     flags.MailOn = false;                                                   // Время 10 минут закончилось
                     rows_mail.fillSprite(TFT_BLACK);
                     rows_mail.pushToSprite(&back_screen, 0, 0, TFT_BLACK);
                     rows_mail.deleteSprite();
                 }
             }
             esp_task_wdt_reset();

             /*рисуем все спрайты*/
             back_screen.pushSprite(0, 0);
         }
     }
 }

 
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 /* ================================= Картинка при старте программы ========================================= */
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

     String Current_version = SettingsMain.getVer();
     tbw1 = tft_radar->textWidth(Current_version);
     x_tft = (tft_radar->width() - tbw1) - 4;
     y_tft = tft_radar->height() - tft_radar->fontHeight() + 10;
     tft_radar->setCursor(x_tft, y_tft);
     tft_radar->print(Current_version);

     esp_task_wdt_reset();
     vTaskDelay(3000);

     esp_task_wdt_reset();

     tft_radar->fillScreen(TFT_NAVY);

     back_screen.fillSprite(backColor);                   // Закрасим поле 
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
     backsprite.pushRotated(&back_screen, angle, TFT_BLACK);
     /***************    TFT_шкала дистанции    *******************/

     //rssi_info.fillSprite(TFT_BLACK);
     //rssi_info.drawRect(0, 0, 80, 25, TFT_DARKGREY);
     //rssi_info.drawString("--- db", 40, 4);


    /*Формируем картинку самолета*/
    /* Рисуем фюзеляж*/

     int width_air = 148+160;
     int height_air = 150;
     back_screen.drawLine(12 + width_air, 0 + height_air, 12 + width_air, 18 + height_air, TFT_DARKGREY);

     /*Рисуем передние крылья*/
     back_screen.drawLine(3 + width_air, 7 + height_air, 20 + width_air, 7 + height_air, TFT_DARKGREY);
     back_screen.drawLine(0 + width_air, 8 + height_air, 23 + width_air, 8 + height_air, TFT_DARKGREY);

     /*Рисуем задние крылья*/
     back_screen.drawLine(7 + width_air, 17 + height_air, 17 + width_air, 17 + height_air, TFT_DARKGREY);
     back_screen.pushSprite(0, 0);
    //   /* Определение местоположения при старте */
    fix = (uint8_t)isValidGNSSFix();

    if (!fix && (settings->mode != SOFTRF_MODE_TXRX_TEST1) && (settings->mode != SOFTRF_MODE_TXRX_TEST2) && (settings->mode != SOFTRF_MODE_TXRX_TEST3) && (settings->mode != SOFTRF_MODE_TXRX_TEST4) && (settings->mode != SOFTRF_MODE_TXRX_TEST5))
    {
            if (!text_call)
            {
                waiting_txt(menuManager);
            }

            vTaskDelay(2000);
            esp_task_wdt_reset();
    }

    tft_radar->fillScreen(backColor);
    esp_task_wdt_reset();
 }


 //=========================================================================================================================================================================
 
 void TFTMenuScreen::drawMessage(TFTMenu* menuManager, int count_message, int count_view)
 {
     TFT_Class* tft_msg = menuManager->getDC();
     if (!tft_msg)
     {
         return;
     }

    // flags.MailOn = true;

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

     TFTScreen->resetIdleTimer();                                                             // сбрасываем таймер ничегонеделанья
     /* номер письма на конверте */
  
     unsigned int cur_adr = (count_message * Number_of_bytes_block) + Start_Block_Text_ADDRESS/* - Number_of_bytes_block*/; //  получить  адрес текущего сообщения.
     MemReadChars(cur_adr + addr_current_message, msg, sizeof(msg));                           // Считать из памяти записанное сообщение.
     MemReadChars(cur_adr + addr_time_this_message, time_msg, sizeof(time_msg));               // Считать из памяти время соббщения в память по текущему адресу 
     uint8_t  confirmation_OK = MemRead(cur_adr + addr_read_NOT_TRANSMITTED);                  // 1 байт - флаг передачи подтверждения "ОК". "1" подтверждение прочтения НЕ ПЕРЕДАНО
     uint8_t not_read = SettingsMain.getCoutNotReadMessage();                                  // получить показания счетчика не подтвержденного количества сообщений
   
        /*Вывести на дисплей сообщение*/
  
     int len_Width1 = mb_strlen(msg);
         
     rows_mail.createSprite(320, 36);
     rows_mail.fillSprite(TFT_BLACK);
     rows_mail.setTextColor(TFT_YELLOW, backColor);
     rows_mail.setTextDatum(TL_DATUM);
     rows_mail.setTextSize(2);
     rows_mail.drawString(msg, 0, 0);
     rows_mail.pushToSprite(&back_screen, 0, 0, TFT_BLACK);
     back_screen.pushSprite(0, 0);

    // Serial.println(msg);

     /* отобразить состояние подтверждения прочтения сообщения*/

 //    char str[1];
 //    int cursorNum = 0;
 ////    dc->setCursor(cursorNum, 3);                                                   // Установить курсор в начало экрана на нижней строке
 //    if (count_view < 10)
 //    {
 //        itoa(count_view, str, 10);                                                  // Преобразуем номер текущего сообщения в строку
 //       // dc->print(str);                                                           // Отображаем в первой позиции номер сообщения.Выводим на дисплей номер текущего сообщения
 //    }
 //    else
 //    {
 //       // dc->print("X");                                                           // Если количество больше 10, выводим символ "Х" для этономии знакомест.
 //    }

 //    if (confirmation_OK == MESSAGE_CONFIRMED)                                       // Для данного сообщения подтверждение о прочтении не передано
 //    {
 //       // dc->print("*");                                                           // Выводим на дисплей символ, обозначающий, было передано подтверждение или нет
 //    }

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

 // Функция подсчёта количества символов в строке utf8,
// состоящей из букв английского и русского алфавитов, цифр, общепринятых символов...

 int TFTMenuScreen::mb_strlen(char* source)  // как в php :)
 {
     int i, k;
     int target = 0;
     unsigned char n;
     char m[2] = { '0', '\0' };
     k = strlen(source);
     i = 0;

     while (i < k) {
         n = source[i]; i++;

         if (n >= 0xBF) {
             switch (n) {
             case 0xD0: {
                 n = source[i]; i++;
                 if (n == 0x81) { n = 0xA8; break; }
                 if (n >= 0x90 && n <= 0xBF) n = n + 0x2F;
                 break;
             }
             case 0xD1: {
                 n = source[i]; i++;
                 if (n == 0x91) { n = 0xB7; break; }
                 if (n >= 0x80 && n <= 0x8F) n = n + 0x6F;
                 break;
             }
             }
         }
         m[0] = n; target = target + 1;
     }
     return target;// -2;
 }


 void TFTMenuScreen::clearMail(TFTMenu* menuManager)
 {

     TFT_Class* dc = menuManager->getDC();

     if (!dc)
     {
         return;
     }

     TFTRus* rusPrinter = menuManager->getRusPrinter();
     flags.MailOn = false;                                                   // Отключить отсчет времени удаления сообщения через 10 минут
     Allow_flashing = false;  // Запретить мигание сообщения
     for (int i = 0; i < (Max_Count_Block_Message * Number_of_bytes_block) + 400; i++)
     {
         MemWrite(i, 0x00);
     }
     MemCommit();
 
     rows_mail.fillSprite(TFT_BLACK);
     rows_mail.pushToSprite(&back_screen, 0, 0, TFT_BLACK);
     rows_mail.deleteSprite();
     back_screen.pushSprite(0, 0);
     dc->setFreeFont(TFT_FONT);
     rusPrinter->print(data_txt2, 75, 50, backColor, TFT_YELLOW);  // Отображаем 
     delay(3000);

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

     //String data = data_txt; //"ОПРЕДЕЛЕНИЕ"
     //String data1 = data_txt1; //"МЕСТОПОЛОЖЕНИЯ"
     int textFontWidth = tft_radar->textWidth(data_txt, 2);                   // Returns pixel width of string in current font

     uint16_t curX = (screenWidth / 2) - (textFontWidth / 2) - 12;        // Координаты вывода 
     uint16_t curY = 110;                                                 // Координаты вывода текста
     rusPrinter->print(data_txt/*.c_str()*/, curX, curY, backColor, TFT_YELLOW);  // Отображаем 

     textFontWidth = tft_radar->textWidth(data_txt1, 2);                      // Returns pixel width of string in current font
 
     curX = (screenWidth / 2) - (textFontWidth / 2) - 18;                  // Координаты вывода 
     curY = 140;                                                           // Координаты вывода текста
     rusPrinter->print(data_txt1/*.c_str()*/, curX, curY, backColor, TFT_YELLOW);  // Отображаем 
 }


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_TFT_MODULE
