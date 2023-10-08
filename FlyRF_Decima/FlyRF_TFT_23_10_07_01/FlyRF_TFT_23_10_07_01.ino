/*
 
 Применяется микроконтроллер на базе ESP32   
 ESP32 Dev Module
 
 Версия печатной платы "LilyGO-T-Beam"
*/

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
  // поднимаем первый UART
  SerialDEBUG.begin(Serial_SPEED);
  while (!SerialDEBUG && millis() < 1000);
 
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) 
  {
      Serial.println("SPIFFS Mount Failed");
      return;
  }

   
#ifdef USE_TFT_MODULE 
 tftModule.Setup();
#endif

 // Печатаем в SerialDEBUG готовность
 SerialDEBUG.println("READY");

}


//--------------------------------------------------------------------------------------------------------------------------------
void loop()
{

	Settings.update();                    // Проверяем состояние кнопки питания
 
	#ifdef USE_TFT_MODULE
		tftModule.Update();
	#endif 

}


//--------------------------------------------------------------------------------------------------------------------------------
