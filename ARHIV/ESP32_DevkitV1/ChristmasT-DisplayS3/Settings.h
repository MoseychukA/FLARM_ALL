#pragma once
//--------------------------------------------------------------------------------------------------------------------------------
#include <Arduino.h>
#include <driver/adc.h>
#include "AT24CX.h"               // Поддержка энергонезависимой памяти
#include "Configuration_ESP32.h"  // Основные настройки программы

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Программа управления питанием одной кнопкой
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef enum
{
    powerViaUSB = 10,
    batteryPower = 20

} PowerType;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

typedef struct
{
    int raw;
    float voltage3;
	float voltage5;

} VoltageData;

//--------------------------------------------------------------------------------------------------------------------------------
#pragma pack(push,1)
typedef struct
{
  bool isValid;
  uint16_t points[5];
  
} TFTCalibrationData;
#pragma pack(pop)
//--------------------------------------------------------------------------------------------------------------------------------
class SettingsClass
{
  public:
    SettingsClass();

   void setup();


   void initBoard();
   bool initPMU();
  // 
  //  int GetControllerID(const char* passedUUID);
  //  void SetControllerID(uint16_t val);


  //// возвращает тип питания - от батарей или USB
  //PowerType getPowerType();
  //void turnPowerOff(); // выключает питание контроллера
  //   // управление подсветкой экрана
  //void displayBacklight(bool bOn);
  bool isBacklightOn() { return backlightFlag; }

  void update();

  /*uint16_t getPowerVoltage5(uint16_t pin);*/

  VoltageData voltage5V;  // Питание аккумуляторов
  VoltageData voltage3V;  // Питание батарейки часов
 

  //bool GetWiFiState();
  //void SetWiFiState(bool st);

  //bool GetWiFiConnect();
  //void SetWiFiConnect(bool WiFiOn);

  //String GetRouterSSID();
 
  //void SetRouterSSID(const String& val);
  //String GetRouterPassword();
  //void SetRouterPassword(const String& val);

  //String GetStationID();
  //void SetStationID(const String& val);
  //String GetStationPassword();
  //void SetStationPassword(const String& val);
  void TestI2C();



 private:


  /*Button powerButton;*/
  PowerType powerType;
  //static void checkPower();
  bool backlightFlag;
 

  bool array_countMax = false;
  int sum = 0;
  uint8_t array_count = 0;
  uint8_t array_size = 30;
  int dimension_array[30];

  bool wifiState = false;
  bool wifiConnect = false;
  String routerSSID;
  String routerPassword;
  String stationID;
  String stationPassword;

  /*uint8_t read8(uint16_t address, uint8_t defaultVal);
  uint16_t read16(uint16_t address, uint16_t defaultVal);
  void write16(uint16_t address, uint16_t val);

  unsigned long read32(uint16_t address, unsigned long defaultVal);
  void write32(uint16_t address, unsigned long val);

  String readString(uint16_t address, byte maxlength);
  void writeString(uint16_t address, const String& v, byte maxlength);*/



};
//--------------------------------------------------------------------------------------------------------------------------------
extern SettingsClass Settings;
//--------------------------------------------------------------------------------------------------------------------------------

