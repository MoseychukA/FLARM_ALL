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
#include <stdio.h>
#include "CoreCommandBuffer.h"
#include "Module1090.h"
#include "TFTMenu.h"

/* Спрайты вывода изображений и информации на экран дисплея */

// create a new sprite
TFT_eSPI tft = TFT_eSPI();

TFT_eSprite back       = TFT_eSprite(&tft);         // Спрайт фона
TFT_eSprite backsprite = TFT_eSprite(&tft);         // Спрайт отображения вращающегося поля воздушной обстановки
TFT_eSprite rows_Message    = TFT_eSprite(&tft);    // Спрайт отображения текстов сообщений
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

int16_t new_angle[MAX_TRACKING_OBJECTS];                           // Для вычисления курса стороннего самолета

int dump1090_speed[MAX_TRACKING_OBJECTS];                          // 
String dump1090_info_txt[MAX_TRACKING_OBJECTS];                    //

bool isTeam_all[MAX_TRACKING_OBJECTS]    = { false };              // Удалить данные по самолету



//......................................colors
#define backColor  0x0026
                                    
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

    /* IC 240(RGB)* 320,*/
    int rot = 3;
    int dRot = 3;
  
    tftDC = new TFT_eSPI();

    tftDC->init();
    tftDC->setRotation(dRot);
    tftDC->fillScreen(TFT_BACK_COLOR);

    tftDC->setTextColor(TFT_RED, TFT_BACK_COLOR);

    delay(200);
   
   // rusPrint.init(tftDC);

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
 
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::onActivate(TFTMenu* menuManager)
 {
	 if (!menuManager->getDC())
	 {
		 return;
	 }
 
     Allow_flashing = false;  // Запретить мигание сообщения
     flags.MailOn   = false;  // Флаг удаления сообщения
     count_buttton = 0;
     setClearButton = false;
     setClearMessage = false;
     setMessageRead = false;
     text_call = false;
     confirm_message = false;

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
       //Air_txt_Sprite[i]->setColorDepth(8);

       arrow[i] = new TFT_eSprite(&tft);              // Спрайт информации стороннего воздушного объекта
       arrow[i]->createSprite(10, 10);                // Спрайт отображения стрелка вверх/вниз
      // arrow[i]->setColorDepth(8);

       airplane [i] = new TFT_eSprite(&tft);          // Спрайт информации стороннего воздушного объекта
       airplane [i]->createSprite(100, 100);          // Спрайт отображения объекта, полученного из DUMP1090. 
       airplane[i]->setPivot(50,50); 
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
        fx[i] = ((r - 5) * cos(rad * a)) + cx;    //Длина линии внешняя точка
        fy[i] = ((r - 5) * sin(rad * a)) + cy;    //Длина линии внешняя точка
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

 //----------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::update(TFTMenu* menuManager)
 {
  
    //-------------------- Блок работы с кнопкой  -----------------------------------------
    //******************** выполнение действий кнопок ******************************
  
    if (setMessageRead)
    {
        setMessageRead = false;
       // Serial.print("setMessageRead");
         
        esp_task_wdt_reset();
        /* проконтролируем в КОМ порту количество неподтвержденных сообщений*/
         
        //----------------------------------------------------------------------------------
         
        if (confirm_message)                                                              // Получен специальный код признака, означает что подтверждение не отправлено. Нужно отправить
        {
            confirm_message = false;                                                      // Запретить отправку подтверждения о прочтении сообщения (подтверждение отправлено).
            char msgOK_Trecker[10] = "|OK";                                               // Формирование строки для ответного сообщения 
            char msgNum[2] = "";                                                          // массив для записи номера ответного сообщения
            char msg[60] = "";                                                            // Массив для приема текстовых сообщений
            char msg_resp[60] = "";                                                       // 
            char msg_resp_tmp[60] = "";                                                   //
 
            strcat(msgOK_Trecker, msgNum);                                                // Добавили в "|OK" номер ответного сообщения
            strcat(msg_resp, msgOK_Trecker);                                              // Добавили к текущему ответу новый ответ. Формируем строку с несколькими ответами
            SERIAL_TRACKER.println(msg_resp);                                              // Передать подтерждение о прочтении сообщения в буфер треккера
         
            rows_Message.fillSprite(TFT_BLACK);                                               // Удаляем сообщение с экрана
            rows_Message.pushToSprite(&back, 0, 0, TFT_BLACK);                                // Удаляем сообщение с экрана
            rows_Message.deleteSprite(); // Удаляем сообщение с экрана
            flags.MailOn = false;                                                             // Отключить отсчет времени удаления сообщения через 10 минут
            Allow_flashing = false;  // Запретить мигание сообщения
            back.pushSprite(0, 0);
            strncpy(msg_mem_tmp, "", strlen(msg_mem_tmp));
            strncpy(CommandHandler.msg_tmp_all, "", strlen(CommandHandler.msg_tmp_all));
        }
    }

    // Удалить сообщение

    if(setClearMessage)
    {
        setClearMessage = false;
        clearMSG(menuManager); // Удалить все  сообщения
    }

    if (count_buttton != 0)
    {
        static uint32_t button_tmr = millis();
        if (millis() - button_tmr > BUTTON_OFF_DELAY)
        {
            button_tmr = millis();
            count_buttton = 0;
            setClearButton = true;
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

         /* Проверяем есть ли данные GPS. */

         static uint32_t tmr1_GNSS = millis(); 
         if ((uint8_t)isValidGNSSFix() == true)
         {
             tmr1_GNSS = millis();
             fix_tmp = true;
         }

         if (millis() - tmr1_GNSS > 10000)
         {
             tmr1_GNSS = millis();
             fix_tmp = false;
           //  Serial.println("fix_tmp = false");
         }

         esp_task_wdt_reset();

         if (!fix_tmp && (settings->mode != FLYRF_MODE_TXRX_TEST1) && (settings->mode != FLYRF_MODE_TXRX_TEST2) && (settings->mode != FLYRF_MODE_TXRX_TEST3) && (settings->mode != FLYRF_MODE_TXRX_TEST4) && (settings->mode != FLYRF_MODE_TXRX_TEST5)) // Эта проверка не проводится в тестовом режиме
         {
             static uint32_t tmr_GNSS = millis();
             if (millis() - tmr_GNSS > 15000)
             {
                 tmr_GNSS = millis();
                // Serial.println("tmr_GNSS");
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
             back.fillSprite(backColor);                   // Закрасим поле 
             backsprite.fillSprite(TFT_BLACK);             // 
             backsprite.setPivot(160, 160);                // Назначаем центр вращения спрайта воздушной обстановки

            /* =================  Сначала зафиксируем положение нашего самолета =================================*/

            //=========================== Сглаживаем основные показатели скорости, высоты  и курса ==================================

            /* =========== Фильтр скорости нашего самолета. ================== */

             bool array_countMax_speed = false;
             int sum_speed = 0;
             uint8_t array_count_speed = 0;
             uint8_t array_size_speed = 20;
             int dimension_array_speed[20];

             dimension_array_speed[array_count_speed] = (int)ThisAircraft.speed;
             array_count_speed++;
             int val_speed = 0;
             if (array_count_speed > array_size_speed)               // проверка заполнения массива первичными данными об величине скорости
             {
                 array_count_speed = 0;
                 array_countMax_speed = true;                       //Разрешить выдавать данные об величине скорости
             }

             sum_speed = 0;                                         //

             if (array_countMax_speed)                              // формируем данные об величине скорости
             {
                 for (int i = 0; i < array_size_speed; i++)
                 {
                     sum_speed += dimension_array_speed[i];
                 }
                 val_speed = sum_speed / array_size_speed;
             }
             else
             {
                 for (int i = 0; i < array_count_speed; i++)       //формируем первичные (заполняем массив) данные об величине скорости
                 {
                     sum_speed += dimension_array_speed[array_count_speed - 1];
                 }
                 val_speed = sum_speed / array_count_speed;
             }
             sum_speed = 0;
             thisAircraft_speed_tmr = val_speed;        // Данные по скорости нашего самолета после фильтра


             /*========== Фильтр курса нашего самолета ================*/

             bool array_countMax_course = false;
             int sum_course = 0;
             uint8_t array_count_course = 0;
             uint8_t array_size_course = 15;
             int dimension_array_course[15];

             dimension_array_course[array_count_course] = (int)ThisAircraft.course;
             array_count_course++;
             int val_course = 0;
             if (array_count_course > array_size_course)               // проверка заполнения массива первичными данными об величине курса
             {
                 array_count_course = 0;
                 array_countMax_course = true;                       //Разрешить выдавать данные об величине курса
             }

             sum_course = 0;                                         //

             if (array_countMax_course)                              // формируем данные об величине курса
             {
                 for (int i = 0; i < array_size_course; i++)
                 {
                     sum_course += dimension_array_course[i];
                 }
                 val_course = sum_course / array_size_course;
             }
             else
             {
                 for (int i = 0; i < array_count_course; i++)       //формируем первичные (заполняем массив) данные об величине курса
                 {
                     sum_course += dimension_array_course[array_count_course - 1];
                 }
                 val_course = sum_course / array_count_course;
             }

             sum_course = 0;
             angle = (360 - val_course) % 360;                   // Данные по курсу нашего самолета после фильтра
  

             /* При малой скорости нашего самолета фиксируем курс нашего самолета */

             if(thisAircraft_speed_tmr >= 0 && thisAircraft_speed_tmr < 5)
             {
                 angle = angle_old;
             }
             else
             {
                 angle_old = angle;
             }

    
              //======================= Фильтр высоты нашего самолета ==========================
             bool array_countMax_altitude = false;
             int sum_altitude = 0;
             uint8_t array_count_altitude = 0;
             uint8_t array_size_altitude = 20;
             int dimension_array_altitude[20];

             dimension_array_altitude[array_count_altitude] = (int)ThisAircraft.altitude;
             array_count_altitude++;
             int val_altitude = 0;
             if (array_count_altitude > array_size_altitude)               // проверка заполнения массива первичными данными об величине курса
             {
                 array_count_altitude = 0;
                 array_countMax_altitude = true;                       //Разрешить выдавать данные об величине курса
             }

             sum_altitude = 0;                                         //

             if (array_countMax_altitude)                              // формируем данные об величине курса
             {
                 for (int i = 0; i < array_size_altitude; i++)
                 {
                     sum_altitude += dimension_array_altitude[i];
                 }
                 val_altitude = sum_altitude / array_size_altitude;
             }
             else
             {
                 for (int i = 0; i < array_count_altitude; i++)       //формируем первичные (заполняем массив) данные об величине курса
                 {
                     sum_altitude += dimension_array_altitude[array_count_altitude - 1];
                 }
                 val_altitude = sum_altitude / array_count_altitude;
             }

             sum_altitude = 0;

             thisAircraft_altitude_tmr = val_altitude;  // Данные по высоте нашего самолета после фильтра

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
    
                 if (count_buttton == 0)
                 {
                     if (min_distance > 16000)
                     {
                         divider = 32000;  // 32000
                         divider_num = 1;
                     }
                     else if (min_distance <= 16000 && min_distance > 8000)  //16000/2
                     {
                         divider = 16000;  // 16000
                         divider_num = 2;
                     }
                     else if (min_distance <= 8000 && min_distance > 4000) // 8000 /2
                     {
                         divider = 8000;  // 8000
                         divider_num = 3;
                     }
                     else if (min_distance <= 4000 && min_distance > 2000) // 4000/2
                     {
                         divider = 4000; // 4000
                         divider_num = 4;
                     }
                     else if (min_distance <= 2000 && min_distance > 1000) // 2000/2
                     {
                         divider = 2000;  //2000
                         divider_num = 5;
                     }
                     else if (min_distance <= 1000 && min_distance > 500) //1000 /2
                     {
                         divider = 1000;  //1000m
                         divider_num = 6;
                     }
                     else if (min_distance <= 500 && min_distance > 200) // 500/2
                     {
                         divider = 500;  // 500 m
                         divider_num = 7;
                     }
                     else if (min_distance <= 200 && min_distance > 100) //200/2
                     {
                         divider = 200;  // 200 m
                         divider_num = 8;
                     }
                     else if (min_distance <= 100)   // 100/2
                     {
                         divider = 100; // 100 m
                         divider_num = 9;
                     }
                 }

    

                 // Установки определения уровней предупреждения. Параметры задаются со смартфона и записываются в EEPROM

                 // settings->alarm_attention;     // Внимание. Параметр - расстояние 
                 // settings->alarm_warning;       // Предупреждение. Параметр - расстояние 
                 // settings->alarm_danger;        // Тревога. Параметр - расстояние        
                 // settings->alarm_height;        // Тревога по высоте. Параметр - высота 


                 //===================================================================================

                 bool rssi_off = false;

                 for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
                 {
                     if (Container[i].addr)  // Если есть данные стороннего самолета
                     {
                         esp_task_wdt_reset();
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
                         int diff_altitude = 10; // Не реагировать если изменение меньше

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
                             esp_task_wdt_reset();
                             // --------------------------------------------------------------------------------
                              /* При малой скорости смотрим в центр экрана на наш самолет. Это означает что самолет не летит (на земле) */
                             if (alien_speed_tmr[i] >= 0 && alien_speed_tmr[i] < 10)
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


                             if (height_difference[i] >= 0)  //height_difference[i] >= 0
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
                             }
                             else if (min_distance <= settings->alarm_attention && min_distance  > settings->alarm_warning) // Чужой самолет на расстоянии предупреждения
                             {
                                 if (VerticalSet > settings->alarm_height)   //  Чужой самолет выше расстояния опасности
                                 {
                                     little_air_color[i] = TFT_WHITE;
                                 }
                                 else
                                 {
                                     //  Чужой самолет на расстоянии предупреждения
                                     little_air_color[i] = TFT_YELLOW;
                                 }
                             }
                             else if (min_distance <= settings->alarm_warning && min_distance > settings->alarm_danger)   // Чужой самолет на расстоянии предупреждения
                             {
                                 if (VerticalSet > settings->alarm_height)                                                // Чужой самолет выше расстояния предупреждения
                                 {
                                     little_air_color[i] = TFT_WHITE;
                                 }
                                 else if (VerticalSet <= settings->alarm_height)
                                 {
                                     // Чужой самолет на расстоянии предупреждения
                                     little_air_color[i] = TFT_ORANGE;
                                 }
                             }
                             else if (min_distance <= settings->alarm_danger && VerticalSet <= settings->alarm_height)  // Чужой самолет на близком расстоянии и по высоте опасен
                             {
                                 little_air_color[i] = TFT_RED;
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

                             if (Container[i].addr)
                             {
                                 int height_tmp = (int)round(height_difference[i] / 10);

                                 if (height_tmp > 0)
                                 {
                                     Air_txt_Sprite[i]->drawString("+" + String(height_tmp), 27, 1, 0);
                                 }
                                 else
                                 {
                                     Air_txt_Sprite[i]->drawString(String(height_tmp), 27, 1, 0);
                                 }

                             }
                              /* Записать скорость в формуляр  движущегося самолета*/
                             alien_speed_view[i] = 50 - ((int)Container[i].speed / 60 * 3);  // Расстояние стороннего самолета для вывода на дисплей
 
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
                                 airplane[i]->fillSprite(TFT_BLACK);                                          // Закрасим поле самолетика
                                 airplane[i]->fillCircle(50, 50, 5, little_air_color[i]),                     // fillCircle //drawCircle
                                 airplane[i]->drawLine(50, 45, 50, alien_speed_view[i] - 4, little_air_color[i]); // Рисуем прямую линию"скорости" с носа самолета
                                 area_airplane[i]->fillSprite(TFT_BLACK);                                          // Закрасим поле 

                                // area_airplane[i]->drawSmoothRoundRect(0, 0, 1, 1, 99, 99, TFT_YELLOW); //!! Только для теста
                                // LoRa_airplane[i]->drawSmoothRoundRect(0, 0, 1, 1, 99, 99, TFT_PINK); //!! Только для теста
                             }
                             else if (Container[i].signal_source == 1 || Container[i].signal_source == 2)  // С учетом данных, полученных с приемника DUMP1090
                             {
                                 /*Рисуем маленький самолетик */
                                airplane[i]->fillSprite(TFT_BLACK);      // Закрасим поле самолетика

                                airplane[i]->drawLine(49, 43, 49, 57, little_air_color[i]);
                                airplane[i]->drawLine(50, 42, 50, 57, little_air_color[i]);
                                airplane[i]->drawLine(51, 43, 51, 57, little_air_color[i]);


                                airplane[i]->drawLine(48, 46, 52, 46, little_air_color[i]);
                                airplane[i]->drawLine(47, 47, 53, 47, little_air_color[i]);
                                airplane[i]->drawLine(46, 48, 54, 48, little_air_color[i]);
                                airplane[i]->drawLine(45, 49, 55, 49, little_air_color[i]);
                                airplane[i]->drawLine(44, 50, 47, 50, little_air_color[i]);
                                airplane[i]->drawLine(53, 50, 56, 50, little_air_color[i]);
                                airplane[i]->drawLine(43, 51, 45, 51, little_air_color[i]);
                                airplane[i]->drawLine(55, 51, 57, 51, little_air_color[i]);
                                airplane[i]->drawLine(42, 52, 43, 52, little_air_color[i]);
                                airplane[i]->drawLine(57, 52, 58, 52, little_air_color[i]);

                                airplane[i]->drawLine(48, 56, 52, 56, little_air_color[i]);
                                airplane[i]->drawLine(47, 57, 53, 57, little_air_color[i]);
                                airplane[i]->drawLine(46, 58, 48, 58, little_air_color[i]);
                                airplane[i]->drawLine(52, 58, 54, 58, little_air_color[i]);
                                airplane[i]->drawLine(45, 59, 46, 59, little_air_color[i]);
                                airplane[i]->drawLine(54, 59, 55, 59, little_air_color[i]);

                                airplane[i]->drawLine(50, 41, 50, alien_speed_view[i] - 4, little_air_color[i]); // Рисуем прямую линию"скорости" с носа самолета
                                area_airplane[i]->fillSprite(TFT_BLACK);      // Закрасим поле 
                             }


                             if (isTeam_all[i] == true)
                             {

                                airplane[i]->pushRotated(area_airplane[i], alient_course[i], TFT_BLACK); // 
                                area_airplane[i]->pushToSprite(&back, radar_center_x + Container_alien_X[i] - 50, radar_center_y - Container_alien_Y[i] - 50, TFT_BLACK);
                                arrow[i]->pushToSprite(&back, radar_center_x + Container_arrow_X[i], radar_center_y - Container_arrow_Y[i], TFT_BLACK);
                                Air_txt_Sprite[i]->pushToSprite(&back, radar_center_x + Container_logbook_X[i], radar_center_y - Container_logbook_Y[i], TFT_BLACK);

                                isTeam_all[i] = false;
                                esp_task_wdt_reset();
                             }



                             esp_task_wdt_reset();

                         } //Закочить обработку данных самолетов с известными координатами

                         //============================= Конец обработки данных самолетов самолетов  с известными координатами ==========================
                     }

                     if (Container[i].rssi < 0 && Container[i].signal_source == 0)
                     {
                         String rssi_txt;
                         rssi_info.fillSprite(TFT_BLACK);
                         rssi_info.drawRect(0, 0, 80, 25, TFT_DARKGREY);
                         rssi_txt = String(Container[i].rssi);
                         rssi_info.drawString(rssi_txt + " db", 40, 4);
                         rssi_off = true;
                     }

                     if (!rssi_off)
                     {
                         rssi_info.fillSprite(TFT_BLACK);
                         rssi_info.drawRect(0, 0, 80, 25, TFT_DARKGREY);
                         rssi_info.drawString("--- db", 40, 4);

                     }
                 }
             }
             else
             {
                 rssi_info.fillSprite(TFT_BLACK);
                 rssi_info.drawRect(0, 0, 80, 25, TFT_DARKGREY);
                 rssi_info.drawString("--- db", 40, 4);
                 divider = 32000;  // 
                 divider_num = 1;
             }


             //============================== Формируем неподвижное базовое изображение на экране =========================================== 

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
   
             time_info.pushToSprite(&back, 259, 1);        // Отображаем табло времени

             if (settings->rssi_view == VIEW_RSSI_ON)
             {
                 rssi_info.pushToSprite(&back, 1, 1);         // Отображаем табло уровня сигнала LoRa
             }

             Draw_circular_scale();

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

              /* Определяем масштаб в ручном режиме */
             if (count_buttton != 0)
             {
                 switch (count_buttton)
                 {
                 case 1:
                     divider = 32000;  // 32000
                     divider_num = 1;
                     break;
                 case 2:
                     divider = 16000;  // 16000
                     divider_num = 2;
                     break;
                 case 3:
                     divider = 8000;  // 8000
                     divider_num = 3;
                     break;
                 case 4:
                     divider = 4000; // 4000
                     divider_num = 4;
                     break;
                 case 5:
                     divider = 2000;  //2000
                     divider_num = 5;
                     break;
                 case 6:
                     divider = 1000;  //1000m
                     divider_num = 6;
                     break;
                 default:
                     divider = 32000;  // 32000
                     divider_num = 1;
                     break;
                 }

             }
             esp_task_wdt_reset();
             if (divider <= 32767)
             {
                 back.loadFont(NotoSansBold15);
                 back.setTextColor(TFT_DARKGREY, TFT_BLACK);
                 back.setTextDatum(TC_DATUM);
                 int data_KM_x = 166;  // Расположение строки по X
                 int data_KM_y = 223;  // Расположение строки по Y

                 switch (divider_num)
                 {
                 case 1:
                     back.drawString("32 km", data_KM_x - 3, data_KM_y);
                     break;
                 case 2:
                     back.drawString("16 km", data_KM_x - 3, data_KM_y);
                     break;
                 case 3:
                     back.drawString("8 km", data_KM_x - 2, data_KM_y);
                     break;
                 case 4:
                     back.drawString("4 km", data_KM_x - 2, data_KM_y);
                     break;
                 case 5:
                     back.drawString("2 km", data_KM_x - 2, data_KM_y);
                     break;
                 case 6:
                     back.drawString("1 km", data_KM_x - 2, data_KM_y);
                     break;
                 case 7:
                     back.drawString("500 m", data_KM_x - 1, data_KM_y);
                     break;
                 case 8:
                     back.drawString("200 m", data_KM_x - 1, data_KM_y);
                     break;
                 case 9:
                     back.drawString("100 m", data_KM_x, data_KM_y);
                     break;
                 default:
                     back.drawString("32 km", data_KM_x - 3, data_KM_y);
                     break;
                     // выполняется, если не выбрана ни одна альтернатива
                 }

                 if (count_buttton != 0)
                 {
  
                    back.drawRect(data_KM_x - 30, data_KM_y-1, 56, 17, TFT_RED);

                 }
             }

             if (settings->ram_view == VIEW_RAM_ON)
             {
                 back.setTextDatum(0);
                 back.setTextColor(TFT_DARKGREY, TFT_BLACK);
                 back.drawString("Free: " + String(ESP.getFreeHeap()), 1, 226);
                 // back.drawString("Heap: " + String(ESP.getHeapSize()) + " / " + String(ESP.getFreeHeap()), 1, 30);
                 // back.drawString("PSRAM: " + String(ESP.getPsramSize()) + " / " + String(ESP.getFreePsram()), 1, 42);
             }

             esp_task_wdt_reset();

             /*Формируем картинку нашего самолета*/
                 /* Рисуем фюзеляж*/
             int width_air = 148;
             int height_air = 150;
             back.drawLine(12 + width_air, 0 + height_air, 12 + width_air, 18 + height_air, TFT_DARKGREY);

             /*Рисуем передние крылья*/
             back.drawLine(3 + width_air, 7 + height_air, 20 + width_air, 7 + height_air, TFT_DARKGREY);
             back.drawLine(0 + width_air, 8 + height_air, 23 + width_air, 8 + height_air, TFT_DARKGREY);

             /*Рисуем задние крылья*/
             back.drawLine(7 + width_air, 17 + height_air, 17 + width_air, 17 + height_air, TFT_DARKGREY);
             //============================== Конец формирования неподвижного базовоо изображения на экране =========================================== 

 
             bool DUMP1090_tmp = moduleDump1090.getNewDUMP_0_Flag();
             if (DUMP1090_tmp)
             {
 
                DUMP1090_info.drawLine(12, 6, 12, 19, TFT_WHITE);
                DUMP1090_info.drawLine(13, 5, 13, 19, TFT_WHITE);
                DUMP1090_info.drawLine(14, 6, 14, 19, TFT_WHITE);

                DUMP1090_info.drawLine(9, 9, 17, 9, TFT_WHITE);
                DUMP1090_info.drawLine(7, 10, 19, 10, TFT_WHITE);
                DUMP1090_info.drawLine(5, 11, 21, 11, TFT_WHITE);

                DUMP1090_info.drawLine(9, 18, 17, 18, TFT_WHITE);
                DUMP1090_info.drawLine(8, 19, 18, 19, TFT_WHITE);

                DUMP1090_info.drawCircle(13, 13, 11, TFT_DARKGREY);

                DUMP1090_info.pushToSprite(&back, 228, 0, TFT_BLACK);
  
   
                static uint32_t tmr_DUMP1090 = millis();
                if (millis() - tmr_DUMP1090 > DUMP_FLASHING_OFF)
                {
                    tmr_DUMP1090 = millis();
                    moduleDump1090.setNewDUMP_0_Flag(false);
                }

                esp_task_wdt_reset();
             }

             //============================== Блок работы с текстовыми сообщениями ===============================================================
 
                /* Проверить пришло ли новое сообщение. */
             bool new_flag = SettingsMain.getNewMessageFlag();                             // Получить признак нового сообщения 

             if (new_flag)                                                                 // если новое сообщение
             {

                flags.MailOn = true;                                                      // Начинаем отсчет времени удаления сообщения через 10 минут
                SettingsMain.setNewMessageFlag(false);                                    // Сбросить флаг нового сообщения. Программа извещена и приступила к обработке нового сообщения.
                confirm_message = true;                                                   // Разрешить отправить подтверждение прочтения сообщения.
                Allow_flashing = true;                                                    // Разрешить мигание сообщения

                strncpy(msg_mem_tmp, CommandHandler.msg_tmp_all, strlen(CommandHandler.msg_tmp_all));
                strncpy( CommandHandler.msg_tmp_all,"", strlen(CommandHandler.msg_tmp_all));
                drawMessage(menuManager);                                                 // вызвать программу отображения информации на дисплее
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
                         rows_Message.fillSprite(TFT_BLACK);
                         rows_Message.pushToSprite(&back, 0, 0, TFT_BLACK);
                         rows_Message.deleteSprite();
                     }
                     else
                     {
                         flashing_on_off = true;
                         drawMessage(menuManager);     // вызвать программу отображения информации на дисплее
                     }
                 }

                 if (flashing_on_off)
                 {
                    drawMessage(menuManager);     // вызвать программу отображения информации на дисплее
                 }
             }


             if (CommandHandler.clear_message)
             {
                 CommandHandler.clear_message = false;
                 clearMSG(menuManager); // Удалить всю почту
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
                     rows_Message.fillSprite(TFT_BLACK);
                     rows_Message.pushToSprite(&back, 0, 0, TFT_BLACK);
                     rows_Message.deleteSprite();
                 }
             }
             esp_task_wdt_reset();

             //if (settings->ram_view == VIEW_RAM_ON)
             //{
             //    back.setTextDatum(0);
             //    back.setTextColor(TFT_DARKGREY, TFT_BLACK);
             //    back.drawString("Heap: "+String(ESP.getFreeHeap()), 1, 226);
             //   // back.drawString("Heap: " + String(ESP.getHeapSize()) + " / " + String(ESP.getFreeHeap()), 1, 30);
             //   // back.drawString("PSRAM: " + String(ESP.getPsramSize()) + " / " + String(ESP.getFreePsram()), 1, 42);
             //}

             /*рисуем все спрайты*/
             back.pushSprite(0, 0);
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
    // TFTRus* rusPrinter = menuManager->getRusPrinter();


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
     back.fillSprite(backColor);                   // Закрасим поле 
     backsprite.fillSprite(backColor);             // 
     backsprite.setPivot(160, 160);                // Назначаем центр вращения спрайта воздушной обстановки

     Draw_circular_scale();


     angle = (360 - (int)ThisAircraft.course) % 360;

     ///*Рисуем малый серый круг*/
     //backsprite.drawCircle(cx, 160, 80, TFT_DARKGREY);

     /*Выполняем поворот по азимуту*/
     backsprite.pushRotated(&back, angle, TFT_BLACK);
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

    if (!fix_tmp && (settings->mode != FLYRF_MODE_TXRX_TEST1) && (settings->mode != FLYRF_MODE_TXRX_TEST2) && (settings->mode != FLYRF_MODE_TXRX_TEST3) && (settings->mode != FLYRF_MODE_TXRX_TEST4) && (settings->mode != FLYRF_MODE_TXRX_TEST5))
    {
            if (!text_call)
            {
                waiting_txt(menuManager);
            }

            vTaskDelay(2000);
            esp_task_wdt_reset();
    }

    esp_task_wdt_reset();
 }

 //----------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::Draw_circular_scale()
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



 //=========================================================================================================================================================================
 void TFTMenuScreen::drawMessage(TFTMenu* menuManager)
 {
     TFT_Class* tft_msg = menuManager->getDC();
     if (!tft_msg)
     {
         return;
     }

    // TFTRus* rusPrinter = menuManager->getRusPrinter();

     char msg_mem[Number_of_bytes_block] = "";
     char time_msg[Number_of_bytes_time] = "";
 
     /*Вывести на дисплей сообщение*/
 
     strncpy(msg_mem, CommandHandler.msg_tmp_all, strlen(CommandHandler.msg_tmp_all));

    // strncpy( CommandHandler.msg_tmp_all,"", strlen(CommandHandler.msg_tmp_all));

     strncpy(msg_mem, msg_mem_tmp, strlen(msg_mem_tmp));

     rows_Message.createSprite(320, 52);
     rows_Message.fillSprite(TFT_BLACK);
     rows_Message.setTextColor(TFT_YELLOW, backColor);
     rows_Message.setTextDatum(TL_DATUM);
     rows_Message.setTextSize(2);

     // Необходимо вычислить количество символов в каждой строке.

     int lette_num = mb_strlen(msg_mem, 21);  //

     char str1[50] = { 0 };

     strncpy(str1, msg_mem, lette_num);
     str1[lette_num + 1] = 0;                // записать 0 в конес первой строчки
     rows_Message.drawString(str1, 0, 0);    // Запишем первую строчку в спрайт

     char str_tmp[80] = { 0 };
     strcpy(str_tmp, msg_mem + lette_num);   // Записать в str_tmp текст начиная со второй строчки
     lette_num = mb_strlen(str_tmp, 21);     // Определить указатель конца второй строчки
     char str2[50] = { 0 };                  // Назначить вторую строчку
     strncpy(str2, str_tmp, lette_num);      // Копируем вторую строчку с ограничением  
     str2[lette_num + 1] = 0;                // записать 0 в конес второй строчки
     rows_Message.drawString(str2, 0, 17);

     char str_tmp1[50] = { 0 };
     lette_num = mb_strlen(msg_mem, 42);     // Ищем конец второй строчки
     strcpy(str_tmp1, msg_mem + lette_num);  // Копируем текст третьей строчки

     lette_num = mb_strlen(str_tmp1, 26);    // Ищем конец третьей строчки
     char str3[50] = { 0 };

     strncpy(str3, str_tmp1, lette_num);
     str3[lette_num + 1] = 0;
     rows_Message.drawString(str3, 0, 35);


     rows_Message.pushToSprite(&back, 0, 0, TFT_BLACK);
     back.pushSprite(0, 0);

 
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------

 // Функция подсчёта количества символов в строке utf8,
// состоящей из букв английского и русского алфавитов, цифр, общепринятых символов...

 int TFTMenuScreen::mb_strlen(char* source, int letter_n)  // как в php :)
 {
     int i, k;
     int target = 0;
     unsigned char n;
     char m[2] = { '0', '\0' };
     k = strlen(source);
     i = 0;

     while (i < k) {
         n = source[i]; i++;

         if (n >= 0xBF)
         {
             switch (n)
             {
             case 0xD0:
             {
                 n = source[i]; i++;
                 if (n == 0x81) { n = 0xA8; break; }
                 if (n >= 0x90 && n <= 0xBF) n = n + 0x2F;
                 break;
             }
             case 0xD1:
             {
                 n = source[i]; i++;
                 if (n == 0x91) { n = 0xB7; break; }
                 if (n >= 0x80 && n <= 0x8F) n = n + 0x6F;
                 break;
             }
             }
         }
         m[0] = n; target = target + 1;
         if (target == letter_n)
             break;
     }
     return i;// target;
 }

 void TFTMenuScreen::clearMSG(TFTMenu* menuManager)
 {

     TFT_Class* dc = menuManager->getDC();

     if (!dc)
     {
         return;
     }

    // TFTRus* rusPrinter = menuManager->getRusPrinter();
     flags.MailOn = false;                                                   // Отключить отсчет времени удаления сообщения через 10 минут
     Allow_flashing = false;  // Запретить мигание сообщения
 /*    for (int i = 0; i < (Max_Count_Block_Message * Number_of_bytes_block) + 400; i++)
     {
         MemWrite(i, 0x00);
     }
     MemCommit();*/
 
     for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
     {
         if (Container[i].signal_source == 2)
         {
             Container_msg[i] = EmptyFO;
             Container[i] = EmptyFO;
         }
     }

     rows_Message.fillSprite(TFT_BLACK);
     rows_Message.pushToSprite(&back, 0, 0, TFT_BLACK);
     rows_Message.deleteSprite();
     back.pushSprite(0, 0);

     int screenWidth = dc->width();
     int screenHeight = dc->height();

     dc->setFreeFont(&FreeSerif12pt7b);
     dc->setTextColor(TFT_YELLOW);                   // Set character (glyph) color only (background not over-written)

     const char msg_txt[] = "MESSAGES DELETED";
    
     uint16_t curX = 40;                                      // Координаты вывода 
     uint16_t curY = 120;                                     // Координаты вывода текста
     dc->setCursor(curX, curY),                               // Set cursor for tft.print()
   
     dc->print(msg_txt);  // Отображаем 
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

     TFT_Class* tft_txt = menuManager->getDC();
     if (!tft_txt)
     {
         return;
     }

     int screenWidth = tft_txt->width();
     int screenHeight = tft_txt->height();

     tft_txt->setFreeFont(&FreeSerif12pt7b);
     tft_txt->setTextColor(TFT_YELLOW);                           // Set character (glyph) color only (background not over-written)

     const char data_txt1[] = "LOCATION";
     const char data_txt2[] = "DETERMINATION";
     const char data_txt3[] = "WAIT...";
 
     int textFontWidth = tft_txt->textWidth(data_txt1, 2);



     uint16_t curX = (screenWidth / 2) - (textFontWidth / 2) - 30; // Координаты вывода 
     uint16_t curY = 80;                                           // Координаты вывода текста
     tft_txt->setCursor(curX, curY),                               // Set cursor for tft.print()
     tft_txt->print(data_txt1);                                    // Отображаем 

     textFontWidth = tft_txt->textWidth(data_txt2, 2);
     curX = (screenWidth / 2) - (textFontWidth / 2) - 50;          // Координаты вывода 
     curY += 30;                                                   // Координаты вывода текста
     tft_txt->setCursor(curX, curY),                               // Set cursor for tft.print()
     tft_txt->print(data_txt2);                                    // Отображаем 

     textFontWidth = tft_txt->textWidth(data_txt3, 2);
     curX = (screenWidth / 2) - (textFontWidth / 2) - 12;          // Координаты вывода 
     curY += 30;                                                   // Координаты вывода текста
     tft_txt->setCursor(curX, curY),                               // Set cursor for tft.print()
     tft_txt->print(data_txt3);                                    // Отображаем 

 }


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
