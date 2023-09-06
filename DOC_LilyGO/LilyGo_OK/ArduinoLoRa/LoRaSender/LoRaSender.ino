#include <LoRa.h>
#include "boards.h"
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library
const int ledPin = 13;//
int counter = 0;
float p = 3.1415926;

void setup()
{
    initBoard();
    // When the power is turned on, a delay is required.
    delay(1000);
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, HIGH);

    // Use this initializer if you're using a 1.8" TFT
    tft.init();   // initialize
    // large block of text
    tft.fillScreen(TFT_BLACK);
    testdrawtext("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur adipiscing ante sed nibh tincidunt feugiat. Maecenas enim massa, fringilla sed malesuada et, malesuada sit amet turpis. Sed porttitor neque ut ante pretium vitae malesuada nunc bibendum. Nullam aliquet ultrices massa eu hendrerit. Ut sed nisi lorem. In vestibulum purus a tortor imperdiet posuere. ", TFT_WHITE);
    delay(1000);

    // tft print function!
    tftPrintTest();
    delay(4000);

    Serial.println("LoRa Sender");
    LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);
    if (!LoRa.begin(LoRa_frequency)) {
        Serial.println("Starting LoRa failed!");
        while (1);
    }
}

void loop()
{
    Serial.print("Sending packet: ");
    Serial.println(counter);

    // send packet
    LoRa.beginPacket();
    LoRa.print("hello ");
    LoRa.print(counter);
    LoRa.endPacket();

#ifdef HAS_DISPLAY
 /*   if (u8g2) {
        char buf[256];
        u8g2->clearBuffer();
        u8g2->drawStr(0, 12, "Transmitting: OK!");
        snprintf(buf, sizeof(buf), "Sending: %d", counter);
        u8g2->drawStr(0, 30, buf);
        u8g2->sendBuffer();
    }*/
#endif
    counter++;
    delay(5000);
}

void testdrawtext(char* text, uint16_t color) {
    tft.setCursor(0, 0);
    tft.setTextColor(color);
    tft.setTextWrap(true);
    tft.print(text);
}
void tftPrintTest() {
    tft.setTextWrap(false);
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 30);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(1);
    tft.println("Hello World!");
    tft.setTextColor(TFT_YELLOW);
    tft.setTextSize(2);
    tft.println("Hello World!");
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(3);
    tft.println("Hello World!");
    tft.setTextColor(TFT_BLUE);
    tft.setTextSize(4);
    tft.print(1234.567);
    delay(1500);
    tft.setCursor(0, 0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(0);
    tft.println("Hello World!");
    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN);
    tft.print(p, 6);
    tft.println(" Want pi?");
    tft.println(" ");
    tft.print(8675309, HEX); // print 8,675,309 out in HEX!
    tft.println(" Print HEX!");
    tft.println(" ");
    tft.setTextColor(TFT_WHITE);
    tft.println("Sketch has been");
    tft.println("running for: ");
    tft.setTextColor(TFT_MAGENTA);
    tft.print(millis() / 1000);
    tft.setTextColor(TFT_WHITE);
    tft.print(" seconds.");
}

