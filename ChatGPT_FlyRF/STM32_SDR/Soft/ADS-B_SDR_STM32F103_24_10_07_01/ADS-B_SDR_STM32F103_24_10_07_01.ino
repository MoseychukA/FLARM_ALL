/*
Generic STM32F1 series   (STM32_GenF1)
https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json

на промежуточной частоте сигнал снова фильтруется и проходит через усилитель с переменным коэффициентом усиления (VGA).
 Фильтр IF обычно более избирательен, чем фильтр RF, поскольку в этом суть супергетеродинных архитектур.
 В случае с R820T он состоит из фильтра нижних частот и фильтра верхних частот, которые могут быть настроены на полосу пропускания до 300 кГц2.
 Однако его “стандартные значения” составляют либо 6, 7, либо 8 МГц, поскольку это полосы пропускания, используемые сигналами DVB-T.

Всего в тюнере предусмотрено 3 усиления, которыми можно управлять с помощью внешней конфигурации: LNA, микшер и VGA.
 Эти коэффициенты усиления можно настроить вручную, хотя их точные значения отсутствуют в техническом описании.
 Их также можно настроить автоматически с помощью автоматической регулировки усиления (AGC) для оптимизации отношения сигнал / шум (SNR).
 LNA и микшер имеют на своих выходах детектор мощности, который используется для управления их соответствующим коэффициентом усиления для этой цели.
 VGA AGC фактически управляется через аналоговый входной порт тюнера, который подключен к детектору мощности в RTL2832U.

 *
 *
 */

#include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <TFT_eSPI.h>             // Поддержка TFT дисплея  
#include <Wire.h>                 // 
#include "TFTModule.h"
#include "CONFIG.h"               // Основные настройки программы

#include "string.h" // это для функции strlen()
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include "stm32f1xx_hal.h"

#include "serial_cmd.h"


TFTModule tftModule;

uint32_t screenIdleTimer = 0;


void setup()
{
    Serial.begin(Serial_SPEED);
    while (!Serial && millis() < 1000);
    Serial.flush();
    delay(1000);

    Serial.println("Start system");
    Serial.println();


  String ver_soft = __FILE__;
  int val_srt = ver_soft.lastIndexOf('\\');
  ver_soft.remove(0, val_srt+1);
  val_srt = ver_soft.lastIndexOf('.');
  ver_soft.remove(val_srt);
  Serial.println(ver_soft);

  tftModule.Setup();

  MainScreen->saveVer(ver_soft);  // Сохранить строку с текущей версией.
   
  byte error;

  Wire.begin();

  Wire.beginTransmission(0x1A);
  error = Wire.endTransmission();

  if (error == 0)
  {
      Serial.println("I2C device found at address 0x1A");
 
  }

  R820T2_init();

  
  Serial.println("Start system END");
  //start_setup = true;
}

static void flush_input(void)
{
    while (Serial.available() > 0)
        Serial.read();
}

void loop()
{
  

  // Serial_CMD();  // Uncomment to enable R820T2 console.
 
   tftModule.Update();

  yield();
}

