
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
  tftTouch = NULL;
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

	tftDC->setFreeFont(TFT_FONT);

	tftDC->setTextColor(TFT_RED, TFT_BACK_COLOR);


	tftTouch = tftDC;

    delay(200);
    Settings.displayBacklight(true); // включаем подсветку

  TFTCalibrationData data = Settings.GetTftCalibrationData();
  if(data.isValid)
  {
    tftTouch->setTouch(data.points);
  }
  else
  {
    uint16_t dt[5] = {304, 3502, 280, 3507, 4};
    tftTouch->setTouch(dt);
  }

	tftTouch->setRotation(rot);
	tftTouch->begin();


  rusPrint.init(tftDC);

  
  resetIdleTimer();

  // добавляем служебные экраны

  // окно сообщения
  TFTScreenInfo mbscrif;
    
  //TFTTouchCalibrationScreen
  mbscrif.screen = new TFTTouchCalibrationScreen();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "TOUCH_CALIBRATION";
  screens.push_back(mbscrif);

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


  //TFTSetTimeLedLCDOff
  mbscrif.screen = new TFTSetTimeLedLCDOff();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "SetTimeLedLCDOff";
  screens.push_back(mbscrif);

  //TFTSetTimePowerOff
  mbscrif.screen = new TFTSetTimePowerOff();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "SetTimePowerOff";
  screens.push_back(mbscrif);
 
  //TFTSaveMenuScreen
  mbscrif.screen = new TFTSaveMenuScreen();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "SAVE_MENU";
  screens.push_back(mbscrif);


/*
  // клавиатура
  mbscrif.screen = KeyboardScreen::create();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "KBD";
  screens.push_back(mbscrif);
*/
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
     TFTCalibrationData dt = Settings.GetTftCalibrationData();
     
      if(dt.isValid)
      {
        
        {
          switchToScreen("MENU");
        }
      }
      else
      {
        switchToScreen("TOUCH_CALIBRATION");
      }
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

///------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTTouchCalibrationScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTTouchCalibrationScreen* TouchCalibrationScreen = NULL;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTTouchCalibrationScreen::TFTTouchCalibrationScreen()
{
	canSwitch = false;
  TouchCalibrationScreen = this;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTTouchCalibrationScreen::~TFTTouchCalibrationScreen()
{

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTTouchCalibrationScreen::setup(TFTMenu* menuManager)
{

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTTouchCalibrationScreen::update(TFTMenu* menuManager)
{
  menuManager->resetIdleTimer();                         // сбрасываем таймер ничегонеделанья, чтобы не переключилось на главный экран
  
	if (canSwitch)
	{
		canSwitch = false;
    
          
			menuManager->switchToScreen("MENU");

	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTTouchCalibrationScreen::draw(TFTMenu* menuManager)
{

	TFT_Class* dc = menuManager->getDC();
	TFTRus* rusPrinter = menuManager->getRusPrinter();

	dc->setFreeFont(TFT_SMALL_FONT);
	dc->fillScreen(TFT_BLACK);
	int fontHeight = FONT_HEIGHT(dc);
	int screenWidth = dc->width();
	int screenHeight = dc->height();
	const int v_spacing = 2;

	Vector<const char*> lines;
	lines.push_back("ТРЕБУЕТСЯ КАЛИБРОВКА ТАЧСКРИНА.");
	lines.push_back("");
	lines.push_back("НАЖИМАЙТЕ ПООЧЕРЁДНО НА УГЛЫ.");

	int top = (screenHeight - lines.size()*(fontHeight + v_spacing)) / 2;

	for (size_t i = 0; i < lines.size(); i++)
	{
		int left = (screenWidth - rusPrinter->textWidth(lines[i])) / 2;

		rusPrinter->print(lines[i], left, top, TFT_BLACK, TFT_WHITE);

		top += fontHeight + v_spacing;
	}
	delay(400);
	TFTCalibrationData calData;

	dc->calibrateTouch(calData.points, TFT_WHITE, TFT_BLACK, 30); 
	dc->setTouch(calData.points);
	Settings.SetTftCalibrationData(calData);

	dc->setFreeFont(TFT_FONT);
	canSwitch = true;

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTTimeSettingsMenuScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTTimeSettingsMenuScreen::TFTTimeSettingsMenuScreen()
 {
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTTimeSettingsMenuScreen::~TFTTimeSettingsMenuScreen()
 {
   delete screenButtons;  
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTTimeSettingsMenuScreen::onActivate(TFTMenu* menuManager)
 {

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTTimeSettingsMenuScreen::setup(TFTMenu* menuManager)
 {

   TFT_Class* dc = menuManager->getDC();

   if (!dc)
   {
     return;
   }

   screenButtons = new TFT_Buttons_Rus(dc, menuManager->getTouch(), menuManager->getRusPrinter());
   screenButtons->setTextFont(TFT_FONT);
   screenButtons->setSymbolFont(SENSOR_FONT);

   screenButtons->setButtonColors(TFT_CHANNELS_BUTTON_COLORS);

   int screenWidth = dc->width();
   int screenHeight = dc->height();

  const int v_spacing = 10;
  const int h_spacing = 5;

  // у нас 4 кнопки
  int button_width = screenWidth - h_spacing*2;
  int button_height = (screenHeight - v_spacing*6)/5;
  int left = h_spacing;
  int top = v_spacing;

  measureTimeButton = screenButtons->addButton(left, top, button_width, button_height, CAL_TIME_LCD_OFF);  // Изменить
  top += v_spacing + button_height;

  calibrationTimeButton = screenButtons->addButton(left, top, button_width, button_height, CAL_TIME_LCD_OFF); // Изменить
  top += v_spacing + button_height;

  ledLCDTimeButton = screenButtons->addButton(left, top, button_width, button_height, CAL_TIME_LCD_OFF);
  top += v_spacing + button_height;

  powerOffTimeButton = screenButtons->addButton(left, top, button_width, button_height, CAL_TIME_POWER_OFF);
  top += v_spacing + button_height;

  backButton  = screenButtons->addButton(left, top, button_width, button_height, WM_BACK_CAPTION);

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  void TFTTimeSettingsMenuScreen::update(TFTMenu* menuManager)
 {
  int pressed_button = screenButtons->checkButtons(ButtonPressed, ButtonReleased);
  if(pressed_button != -1)
  {
    if(pressed_button == backButton)
    {
      menuManager->switchToScreen("SERVICE_MENU");
    }
    else
    if(pressed_button == measureTimeButton)
    {
      menuManager->switchToScreen("MEASURE_SETTINGS");
    }
    else
    if(pressed_button == calibrationTimeButton)
    {
      menuManager->switchToScreen("CALIBRATION_SETTINGS");
    }
    else
    if (pressed_button == ledLCDTimeButton)
    {
        menuManager->switchToScreen("SetTimeLedLCDOff");
    }
    else
    if (pressed_button == powerOffTimeButton)
    {
        menuManager->switchToScreen("SetTimePowerOff");
    }
    
  }

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTTimeSettingsMenuScreen::draw(TFTMenu* menuManager)
 {

   if (screenButtons)
   {
     screenButtons->drawButtons(drawButtonsYield);
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
   delete screenButtons;  
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

   screenButtons = new TFT_Buttons_Rus(dc, menuManager->getTouch(), menuManager->getRusPrinter());
   screenButtons->setTextFont(TFT_FONT);
   screenButtons->setSymbolFont(SENSOR_FONT);

   screenButtons->setButtonColors(TFT_CHANNELS_BUTTON_COLORS);

   int screenWidth = dc->width();
   int screenHeight = dc->height();

  const int v_spacing = 8;
  const int h_spacing = 5;

  // у нас 4 кнопки
  int button_width = screenWidth - h_spacing*2;
  int button_height = (screenHeight - v_spacing*7)/6;
  int left = h_spacing;
  int top = v_spacing;

 

  setDataButton = screenButtons->addButton(left, top, button_width, button_height, TIME_CAPTION);      //К//Удалить
  top += v_spacing + button_height;

  calButton = screenButtons->addButton(left, top, button_width, button_height, CAL_CAPTION);             ////Удалить
  top += v_spacing + button_height;

  setAtmButton = screenButtons->addButton(left, top, button_width, button_height, TIME_CAPTION);            //К//Удалить
  top += v_spacing + button_height;

  timeButton = screenButtons->addButton(left, top, button_width, button_height, TIME_CAPTION);           //Кнопка "ВРЕМЯ РАБОТЫ"
  top += v_spacing + button_height;

  changePasswordButton = screenButtons->addButton(left, top, button_width, button_height, TIME_CAPTION); //Удалить
  top += v_spacing + button_height;

  backButton  = screenButtons->addButton(left, top, button_width, button_height, WM_BACK_CAPTION);       //Кнопка "< НАЗАД"

  //screenButtons->disableButton(setAtmButton);  // Отключить кнопку "ВВОД <0> ТЕСТ" на время работы

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  void TFTServiceMenuScreen::update(TFTMenu* menuManager)
 {
  int pressed_button = screenButtons->checkButtons(ButtonPressed, ButtonReleased);
  if(pressed_button != -1)
  {
    if(pressed_button == backButton)
    {
      menuManager->switchToScreen("MENU");
    }
    else
    if(pressed_button == changePasswordButton)
    {
      //menuManager->switchToScreen("VARIANT_PASSWORD");
    }
    else
    if(pressed_button == timeButton)
    {
      //menuManager->switchToScreen("PASSWORD_TIME");
    }
    else 
    if(pressed_button == setAtmButton)
    {
      //menuManager->switchToScreen("SET_ATMOSFERA");
    }
	else
	if (pressed_button == setDataButton)
	{
		//menuManager->switchToScreen("SET_DATE_TIME");
	}
	else
	if (pressed_button == calButton)
	{
		menuManager->switchToScreen("CAL_SETTINGS");
	}


    
  }

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTServiceMenuScreen::draw(TFTMenu* menuManager)
 {

   if (screenButtons)
   {
     screenButtons->drawButtons(drawButtonsYield);
   }

 }
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTCalibrationSettingsScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTCalibrationSettingsScreen::TFTCalibrationSettingsScreen()
 {
  tickerButton = -1;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTCalibrationSettingsScreen::~TFTCalibrationSettingsScreen()
 {
   delete screenButtons;
   delete settingsBox;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTCalibrationSettingsScreen::onActivate(TFTMenu* menuManager)
 {
    setting = Settings.GetCalibrationTime();
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTCalibrationSettingsScreen::setup(TFTMenu* menuManager)
 {
   TFT_Class* dc = menuManager->getDC();

   if (!dc)
   {
     return;
   }

   screenButtons = new TFT_Buttons_Rus(dc, menuManager->getTouch(), menuManager->getRusPrinter());
   screenButtons->setTextFont(TFT_FONT);
   screenButtons->setSymbolFont(SENSOR_FONT);

   screenButtons->setButtonColors(TFT_CHANNELS_BUTTON_COLORS);

   int screenWidth = dc->width();
   int screenHeight = dc->height();

   const int v_spacing = 5;

   int button_width = screenWidth - v_spacing*2;
   int button_height = 60;

   int left = v_spacing;
   int top = 42;

   upButton = screenButtons->addButton(left, top, button_width, button_height, "c");
   screenButtons->setButtonFont(upButton,VARIOUS_SYMBOLS_32x32);
   top += button_height + v_spacing;


   dc->setFreeFont(TFT_FONT);
   int fontHeight = FONT_HEIGHT(dc);
  
   settingsBox  = new TFTInfoBox("", button_width, 100, left, top - fontHeight - INFO_BOX_CONTENT_PADDING);   
   top += button_height + v_spacing*4;

   downButton = screenButtons->addButton(left, top, button_width, button_height, "d");
   screenButtons->setButtonFont(downButton,VARIOUS_SYMBOLS_32x32);
   

   backButton = screenButtons->addButton(left, screenHeight - button_height - v_spacing, button_width, button_height, WM_BACK_CAPTION);

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  void TFTCalibrationSettingsScreen::onButtonPressed(TFTMenu* menuManager, int buttonID)
 {
   tickerButton = -1;

   if(buttonID == upButton || buttonID == downButton)
   {
      tickerButton = buttonID;
      Ticker.start(this);
   }
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTCalibrationSettingsScreen::onButtonReleased(TFTMenu* menuManager, int buttonID)
 {
   Ticker.stop();
   tickerButton = -1;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTCalibrationSettingsScreen::incSetting(int val)
 {
    int32_t old = setting;
    
    setting += val;
  /*  if(setting < 1)
    {
      setting = 1;
    }
    if(setting > MAX_TIME_VALUE)
    {
      setting = MAX_TIME_VALUE;
    }

    if(old != setting)
    {
      drawValueInBox(settingsBox, String(setting));
    }*/
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTCalibrationSettingsScreen::onTick()
 {
    TFTScreen->resetIdleTimer(); // сбрасываем таймер ничегонеделанья, чтобы не переключилось на главный экран
  
   if (tickerButton == upButton)
     incSetting(3);
   else
   if (tickerButton == downButton)
     incSetting(-3);
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  void TFTCalibrationSettingsScreen::update(TFTMenu* menuManager)
 {
   if (!menuManager->getDC())
   {
     return;
   }

  int pressed_button = screenButtons->checkButtons(ButtonPressed, ButtonReleased);
  if(pressed_button != -1)
  {
    if(pressed_button == backButton)
    {
      Settings.SetCalibrationTime(setting);
      menuManager->switchToScreen("TIME_SETTINGS");
    }
    else
    if(pressed_button == upButton)
    {
       incSetting(1);
    }
    else
    if(pressed_button == downButton)
    {
       incSetting(-1);
    }
  }

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTCalibrationSettingsScreen::draw(TFTMenu* menuManager)
 {

     /*drawScreenCaption(menuManager,CAL_TIME_CAPTION);

     settingsBox->draw(menuManager);
     drawValueInBox(settingsBox, String(setting));
     
     screenButtons->drawButtons(drawButtonsYield);*/

 }

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTSetTimeLedLCDOff
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTSetTimeLedLCDOff::TFTSetTimeLedLCDOff()
 {
     stage = 0;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTSetTimeLedLCDOff::~TFTSetTimeLedLCDOff()
 {
     delete screenButtons;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTSetTimeLedLCDOff::setup(TFTMenu* menuManager)
 {
     TFT_Class* dc = menuManager->getDC();
     screenButtons = new TFT_Buttons_Rus(dc, menuManager->getTouch(), menuManager->getRusPrinter());
     screenButtons->setTextFont(TFT_FONT);
     screenButtons->setSymbolFont(SENSOR_FONT);

     screenButtons->setButtonColors(TFT_CHANNELS_BUTTON_COLORS);

     TFTRus* rusPrinter = menuManager->getRusPrinter();

     int screenWidth = dc->width();
     int screenHeight = dc->height();

     dc->setFreeFont(TFT_FONT);
     int textFontHeight = FONT_HEIGHT(dc);

     // создаём кнопки клавиатуры
     static const char* captions[] = {
      "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "C", ENTER_CAPTION
     };

     const int v_spacing = 5;
     const int buttons_in_row = 3;
     int button_width = (screenWidth - v_spacing * (1 + buttons_in_row)) / buttons_in_row;
     int button_height = textFontHeight * 2 + v_spacing * 2 + 4;
     int top = button_height + v_spacing * 3;
     int left = v_spacing;

     int row_cntr = 0;
     lastKeyButtonID = 0;
     for (int i = 0; i < sizeof(captions) / sizeof(captions[0]); i++)
     {
         if (row_cntr >= buttons_in_row)
         {
             row_cntr = 0;
             left = v_spacing;
             top += button_height + v_spacing;
         }

         lastKeyButtonID = screenButtons->addButton(left, top, button_width, button_height, captions[i]);
         left += v_spacing + button_width;
         row_cntr++;
     }

     int small_button_width = button_width;

     // добавляем текс-бокс
     textBox = screenButtons->addButton(v_spacing, v_spacing, screenWidth - v_spacing * 2, button_height, "");
     screenButtons->disableButton(textBox);
     screenButtons->setButtonInactiveFontColor(textBox, TFT_FONT_COLOR);

     // добавляем кнопку "Назад"
     button_width = screenWidth - v_spacing * 2;

     left = (screenWidth - button_width) / 2;
     top = (screenHeight - (button_height * 2 + v_spacing * 2));

     top += button_height + v_spacing;
     backButton = screenButtons->addButton(left, top, button_width, button_height, WM_BACK_CAPTION);

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTSetTimeLedLCDOff::update(TFTMenu* menuManager)
 {

     int pressed_button = screenButtons->checkButtons(ButtonPressed, ButtonReleased);
     if (pressed_button != -1)
     {
         if (pressed_button <= lastKeyButtonID) // кнопки клавиатуры
         {
             if (!strcmp(screenButtons->getLabel(pressed_button), ENTER_CAPTION))
             {
                 if (enteredTimeLCD.length() > 0)
                 {
                     // Сохранить 
                     int intVar;
                     intVar = enteredTimeLCD.toInt();
                     Settings.SetTimeLedLCD(intVar);
                 }
                 else
                 {
                     stage = 1;
                     enteredTimeLCD = "";
                     relabelStageMessage(true); // пишем приглашение
                 }
             }
             else if (!strcmp(screenButtons->getLabel(pressed_button), "C"))  // Очистить поле ввода данных
             {
                 enteredTimeLCD = "";
                 relabelStageMessage(true); // пишем приглашение
             }
             else if (enteredTimeLCD.length() < MAX_TIME_LCD_LENGTH) // защита от длинного ввода времени отключения подсветки дисплея
             {
                 enteredTimeLCD += screenButtons->getLabel(pressed_button);  // Отобразить новые данные
                 screenButtons->relabelButton(textBox, enteredTimeLCD.c_str(), true);
             }
         }
         else
         {
             // другие кнопки

             if (pressed_button == backButton)
             {
                 menuManager->switchToScreen("TIME_SETTINGS");
             }
         }
     }
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTSetTimeLedLCDOff::relabelStageMessage(bool redraw) // Приглашение к вводу новых данных
 {
     const char* message = ">Задержка сек.";
     switch (stage)
     {
     case 0: break;
     case 1: message = "> Введите данные"; break;
     }

     screenButtons->relabelButton(textBox, message, redraw);
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTSetTimeLedLCDOff::onActivate(TFTMenu* menuManager)  // Отобразить текущие данные при запуске функции
 {
     enteredTimeLCD = "";
     int time_LCD = Settings.GetTimeLedLCD();
     enteredTimeLCD = String(time_LCD, DEC);
     stage = 0;
     screenButtons->relabelButton(textBox, enteredTimeLCD.c_str(), true);
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTSetTimeLedLCDOff::draw(TFTMenu* menuManager)
 {

     // рисуем кнопки
     screenButtons->drawButtons(drawButtonsYield);

 }

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTSetTimePowerOff
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTSetTimePowerOff::TFTSetTimePowerOff()
 {
     stage = 0;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTSetTimePowerOff::~TFTSetTimePowerOff()
 {
     delete screenButtons;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTSetTimePowerOff::setup(TFTMenu* menuManager)
 {
     TFT_Class* dc = menuManager->getDC();
     screenButtons = new TFT_Buttons_Rus(dc, menuManager->getTouch(), menuManager->getRusPrinter());
     screenButtons->setTextFont(TFT_FONT);
     screenButtons->setSymbolFont(SENSOR_FONT);

     screenButtons->setButtonColors(TFT_CHANNELS_BUTTON_COLORS);

     TFTRus* rusPrinter = menuManager->getRusPrinter();

     int screenWidth = dc->width();
     int screenHeight = dc->height();

     dc->setFreeFont(TFT_FONT);
     int textFontHeight = FONT_HEIGHT(dc);

     // создаём кнопки клавиатуры
     static const char* captions[] = {
      "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "C", ENTER_CAPTION
     };

     const int v_spacing = 5;
     const int buttons_in_row = 3;
     int button_width = (screenWidth - v_spacing * (1 + buttons_in_row)) / buttons_in_row;
     int button_height = textFontHeight * 2 + v_spacing * 2 + 4;
     int top = button_height + v_spacing * 3;
     int left = v_spacing;

     int row_cntr = 0;
     lastKeyButtonID = 0;
     for (int i = 0; i < sizeof(captions) / sizeof(captions[0]); i++)
     {
         if (row_cntr >= buttons_in_row)
         {
             row_cntr = 0;
             left = v_spacing;
             top += button_height + v_spacing;
         }

         lastKeyButtonID = screenButtons->addButton(left, top, button_width, button_height, captions[i]);
         left += v_spacing + button_width;
         row_cntr++;
     }

     int small_button_width = button_width;

     // добавляем текс-бокс
     textBox = screenButtons->addButton(v_spacing, v_spacing, screenWidth - v_spacing * 2, button_height, "");
     screenButtons->disableButton(textBox);
     screenButtons->setButtonInactiveFontColor(textBox, TFT_FONT_COLOR);

     // добавляем кнопку "Назад"
     button_width = screenWidth - v_spacing * 2;

     left = (screenWidth - button_width) / 2;
     top = (screenHeight - (button_height * 2 + v_spacing * 2));

     top += button_height + v_spacing;
     backButton = screenButtons->addButton(left, top, button_width, button_height, WM_BACK_CAPTION);

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTSetTimePowerOff::update(TFTMenu* menuManager)
 {

     int pressed_button = screenButtons->checkButtons(ButtonPressed, ButtonReleased);
     if (pressed_button != -1)
     {
         if (pressed_button <= lastKeyButtonID) // кнопки клавиатуры
         {
             if (!strcmp(screenButtons->getLabel(pressed_button), ENTER_CAPTION))
             {
                 if (enteredPowerOff.length() > 0)
                 {
                     // Сохранить 
                     int intVar;
                     intVar = enteredPowerOff.toInt();
                     Settings.SetTimePowerOff(intVar);
                 }
                 else
                 {
                     stage = 1;
					 enteredPowerOff = "";
                     relabelStageMessage(true); // пишем приглашение
                 }
             }
             else if (!strcmp(screenButtons->getLabel(pressed_button), "C"))
             {
				 enteredPowerOff = "";
                 relabelStageMessage(true); // пишем приглашение
             }
             else if (enteredPowerOff.length() < MAX_TIME_POWER_LENGTH) // защита от длинного ввода таймера отключения питания
             {
				 enteredPowerOff += screenButtons->getLabel(pressed_button);
                 screenButtons->relabelButton(textBox, enteredPowerOff.c_str(), true);
             }
         }
         else
         {
             // другие кнопки

             if (pressed_button == backButton)
             {
                 menuManager->switchToScreen("TIME_SETTINGS");
             }
         }
     }
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTSetTimePowerOff::relabelStageMessage(bool redraw)
 {
     const char* message = ">Задержка сек.";
     switch (stage)
     {
     case 0: break;
     case 1: message = "> Введите данные"; break;
     }

     screenButtons->relabelButton(textBox, message, redraw);
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTSetTimePowerOff::onActivate(TFTMenu* menuManager)
 {
	 enteredPowerOff = "";
     int PowerOff = Settings.GetTimePowerOff();
	 enteredPowerOff = String(PowerOff, DEC);
     stage = 0;
     screenButtons->relabelButton(textBox, enteredPowerOff.c_str(), true);
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTSetTimePowerOff::draw(TFTMenu* menuManager)
 {

     // рисуем кнопки
     screenButtons->drawButtons(drawButtonsYield);

 }

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTSaveMenuScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTSaveMenuScreen::TFTSaveMenuScreen()
 {
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTSaveMenuScreen::~TFTSaveMenuScreen()
 {
     delete screenButtons;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTSaveMenuScreen::onActivate(TFTMenu* menuManager)
 {

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTSaveMenuScreen::setup(TFTMenu* menuManager)
 {


     TFT_Class* dc = menuManager->getDC();

     if (!dc)
     {
         return;
     }

     screenButtons = new TFT_Buttons_Rus(dc, menuManager->getTouch(), menuManager->getRusPrinter());
     screenButtons->setTextFont(TFT_FONT);
     screenButtons->setSymbolFont(SENSOR_FONT);

     screenButtons->setButtonColors(TFT_CHANNELS_BUTTON_COLORS);

     int screenWidth = dc->width();
     int screenHeight = dc->height();

     const int v_spacing = 8;
     const int h_spacing = 5;

     // у нас 4 кнопки
     int button_width = screenWidth - h_spacing * 2;
     int button_height = (screenHeight - v_spacing * 7) / 6;
     int left = h_spacing;
     int top = v_spacing;



     setDataButton = screenButtons->addButton(left, top, button_width, button_height, SET_FREE);       //Кнопка " "
     top += v_spacing + button_height;

     calButton = screenButtons->addButton(left, top, button_width, button_height, SET_FREE);           //Кнопка " "
     top += v_spacing + button_height;

     setAtmButton = screenButtons->addButton(left, top, button_width, button_height, SET_FREE);        //Кнопка " "
     top += v_spacing + button_height;

     timeButton = screenButtons->addButton(left, top, button_width, button_height, SET_FREE);           //Кнопка " "
     top += v_spacing + button_height;

     changePasswordButton = screenButtons->addButton(left, top, button_width, button_height, SET_CONTROLLER_ID); //Кнопка " "
     top += v_spacing + button_height;

     backButton = screenButtons->addButton(left, top, button_width, button_height, WM_BACK_CAPTION);    //Кнопка "< НАЗАД"
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTSaveMenuScreen::update(TFTMenu* menuManager)
 {
    int pressed_button = screenButtons->checkButtons(ButtonPressed, ButtonReleased);
    if (pressed_button != -1)
    {
        if (pressed_button == backButton)
        {
            menuManager->switchToScreen("MENU");
        }
        else if (pressed_button == changePasswordButton)
        {
           // menuManager->switchToScreen("VARIANT_PASSWORD");
        }
        else if (pressed_button == timeButton)
        {
          //  menuManager->switchToScreen("PASSWORD_TIME");
        }
        else if (pressed_button == setAtmButton)
        {
           // menuManager->switchToScreen("SET_ATMOSFERA");
        }
        else if (pressed_button == setDataButton)
        {
           // menuManager->switchToScreen("SET_DATE_TIME");
        }
        else if (pressed_button == calButton)
        {
          //  menuManager->switchToScreen("CAL_SETTINGS");
        }
     }
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTSaveMenuScreen::draw(TFTMenu* menuManager)
 {

     if (screenButtons)
     {
         screenButtons->drawButtons(drawButtonsYield);
     }

 }

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTMenuScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

 TFTMenuScreen* MainScreen = NULL;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTMenuScreen::TFTMenuScreen()
 {
  tickerButton = -1;
  MainScreen = this;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 TFTMenuScreen::~TFTMenuScreen()
 {
	 delete screenButtons;	
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

	 screenButtons = new TFT_Buttons_Rus(dc, menuManager->getTouch(), menuManager->getRusPrinter());


	 screenButtons->setTextFont(TFT_FONT);
	 screenButtons->setSymbolFont(SENSOR_FONT);

	 screenButtons->setButtonColors(TFT_CHANNELS_BUTTON_COLORS);

	 int screenWidth = dc->width();
	 int screenHeight = dc->height();

   const int BUTTON_WIDTH = 220;
   const int BUTTON_HEIGHT = 50;
   const int BUTTON_HEIGHT1 = 30;
   const int V_SPACING = 10;
   const int V_SPACING1 = 60;

   int left = (screenWidth - BUTTON_WIDTH)/2;
   int top = (screenHeight - (BUTTON_HEIGHT * 4 + V_SPACING1)) / 2;

   //startButton = screenButtons->addButton(left, top + (BUTTON_HEIGHT/2), BUTTON_WIDTH, BUTTON_HEIGHT*2 + (BUTTON_HEIGHT / 2), START_CAPTION); // кнопка "ПУСК"
   //screenButtons->setButtonBackColor(startButton,TFT_FONT_COLOR);
   //screenButtons->setButtonFontColor(startButton,TFT_BACK_COLOR);
  //
  // top += BUTTON_HEIGHT*3 + V_SPACING;
  //// saveButton = screenButtons->addButton(left, top, BUTTON_WIDTH, BUTTON_HEIGHT1, WM_MENU_CAPTION);  //Кнопка "СОХРАНИТЬ" на главном экране
  // top += BUTTON_HEIGHT1 * 1 + V_SPACING;
  // menuButton = screenButtons->addButton(left, top+40, BUTTON_WIDTH, BUTTON_HEIGHT1, MENU_CAPTION);     //Кнопка "СЕРВИСНОЕ МЕНЮ" на главном экране
  // tmr = millis();

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  void TFTMenuScreen::onButtonPressed(TFTMenu* menuManager, int buttonID)
 {
	 tickerButton = -1;

	 //if (buttonID == dec25PercentsButton || buttonID == inc25PercentsButton || buttonID == dec50PercentsButton
		// || buttonID == inc50PercentsButton || buttonID == dec75PercentsButton || buttonID == inc75PercentsButton
		// || buttonID == dec100PercentsButton || buttonID == inc100PercentsButton)
	 //{
		// tickerButton = buttonID;
		// Ticker.start(this);
	 //}
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::onButtonReleased(TFTMenu* menuManager, int buttonID)
 {
	 Ticker.stop();
	 tickerButton = -1;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void TFTMenuScreen::onTick()
 {
    TFTScreen->resetIdleTimer(); // сбрасываем таймер ничегонеделанья, чтобы не переключилось на главный экран
  
	/* if (tickerButton == dec25PercentsButton)
		 inc25Temp(-3);
	 else
		 if (tickerButton == inc25PercentsButton)
			 inc25Temp(3);
		 else
			 if (tickerButton == dec50PercentsButton)
				 inc50Temp(-3);
			 else
				 if (tickerButton == inc50PercentsButton)
					 inc50Temp(3);
				 else
					 if (tickerButton == dec75PercentsButton)
						 inc75Temp(-3);
					 else
						 if (tickerButton == inc75PercentsButton)
							 inc75Temp(3);
						 else
							 if (tickerButton == dec100PercentsButton)
								 inc100Temp(-3);
							 else
								 if (tickerButton == inc100PercentsButton)
									 inc100Temp(3);*/
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

   // проверяем на перемещение курсора
   TOUCH_Class* touch = menuManager->getTouch();

   uint16_t touch_x, touch_y;

   if (touch->getTouch(&touch_x, &touch_y))
   {

       Serial.println(touch_x);
       Serial.println(touch_y);

   }


  // chargeControl(menuManager); // Отображение заряда аккумуляторов
  // 	  
  //int pressed_button = screenButtons->checkButtons(ButtonPressed, ButtonReleased);
  //if(pressed_button != -1)
  //{
  //  //if(pressed_button == startButton)
  //  //{
  //  //  menuManager->switchToScreen("MEASURE");
  //  //}
  //  //else
  //  //if(pressed_button == saveButton)
  //  //{
  //  //   menuManager->switchToScreen("SAVE_MENU");  // Запись результата измерения
  //  //}
  //  //else
  //  //if (pressed_button == menuButton)
  //  //{
  //  //    menuManager->switchToScreen("PASSWORD");
  //  //}
  //}

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


	 //if (screenButtons)
	 //{
		// screenButtons->drawButtons(drawButtonsYield);
	 //}


     int screenWidth = dc->width();
     int screenHeight = dc->height();

     dc->setFreeFont(TFT_SMALL_FONT);
     int textFontHeight = FONT_HEIGHT(dc);

     String data = SOFTWARE_VERSION;

     int textFontWidth = dc->textWidth(data);              // Returns pixel width of string in current font
     uint16_t curX = screenWidth - textFontWidth - 10;     // Координаты вывода 
     uint16_t curY = 5;// 305;                             // Координаты вывода версии



     rusPrinter->print(data.c_str(), curX, curY, TFT_BLACK, TFT_WHITE); // Отображаем версию программы

    dc->setFreeFont(TFT_FONT);

    dc->drawCircle(120, 135, 100, TFT_WHITE);
    dc->drawCircle(120, 135, 99, TFT_WHITE);
    dc->drawCircle(120, 135, 60, TFT_YELLOW);
    dc->fillCircle(120, 135, 5, TFT_WHITE);
    dc->fillTriangle(112, 53, 120, 38, 128, 53, TFT_BLUE);
    dc->drawTriangle(187, 130, 195, 115, 203, 130, TFT_GREEN);

    dc->drawTriangle(107, 170, 115, 155, 123, 170, TFT_RED);

    dc->setFreeFont(TFT_FONT);
   // dc->setSymbolFont(SENSOR_FONT);
    dc->setCursor(5, 50);
    dc->print("10KM");
    dc->setCursor(112, 72);
    dc->print("N");
    dc->setCursor(160, 150);
    dc->print("-1230");

    dc->setCursor(80, 190);
    dc->print("-950");
     //dc->print("TEST");







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

	 dc->setFreeFont(TFT_SMALL_FONT);
	 int textFontHeight = FONT_HEIGHT(dc);

	 String data = SOFTWARE_VERSION;

	 int textFontWidth = dc->textWidth(data);              // Returns pixel width of string in current font
	 uint16_t curX = screenWidth - textFontWidth - 10;     // Координаты вывода 
	 uint16_t curY = 5;// 305;                             // Координаты вывода версии



	 rusPrinter->print(data.c_str(), curX, curY, TFT_WHITE, TFT_BLACK); // Отображаем версию программы

	 dc->setFreeFont(TFT_FONT);

	

	 dc->setFreeFont(TFT_FONT);

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

     dc->setFreeFont(TFT_FONT);
     //dc->setFreeFont(TFT_SMALL_FONT);
      uint16_t curX = 10;     // Координаты вывода WiFi
      uint16_t curY = 2;
      bool WiFi_On = Settings.GetWiFiState();
      bool WiFi_Connect = Settings.GetWiFiConnect();   // Признак подключения к роутеру

      WiFi_On = true;                 // Признак подключения модуля в работу
     // WiFi_Connect = true;            // Признак подключения к роутеру

      if (WiFi_On)
      {
          if (WiFi_Connect)
          {
              rusPrinter->print("           ", curX, curY, TFT_WHITE, TFT_WHITE);
              rusPrinter->print("WiFi on    ", curX, curY, TFT_WHITE, TFT_DARKGREEN);

          }
          else
          {
              rusPrinter->print("WiFi off", curX, curY, TFT_WHITE, TFT_RED);

          }
      }
      else
      {
          rusPrinter->print("           ", curX, curY, TFT_WHITE, TFT_WHITE);

      }


     //RTC_DS3231  rtc = Settings.GetClock();
     // DateTime now = rtc.now();

     //RTCTime  now = rtc.getTime();  // обновить время

     //uint16_t curX = 10;     // Координаты вывода даты
     //uint16_t curY = 18;
     //

     // //получаем компоненты даты в виде строк
     //String strDate = rtc.getDateStr(now);
     //String strTime = rtc.getTimeStr(now);

     //rusPrinter->print(strDate.c_str(), curX, curY, TFT_WHITE, TFT_BLACK);

     //dc->setFreeFont(TFT_FONT);

     //strTime += " ";

     //curX = 130;   // Координаты вывода времени
     //curY = 18;   // Координаты вывода 
     //rusPrinter->print(strTime.c_str(), curX, curY, TFT_WHITE, TFT_BLACK);
 }


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// KeyboardScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
KeyboardScreen* Keyboard;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
KeyboardScreen::KeyboardScreen() : AbstractTFTScreen()
{
  inputTarget = NULL;
  maxLen = 20;
  isRusInput = true;
  
  if(!TFTScreen->getDC())
  {
    return;
  }
  

  buttons = new TFT_Buttons_Rus(TFTScreen->getDC(), TFTScreen->getTouch(),TFTScreen->getRusPrinter(),60);
  
  buttons->setTextFont(TFT_FONT);
  buttons->setButtonColors(TFT_CHANNELS_BUTTON_COLORS);
  buttons->setSymbolFont(VARIOUS_SYMBOLS_32x32);

  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
KeyboardScreen::~KeyboardScreen()
{
  for(size_t i=0;i<captions.size();i++)
  {
    delete captions[i];
  }
  for(size_t i=0;i<altCaptions.size();i++)
  {
    delete altCaptions[i];
  }
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void KeyboardScreen::switchInput(bool redraw)
{
  isRusInput = !isRusInput;
  Vector<String*>* pVec = isRusInput ? &captions : &altCaptions;

  // у нас кнопки изменяемой клавиатуры начинаются с индекса 10
  size_t startIdx = 10;

  for(size_t i=startIdx;i<pVec->size();i++)
  {
    buttons->relabelButton(i,(*pVec)[i]->c_str(),redraw);
  }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void KeyboardScreen::setup(TFTMenu* dc)
{
  if(!dc->getDC())
  {
    return;
  }
	
  createKeyboard(dc);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void KeyboardScreen::update(TFTMenu* menu)
{
  if(!menu->getDC())
  {
    return;
  }
	
    // тут обновляем внутреннее состояние
    // раз нас вызвали, то пока не нажмут кнопки - мы не выйдем, поэтому всегда сообщаем, что на экране что-то происходит
    menu->resetIdleTimer();

    // мигаем курсором
    static uint32_t cursorTimer = millis();
    if(millis() - cursorTimer > 500)
    {
      static bool cursorVisible = true;
      cursorVisible = !cursorVisible;

      redrawCursor(menu,cursorVisible);

      cursorTimer = millis();
    }
    
    // проверяем на перемещение курсора
    TOUCH_Class* touch = menu->getTouch();

    uint16_t touch_x, touch_y;
    
    if(touch->getTouch(&touch_x, &touch_y))
    {
      // проверяем на попадание в прямоугольную область ввода текста
      TFT_Class* dc = menu->getDC();
      dc->setFreeFont(TFT_FONT);
      
      int screenWidth = dc->width();
      const int fontWidth = 8;
      
      if(touch_x >= KBD_SPACING && touch_x <= (screenWidth - KBD_SPACING) && touch_y >= KBD_SPACING && touch_y <= (KBD_SPACING + KBD_BUTTON_HEIGHT))
      {
       
        // кликнули на области ввода, ждём отпускания тача
        while (touch->getTouch(&touch_x, &touch_y)) { yield(); }
        

        // вычисляем, на какой символ приходится клик тачем
        int symbolNum = touch_x/fontWidth - 1;
        
        if(symbolNum < 0)
          symbolNum = 0;
          
        int valLen = menu->getRusPrinter()->getStringLength(inputVal.c_str());

        if(symbolNum > valLen)
          symbolNum = valLen;

        redrawCursor(menu,true);
        cursorPos = symbolNum;
        redrawCursor(menu,false);
      }
    } // if (touch->dataAvailable())
  
    int pressed_button = buttons->checkButtons(ButtonPressed, ButtonReleased);
    if(pressed_button != -1)
    {
      
       if(pressed_button == backspaceButton)
       {
        // удалить последний введённый символ
        drawValue(menu,true);
       }
       else
       if(pressed_button == okButton)
       {
          // закрыть всё нафик
          if(inputTarget)
          {
            inputTarget->onKeyboardInputResult(inputVal,true);
            inputVal = "";
          }
       }
        else
       if(pressed_button == switchButton)
       {
          // переключить раскладку
          switchInput(true);
       }
       else
       if(pressed_button == cancelButton)
       {
          // закрыть всё нафик
          if(inputTarget)
          {
            inputTarget->onKeyboardInputResult(inputVal,false);
            inputVal = "";
          }
       }
       else
       {
         // одна из кнопок клавиатуры, добавляем её текст к буферу, но - в позицию курсора!!!
         int oldLen = menu->getRusPrinter()->getStringLength(inputVal.c_str());
         const char* lbl = buttons->getLabel(pressed_button);
         
         if(!oldLen) // пустая строка
         {
          inputVal = lbl;
         }
         else
         if(oldLen < maxLen)
         {
            
            String buff;            
            const char* ptr = inputVal.c_str();
            
            for(int i=0;i<oldLen;i++)
            {
              unsigned char curChar = (unsigned char) *ptr;
              unsigned int charSz = utf8GetCharSize(curChar);
              for(byte k=0;k<charSz;k++) 
              {
                utf8Bytes[k] = *ptr++;
              }
              utf8Bytes[charSz] = '\0'; // добавляем завершающий 0
              
              if(i == cursorPos)
              {
                buff += lbl;
              }
              
              buff += utf8Bytes;
              
            } // for

            if(cursorPos >= oldLen)
              buff += lbl;

          inputVal = buff;
          
         } // if(oldLen < maxLen)
         

          int newLen = menu->getRusPrinter()->getStringLength(inputVal.c_str());

          if(newLen <= maxLen)
          {
            drawValue(menu);
                     
            if(newLen != oldLen)
            {
              redrawCursor(menu,true);
              cursorPos++;
              redrawCursor(menu,false);
            }
            
          }
          

         
       } // else одна из кнопок клавиатуры
    
    } // if(pressed_button != -1)
    
    
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void KeyboardScreen::draw(TFTMenu* menu)
{
  if(!menu->getDC())
  {
    return;
  }
	

  buttons->drawButtons(drawButtonsYield);

  TFT_Class* dc = menu->getDC();
  int screenWidth = dc->width();
  dc->drawRoundRect(KBD_SPACING, KBD_SPACING, screenWidth-KBD_SPACING*2, KBD_BUTTON_HEIGHT,2, TFT_LIGHTGREY);

  drawValue(menu);
  redrawCursor(menu,false);
}
//--------------------------------------------------------------------------------------------------------------------------------
void KeyboardScreen::redrawCursor(TFTMenu* menu, bool erase)
{
  TFT_Class* dc = menu->getDC();
  TFTRus* rus = menu->getRusPrinter();

  dc->setFreeFont(TFT_FONT);
  uint8_t fontHeight = FONT_HEIGHT(dc);

  int top = KBD_SPACING + (KBD_BUTTON_HEIGHT - fontHeight)/2;
  
  String tmp = inputVal.substring(0,cursorPos);
  
  int left = KBD_SPACING*2 + rus->textWidth(tmp.c_str());

  uint16_t fgColor = TFT_BACK_COLOR;

  if(erase)
    fgColor = TFT_BACK_COLOR;
  else
    fgColor = TFT_FONT_COLOR;
  
  dc->fillRect(left,top,1,fontHeight,fgColor);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void KeyboardScreen::drawValue(TFTMenu* menu, bool deleteCharAtCursor)
{
  if(!inputVal.length())
    return;


   TFT_Class* dc = menu->getDC();

  if(deleteCharAtCursor)
  {
    // надо удалить символ слева от позиции курсора.

    String buff;
    int len = menu->getRusPrinter()->getStringLength(inputVal.c_str());
    const char* ptr = inputVal.c_str();
    
    for(int i=0;i<len;i++)
    {
      unsigned char curChar = (unsigned char) *ptr;
      unsigned int charSz = utf8GetCharSize(curChar);
      for(byte k=0;k<charSz;k++) 
      {
        utf8Bytes[k] = *ptr++;
      }
      utf8Bytes[charSz] = '\0'; // добавляем завершающий 0
      
      if(i != (cursorPos-1)) // игнорируем удаляемый символ
      {
        buff += utf8Bytes;
      }
      
    } // for
    
    buff += ' '; // маскируем последний символ для корректной перерисовки на экране
    inputVal = buff;

  }

  dc->setFreeFont(TFT_FONT);
  
  uint8_t fontHeight = FONT_HEIGHT(dc);


  int top = KBD_SPACING + (KBD_BUTTON_HEIGHT - fontHeight)/2;
  int left = KBD_SPACING*2;

  menu->getRusPrinter()->print(inputVal.c_str(),left,top,TFT_BACK_COLOR,TFT_FONT_COLOR);

  if(deleteCharAtCursor)
  {
    // если надо удалить символ слева от позиции курсора, то в этом случае у нас последний символ - пробел, и мы его удаляем
    inputVal.remove(inputVal.length()-1,1);

    redrawCursor(menu,true);

    cursorPos--;
    if(cursorPos < 0)
      cursorPos = 0;

    redrawCursor(menu,false);
  }
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void KeyboardScreen::createKeyboard(TFTMenu* menu)
{
  buttons->deleteAllButtons();

  TFT_Class* dc = menu->getDC();
  int screenWidth = dc->width();
  int screenHeight = dc->height();  

  // создаём клавиатуру

  int colCounter = 0;
  int left = KBD_SPACING;
  int top = KBD_SPACING*2 + KBD_BUTTON_HEIGHT;

  // сперва у нас кнопки 0-9
  for(uint8_t i=0;i<10;i++)
  {
    char c = '0' + i;
    String* s = new String(c);
    captions.push_back(s);

    String* altS = new String(c);
    altCaptions.push_back(altS);    

    /*int addedBtn = */buttons->addButton(left, top, KBD_BUTTON_WIDTH, KBD_BUTTON_HEIGHT, s->c_str());
   // buttons->setButtonBackColor(addedBtn, VGA_GRAY);
   // buttons->setButtonFontColor(addedBtn, VGA_BLACK);
    
    left += KBD_BUTTON_WIDTH + KBD_SPACING;
    colCounter++;
    if(colCounter >= KBD_BUTTONS_IN_ROW)
    {
      colCounter = 0;
      left = KBD_SPACING;
      top += KBD_SPACING + KBD_BUTTON_HEIGHT;
    }
  }
  // затем - А-Я
  const char* letters[] = {
    "А", "Б", "В", "Г", "Д", "Е",
    "Ж", "З", "И", "Й", "К", "Л",
    "М", "Н", "О", "П", "Р", "С",
    "Т", "У", "Ф", "Х", "Ц", "Ч",
    "Ш", "Щ", "Ъ", "Ы", "Ь", "Э",
    "Ю", "Я", NULL
  };

  const char* altLetters[] = {
    "A", "B", "C", "D", "E", "F",
    "G", "H", "I", "J", "K", "L",
    "M", "N", "O", "P", "Q", "R",
    "S", "T", "U", "V", "W", "X",
    "Y", "Z", ".", ",", ":", ";",
    "!", "?", NULL
  };  

  int lettersIterator = 0;
  while(letters[lettersIterator])
  {
    String* s = new String(letters[lettersIterator]);
    captions.push_back(s);

    String* altS = new String(altLetters[lettersIterator]);
    altCaptions.push_back(altS);

    buttons->addButton(left, top, KBD_BUTTON_WIDTH, KBD_BUTTON_HEIGHT, s->c_str());
    left += KBD_BUTTON_WIDTH + KBD_SPACING;
    colCounter++;
    if(colCounter >= KBD_BUTTONS_IN_ROW)
    {
      colCounter = 0;
      left = KBD_SPACING;
      top += KBD_SPACING + KBD_BUTTON_HEIGHT;
    } 

    lettersIterator++;
  }
  // затем - кнопка переключения ввода
    switchButton = buttons->addButton(left, top, KBD_BUTTON_WIDTH, KBD_BUTTON_HEIGHT, "q", BUTTON_SYMBOL);
    buttons->setButtonBackColor(switchButton, TFT_MAROON);
    buttons->setButtonFontColor(switchButton, TFT_WHITE);

    left += KBD_BUTTON_WIDTH + KBD_SPACING;
  
  // затем - пробел,
    spaceButton = buttons->addButton(left, top, KBD_BUTTON_WIDTH*5 + KBD_SPACING*4, KBD_BUTTON_HEIGHT, " ");
    //buttons->setButtonBackColor(spaceButton, VGA_GRAY);
    //buttons->setButtonFontColor(spaceButton, VGA_BLACK);
       
    left += KBD_BUTTON_WIDTH*5 + KBD_SPACING*5;
   
  // backspace, 
    backspaceButton = buttons->addButton(left, top, KBD_BUTTON_WIDTH*2 + KBD_SPACING, KBD_BUTTON_HEIGHT, ":", BUTTON_SYMBOL);
    buttons->setButtonBackColor(backspaceButton, TFT_MAROON);
    buttons->setButtonFontColor(backspaceButton, TFT_WHITE);

    left = KBD_SPACING;
    top = screenHeight - KDB_BIG_BUTTON_HEIGHT - KBD_SPACING;
   
  // OK,
    int okCancelButtonWidth = (screenWidth - KBD_SPACING*3)/2;
    okButton = buttons->addButton(left, top, okCancelButtonWidth, KDB_BIG_BUTTON_HEIGHT, "OK");
    left += okCancelButtonWidth + KBD_SPACING;
  
  // CANCEL
    cancelButton = buttons->addButton(left, top, okCancelButtonWidth, KDB_BIG_BUTTON_HEIGHT, "ОТМЕНА");

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void KeyboardScreen::applyType(KeyboardType keyboardType)
{
  if(ktFull == keyboardType)
  {
    buttons->enableButton(spaceButton);
    buttons->enableButton(switchButton);

    // включаем все кнопки
    // у нас кнопки изменяемой клавиатуры начинаются с индекса 10
    size_t startIdx = 10;
  
    for(size_t i=startIdx;i<altCaptions.size();i++)
    {
      buttons->enableButton(i);
    }    

    isRusInput = false;
    switchInput(false);

    return;
  }

  if(ktNumbers == keyboardType)
  {
    buttons->disableButton(spaceButton);
    buttons->disableButton(switchButton);

    // выключаем все кнопки, кроме номеров и точки
    // у нас кнопки изменяемой клавиатуры начинаются с индекса 10
    size_t startIdx = 10;
  
    for(size_t i=startIdx;i<altCaptions.size();i++)
    {
      if(*(altCaptions[i]) != ".")
        buttons->disableButton(i);
    }        

    isRusInput = true;
    switchInput(false);

    return;
  }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void KeyboardScreen::show(const String& val, int ml, KeyboardInputTarget* it, KeyboardType keyboardType, bool eng)
{
  if(!TFTScreen->getDC())
  {
    return;
  }
	
  inputVal = val;
  inputTarget = it;
  maxLen = ml;

  cursorPos = TFTScreen->getRusPrinter()->getStringLength(inputVal.c_str());

  applyType(keyboardType);

  if(eng && isRusInput)
    switchInput(false);

  TFTScreen->switchToScreen(this);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
AbstractTFTScreen* KeyboardScreen::create()
{
    Keyboard = new KeyboardScreen();
    return Keyboard;  
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
  

  buttons = new TFT_Buttons_Rus(TFTScreen->getDC(), TFTScreen->getTouch(),TFTScreen->getRusPrinter());
  
  buttons->setTextFont(TFT_FONT);
  buttons->setButtonColors(TFT_CHANNELS_BUTTON_COLORS);
   
  
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
	
    // тут обновляем внутреннее состояние    
 
    int pressed_button = buttons->checkButtons(ButtonPressed, ButtonReleased);
    if(pressed_button != -1)
    {
      // сообщаем, что у нас нажата кнопка
      dc->resetIdleTimer();
      
       if(pressed_button == noButton && targetCancelScreen)
       {
        if(resultSubscriber)
          resultSubscriber->onMessageBoxResult(false);
          
        dc->switchToScreen(targetCancelScreen);
       }
       else
       if(pressed_button == yesButton && targetOkScreen)
       {
          if(resultSubscriber)
            resultSubscriber->onMessageBoxResult(true);
            
            dc->switchToScreen(targetOkScreen);
       }
    
    } // if(pressed_button != -1)

    
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

  buttons->drawButtons(drawButtonsYield);

  if(boxType == mbHalt && haltInWhile)
  {
    while(1)
    {
      #ifdef USE_EXTERNAL_WATCHDOG
        updateExternalWatchdog();
      #endif      
    }
  }

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void MessageBoxScreen::recreateButtons()
{
  buttons->deleteAllButtons();
  yesButton = -1;
  noButton = -1;
  
  TFT_Class* dc = TFTScreen->getDC();
  
  int screenWidth = dc->width();
  int screenHeight = dc->height();
  int buttonsWidth = 200;

  int numOfButtons = boxType == mbShow ? 1 : 2;

  int top = screenHeight - ALL_CHANNELS_BUTTON_HEIGHT - INFO_BOX_V_SPACING;
  int left = (screenWidth - (buttonsWidth*numOfButtons + INFO_BOX_V_SPACING*(numOfButtons-1)))/2;
  
  yesButton = buttons->addButton(left, top, buttonsWidth, ALL_CHANNELS_BUTTON_HEIGHT, boxType == mbShow ? "OK" : "ДА");

  if(boxType == mbConfirm)
  {
    left += buttonsWidth + INFO_BOX_V_SPACING;
    noButton = buttons->addButton(left, top, buttonsWidth, ALL_CHANNELS_BUTTON_HEIGHT, "НЕТ");  
  }
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

  buttons->deleteAllButtons();
  yesButton = -1;
  noButton = -1;
    
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
// TickerClass
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TickerClass Ticker;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TickerClass::TickerClass()
{
  started = false;
  beforeStartTickInterval = 1000;
  tickInterval = 100;
  waitBefore = true;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TickerClass::~TickerClass()
{
  stop();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TickerClass::setIntervals(uint16_t _beforeStartTickInterval,uint16_t _tickInterval)
{
  beforeStartTickInterval = _beforeStartTickInterval;
  tickInterval = _tickInterval;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TickerClass::start(ITickHandler* h)
{
  if(started)
    return;

  handler = h;

  timer = millis();
  waitBefore = true;
  started = true;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TickerClass::stop()
{
  if(!started)
    return;

  handler = NULL;

  started = false;
  waitBefore = true;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TickerClass::tick()
{
  if(!started)
    return;

  uint32_t now = millis();

  if(waitBefore)
  {
    if(now - timer > beforeStartTickInterval)
    {
      waitBefore = false;
      timer = now;
      if(handler)
        handler->onTick();
    }
    return;
  }

  if(now - timer > tickInterval)
  {
    timer = now;
    if(handler)
      handler->onTick();
  }

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_TFT_MODULE
