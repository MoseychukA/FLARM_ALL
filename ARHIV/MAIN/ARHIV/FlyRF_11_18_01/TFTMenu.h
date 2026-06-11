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

private:

    float bearing_calc(float lat, float lon, float lat2, float lon2);
    double distance_form(double lat1, double long1, double lat2, double long2);
    int alien_count();
    bool coordinates_waiting();
    void waiting_txt(TFTMenu* menuManager); // Вывод текста "ОПРЕДЕЛЕНИЕ МЕСТОПОЛОЖЕНИЯ"
    uint16_t getSpeed(uint16_t speed); //
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
    int angle_air = 0;
    int angle_tmp = -1;
    bool wifi_set = false;
    int distance_var = 2;

    int test_curse = 0;
    float latitude_old = 0;
    float longitude_old= 0;

    //............................dont edit this
    int cx = 160;
    int cy = 160;
    int r  = 158;
    int n = 0;
    
    float x[360]; //outer points of Speed gaouges
    float y[360];
    float px[360]; //ineer point of Speed gaouges
    float py[360];
    float px1[360]; //ineer point of Speed gaouges
    float py1[360];
    float lx[360]; //text of Speed gaouges
    float ly[360];
    float nx[360]; //needle low of Speed gaouges
    float ny[360];


    /* TFT_Draw_Radar */
    double rad = 0.01745;
    unsigned short color1;
    unsigned short color2;

    int16_t  tbx, tby;
    uint16_t tbw, tbh;
    char cog_text[6];


    int32_t divider = 2000;  //делитель равен половине полной шкалы
    uint16_t x_cont;
    uint16_t y_cont;
    uint16_t radar_x = 0;
    uint16_t radar_y = 0; //(tft_radar->width() - tft_radar->height()) / 2;
    uint16_t radar_w = 320; //tft->width();

    uint16_t radar_center_x = radar_w / 2;
    uint16_t radar_center_y = radar_y + radar_w / 2;
    uint16_t radius = radar_w / 2 - 2;
  
    int16_t rel_x;
    int16_t rel_y;
    int16_t new_rel_x;
    int16_t new_rel_y;
    int16_t new_form_x;
    int16_t new_form_y;

    int16_t x1;
    int16_t y1;
    int16_t new_x;
    int16_t new_y;

    int16_t form_x=0;
    int16_t form_y=0;

   /* uint8_t arrow_up_down = 0;*/
    word  txt_color = TFT_WHITE;
    String Current_version;
    
    bool this_altitude_array_countMax = false;
    int this_altitude_sum = 0;
    uint8_t this_altitude_array_count = 0;
    int this_alien_altitude_tmr = 0;

    bool this_speed_array_countMax = false;
    int this_speed_sum = 0;
    uint8_t this_speed_array_count = 0;
    int this_alien_speed_tmr = 0;
    int view_alien_count = 0;

    table_alien set_table_alien[alien_count_base];

    bool text_call = false;
    uint8_t  fix = false;

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
