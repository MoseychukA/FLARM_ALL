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
#include "LED.h"
#include "EEPROMRF.h"
#include "GNSS.h"
#include "NotoSansMonoSCB20.h"
#include "NotoSansBold15.h"
#include "RF.h"
#include <stdio.h>

#ifdef USE_TFT_MODULE

#include "TFTMenu.h"

/* Спрайты вывода изображений и информации на экран дисплея */

TFT_eSPI tft = TFT_eSPI();



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

   static uint32_t tmr = millis();

   /* Проверяем наличие новой информации */
   if (millis() - tmr > DATA_MEASURE_THRESHOLD)
   {

       tmr = millis();



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

     esp_task_wdt_reset();

    esp_task_wdt_reset();
    vTaskDelay(4000);
    esp_task_wdt_reset();
 
 }

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

 
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_TFT_MODULE
