#include "TFTMenu.h"

#ifdef USE_TFT_MODULE

#include "Settings.h"
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
void drawValueInBox(TFTInfoBox* box, const String& strVal, FONTTYPE font)
{
	TFT_Class* dc = TFTScreen->getDC();

	if (!dc)
	{
		return;
	}

	TFTRus* rusPrinter = TFTScreen->getRusPrinter();

	TFTInfoBoxContentRect rc = box->getContentRect(TFTScreen);
	dc->fillRect(rc.x, rc.y, rc.w, rc.h, INFO_BOX_BACK_COLOR);
	yield();

	dc->setFreeFont(font);


	int fontHeight = FONT_HEIGHT(dc);
	int strLen = rusPrinter->textWidth(strVal.c_str());

	int leftPos = rc.x + (rc.w - strLen) / 2;
	int topPos = rc.y + (rc.h - fontHeight) / 2;
	rusPrinter->print(strVal.c_str(), leftPos, topPos, INFO_BOX_BACK_COLOR, SENSOR_BOX_FONT_COLOR);
	yield();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawValueInBox(TFTInfoBox* box, int val)
{
	return drawValueInBox(box, String(val));
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawValueInBox(TFTInfoBox* box, int16_t val)
{
	return drawValueInBox(box, String(val));
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawValueInBox(TFTInfoBox* box, uint16_t val)
{
	return drawValueInBox(box, String(val));
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawValueInBox(TFTInfoBox* box, int8_t val)
{
	return drawValueInBox(box, String(val));
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawValueInBox(TFTInfoBox* box, uint8_t val)
{
	return drawValueInBox(box, String(val));
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawValueInBox(TFTInfoBox* box, uint32_t val)
{
	return drawValueInBox(box, String(val));
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
	if (!dc)
	{
		return;
	}

	dc->setFreeFont(TFT_FONT);

	menuManager->getRusPrinter()->print(caption, posX + captionXOffset, posY, TFT_BACK_COLOR, INFO_BOX_CAPTION_COLOR);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTInfoBox::draw(TFTMenu* menuManager)
{
	drawCaption(menuManager, boxCaption);

	int curTop = posY;

	TFT_Class* dc = menuManager->getDC();
	if (!dc)
	{
		return;
	}

	dc->setFreeFont(TFT_FONT);

	int fontHeight = FONT_HEIGHT(dc);

	////curTop += fontHeight + INFO_BOX_CONTENT_PADDING;

	//dc->fillRoundRect(posX, curTop, boxWidth, (boxHeight - fontHeight - INFO_BOX_CONTENT_PADDING), 2, INFO_BOX_BACK_COLOR);

	//yield();

	//dc->drawRoundRect(posX, curTop, boxWidth, (boxHeight - fontHeight - INFO_BOX_CONTENT_PADDING), 2, INFO_BOX_BORDER_COLOR);

	//yield();

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTInfoBoxContentRect TFTInfoBox::getContentRect(TFTMenu* menuManager)
{
	TFTInfoBoxContentRect result;
	TFT_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return result;
	}

	dc->setFreeFont(TFT_FONT);

	int fontHeight = FONT_HEIGHT(dc);

	/*result.x = posX + INFO_BOX_CONTENT_PADDING;
	result.y = posY + fontHeight + INFO_BOX_CONTENT_PADDING * 2;

	result.w = boxWidth - INFO_BOX_CONTENT_PADDING * 2;
	result.h = boxHeight - (fontHeight + INFO_BOX_CONTENT_PADDING * 3);*/

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
	int rot = 0;
	int dRot = 0;


	tftDC = new TFT_eSPI();

	tftDC->init();
	tftDC->setRotation(dRot);
	tftDC->fillScreen(TFT_BACK_COLOR);

	tftDC->setFreeFont(TFT_FONT);

	tftDC->setTextColor(TFT_WHITE, TFT_BACK_COLOR);


	delay(200);
	//Settings.displayBacklight(true); // включаем подсветку


	rusPrint.init(tftDC);
	//tftDC->setCursor(10, 20);
	//tftDC->print("test");


	//TFTRus* rusPrinter = tftDC->getRusPrinter();
	////rusPrint.print("Проба",10,40);

	//rusPrinter->print("Введите пароль", 25, 5, TFT_BACK_COLOR, TFT_FONT_COLOR);
	//resetIdleTimer();

	//// добавляем служебные экраны

	// окно сообщения
	TFTScreenInfo mbscrif;

	//TFTFirstScreen
	mbscrif.screen = new TFTFirstScreen();
	mbscrif.screen->setup(this);
	mbscrif.screenName = "First";
	screens.push_back(mbscrif);

	
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::update()
{
	if (!tftDC)
	{
		return;
	}

	if (currentScreenIndex == -1 && !switchTo)                         // ни разу не рисовали ещё ничего, исправляемся
	{

		switchToScreen("First");

	}

	if (switchTo != NULL)
	{
		tftDC->fillScreen(TFT_BACK_COLOR); // clear screen first      
		yield();
		currentScreenIndex = switchToIndex;
		switchTo->onActivate(this);
		switchTo->update(this);
		yield();
		switchTo->draw(this);
		yield();
		//resetIdleTimer(); // сбрасываем таймер ничегонеделанья

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
	for (size_t i = 0; i < screens.size(); i++)
	{
		TFTScreenInfo* si = &(screens[i]);
		if (!strcmp(si->screenName, screenName))
		{
			return si->screen;
		}
	}

	return NULL;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTMenu::switchToScreen(AbstractTFTScreen* to)
{
	if (!tftDC)
	{
		return;
	}
	// переключаемся на запрошенный экран
	for (size_t i = 0; i < screens.size(); i++)
	{
		TFTScreenInfo* si = &(screens[i]);
		if (si->screen == to)
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
	if (!tftDC)
	{
		return;
	}

	// переключаемся на запрошенный экран
	for (size_t i = 0; i < screens.size(); i++)
	{
		TFTScreenInfo* si = &(screens[i]);
		if (!strcmp(si->screenName, screenName))
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
	if (currentScreenIndex > -1 && screens.size())
	{
		TFTScreenInfo* currentScreenInfo = &(screens[currentScreenIndex]);
		return (currentScreenInfo->screen);
	}

	return NULL;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TFTFirstScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

TFTFirstScreen* MainScreen = NULL;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTFirstScreen::TFTFirstScreen()
{

	MainScreen = this;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TFTFirstScreen::~TFTFirstScreen()
{
	
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTFirstScreen::onActivate(TFTMenu* menuManager)
{
	if (!menuManager->getDC())
	{
		return;
	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTFirstScreen::setup(TFTMenu* menuManager)
{
	TFT_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}

	

}


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTFirstScreen::update(TFTMenu* menuManager)
{



}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TFTFirstScreen::draw(TFTMenu* menuManager)
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
	uint16_t curX = 10;//screenWidth - textFontWidth - 10;     // Координаты вывода 
	uint16_t curY = 5;// 305;                             // Координаты вывода версии

	//rusPrinter->print("ТЕСТ"/*data.c_str()*/, curX, curY, TFT_WHITE, TFT_BLACK); // Отображаем версию программы

	dc->setCursor(10, 40);
	dc->print(data.c_str()); // Отображаем версию программы



	dc->setFreeFont(TFT_FONT);




}









#endif // USE_TFT_MODULE