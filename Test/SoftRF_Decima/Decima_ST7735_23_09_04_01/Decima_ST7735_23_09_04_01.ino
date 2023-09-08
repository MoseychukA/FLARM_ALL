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
 
    delay(1000);
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, HIGH);

    // Use this initializer if you're using a 1.8" TFT
    tft.init();   // initialize
 
    tft.fillScreen(TFT_BLACK);

    tft.setTextWrap(false);
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 10);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.println("LoRa Sender");


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

    tft.setTextWrap(true, true);

    // Font and background colour, background colour is used for anti-alias blending
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    //int xpos = tft.width() / 2; // Half the screen width
    //int ypos = 10;
    //tft.setTextDatum(TC_DATUM); // Top Centre datum

    //tft.setFreeFont(FMB9);     // Select the orginal small TomThumb font


    //tft.loadFont(AA_FONT_SMALL); // Must load the font first

    //tft.drawString("Small 15pt font", xpos, ypos);

    //ypos += tft.fontHeight();   // Get the font height and move ypos down

    //tft.setTextColor(TFT_WHITE, TFT_BLACK);

    //// If the string does not fit the screen width, then the next character will wrap to a new line
    //tft.drawString("Ode To A Small Lump Of Green Putty I Found In My Armpit One Midsummer Morning", xpos, ypos);

    //tft.setTextColor(TFT_WHITE, TFT_BLUE); // Background colour does not match the screen background!
    //tft.drawString("Anti-aliasing causes odd looking shadow effects if the text and screen background colours are not the same!", xpos, ypos + 60);

    //tft.unloadFont(); // Remove the font to recover memory used


    // Load the font
    //tft.loadFont(Final_Frontier_28);

    //// Display all characters of the font
    //tft.showFont(2000);



#ifdef HAS_DISPLAY
    if (u8g2) {
        char buf[256];
        u8g2->clearBuffer();
        u8g2->drawStr(0, 12, "Transmitting: OK!");
        snprintf(buf, sizeof(buf), "Sending: %d", counter);
        u8g2->drawStr(0, 30, buf);
        u8g2->sendBuffer();
       // tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 30);
        tft.println("Transmitting: OK!");
 /*       tft.setCursor(0, 40);
        tft.print("                ");
        tft.setCursor(0, 40);*/
        tft.drawString("Sending: ", 0, 40, 1);
      //  tft.setTextColor(TFT_BLACK);
        tft.fillRect(47, 38, 20, 12, TFT_BLACK);

        //tft.setCursor(50, 40);
        //tft.print("    ");
        //tft.setTextColor(TFT_WHITE);
        tft.drawNumber(counter, 50, 40, 1);
       // tft.print(buf);
    }
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
