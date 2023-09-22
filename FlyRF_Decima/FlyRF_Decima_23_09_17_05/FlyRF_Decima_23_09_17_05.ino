// Visual Micro is in vMicro>General>Tutorial Mode
// 
/*
    Name:       FlyRF_Decima_23_09_17_01.ino
    Created:	17.09.2023 16:05:39
    Author:     MASTER\Alex
*/

#include "Configuration_ESP32.h"
#include "Settings.h"
#include <TFT_eSPI.h>             // Hardware-specific library
#include <SPI.h>
#include <LoRa.h>                 // Только для тестирования

TFT_eSPI tft = TFT_eSPI();        // Вызов пользовательской библиотеки TFT

#ifdef LoRa_test
int counter = 0;
unsigned long previousMillis = 0;       // will store last time LoRa was updated
const long interval          = 3000;    // interval at which to blink (milliseconds)
#endif

#ifdef USE_TFT_MODULE
#include "TFTModule.h"
#endif

//char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

#ifdef USE_TFT_MODULE
TFTModule tftModule;

#endif

// The setup() function runs once each time the micro-controller starts
void setup()
{
    Settings.initBoard();                  // Инициализация модулей устройства

    delay(100);
 

#ifdef USE_TFT_MODULE 
    tftModule.Setup();
#endif

    //screenIdleTimer = millis();

    //TFTScreen->onAction(screenAction);  // 


    // Печатаем в Serial готовность
    Serial.println("READY");



    //// Use this initializer if you're using a 1.8" TFT
    //tft.init();   // initialize

    //tft.fillScreen(TFT_BLACK);

    //tft.setTextWrap(false);
    //tft.fillScreen(TFT_BLACK);
    //tft.setCursor(20, 2);
    //tft.setTextColor(TFT_WHITE);
    //tft.setTextSize(1);
    //tft.println("FlyRF by Decima");
    //tft.setTextSize(1);

#ifdef LoRa_test
 //   tft.println("LoRa Sender");
    Serial.println("LoRa Sender");
    LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);
    if (!LoRa.begin(LoRa_frequency)) {
        Serial.println("Starting LoRa failed!");
        while (1);
    }

#endif

}

// Add the main program code into the continuous loop() function
void loop()
{


#ifdef USE_TFT_MODULE
    tftModule.Update();
#endif 




#ifdef LoRa_test
    int packetSize = LoRa.parsePacket();
    if (packetSize)
    {
        // received a packet
        Serial.print("Received packet '");

        String recv = "";
        // read packet
        while (LoRa.available()) {
            recv += (char)LoRa.read();
        }

        Serial.println(recv);

        // print RSSI of packet
        Serial.print("' with RSSI ");
        Serial.println(LoRa.packetRssi());
//#ifdef HAS_DISPLAY
//        if (u8g2) {
//            //  u8g2->clearBuffer();
//            char buf[256];
//            u8g2->drawStr(0, 12, "Received OK!");
//            u8g2->setDrawColor(0);// Black
//            u8g2->drawBox(0, 14, 80, 12);
//            u8g2->drawBox(0, 28, 80, 12);
//            u8g2->setDrawColor(1);
//            u8g2->drawStr(0, 26, recv.c_str());
//            snprintf(buf, sizeof(buf), "RSSI:%i", LoRa.packetRssi());
//            u8g2->drawStr(0, 40, buf);
//            /*  snprintf(buf, sizeof(buf), "SNR:%.1f", LoRa.packetSnr());
//              u8g2->drawStr(0, 56, buf);*/
//            u8g2->sendBuffer();
//        }
//#endif
    }

    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval)
    {
        // save the last time you blinked the LED
        previousMillis = currentMillis;

        Serial.print("Sending packet: ");
        Serial.println(counter);

        // send packet
        LoRa.beginPacket();
        LoRa.print("hello3 ");
        LoRa.print(counter);
        LoRa.endPacket();

        //Serial.print("REG_PA_CONFIG = ");
        //Serial.println(LoRa.readRegister(0x09), HEX);
        //Serial.println(LoRa.readRegister(0x09), BIN);

        //Serial.print("REG_PA_DAC = ");
        //Serial.println(LoRa.readRegister(0x4d), HEX);
        //Serial.println(LoRa.readRegister(0x4d), BIN);

        //Serial.print("REG_OCP = ");
        //Serial.println(LoRa.readRegister(0x0b), HEX);
        //Serial.println(LoRa.readRegister(0x0b), BIN);
//#ifdef HAS_DISPLAY
//        if (u8g2) {
//            char buf[256];
//            u8g2->drawStr(0, 52, "Transmitting: OK!");
//            snprintf(buf, sizeof(buf), "Sending: %d", counter);
//            u8g2->setDrawColor(0);// Black
//            u8g2->drawBox(64, 53, 30, 11);
//            u8g2->setDrawColor(1);
//            u8g2->drawStr(0, 64, buf);
//            u8g2->sendBuffer();
//        }
//#endif
        counter++;

    }

#endif

}
