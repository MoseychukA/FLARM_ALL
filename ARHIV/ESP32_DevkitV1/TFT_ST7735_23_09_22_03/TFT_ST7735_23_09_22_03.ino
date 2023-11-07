#include <LoRa.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>


TFT_eSPI tft = TFT_eSPI();       // Invoke custom library


int counter = 0;
float p = 3.1415926;

#define RADIO_CS_PIN   22
#define RADIO_RST_PIN  15
#define RADIO_DIO0_PIN -1
#define LoRa_frequency 868800000

int TFT_LED  =  5;       //

//int ledState = LOW;             // ledState used to set the LED

unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 3000;           // interval at which to blink (milliseconds)



void setup()
{ 
    Serial.begin(115200);
    Serial.println();


    delay(1000);
    pinMode(TFT_LED, OUTPUT);
    digitalWrite(TFT_LED, HIGH);

    // Use this initializer if you're using a 1.8" TFT
    tft.init();   // initialize
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);

    tft.setTextWrap(false);
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 10);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
   // tft.println("ÐÓÑÑÊÈÉ");


    Serial.println("LoRa Sender");
    LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);
    if (!LoRa.begin(LoRa_frequency)) {
        Serial.println("Starting LoRa failed!");
        while (1);
    }

    LoRa.setTxPower(20, PA_OUTPUT_PA_BOOST_PIN);

}

void loop()
{

 /*   tft.setTextWrap(true, true);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);*/

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

        char buf[256];
        tft.setCursor(0, 30);
        tft.println("Received OK!");
        tft.drawString("Received: ", 0, 40, 1);
        //  tft.fillRect(40, 38, 20, 12, TFT_BLACK);
        tft.drawString(recv.c_str(), 60, 40);
        snprintf(buf, sizeof(buf), "RSSI:%i", LoRa.packetRssi());

        tft.drawString(buf, 0, 50);


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
        LoRa.print("hello1 ");
        LoRa.print(counter);
        LoRa.endPacket();

        tft.setCursor(0, 70);
        tft.println("Transmitting: OK!");
        tft.drawString("Sending: hello1", 0, 80, 1);
        //  tft.setTextColor(TFT_BLACK);
      //  tft.fillRect(47, 78, 20, 12, TFT_BLACK);
        tft.drawNumber(counter, 95, 80, 1);


        //if (ledState == LOW) {
        //    ledState = HIGH;
        //}
        //else {
        //    ledState = LOW;
        //}

        //// set the LED with the ledState of the variable:
        //digitalWrite(ledPin, ledState);

        counter++;

    }


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
