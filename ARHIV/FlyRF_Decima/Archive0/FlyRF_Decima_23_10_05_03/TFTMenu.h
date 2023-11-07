#pragma once
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#include "Configuration_ESP32.h"
#include "SPI.h"

#ifdef USE_TFT_MODULE

#include "TinyVector.h"
#include "TFTRus.h"
#include "TFT_Includes.h"

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#define INFO_BOX_V_SPACING 5
#define INFO_BOX_CONTENT_PADDING 8
#define ALL_CHANNELS_BUTTON_HEIGHT 60

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class TFTMenu;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef struct
{
  int x;
  int y;
  int w;
  int h;
} TFTInfoBoxContentRect;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class TFTInfoBox
{
  public:
    TFTInfoBox(const char* caption, int width, int height, int x, int y, int captionXOffset=0);
    ~TFTInfoBox();

    void draw(TFTMenu* menuManager);
    void drawCaption(TFTMenu* menuManager, const char* caption);
    int getWidth() {return boxWidth;}
    int getHeight() {return boxHeight;}
    int getX() {return posX;}
    int getY() {return posY;}
    const char* getCaption() {return boxCaption;}

    TFTInfoBoxContentRect getContentRect(TFTMenu* menuManager);

   private:

    int boxWidth, boxHeight, posX, posY, captionXOffset;
    const char* boxCaption;
};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawValueInBox(TFTInfoBox* box, const String& strVal,FONTTYPE font = SEVEN_SEG_NUM_FONT_MDS);
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
class TFTServiceMenuScreen : public AbstractTFTScreen
{
public:

  TFTServiceMenuScreen();
  ~TFTServiceMenuScreen();

  void setup(TFTMenu* menuManager);
  void update(TFTMenu* menuManager);
  void draw(TFTMenu* menuManager);
  void onActivate(TFTMenu* menuManager);

private:

 

};


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
    void Rotate_and_Draw_Bitmap(TFTMenu* menuManager, const uint8_t* bitmap, int winkel, uint8_t x, uint8_t y, uint8_t color);
	void drawWiFi(TFTMenu* menuManager);
    void drawData();


private:

  
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
};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern TFTMenuScreen* MainScreen;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Контроль питания аккумуляторов
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
typedef enum
{
  mbShow,
  mbConfirm,
  mbHalt
  
} MessageBoxType;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class MessageBoxScreen : public AbstractTFTScreen
{
  public:

  static AbstractTFTScreen* create();

  void confirm(const char* caption, Vector<const char*>& lines, AbstractTFTScreen* okTarget, AbstractTFTScreen* cancelTarget, MessageBoxResultSubscriber* sub=NULL);
  void show(const char* caption, Vector<const char*>& lines, AbstractTFTScreen* okTarget, MessageBoxResultSubscriber* sub=NULL);
  
  void halt(const char* caption, Vector<const char*>& lines, bool errorColors=true, bool haltInWhile=false);
    
protected:

    virtual void setup(TFTMenu* dc);
    virtual void update(TFTMenu* dc);
    virtual void draw(TFTMenu* dc);

private:
      MessageBoxScreen();

      AbstractTFTScreen* targetOkScreen;
      AbstractTFTScreen* targetCancelScreen;
      Vector<const char*> lines;

      const char* caption;

      MessageBoxResultSubscriber* resultSubscriber;
      bool errorColors, haltInWhile;

      //TFT_Buttons_Rus* buttons;
      //int yesButton, noButton;

      MessageBoxType boxType;

      void recreateButtons();
  
};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern MessageBoxScreen* MessageBox;
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


 // TOUCH_Class* getTouch() { return tftTouch; };
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
