

//==========================================================================
#include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <TFT_eSPI.h>             // Поддержка TFT дисплея 
#include <SD.h>                   // Поддержка SD карты
#include "SPIFFS.h"
#include "FS.h"
#include "Configuration_ESP32.h"  // Основные настройки программы
#include "Settings.h"             //  
#include "CoreButton.h"           //
#include "CoreCommandBuffer.h"    // обработчик входящих по UART команд
#include <Wire.h>                 // 
#include "AT24CX.h"               // Поддержка энергонезависимой памяти


#define FORMAT_SPIFFS_IF_FAILED true


#include "TFTModule.h"

TFTModule tftModule;



void setup()
{
    Serial.begin(115200);

    Serial.println("Setup start");

  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) 
  {
      Serial.println("SPIFFS Mount Failed");
      return;
  }
  Serial.println("SPIFFS Mount Ok");
  

 tftModule.Setup();

 Serial.println("Setup End");
 
}

void loop()
{
 
  tftModule.Update();
   
  yield();
}

//----------------------------------------------------------------------------------------

