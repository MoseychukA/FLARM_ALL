#pragma once
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#include "Configuration_ESP32.h"
#include "SPI.h"

#ifdef USE_TFT_MODULE

#include "TinyVector.h"
#include "TFTRus.h"
#include "TFT_Includes.h"
#include "SoftRF.h"
//#include "src/driver/GNSS.h"

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#define TFT_EXPIRATION_TIME     15 /* seconds */
#define isTimeToDisplay()       (millis() - TFTTimeMarker > 1000)
#define maxof2(a,b)             (a > b ? a : b)
#define TFT_RADAR_V_THRESHOLD   50      /* metres */

//--------------------------------------------------------------------------------------------------------------------------------------
#pragma pack(push,1)
typedef struct
{
    uint32_t addr;                           // Адрес самолета
    uint8_t Container_i;                     // Номер самолета в контейнере
    uint8_t screen_side_width;               // Сторона экрана лево/право
    uint8_t screen_side_height;              // Сторона экрана верх/низ
    uint8_t base_alien[alien_count_base];    // Перечень в базе
    uint8_t base_index;                      // Порядковый номер в базе
    uint16_t alien_X;                        // Координата X
    uint16_t alien_Y;                        // Координата Y
    uint16_t altitude;                       // Высота
    uint16_t lat;                            // 
    uint16_t lon;                            // 
    uint16_t speed;                          // Скорость км/час
    uint8_t signal_source;                   // Источник пакета
    uint16_t heading;                        // Курс в градусах
} table_alien; // Таблица сторонних самолетов
#pragma pack(pop)






//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class TFTMenu;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// абстрактный класс экрана для TFT
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class AbstractTFTScreen
{
  public:

    virtual void setup(TFTMenu* menuManager) = 0;
    virtual void update(TFTMenu* menuManager) = 0;
    virtual void draw(TFTMenu* menuManager) = 0;
    virtual void onActivate(TFTMenu* menuManager){}
    virtual void onButtonPressed(TFTMenu* menuManager,int buttonID) {}
    virtual void onButtonReleased(TFTMenu* menuManager,int buttonID) {}
  
    AbstractTFTScreen();
    virtual ~AbstractTFTScreen();
};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// класс-менеджер работы с экраном
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef void (*OnScreenAction)(AbstractTFTScreen* screen);
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class TFTMenuScreen : public AbstractTFTScreen
{
public:

	TFTMenuScreen();
	~TFTMenuScreen();

	void setup(TFTMenu* menuManager);
	void update(TFTMenu* menuManager);
	void draw(TFTMenu* menuManager);
	void onActivate(TFTMenu* menuManager);

    void drawVoltage(TFTMenu* menuManager);
	void chargeControl(TFTMenu* menuManager);
   // void Rotate_and_Draw_Bitmap(TFTMenu* menuManager, const uint8_t* bitmap, int winkel, uint8_t x, uint8_t y, uint8_t color);
	void drawWiFi(TFTMenu* menuManager);
    void saveVer(String ver);
    String getVer();
 
    String Current_version;

private:


    uint16_t getPowerVoltageAkk(uint16_t pin); // Контроль напряжения питания внутренних источников (аккумуляторов).
	bool isActive;
    int  last5Vvoltage;
	int  last3Vvoltage;
    word color = TFT_RED;

	bool power_supple;
	bool power_supple_old;

	int control_X = 10;
	uint32_t tmr = 0;
	bool charge_on = false;
	int y_val = 45;

    long unsigned int startMillis;
    short unsigned int iter = 0;              // used to calculate the frames per second (FPS)
    int winkel = 0;
    int angle = 0;
    bool wifi_set = false;


   /* uint8_t arrow_up_down = 0;*/
    word  txt_color = TFT_GREEN;
 /*   String Current_version;*/
    
 
};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern TFTMenuScreen* MainScreen;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct MessageBoxResultSubscriber
{
  virtual void onMessageBoxResult(bool okPressed) = 0;
};

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// класс экрана ожидания
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#pragma pack(push,1)
typedef struct
{
  bool isWindowsOpen : 1;
  bool windowsAutoMode : 1;

  bool isWaterOn : 1;
  bool waterAutoMode : 1;

  bool isLightOn : 1;
  bool lightAutoMode : 1;
  
} IdleScreenFlags;
#pragma pack(pop)
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef struct
{
  const char* screenName;
  AbstractTFTScreen* screen;
  
} TFTScreenInfo;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef Vector<TFTScreenInfo> TFTScreensList; // список экранов
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// класс-менеджер работы с TFT
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#pragma pack(push,1)
typedef struct
{
  bool isLCDOn : 1;
  byte pad : 7;
  
} TFTMenuFlags;
#pragma pack(pop)

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class TFTMenu
{

public:
  TFTMenu();

  void setup();
  void update();

  void switchToScreen(const char* screenName);
  void switchToScreen(AbstractTFTScreen* to);

  AbstractTFTScreen* getScreen(const char* screenName);
  AbstractTFTScreen* getActiveScreen();
  
  
  // Добавил 
  void onAction(OnScreenAction handler) { on_action = handler; }
  
  TFT_Class* getDC() { return tftDC; };

  TFTRus* getRusPrinter() { return &rusPrint; };
 
  void resetIdleTimer();

  void onButtonPressed(int button);
  void onButtonReleased(int button);

private:

  TFTScreensList screens;
  TFT_Class* tftDC;


  TFTRus rusPrint;

  int currentScreenIndex;
  
  AbstractTFTScreen* switchTo;
  int switchToIndex;

  OnScreenAction on_action;

  unsigned long idleTimer;
  
  TFTMenuFlags flags;
 
  
};
extern TFTMenu* TFTScreen;
#endif // USE_TFT_MODULE
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
