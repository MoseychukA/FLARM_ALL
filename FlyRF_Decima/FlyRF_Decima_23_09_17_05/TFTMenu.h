#pragma once


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#include "Configuration_ESP32.h"
#include "SPI.h"

#ifdef USE_TFT_MODULE

#include "TinyVector.h"
#include "TFTRus.h"
#include "TFT_Includes.h"

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
    TFTInfoBox(const char* caption, int width, int height, int x, int y, int captionXOffset = 0);
    ~TFTInfoBox();

    void draw(TFTMenu* menuManager);
    void drawCaption(TFTMenu* menuManager, const char* caption);
    int getWidth() { return boxWidth; }
    int getHeight() { return boxHeight; }
    int getX() { return posX; }
    int getY() { return posY; }
    const char* getCaption() { return boxCaption; }

    TFTInfoBoxContentRect getContentRect(TFTMenu* menuManager);

private:

    int boxWidth, boxHeight, posX, posY, captionXOffset;
    const char* boxCaption;
};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawValueInBox(TFTInfoBox* box, const String& strVal, FONTTYPE font = SEVEN_SEG_NUM_FONT_MDS);
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
    virtual void onActivate(TFTMenu* menuManager) {}
    virtual void onButtonPressed(TFTMenu* menuManager, int buttonID) {}
    virtual void onButtonReleased(TFTMenu* menuManager, int buttonID) {}

    AbstractTFTScreen();
    virtual ~AbstractTFTScreen();
};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// класс-менеджер работы с экраном
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef void (*OnScreenAction)(AbstractTFTScreen* screen);
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class TFTFirstScreen : public AbstractTFTScreen/*, public ITickHandler*/
{
public:

    TFTFirstScreen();
    ~TFTFirstScreen();

    void setup(TFTMenu* menuManager);
    void update(TFTMenu* menuManager);
    void draw(TFTMenu* menuManager);
    void onActivate(TFTMenu* menuManager);


private:

  

};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern TFTFirstScreen* MainScreen;

