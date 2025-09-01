#include <mutex>
#include "Arduino.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "adsbee.h"
#include "bsp.h"

BSP bsp = BSP({});
ADSBee adsbee = ADSBee({});

static constexpr uint8_t UART2_TX = 4, UART2_RX = 5;

/*Настройки только для теста*/
const int ledPin = 15;                       // the number of the LED pin
int ledState = LOW;                          // ledState used to set the LED
unsigned long previousMillis = 0;            // will store last time LED was updated
const long interval = 1000;                  // interval at which to blink (milliseconds)

const int ledPin1 = 25;
int ledState1 = LOW;
unsigned long previousMillis1 = 0;
const long interval1 = 300;


void setup() 
{
    bi_decl(bi_program_description("ADSBee 1090 ADSB Receiver"));

    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && !Serial.dtr() && (millis() - t0) < 8000) delay(10);
    delay(2000);

    Serial2.setTX(UART2_TX);
    Serial2.setRX(UART2_RX);
    Serial2.begin(921600);

    adsbee.Init();

    Serial.print("\r\nSetup End\r\n");
}


void setup1()
{
    pinMode(ledPin1, OUTPUT);
}



void loop() 
{

     unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) 
    {
        previousMillis = currentMillis;
        if (ledState == LOW) 
        {
            ledState = HIGH;
        }
        else 
        {
            ledState = LOW;
        }
        digitalWrite(ledPin, ledState);
    }
}


void loop1()
{

    unsigned long currentMillis1 = millis();
    if (currentMillis1 - previousMillis1 >= interval1)
    { 
        previousMillis1 = currentMillis1;
        if (ledState1 == LOW)
        {
            ledState1 = HIGH;
        }
        else
        {
            ledState1 = LOW;
        }
       digitalWrite(ledPin1, ledState1);
    }
}
