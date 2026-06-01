/* Вычисление координат и курса стороннего самолета.


 */


#include "CONFIG.h"               // Основные настройки программы
#include "TFT_eSPI.h"
#include <SPI.h>
#include "SoftRF.h"
#include "NotoSansMonoSCB20.h"
#include "NotoSansBold15.h"

#ifdef USE_TFT_MODULE

#include "TFTMenu.h"

TFT_eSPI tft = TFT_eSPI();

//TFT_eSprite back       = TFT_eSprite(&tft);         // Спрайт фона
//TFT_eSprite backsprite = TFT_eSprite(&tft);         // Спрайт отображения вращающегося поля воздушной обстановки
//TFT_eSprite dist_info  = TFT_eSprite(&tft);         // Спрайт окна информации расстояние до ближайшего стороннего воздушного объекта
//TFT_eSprite Airplane   = TFT_eSprite(&tft);         // Спрайт нашего самолета
//TFT_eSprite data_az    = TFT_eSprite(&tft);         // Информационный спрайт.Азимут (угол направления нашего самолета)
//TFT_eSprite data_KM    = TFT_eSprite(&tft);         // Информационный спрайт. Дипазон расстояний всего поля 
//TFT_eSprite power1     = TFT_eSprite(&tft);         // Спрайт отображения заряда аккумулятора 
//
//TFT_eSprite* arrow[MAX_TRACKING_OBJECTS];           // Спрайт отображения стрелки
//TFT_eSprite* arrow_old[MAX_TRACKING_OBJECTS];       // Спрайт отображения стрелка 
//
//TFT_eSprite* Air_txt_Sprite[MAX_TRACKING_OBJECTS];  // Этот спрайт, площадка в котором будет располагатся формуляр стороннего самолета
//TFT_eSprite* little_airplane[MAX_TRACKING_OBJECTS]; // Этот спрайт, площадка в котором будет располагатся изображение стороннего самолета
//TFT_eSprite* area_airplane[MAX_TRACKING_OBJECTS];   // Этот спрайт, площадка в котором будет располагатся спрайт little_airplane стороннего самолета
//
//int alien_altitude_old[MAX_TRACKING_OBJECTS];       // Предыдущее значение высоты стороннего самолета
//int alien_altitude_actual[MAX_TRACKING_OBJECTS];    // Высота стороннего самолета
//int alien_altitude_arrow[MAX_TRACKING_OBJECTS];     // Предыдущая высота стороннего самолета для отображения стрелок выше/ниже.
//int alien_altitude_hysteresis[MAX_TRACKING_OBJECTS];// Обработанная высота стороннего самолета
//int height_difference[MAX_TRACKING_OBJECTS];        // Разность высот нашего и стороннего самолета
//int alien_speed_tmr[MAX_TRACKING_OBJECTS];          // Скорость стороннего самолета
//int bearing_tmr[MAX_TRACKING_OBJECTS];              // Угол в градусах между нашим самолетом и сторонним
//int distance_tmr[MAX_TRACKING_OBJECTS];             // Дистанция между нашим и сторонним самолетом
//int alien_curse[MAX_TRACKING_OBJECTS];              // Курс стороннего самолета
//int Container_alien_X[MAX_TRACKING_OBJECTS];        // Координаты стороннего самолета
//int Container_alien_Y[MAX_TRACKING_OBJECTS];        // Координаты стороннего самолета
//int Container_logbook_X[MAX_TRACKING_OBJECTS];      // Координаты формуляра стороннего самолета
//int Container_logbook_Y[MAX_TRACKING_OBJECTS];      // Координаты формуляра стороннего самолета
//uint8_t arrow_up_down[MAX_TRACKING_OBJECTS];        // флаг стрелки вверх или вниз
//uint8_t arrow_up_down_old[MAX_TRACKING_OBJECTS];        // флаг стрелки вверх или вниз
//bool Air_txt_left[MAX_TRACKING_OBJECTS];            // флаг расположения формуляра слева или справа
//
//word  little_air_color[MAX_TRACKING_OBJECTS];       // Цвет предупреждения столкновения с сторонним самолетом 
//
//int alien_speed_filtre[MAX_TRACKING_OBJECTS][speed_array_size];     // Фильтр скорости стороннего самолета
//int alien_altitude_filtre[MAX_TRACKING_OBJECTS][speed_array_size];  // Фильтр высоты стороннего самолета
//
//bool alien_speed_array_countMax[MAX_TRACKING_OBJECTS];             // Флаг заполнения массиво фильтра скорости стороннего самолета
//int alien_speed_sum[MAX_TRACKING_OBJECTS];                         // = 0;
//uint8_t alien_speed_array_count[MAX_TRACKING_OBJECTS];             // Счетчик фильтра скорости стороннего самолета
//
//bool alien_altitude_array_countMax[MAX_TRACKING_OBJECTS];          // Флаг заполнения массиво фильтра высоты стороннего самолета
//int alien_altitude_sum[MAX_TRACKING_OBJECTS];                      // = 0;
//uint8_t alien_altitude_array_count[MAX_TRACKING_OBJECTS];          // Счетчик фильтра высоты стороннего самолета
//
//int this_speed_filtre[speed_array_size];                            // Фильтр скорости нашего самолета
//int this_altitude_filtre[speed_array_size];                         // Фильтр высоты нашего самолета


//......................................colors
#define backColor     0x0026
#define gaugeColor    0x055D
#define dataColor     0x0311
#define purple        0xEA16
#define Air_infoColor 0xF811

//static int TFT_zoom = ZOOM_MEDIUM;
//
//bool isTeam_all[MAX_TRACKING_OBJECTS] = { false };
//bool isThere_plane[MAX_TRACKING_OBJECTS] = { false };

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

    Serial.println("TFTMenuScreen::setup");

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

    /* Проверяем наличие новой информации */ 
    if (millis() - tmr > DATA_MEASURE_THRESHOLD)
    {
    //    // drawVoltage(menuManager);
    //    // drawWiFi(menuManager);
        tmr = millis();
        
      //  DBGLN("Current_version");



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
     const char EPD_SoftRF_text6[] = "(C) 2023  "; 
  

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

     tft_radar->print(EPD_SoftRF_text6);
     tft_radar->print(EPD_SoftRF_text2);

 
     tbw1 = tft_radar->textWidth(Current_version);
     x_tft = (tft_radar->width() - tbw1) - 4;
     y_tft = tft_radar->height() - tft_radar->fontHeight() + 10;
     tft_radar->setCursor(10, y_tft);
     tft_radar->print(Current_version);
 
     delay(5000);


 }

 void TFTMenuScreen::saveVer(String ver)
 {
    
     Current_version = ver;
    // Serial.println(Current_version);
 }

 //void TFTMenuScreen::readVer()
 //{
 //    String ver1 = Current_version;
 //  // return ver1;
 //    // Serial.println(Current_version);
 //}

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


 uint16_t TFTMenuScreen::getSpeed(uint16_t speed) // Контроль напряжения питания внутренних источников (аккумуляторов).
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


 int TFTMenuScreen::alien_count()
 {
     int count = 0;

     //for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) 
     //{
     //    if (Container[i].addr) {
     //        count++;
     //    }
     //}

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

     Serial.print("screenWidth ");
     Serial.print(screenWidth);

     Serial.print(" | screenHeight ");
     Serial.print(screenHeight);


     tft_radar->setFreeFont(TFT_FONT);
     int textFontHeight = FONT_HEIGHT(tft_radar);

     //String data = data_txt; //"ОПРЕДЕЛЕНИЕ"
     //String data1 = data_txt1; //"МЕСТОПОЛОЖЕНИЯ"
     //int textFontWidth = tft_radar->textWidth(data, 2);                   // Returns pixel width of string in current font

     //Serial.print("textFontWidth ");
     //Serial.print(textFontWidth);

     //uint16_t curX = (screenWidth / 2) - (textFontWidth / 2) - 12;        // Координаты вывода 
     //uint16_t curY = 110;                                                 // Координаты вывода текста
     //rusPrinter->print(data.c_str(), curX, curY, backColor, TFT_YELLOW);  // Отображаем 

     //textFontWidth = tft_radar->textWidth(data1, 2);                      // Returns pixel width of string in current font
     //Serial.print(" | textFontWidth2 ");
     //Serial.print(textFontWidth);

     //curX = (screenWidth / 2) - (textFontWidth / 2) - 18;                  // Координаты вывода 
     //curY = 140;                                                           // Координаты вывода текста
     //rusPrinter->print(data1.c_str(), curX, curY, backColor, TFT_YELLOW);  // Отображаем 


 }



//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#endif // USE_TFT_MODULE
