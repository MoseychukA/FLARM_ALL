#include "Configuration_ESP32.h"
#include "TFT_eSPI.h"
#include <SPI.h>
#include <esp_task_wdt.h>
#include "NotoSansMonoSCB20.h"
#include "NotoSansBold15.h"
#include "Settings.h"

#include <stdio.h>

#ifdef USE_TFT_MODULE

#include "TFTMenu.h"

/* Спрайты вывода изображений и информации на экран дисплея */

TFT_eSPI tft = TFT_eSPI();

TFT_eSprite back        = TFT_eSprite(&tft);         // Спрайт фона
TFT_eSprite GNSS_Spr    = TFT_eSprite(&tft);         // Спрайт  
TFT_eSprite GPS_Spr     = TFT_eSprite(&tft);         // Спрайт  
TFT_eSprite GLONASS_Spr = TFT_eSprite(&tft);         // Спрайт  
TFT_eSprite Galileo_Spr = TFT_eSprite(&tft);         // Спрайт  
TFT_eSprite nSat_Spr    = TFT_eSprite(&tft);         // Спрайт  
TFT_eSprite lat_Spr     = TFT_eSprite(&tft);         // Спрайт  
TFT_eSprite lon_Spr     = TFT_eSprite(&tft);         // Спрайт  
TFT_eSprite alt_Spr     = TFT_eSprite(&tft);         // Спрайт 

TFT_eSprite txt_Heading     = TFT_eSprite(&tft);         // Спрайт фона
TFT_eSprite txt_GNSS_Spr    = TFT_eSprite(&tft);         // Спрайт  
TFT_eSprite txt_GLONASS_Spr = TFT_eSprite(&tft);         // Спрайт  
TFT_eSprite txt_nSat_Spr    = TFT_eSprite(&tft);         // Спрайт  
TFT_eSprite txt_lat_Spr     = TFT_eSprite(&tft);         // Спрайт  
TFT_eSprite txt_lon_Spr     = TFT_eSprite(&tft);         // Спрайт  
TFT_eSprite txt_alt_Spr     = TFT_eSprite(&tft);         // Спрайт 


//......................................colors
#define backColor     0x0026
#define gaugeColor    0x055D
#define dataColor     0x0311
#define purple        0xEA16
#define Air_infoColor 0xF811

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
    
     back.createSprite(320, 320);
     back.fillSprite(backColor);                   // Закрасим поле 

     txt_Heading.createSprite(319, 50);         // Спрайт  
     txt_Heading.setTextColor(TFT_WHITE, backColor);
     txt_Heading.loadFont(NotoSansMonoSCB20);          // Загружаем шрифты символов направления света
     txt_Heading.setSwapBytes(true);
     txt_Heading.setTextDatum(4);

     txt_GNSS_Spr.createSprite(240, 23);         // Спрайт  
     txt_GNSS_Spr.setTextColor(TFT_WHITE, backColor);
     txt_GNSS_Spr.loadFont(NotoSansBold15);          // Загружаем шрифты символов направления света
     txt_GNSS_Spr.setSwapBytes(true);
     txt_GNSS_Spr.setTextDatum(3);

     txt_GLONASS_Spr.createSprite(240, 23);         // Спрайт  
     txt_GLONASS_Spr.setTextColor(TFT_WHITE, backColor);
     txt_GLONASS_Spr.loadFont(NotoSansBold15);          // Загружаем шрифты символов направления света
     txt_GLONASS_Spr.setSwapBytes(true);
     txt_GLONASS_Spr.setTextDatum(3);

     txt_nSat_Spr.createSprite(170, 23);         // Спрайт  
     txt_nSat_Spr.setTextColor(TFT_WHITE, backColor);
     txt_nSat_Spr.loadFont(NotoSansBold15);          // Загружаем шрифты символов направления света
     txt_nSat_Spr.setSwapBytes(true);
     txt_nSat_Spr.setTextDatum(3);

     txt_lat_Spr.createSprite(170, 23);         // Спрайт  
     txt_lat_Spr.setTextColor(TFT_WHITE, backColor);
     txt_lat_Spr.loadFont(NotoSansBold15);          // Загружаем шрифты символов направления света
     txt_lat_Spr.setSwapBytes(true);
     txt_lat_Spr.setTextDatum(3);

     txt_lon_Spr.createSprite(170, 23);         // Спрайт  
     txt_lon_Spr.setTextColor(TFT_WHITE, backColor);
     txt_lon_Spr.loadFont(NotoSansBold15);          // Загружаем шрифты символов направления света
     txt_lon_Spr.setSwapBytes(true);
     txt_lon_Spr.setTextDatum(3);

     txt_alt_Spr.createSprite(170, 23);         // Спрайт  
     txt_alt_Spr.setTextColor(TFT_WHITE, backColor);
     txt_alt_Spr.loadFont(NotoSansBold15);          // Загружаем шрифты символов направления света
     txt_alt_Spr.setSwapBytes(true);
     txt_alt_Spr.setTextDatum(3);

//===============================================================================

     GNSS_Spr.createSprite(30, 23);         // Спрайт  
     GNSS_Spr.drawRect(0, 0, 29, 22, TFT_DARKGREY);
     GNSS_Spr.setTextColor(TFT_WHITE, backColor);
     GNSS_Spr.loadFont(NotoSansBold15);          // Загружаем шрифты символов направления света
     GNSS_Spr.setSwapBytes(true);
     GNSS_Spr.setTextDatum(4);

     GPS_Spr.createSprite(30, 23);          // Спрайт  
     GPS_Spr.drawRect(0, 0, 29, 22, TFT_DARKGREY);
     GPS_Spr.setTextColor(TFT_WHITE, backColor);
     GPS_Spr.loadFont(NotoSansBold15);          // Загружаем шрифты символов направления света
     GPS_Spr.setSwapBytes(true);
     GPS_Spr.setTextDatum(4);

     GLONASS_Spr.createSprite(30, 23);         // Спрайт 
     GLONASS_Spr.drawRect(0, 0, 29, 22, TFT_DARKGREY);
     GLONASS_Spr.setTextColor(TFT_WHITE, backColor);
     GLONASS_Spr.loadFont(NotoSansBold15);          // Загружаем шрифты символов направления света
     GLONASS_Spr.setSwapBytes(true);
     GLONASS_Spr.setTextDatum(4);

     Galileo_Spr.createSprite(30, 23);          // Спрайт  
     Galileo_Spr.drawRect(0, 0, 29, 22, TFT_DARKGREY);
     Galileo_Spr.setTextColor(TFT_WHITE, backColor);
     Galileo_Spr.loadFont(NotoSansBold15);          // Загружаем шрифты символов направления света
     Galileo_Spr.setSwapBytes(true);
     Galileo_Spr.setTextDatum(4);

     nSat_Spr.createSprite(40, 23);          // Спрайт  
     nSat_Spr.drawRect(0, 0, 39, 22, TFT_DARKGREY);
     nSat_Spr.setTextColor(TFT_WHITE, backColor);
     nSat_Spr.loadFont(NotoSansBold15);          // Загружаем шрифты символов направления света
     nSat_Spr.setSwapBytes(true);
     nSat_Spr.setTextDatum(4);

     lat_Spr.createSprite(100, 23);          // Спрайт 
     lat_Spr.drawRect(0, 0, 99, 22, TFT_DARKGREY);
     lat_Spr.setTextColor(TFT_WHITE, backColor);
     lat_Spr.loadFont(NotoSansBold15);          // Загружаем шрифты символов направления света
     lat_Spr.setSwapBytes(true);
     lat_Spr.setTextDatum(4);

     lon_Spr.createSprite(100, 23);          // Спрайт
     lon_Spr.drawRect(0, 0, 99, 22, TFT_DARKGREY);
     lon_Spr.setTextColor(TFT_WHITE, backColor);
     lon_Spr.loadFont(NotoSansBold15);          // Загружаем шрифты символов направления света
     lon_Spr.setSwapBytes(true);
     lon_Spr.setTextDatum(4);

     alt_Spr.createSprite(60, 23);         // Спрайт 
     alt_Spr.drawRect(0, 0, 59, 22, TFT_DARKGREY);
     alt_Spr.setTextColor(TFT_WHITE, backColor);
     alt_Spr.loadFont(NotoSansBold15);          // Загружаем шрифты символов направления света
     alt_Spr.setSwapBytes(true);
     alt_Spr.setTextDatum(4);

     GNSS_Spr.drawString(String(int(0)),15, 12,0);                 // Спрайт  
     GPS_Spr.drawString(String(int(0)),15, 12,0);                  // Спрайт  
     GLONASS_Spr.drawString(String(int(0)), 15, 12,0);             // Спрайт  
     Galileo_Spr.drawString(String(int(0)), 15, 12,0);             // Спрайт  
     nSat_Spr.drawString(String(int(0)), 20, 12,0);                // Спрайт  
     lat_Spr.drawString(String(float(00.000000),6), 50, 12,0);     // Спрайт  
     lon_Spr.drawString(String(float(00.000000),6), 50, 12,0);     // Спрайт  
     alt_Spr.drawString(String(int(0)), 30, 12,0);                 // Спрайт 

     txt_Heading.drawString("Test GPS", 150, 20, 0);

     txt_GNSS_Spr.drawString("0=NONE, N=GNSS, P=GPS", 2, 12, 0);                 // Спрайт  
     txt_GLONASS_Spr.drawString("L=GLONASS, A=Galileo", 2, 12, 0);             // Спрайт  
     txt_nSat_Spr.drawString("Number of satellites", 2, 12, 0);          // Спрайт  
     txt_lat_Spr.drawString("Latitude", 2, 12, 0);  // Спрайт  
     txt_lon_Spr.drawString("Longitude", 2, 12, 0);  // Спрайт  
     txt_alt_Spr.drawString("Altitude meter", 2, 12, 0);            // Спрайт 

 }

 
 //----------------------------------------------------------------------------------------------------------------------------------------------------------------------
 
 void TFTMenuScreen::update(TFTMenu* menuManager)
 {
     TFT_Class* tft_radar = menuManager->getDC();
     if (!tft_radar)
     {
         return;
     }
     TFTRus* rusPrinter = menuManager->getRusPrinter();


   static uint32_t tmr = millis();
   /* Проверяем наличие новой информации */
   if (millis() - tmr > DATA_MEASURE_THRESHOLD)
   {
       tmr = millis();

       unsigned int sats = Settings.getNumSat(); // Number of satellites in use
       float alt = Settings.getAltitude();

       long latitude_t = Settings.getCurrentLatitude();
       float latitude_f = latitude_t / 1000000.0;

       long longitude_t = Settings.getLCurrentLongitude();
       float longitude_f = longitude_t / 1000000.0;

       txt_Heading.drawString("Test GPS", 150, 20, 0);

       txt_GNSS_Spr.drawString("0=NONE, N=GNSS, P=GPS", 2, 12, 0);                 // Спрайт  
       txt_GLONASS_Spr.drawString("L=GLONASS, A=Galileo", 2, 12, 0);             // Спрайт  
       txt_nSat_Spr.drawString("Number of satellites", 2, 12, 0);          // Спрайт  
       txt_lat_Spr.drawString("Latitude", 2, 12, 0);  // Спрайт  
       txt_lon_Spr.drawString("Longitude", 2, 12, 0);  // Спрайт  
       txt_alt_Spr.drawString("Altitude meter", 2, 12, 0);            // Спрайт 

       char navSystem = Settings.getnavSystem();

       if (navSystem == 'N')
       {
           GNSS_Spr.fillSprite(backColor);                                 // Закрасим поле 
           GNSS_Spr.drawRect(0, 0, 29, 22, TFT_DARKGREY);
           GNSS_Spr.drawString("N", 15, 12, 0);                 // Спрайт  
           GNSS_Spr.pushToSprite(&back, 320 - 10 - 70, 54, TFT_BLACK);
       }
       else if (navSystem == 'P')
       {

           GPS_Spr.fillSprite(backColor);                   // Закрасим поле 
           GPS_Spr.drawRect(0, 0, 29, 22, TFT_DARKGREY);
           GPS_Spr.drawString("P", 15, 12, 0);                  // Спрайт 
           GPS_Spr.pushToSprite(&back, 320 - 10 - 30, 54, TFT_BLACK);
       }
       else if (navSystem == 'L')
       {
           GLONASS_Spr.fillSprite(backColor);                              // Закрасим поле 
           GLONASS_Spr.drawRect(0, 0, 29, 22, TFT_DARKGREY);
           GLONASS_Spr.drawString("L", 15, 12, 0);              // Спрайт 
           GLONASS_Spr.pushToSprite(&back, 320 - 10 - 70, 84, TFT_BLACK);
       }

       else if (navSystem == 'A')
       {
           Galileo_Spr.fillSprite(backColor);                              // Закрасим поле 
           Galileo_Spr.drawRect(0, 0, 29, 22, TFT_DARKGREY);
           Galileo_Spr.drawString("A", 15, 12, 0);              // Спрайт 
           Galileo_Spr.pushToSprite(&back, 320 - 10 - 30, 84, TFT_BLACK);
       }
       else if (navSystem == 0)
       {
           GNSS_Spr.fillSprite(backColor);                                 // Закрасим поле 
           GNSS_Spr.drawRect(0, 0, 29, 22, TFT_DARKGREY);
           GNSS_Spr.drawString("---", 15, 12, 0);                 // Спрайт  
           GNSS_Spr.pushToSprite(&back, 320 - 10 - 70, 54, TFT_BLACK);

           GPS_Spr.fillSprite(backColor);                   // Закрасим поле 
           GPS_Spr.drawRect(0, 0, 29, 22, TFT_DARKGREY);
           GPS_Spr.drawString("---", 15, 12, 0);                  // Спрайт 
           GPS_Spr.pushToSprite(&back, 320 - 10 - 30, 54, TFT_BLACK);

           GLONASS_Spr.fillSprite(backColor);                              // Закрасим поле 
           GLONASS_Spr.drawRect(0, 0, 29, 22, TFT_DARKGREY);
           GLONASS_Spr.drawString("---", 15, 12, 0);              // Спрайт 
           GLONASS_Spr.pushToSprite(&back, 320 - 10 - 70, 84, TFT_BLACK);

           Galileo_Spr.fillSprite(backColor);                              // Закрасим поле 
           Galileo_Spr.drawRect(0, 0, 29, 22, TFT_DARKGREY);
           Galileo_Spr.drawString("---", 15, 12, 0);              // Спрайт 
           Galileo_Spr.pushToSprite(&back, 320 - 10 - 30, 84, TFT_BLACK);
       }


       int y = 30;
       y_tft = 54;

       y_tft += y;
       y_tft += y;

       nSat_Spr.fillSprite(backColor);                                 // Закрасим поле 
       nSat_Spr.drawRect(0, 0, 39, 22, TFT_DARKGREY);
       nSat_Spr.drawString(String(int(sats)), 20, 12, 0);                 // Спрайт  
       nSat_Spr.pushToSprite(&back, 320 - 10 - 40, y_tft, TFT_BLACK);
       y_tft += y;

       lat_Spr.fillSprite(backColor);                                  // Закрасим поле 
       lat_Spr.drawRect(0, 0, 99, 22, TFT_DARKGREY);
       lat_Spr.drawString(String(float(latitude_f), 6), 50, 12, 0);     // Спрайт  
       lat_Spr.pushToSprite(&back, 320 - 10 - 100, y_tft, TFT_BLACK);
       y_tft += y;

       lon_Spr.fillSprite(backColor);                                   // Закрасим поле 
       lon_Spr.drawRect(0, 0, 99, 22, TFT_DARKGREY);
       lon_Spr.drawString(String(float(longitude_f), 6), 50, 12, 0);     // Спрайт  
       lon_Spr.pushToSprite(&back, 320 - 10 - 100, y_tft, TFT_BLACK);
       y_tft += y;

       alt_Spr.fillSprite(backColor);                                  // Закрасим поле 
       alt_Spr.drawRect(0, 0, 59, 22, TFT_DARKGREY);
       alt_Spr.drawString(String(float(alt),1), 30, 12, 0);            // Спрайт 
       alt_Spr.pushToSprite(&back, 320 - 10 - 60, y_tft, TFT_BLACK);
     
       back.pushSprite(0, 0);

       esp_task_wdt_reset();

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

     const char EPD_SoftRF_text1[] = "FlyRF";
     const char EPD_SoftRF_text2[] = "www.decima.ru";
     const char EPD_SoftRF_text3[] = "Test GPS";
     const char EPD_SoftRF_text6[] = "(C) 2024";


     tft_radar->fillScreen(TFT_NAVY);

     tft_radar->setTextColor(TFT_WHITE); //TFT_WHITE TFT_BLACK
     tft_radar->setTextWrap(false);

     tft_radar->setFreeFont(&FreeMonoBold18pt7b);

     x_tft = 90;
     y_tft = 80;
     tft_radar->setCursor(x_tft, y_tft);
     tft_radar->print(EPD_SoftRF_text1);

     x_tft = 60;
     y_tft = 140;
     tft_radar->setCursor(x_tft, y_tft);
     tft_radar->print("Test GPS");

     tft_radar->setFreeFont(&FreeSerif9pt7b);

     x_tft = 10;
     y_tft = 205;
     tft_radar->setCursor(x_tft, y_tft);
     tft_radar->print(EPD_SoftRF_text2);

     x_tft = 10;
     y_tft = tft_radar->height() - tft_radar->fontHeight() + 10;
     tft_radar->setCursor(x_tft, y_tft);
     tft_radar->print(EPD_SoftRF_text6);

     String Current_version = Settings.getVer();  //Current_version

     tbw1 = tft_radar->textWidth(Current_version);
     x_tft = (tft_radar->width() - tbw1) - 4;
     y_tft = tft_radar->height() - tft_radar->fontHeight() + 10;
     tft_radar->setCursor(x_tft, y_tft);
     tft_radar->print(Current_version);

     esp_task_wdt_reset();
     vTaskDelay(4000);

     tft_radar->fillScreen(TFT_NAVY);
     tft_radar->setTextColor(TFT_WHITE); //TFT_WHITE TFT_BLACK
     tft_radar->setTextWrap(false);

     txt_Heading.pushToSprite(&back,10,5, TFT_BLACK);
 
     int y = 30;
     y_tft = 54;

     txt_GNSS_Spr.pushToSprite(&back,10, y_tft, TFT_BLACK);
     GNSS_Spr.pushToSprite(&back, 320 - 10 - 70, y_tft, TFT_BLACK);
     GPS_Spr.pushToSprite(&back, 320 - 10 - 30, y_tft, TFT_BLACK);
     y_tft += y;

     txt_GLONASS_Spr.pushToSprite(&back, 10, y_tft, TFT_BLACK);
     GLONASS_Spr.pushToSprite(&back, 320 - 10 - 70, y_tft, TFT_BLACK);
     Galileo_Spr.pushToSprite(&back, 320 - 10 - 30, y_tft, TFT_BLACK);
     y_tft += y;

     txt_nSat_Spr.pushToSprite(&back, 10, y_tft, TFT_BLACK);
     nSat_Spr.pushToSprite(&back, 320 - 10 - 40, y_tft, TFT_BLACK);
     y_tft += y;

     txt_lat_Spr.pushToSprite(&back, 10, y_tft, TFT_BLACK);
     lat_Spr.pushToSprite(&back, 320 - 10 - 100, y_tft, TFT_BLACK);
     y_tft += y;

     txt_lon_Spr.pushToSprite(&back, 10, y_tft, TFT_BLACK);
     lon_Spr.pushToSprite(&back, 320 - 10 - 100, y_tft, TFT_BLACK);
     y_tft += y;

     txt_alt_Spr.pushToSprite(&back, 10, y_tft, TFT_BLACK);
     alt_Spr.pushToSprite(&back, 320 - 10 - 60, y_tft, TFT_BLACK);

     back.pushSprite(0, 0);
     esp_task_wdt_reset();
   
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
