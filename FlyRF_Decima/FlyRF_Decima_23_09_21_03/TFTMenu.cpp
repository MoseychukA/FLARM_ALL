
#include "Settings.h"


#ifdef USE_TFT_MODULE

#include "TFTMenu.h"

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#define FONT_HEIGHT(dc) dc->fontHeight(1)

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static const char* WEEKDAYS[] = {

  "ПН",
  "ВТ",
  "СР",
  "ЧТ",
  "ПТ",
  "СБ",
  "ВС"

};

int variantPassword = 0;

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
void ButtonPressed(int btn)
{
  if(btn != -1)
  {
 
  }

  TFTScreen->onButtonPressed(btn);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void ButtonReleased(int btn)
{
  TFTScreen->onButtonReleased(btn);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawButtonsYield() // вызывается после отрисовки каждой кнопки
{
  yield();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawScreenCaption(TFTMenu* hal, const String& str) // рисуем заголовок экрана
{

  TFT_Class* dc = hal->getDC();
  
  if(!dc)
  {
    return;
  }
  
  TFTRus* rusPrinter = hal->getRusPrinter();
  
  int screenWidth = dc->width();
  
  dc->setFreeFont(TFT_FONT);
  
  int fontHeight = FONT_HEIGHT(dc);
  int top = 10;

  // подложка под заголовок
  dc->fillRect(0, 0, screenWidth, top*2 + fontHeight, TFT_NAVY);
   
  int left = (screenWidth - rusPrinter->textWidth(str.c_str()))/2;

  rusPrinter->print(str.c_str(),left,top, TFT_NAVY, TFT_WHITE);    
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawTimeInBox(TFTInfoBox* box, uint32_t val)
{
  TFT_Class* dc = TFTScreen->getDC();
  
  if(!dc)
  {
    return;
  }
  
  TFTRus* rusPrinter = TFTScreen->getRusPrinter();
  
  TFTInfoBoxContentRect rc =  box->getContentRect(TFTScreen);
  dc->fillRect(rc.x,rc.y,rc.w,rc.h, INFO_BOX_BACK_COLOR);
  yield();

  dc->setFreeFont(SEVEN_SEG_NUM_FONT_PLUS);

  uint8_t hours = val/60;
  uint8_t mins = val%60;

  String strVal;
  if(hours < 10)
    strVal += '0';

  strVal += hours;
  strVal += ':';

  if(mins < 10)
    strVal += '0';

  strVal += mins;

  
  int fontHeight = FONT_HEIGHT(dc);
  int strLen = rusPrinter->textWidth(strVal.c_str());

  int leftPos = rc.x + (rc.w - strLen)/2;
  int topPos = rc.y + (rc.h - fontHeight)/2;
  rusPrinter->print(strVal.c_str(),leftPos,topPos,INFO_BOX_BACK_COLOR,SENSOR_BOX_FONT_COLOR);
  yield();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawValueInBox(TFTInfoBox* box, const String& strVal, FONTTYPE font)
{
  TFT_Class* dc = TFTScreen->getDC();
  
  if(!dc)
  {
    return;
  }
  
  TFTRus* rusPrinter = TFTScreen->getRusPrinter();
  
  TFTInfoBoxContentRect rc =  box->getContentRect(TFTScreen);
  dc->fillRect(rc.x,rc.y,rc.w,rc.h, INFO_BOX_BACK_COLOR);
  yield();

  dc->setFreeFont(font);

  
  int fontHeight = FONT_HEIGHT(dc);
  int strLen = rusPrinter->textWidth(strVal.c_str());

  int leftPos = rc.x + (rc.w - strLen)/2;
  int topPos = rc.y + (rc.h - fontHeight)/2;
  rusPrinter->print(strVal.c_str(),leftPos,topPos,INFO_BOX_BACK_COLOR,SENSOR_BOX_FONT_COLOR);
  yield();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawValueInBox(TFTInfoBox* box, int val)
{
  return drawValueInBox(box,String(val));
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawValueInBox(TFTInfoBox* box, int16_t val)
{
  return drawValueInBox(box,String(val));
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawValueInBox(TFTInfoBox* box, uint16_t val)
{
  return drawValueInBox(box,String(val));
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawValueInBox(TFTInfoBox* box, int8_t val)
{
  return drawValueInBox(box,String(val));
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawValueInBox(TFTInfoBox* box, uint8_t val)
{
  return drawValueInBox(box,String(val));
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawValueInBox(TFTInfoBox* box, uint32_t val)
{
  return drawValueInBox(box,String(val));
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTInfoBox
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTInfoBox::TFTInfoBox(const char* caption, int width, int height, int x, int y, int cxo)
{
  boxCaption = caption;
  boxWidth = width;
  boxHeight = height;
  posX = x;
  posY = y;
  captionXOffset = cxo;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTInfoBox::~TFTInfoBox()
{
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTInfoBox::drawCaption(TFTMenu* menuManager, const char* caption)
{
  TFT_Class* dc = menuManager->getDC();
  if(!dc)
  {
    return;
  }  
  
  dc->setFreeFont(TFT_FONT);
  
  menuManager->getRusPrinter()->print(caption,posX+captionXOffset,posY,TFT_BACK_COLOR,INFO_BOX_CAPTION_COLOR);  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTInfoBox::draw(TFTMenu* menuManager)
{
  drawCaption(menuManager,boxCaption);
  
  int curTop = posY;

  TFT_Class* dc = menuManager->getDC();
  if(!dc)
  {
    return;
  }

  dc->setFreeFont(TFT_FONT);
    
  int fontHeight = FONT_HEIGHT(dc);
  
  curTop += fontHeight + INFO_BOX_CONTENT_PADDING;

  dc->fillRoundRect(posX, curTop, boxWidth, (boxHeight - fontHeight - INFO_BOX_CONTENT_PADDING),2,INFO_BOX_BACK_COLOR);

  yield();

  dc->drawRoundRect(posX, curTop, boxWidth, (boxHeight - fontHeight - INFO_BOX_CONTENT_PADDING),2,INFO_BOX_BORDER_COLOR);

  yield();
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTInfoBoxContentRect TFTInfoBox::getContentRect(TFTMenu* menuManager)
{
    TFTInfoBoxContentRect result;
    TFT_Class* dc = menuManager->getDC();
	
	if(!dc)
	{
		return result;
	}	

    dc->setFreeFont(TFT_FONT);
    
    int fontHeight = FONT_HEIGHT(dc);

    result.x = posX + INFO_BOX_CONTENT_PADDING;
    result.y = posY + fontHeight + INFO_BOX_CONTENT_PADDING*2;

    result.w = boxWidth - INFO_BOX_CONTENT_PADDING*2;
    result.h = boxHeight - (fontHeight + INFO_BOX_CONTENT_PADDING*3);

    return result;
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

	//tftDC->setFreeFont(TFT_FONT);

	tftDC->setTextColor(TFT_RED, TFT_BACK_COLOR);


	//tftTouch = tftDC;

    delay(200);
    Settings.displayBacklight(true); // включаем подсветку

 // TFTCalibrationData data = Settings.GetTftCalibrationData();
 // if(data.isValid)
 // {
 //   tftTouch->setTouch(data.points);
 // }
 // else
 // {
 //   uint16_t dt[5] = {304, 3502, 280, 3507, 4};
 //   tftTouch->setTouch(dt);
 // }

	//tftTouch->setRotation(rot);
	//tftTouch->begin();


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

  //TFTServiceMenuScreen
  mbscrif.screen = new TFTServiceMenuScreen();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "SERVICE_MENU";
  screens.push_back(mbscrif);


  mbscrif.screen = MessageBoxScreen::create();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "MB";
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
// TFTServiceMenuScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTServiceMenuScreen::TFTServiceMenuScreen()
 {
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTServiceMenuScreen::~TFTServiceMenuScreen()
 {
   //delete screenButtons;  
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTServiceMenuScreen::onActivate(TFTMenu* menuManager)
 {

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTServiceMenuScreen::setup(TFTMenu* menuManager)
 {


   TFT_Class* dc = menuManager->getDC();

   if (!dc)
   {
     return;
   }

 
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  void TFTServiceMenuScreen::update(TFTMenu* menuManager)
 {
 

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTServiceMenuScreen::draw(TFTMenu* menuManager)
 {


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



 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  void TFTMenuScreen::update(TFTMenu* menuManager)
 {
	

   static uint32_t tmr = millis();
   if(millis() - tmr > DATA_MEASURE_THRESHOLD)
   {
 /*      drawVoltage(menuManager);
       drawDateTime(menuManager);*/
       drawWiFi(menuManager);
       tmr = millis();
   }

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::draw(TFTMenu* menuManager)
 {
     TFT_Class* dc = menuManager->getDC();
     if (!dc)
     {
         return;
     }
     TFTRus* rusPrinter = menuManager->getRusPrinter();

     dc->setTextFont(1);

     int screenWidth = dc->width();
     int screenHeight = dc->height();

    // dc->setFreeFont(TFT_SMALL_FONT);
     int textFontHeight = FONT_HEIGHT(dc);

     String data = SOFTWARE_VERSION;

     int textFontWidth = dc->textWidth(data);              // Returns pixel width of string in current font
     uint16_t curX = screenWidth - textFontWidth - 3;      // Координаты вывода 
     uint16_t curY = 120;// 305;                             // Координаты вывода версии

     rusPrinter->print(data.c_str(), curX, curY, TFT_BLACK, TFT_DARKGREY); // Отображаем версию программы

   // dc->setFreeFont(TFT_FONT);
     int centerX_Circle = 80;
     int centerY_Circle = 85;

     int centerX_Triangle = 60;
     int centerY_Triangle = 35;


    dc->drawCircle(centerX_Circle, centerY_Circle, 70, TFT_DARKGREY);
   // dc->drawCircle(centerX_Circle, centerY_Circle, 99, TFT_WHITE);
    dc->drawCircle(centerX_Circle, centerY_Circle, 40, TFT_OLIVE);
    dc->fillCircle(centerX_Circle, centerY_Circle, 2, TFT_WHITE);
    dc->fillTriangle(76, 23, 80, 16, 84, 23, TFT_WHITE);

    dc->drawTriangle(centerX_Triangle-4, centerY_Triangle+4, centerX_Triangle, centerY_Triangle-7, centerX_Triangle+4, centerY_Triangle + 4, TFT_WHITE);

    centerX_Triangle = 130;
    centerY_Triangle = 80;

    dc->fillTriangle(centerX_Triangle - 4, centerY_Triangle + 4, centerX_Triangle, centerY_Triangle - 7, centerX_Triangle + 4, centerY_Triangle + 4, TFT_WHITE);
   // dc->drawTriangle(107, 170, 115, 155, 123, 170, TFT_RED);

    //dc->setFreeFont(TFT_GFXFONT);
   // dc->setSymbolFont(SENSOR_FONT);

    dc->setTextColor(TFT_WHITE, TFT_BLACK);

    dc->setCursor(5, 25);
    dc->print("10км");
    dc->setCursor(78, 25);
    dc->print("N");
    dc->setCursor(100, 50);
    dc->print("-1230\x1F");
    dc->setTextColor(TFT_ORANGE, TFT_BLACK);
    dc->setCursor(43, 78);
    dc->print("720\x1E");
   /* dc->setTextColor(TFT_WHITE, TFT_BLACK);*/
    dc->setFreeFont(SIGN_FONT);
    dc->setCursor(45, 75);
    dc->print("0");

    dc->setCursor(90, 105);
    dc->print("1");
    dc->setTextColor(TFT_WHITE, TFT_BLACK);
    dc->setCursor(110, 50);
    dc->print("2");

    dc->setCursor(135, 38);
    dc->print("3");

    dc->setCursor(5, 55);
    dc->print("4");
    dc->setTextColor(TFT_RED, TFT_BLACK);
    dc->setCursor(35, 60);
    dc->print("5");
    dc->setTextColor(TFT_WHITE, TFT_BLACK);
    dc->setCursor(5, 80);
    dc->print("6");

    dc->setCursor(20, 110);
    dc->print("7");

    dc->setTextColor(TFT_SKYBLUE, TFT_BLACK);
    dc->setCursor(138, 17);
    dc->print("8");

    dc->setTextFont(1);

    dc->setCursor(124, 4);
    dc->print(0);
    dc->print("%");

    dc->setTextColor(TFT_WHITE, TFT_BLACK);

    //dc->setCursor(5, 5);
    //dc->print("РУССКИЙ");




 }



 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 // Контроль внутреннего источника питания (аккумуляторов)
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::drawVoltage(TFTMenu* menuManager)
 {
	 TFT_Class* dc = menuManager->getDC();
	 if (!dc)
	 {
		 return;
	 }
	 TFTRus* rusPrinter = menuManager->getRusPrinter();


	 int screenWidth = dc->width();
	 int screenHeight = dc->height();

	// dc->setFreeFont(TFT_SMALL_FONT);
	 int textFontHeight = FONT_HEIGHT(dc);

	 String data = SOFTWARE_VERSION;

	 int textFontWidth = dc->textWidth(data);              // Returns pixel width of string in current font
	 uint16_t curX = screenWidth - textFontWidth - 10;     // Координаты вывода 
	 uint16_t curY = 5;// 305;                             // Координаты вывода версии



	 rusPrinter->print(data.c_str(), curX, curY, TFT_WHITE, TFT_BLACK); // Отображаем версию программы

	 //dc->setFreeFont(TFT_FONT);

	
	 VoltageData vData5 = Settings.voltage5V;     // Контроль источника питания +5.0в

	 if (last5Vvoltage != vData5.raw)
	 {
		 last5Vvoltage = vData5.raw;
     
		 int y_val = 37;
		 int x_val = map(vData5.voltage5, 10, 230, 0, 100);

		 if (x_val < 20)
		 {
			 dc->fillRect(10, y_val, vData5.voltage5, 7, TFT_RED);
		 }
		 else if ((x_val >= 20) && (x_val < 60))
		 {
			 dc->fillRect(10, y_val, vData5.voltage5, 7, TFT_YELLOW);
		 }
		 else if (x_val >= 60)
		 {
			 dc->fillRect(10, y_val, vData5.voltage5, 7, TFT_GREEN);
		 }
		 dc->fillRect(vData5.voltage5, y_val-1, 230, 7+1, TFT_WHITE);

	 }

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
 // Вывод текущей даты и времени
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::drawDateTime(TFTMenu* menuManager)
 {

#ifdef USE_DS3231_REALTIME_CLOCK

	 TFT_Class* dc = menuManager->getDC();
	 if (!dc)
	 {
		 return;
	 }
	 TFTRus* rusPrinter = menuManager->getRusPrinter();

	 dc->setFreeFont(TFT_FONT);
	 //dc->setFreeFont(TFT_SMALL_FONT);

	 RTC_DS3231  rtc = Settings.GetClock();
	 // DateTime now = rtc.now();

	 RTCTime  now = rtc.getTime();  // обновить время

	 uint16_t curX = 10;     // Координаты вывода даты
	 uint16_t curY = 18;
	 //

	 // получаем компоненты даты в виде строк
	 String strDate = rtc.getDateStr(now);
	 String strTime = rtc.getTimeStr(now);

	 rusPrinter->print(strDate.c_str(), curX, curY, TFT_WHITE, TFT_BLACK);

	 dc->setFreeFont(TFT_FONT);

	 strTime += " ";

	 curX = 130;   // Координаты вывода времени
	 curY = 18;   // Координаты вывода 
	 rusPrinter->print(strTime.c_str(), curX, curY, TFT_WHITE, TFT_BLACK);

#endif 
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

     dc->setTextFont(1);
     //dc->setFreeFont(TFT_FONT);
     //dc->setFreeFont(TFT_SMALL_FONT);
      uint16_t curX = 5;     // Координаты вывода WiFi
      uint16_t curY = 2;
      bool WiFi_On = Settings.GetWiFiState();
      bool WiFi_Connect = Settings.GetWiFiConnect();   // Признак подключения к роутеру

      WiFi_On = true;                 // Признак подключения модуля в работу
     // WiFi_Connect = true;            // Признак подключения к роутеру

      if (WiFi_On)
      {
          if (WiFi_Connect)
          {
              rusPrinter->print("           ", curX, curY, TFT_BLACK, TFT_BLACK);
              rusPrinter->print("WiFi on    ", curX, curY, TFT_BLACK, TFT_DARKGREEN);

          }
          else
          {
              rusPrinter->print("WiFi off", curX, curY, TFT_BLACK, TFT_RED);

          }
      }
      else
      {
          rusPrinter->print("           ", curX, curY, TFT_BLACK, TFT_WHITE);

      }


 }

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
MessageBoxScreen* MessageBox;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
MessageBoxScreen::MessageBoxScreen() : AbstractTFTScreen()
{
  targetOkScreen = NULL;
  targetCancelScreen = NULL;
  resultSubscriber = NULL;
  caption = NULL;
  
  if(!TFTScreen->getDC())
  {
    return;
  }
 
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void MessageBoxScreen::setup(TFTMenu* dc)
{
  if(!dc->getDC())
  {
    return;
  }

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void MessageBoxScreen::update(TFTMenu* dc)
{
  if(!dc->getDC())
  {
    return;
  }
	
    //// тут обновляем внутреннее состояние    
 
    //int pressed_button = buttons->checkButtons(ButtonPressed, ButtonReleased);
    //if(pressed_button != -1)
    //{
    //  // сообщаем, что у нас нажата кнопка
    //  dc->resetIdleTimer();
    //  
    //   if(pressed_button == noButton && targetCancelScreen)
    //   {
    //    if(resultSubscriber)
    //      resultSubscriber->onMessageBoxResult(false);
    //      
    //    dc->switchToScreen(targetCancelScreen);
    //   }
    //   else
    //   if(pressed_button == yesButton && targetOkScreen)
    //   {
    //      if(resultSubscriber)
    //        resultSubscriber->onMessageBoxResult(true);
    //        
    //        dc->switchToScreen(targetOkScreen);
    //   }
    //
    //} // if(pressed_button != -1)

    
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void MessageBoxScreen::draw(TFTMenu* hal)
{
  TFT_Class* dc = hal->getDC();
  
  if(!dc)
  {
    return;
  }
  
  dc->setFreeFont(TFT_FONT);
  TFTRus* rusPrinter = hal->getRusPrinter();
  
  uint8_t fontHeight = FONT_HEIGHT(dc);
  
  int displayWidth = dc->width();
  int displayHeight = dc->height();
  
  int lineSpacing = 6; 
  int topOffset = 10;
  int curX = 0;
  int curY = topOffset;

  int lineLength = 0;

  uint16_t fgColor = TFT_NAVY, bgColor = TFT_WHITE;
  
  // подложка под заголовок
  if(boxType == mbHalt && errorColors)
  {
    fgColor = TFT_RED;
  }
  else
  {
    fgColor = TFT_NAVY;
  }
    
  dc->fillRect(0, 0, displayWidth, topOffset + fontHeight+4,fgColor);
  
  if(caption)
  {
    if(boxType == mbHalt && errorColors)
    {
      bgColor = TFT_RED;
      fgColor = TFT_WHITE;
    }
    else
    {
      bgColor = TFT_NAVY;
      fgColor = TFT_WHITE;      
    }
    lineLength = rusPrinter->textWidth(caption);
    curX = (displayWidth - lineLength)/2; 
    rusPrinter->print(caption,curX,curY,bgColor,fgColor);
  }

  curY = (displayHeight - ALL_CHANNELS_BUTTON_HEIGHT - (lines.size()*fontHeight + (lines.size()-1)*lineSpacing))/2;

  for(size_t i=0;i<lines.size();i++)
  {
    lineLength = rusPrinter->textWidth(lines[i]);
    curX = (displayWidth - lineLength)/2;    
    rusPrinter->print(lines[i],curX,curY,TFT_BACK_COLOR,TFT_FONT_COLOR);
    curY += fontHeight + lineSpacing;
  }

 /* buttons->drawButtons(drawButtonsYield);

  if(boxType == mbHalt && haltInWhile)
  {
    while(1)
    {
      #ifdef USE_EXTERNAL_WATCHDOG
        updateExternalWatchdog();
      #endif      
    }
  }*/

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void MessageBoxScreen::recreateButtons()
{
 /* buttons->deleteAllButtons();
  yesButton = -1;
  noButton = -1;
  */
  TFT_Class* dc = TFTScreen->getDC();
  
  int screenWidth = dc->width();
  int screenHeight = dc->height();
  int buttonsWidth = 200;

  int numOfButtons = boxType == mbShow ? 1 : 2;

 /* int top = screenHeight - ALL_CHANNELS_BUTTON_HEIGHT - INFO_BOX_V_SPACING;
  int left = (screenWidth - (buttonsWidth*numOfButtons + INFO_BOX_V_SPACING*(numOfButtons-1)))/2;
  
  yesButton = buttons->addButton(left, top, buttonsWidth, ALL_CHANNELS_BUTTON_HEIGHT, boxType == mbShow ? "OK" : "ДА");

  if(boxType == mbConfirm)
  {
    left += buttonsWidth + INFO_BOX_V_SPACING;
    noButton = buttons->addButton(left, top, buttonsWidth, ALL_CHANNELS_BUTTON_HEIGHT, "НЕТ");  
  }*/
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void MessageBoxScreen::halt(const char* _caption, Vector<const char*>& _lines, bool _errorColors, bool _haltInWhile)
{
  if(!TFTScreen->getDC())
  {
    return;
  }
	
  lines = _lines;
  caption = _caption;
  boxType = mbHalt;
  errorColors = _errorColors;
  haltInWhile = _haltInWhile;

  /*buttons->deleteAllButtons();
  yesButton = -1;
  noButton = -1;*/
    
  targetOkScreen = NULL;
  targetCancelScreen = NULL;
  resultSubscriber = NULL;  

  TFTScreen->switchToScreen(this);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void MessageBoxScreen::show(const char* _caption, Vector<const char*>& _lines, AbstractTFTScreen* okTarget, MessageBoxResultSubscriber* sub)
{
  if(!TFTScreen->getDC())
  {
    return;
  }
	
  lines = _lines;
  caption = _caption;
  errorColors = false;

  boxType = mbShow;
  recreateButtons();
    
  targetOkScreen = okTarget;
  targetCancelScreen = NULL;
  resultSubscriber = sub;

  TFTScreen->switchToScreen(this);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void MessageBoxScreen::confirm(const char* _caption, Vector<const char*>& _lines, AbstractTFTScreen* okTarget, AbstractTFTScreen* cancelTarget, MessageBoxResultSubscriber* sub)
{
  if(!TFTScreen->getDC())
  {
    return;
  }
	
  lines = _lines;
  caption = _caption;
  errorColors = false;

  boxType = mbConfirm;
  recreateButtons();
  
  targetOkScreen = okTarget;
  targetCancelScreen = cancelTarget;
  resultSubscriber = sub;

  TFTScreen->switchToScreen(this);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
AbstractTFTScreen* MessageBoxScreen::create()
{
    MessageBox = new MessageBoxScreen();
    return MessageBox;  
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_TFT_MODULE
