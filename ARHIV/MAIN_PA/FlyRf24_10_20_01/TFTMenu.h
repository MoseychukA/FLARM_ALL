#pragma once
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#include "Configuration_ESP32.h"
#include "SPI.h"
#include "SettingsMain.h"

#ifdef USE_TFT_MODULE

#include "TinyVector.h"
//#include "TFTRus.h"
#include "TFT_Includes.h"
#include "SoftRF.h"


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// класс-менеджер работы с TFT
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#pragma pack(push,1)
typedef struct
{
    bool MailOn : 1;

} TFTMenuFlags;
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
    virtual void onButtonisRetention(TFTMenu* menuManager, int buttonID) {}
    virtual void onButtonisDoubleClicked(TFTMenu* menuManager, int buttonID) {}
  
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

    int mb_strlen(char* source, int letter_n);
    int angle = 0;
    int angle_old = 0;

    uint8_t count_buttton = 0;
    bool setClearButton   = false;
    bool setClearMessage  = false;
    bool setMessageRead   = false;

 
private:
    void drawMessage(TFTMenu* menuManager);
    float bearing_calc(float lat, float lon, float lat2, float lon2);
    double distance_form(double lat1, double long1, double lat2, double long2);
    int alien_count();
    bool coordinates_waiting();
    void waiting_txt(TFTMenu* menuManager); // Вывод текста "ОПРЕДЕЛЕНИЕ МЕСТОПОЛОЖЕНИЯ"
    void clearMSG(TFTMenu* menuManager);
    void Draw_circular_scale();


    unsigned long previousMillis_msg = 0;        // will store last time LED was updated
    const long interval_on = 2500;           // interval at which to blink (milliseconds)
    const long interval_off = 500;           // interval at which to blink (milliseconds)
    int divider_num = 1;
	bool isActive;
    word color = TFT_RED;

    uint8_t  confirmation_OK;

    //char msg[Number_of_bytes_block] = "";
    /*char time_msg[Number_of_bytes_time] = "";*/

    bool Allow_flashing;   //Разрешить мигание
    bool flashing_on_off;  //Режим включен или потушен
	bool confirm_message;
    char msg_mem_tmp[Number_of_bytes_block] = "";
  
    int control_X = 10;
	uint32_t tmr = 0;
	bool charge_on = false;
	int y_val = 45;

    long unsigned int startMillis;
    short unsigned int iter = 0;              // used to calculate the frames per second (FPS)
    int winkel = 0;
    bool wifi_set = false;
    int distance_var = 2;

    int test_curse = 0;
    float Aircraft_latitude_old = 0;
    float Aircraft_longitude_old= 0;

    //............................dont edit this
    int cx = 160;
    int cy = 160;
    int r  = 158;
    int n = 0;
    
    float fx[360]; //outer points of Speed gaouges
    float fy[360];
    float px[360]; //ineer point of Speed gaouges
    float py[360];
    float px1[360]; //ineer point of Speed gaouges
    float py1[360];
    float lx[360]; //text of Speed gaouges
    float ly[360];
    float nx[360]; //needle low of Speed gaouges
    float ny[360];

    uint8_t  fix_tmp = false;
    /* TFT_Draw_Radar */
    double rad = 0.01745;
  /*  unsigned short color1;
    unsigned short color2;*/

    int16_t  tbx, tby;
    uint16_t tbw, tbh;
    //char cog_text[6];

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
    int16_t form_arrow_x = 0;
    int16_t form_arrow_y = 0;

    int16_t alient_course0 = 0; // Курс ближайшего стороннего самолета
    int16_t alient_speed0 = 0;  // Скорость ближайшего стороннего самолета
    int8_t  txt_loc_speed = 87; // место вывода текста скорости стороннегосамолета
    
    /* Переменные для фильтра высоты искорости нашего самолета*/
 
    int thisAircraft_altitude_tmr = 0;           // ThisAircraft
    int thisAircraft_speed_tmr = 0;
    int thisAircraft_course_tmr = 0;

    uint8_t index_nearest_aircraft = 0;          // индекс ближайшего самолета

    int view_alien_count = 0;                    // Переменная для определения количества сторонних самолетов.
   // uint8_t  mail_tmp[Max_Count_Block_Message][128];             // Массив для сообщений почты

    TFTMenuFlags flags;

    bool text_call = false;
    //bool mail_on = false;
    //uint8_t mail_count = 0;
    //uint8_t  fix = false;
    /*String rssi_txt;*/
   // uint8_t rssi_vol=0;

    /* Переменные для фильтра скорости нашего самолета*/
    //bool array_countMax_speed = false;
    //int sum_speed = 0;
    //uint8_t array_count_speed = 0;
    //uint8_t array_size_speed = 20;
    /*int dimension_array_speed[20];*/

    /* Переменные для фильтра курса нашего самолета*/

 /*   bool array_countMax_course = false;
    int sum_course = 0;
    uint8_t array_count_course = 0;
    uint8_t array_size_course = 15;
    int dimension_array_course[15];*/

    /* Переменные для фильтра высоты нашего самолета*/


   /* bool array_countMax_altitude = false;
    int sum_altitude = 0;
    uint8_t array_count_altitude = 0;
    uint8_t array_size_altitude = 20;
    int dimension_array_altitude[20];*/

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
//#pragma pack(push,1)
//typedef struct
//{
// /* bool isWindowsOpen : 1;
//  bool windowsAutoMode : 1;
//
//  bool isWaterOn : 1;
//  bool waterAutoMode : 1;*/
//
//  bool isLightOn : 1;
//  bool lightAutoMode : 1;
//  
//} IdleScreenFlags;
//#pragma pack(pop)
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

 // TFTRus* getRusPrinter() { return &rusPrint; };

private:

  TFTScreensList screens;
  TFT_Class* tftDC;


 // TFTRus rusPrint;

  int currentScreenIndex;
  
  AbstractTFTScreen* switchTo;
  int switchToIndex;

  OnScreenAction on_action;

};
extern TFTMenu* TFTScreen;
#endif // USE_TFT_MODULE
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
