#pragma once
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#include "Configuration_ESP32.h"
#include "SPI.h"

#ifdef USE_TFT_MODULE

#include "TinyVector.h"
#include "TFTRus.h"
#include "TFT_Buttons_Rus.h"
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
struct ITickHandler
{
  virtual void onTick() = 0;
};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class TickerClass
{
  public:
    TickerClass();
    ~TickerClass();

   void setIntervals(uint16_t beforeStartTickInterval,uint16_t tickInterval);
   void start(ITickHandler* h);
   void stop();
   void tick();

private:

  uint16_t beforeStartTickInterval;
  uint16_t tickInterval;

  uint32_t timer;
  bool started, waitBefore;

  ITickHandler* handler;
  
};
extern TickerClass Ticker;
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
class TFTTouchCalibrationScreen : public AbstractTFTScreen
{
public:

  TFTTouchCalibrationScreen();
  ~TFTTouchCalibrationScreen();

  void setup(TFTMenu* menuManager);
  void update(TFTMenu* menuManager);
  void draw(TFTMenu* menuManager);

private:

  bool canSwitch;

};

extern TFTTouchCalibrationScreen* TouchCalibrationScreen;


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

  int setDataButton, calButton, setAtmButton, timeButton, changePasswordButton, backButton;
  TFT_Buttons_Rus* screenButtons; 

};

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class TFTTimeSettingsMenuScreen : public AbstractTFTScreen
{
public:

  TFTTimeSettingsMenuScreen();
  ~TFTTimeSettingsMenuScreen();

  void setup(TFTMenu* menuManager);
  void update(TFTMenu* menuManager);
  void draw(TFTMenu* menuManager);
  void onActivate(TFTMenu* menuManager);

private:


  int measureTimeButton, calibrationTimeButton, ledLCDTimeButton, powerOffTimeButton, backButton;
  TFT_Buttons_Rus* screenButtons; 

};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class TFTCalibrationSettingsScreen : public AbstractTFTScreen, public ITickHandler
{
public:

  TFTCalibrationSettingsScreen();
  ~TFTCalibrationSettingsScreen();

  void setup(TFTMenu* menuManager);
  void update(TFTMenu* menuManager);
  void draw(TFTMenu* menuManager);
  void onActivate(TFTMenu* menuManager);
  void onButtonPressed(TFTMenu* menuManager, int buttonID);
  void onButtonReleased(TFTMenu* menuManager, int buttonID);
  void onTick();

private:

  int tickerButton;

  TFTInfoBox* settingsBox;

  int32_t setting;
  void incSetting(int val);

  int backButton, upButton, downButton;
  TFT_Buttons_Rus* screenButtons; 
};

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class TFTSaveMenuScreen : public AbstractTFTScreen
{
public:

	TFTSaveMenuScreen();
	~TFTSaveMenuScreen();

	void setup(TFTMenu* menuManager);
	void update(TFTMenu* menuManager);
	void draw(TFTMenu* menuManager);
	void onActivate(TFTMenu* menuManager);

private:

	int setDataButton, calButton, setAtmButton, timeButton, changePasswordButton, backButton;
	TFT_Buttons_Rus* screenButtons;

};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class TFTMenuScreen : public AbstractTFTScreen, public ITickHandler
{
public:

	TFTMenuScreen();
	~TFTMenuScreen();

	void setup(TFTMenu* menuManager);
	void update(TFTMenu* menuManager);
	void draw(TFTMenu* menuManager);
	void onActivate(TFTMenu* menuManager);
	void onButtonPressed(TFTMenu* menuManager, int buttonID);
	void onButtonReleased(TFTMenu* menuManager, int buttonID);
	void onTick();
    void drawVoltage(TFTMenu* menuManager);
	void chargeControl(TFTMenu* menuManager);
    void drawDateTime(TFTMenu* menuManager);
	void drawWiFi(TFTMenu* menuManager);

private:

  int tickerButton;

  int startButton, menuButton, saveButton;
	TFT_Buttons_Rus* screenButtons;	
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

};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern TFTMenuScreen* MainScreen;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class TFTSetTimeLedLCDOff : public AbstractTFTScreen
{
public:

	TFTSetTimeLedLCDOff();
	~TFTSetTimeLedLCDOff();

	void setup(TFTMenu* menuManager);
	void update(TFTMenu* menuManager);
	void draw(TFTMenu* menuManager);
	void onActivate(TFTMenu* menuManager);

private:

	TFT_Buttons_Rus* screenButtons;
	String enteredTimeLCD;
	int textBox;
	int lastKeyButtonID;
	int backButton, clearButton, enterButton;
	int stage;

	void relabelStageMessage(bool redraw);
};

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class TFTSetTimePowerOff : public AbstractTFTScreen
{
public:

	TFTSetTimePowerOff();
	~TFTSetTimePowerOff();

	void setup(TFTMenu* menuManager);
	void update(TFTMenu* menuManager);
	void draw(TFTMenu* menuManager);
	void onActivate(TFTMenu* menuManager);

private:

	TFT_Buttons_Rus* screenButtons;
	String enteredPowerOff;
	int textBox;
	int lastKeyButtonID;
	int backButton, clearButton, enterButton;
	int stage;

	void relabelStageMessage(bool redraw);
};


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Контроль питания аккумуляторов
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct MessageBoxResultSubscriber
{
  virtual void onMessageBoxResult(bool okPressed) = 0;
};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct KeyboardInputTarget
{
  virtual void onKeyboardInputResult(const String& input, bool okPressed) = 0;
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

      TFT_Buttons_Rus* buttons;
      int yesButton, noButton;

      MessageBoxType boxType;

      void recreateButtons();
  
};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern MessageBoxScreen* MessageBox;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef enum
{
  ktFull, // полная клавиатура
  ktNumbers, // ввод только чисел и точки
  
} KeyboardType;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#define KBD_BUTTONS_IN_ROW  10
#define KBD_BUTTON_WIDTH  25
#define KBD_BUTTON_HEIGHT  20
#define KBD_SPACING  6
#define KDB_BIG_BUTTON_HEIGHT 30
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class KeyboardScreen : public AbstractTFTScreen
{
  public:

  static AbstractTFTScreen* create();

  void show(const String& val, int maxLen, KeyboardInputTarget* inputTarget, KeyboardType keyboardType=ktFull, bool eng=false);

  ~KeyboardScreen();  
protected:

    virtual void setup(TFTMenu* dc);
    virtual void update(TFTMenu* dc);
    virtual void draw(TFTMenu* dc);

private:
      KeyboardScreen();

      KeyboardInputTarget* inputTarget;
      String inputVal;
      int maxLen;
      bool isRusInput;

      TFT_Buttons_Rus* buttons;
      Vector<String*> captions;
      Vector<String*> altCaptions;

      int backspaceButton, okButton, cancelButton, switchButton, spaceButton;

      void createKeyboard(TFTMenu* dc);
      void drawValue(TFTMenu* dc, bool deleteCharAtCursor=false);
      void switchInput(bool redraw);

      void applyType(KeyboardType keyboardType);

      int cursorPos;
      void redrawCursor(TFTMenu* dc, bool erase);

      char utf8Bytes[7];
  
};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern KeyboardScreen* Keyboard;
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
  TOUCH_Class* getTouch() { return tftTouch; };
  TFTRus* getRusPrinter() { return &rusPrint; };
 
  void resetIdleTimer();

  void onButtonPressed(int button);
  void onButtonReleased(int button);

private:

  TFTScreensList screens;
  TFT_Class* tftDC;
  TOUCH_Class* tftTouch;
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
