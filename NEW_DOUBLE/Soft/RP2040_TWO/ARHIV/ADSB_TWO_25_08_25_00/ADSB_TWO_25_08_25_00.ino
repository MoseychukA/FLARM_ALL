#include "Arduino.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "adsbee.h"
#include "bsp.h"
#include "data_structures.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

BSP bsp = BSP({});
ADSBee adsbee = ADSBee({});

/* Тест-индикаторы */
const int ledPin = 15;
int ledState = LOW;
unsigned long previousMillis = 0;
const long interval = 1000;

const int ledPin1 = 25;
int ledState1 = LOW;
unsigned long previousMillis1 = 0;
const long interval1 = 300;
static constexpr uint8_t UART2_TX = 4;
static constexpr uint8_t UART2_RX = 5;

//=============================================================================================================

/* setup/loop */
void setup() {
    bi_decl(bi_program_description("ADSBee 1090 ADSB Receiver"));
    Serial.begin(115200);
    unsigned long t0 = millis(); while (!Serial && !Serial.dtr() && (millis() - t0) < 8000) delay(10);
    delay(2000);
    Serial.print("Software ");
    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    ver_soft.remove(val_srt);
    Serial.println(ver_soft);
   
    Serial2.setTX(UART2_TX);
    Serial2.setRX(UART2_RX);
    Serial2.begin(921600);

    adsbee.Init();
    pinMode(ledPin, OUTPUT);
    Serial.println("Setup End");
}
void setup1() { pinMode(ledPin1, OUTPUT); }

void loop() {
 
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) 
    {
        previousMillis = currentMillis;
        ledState = (ledState == LOW) ? HIGH : LOW;
        digitalWrite(ledPin, ledState);
    }
}
void loop1() 
{
    unsigned long currentMillis1 = millis();
    if (currentMillis1 - previousMillis1 >= interval1) 
    {
        previousMillis1 = currentMillis1;
        ledState1 = (ledState1 == LOW) ? HIGH : LOW;
        digitalWrite(ledPin1, ledState1);
    }
}






