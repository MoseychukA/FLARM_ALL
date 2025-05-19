#include <SPI.h>
#include <LoRa.h>
#include <TFT_eSPI.h> // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

int counter = 0;
const int ledPin =  4;//LED_BUILTIN;// the number of the LED pin
//const int ledTFT = 5; 
int ledState = LOW;             // ledState used to set the LED 

unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 3000;           // interval at which to blink (milliseconds)


# define LORA_SS_PIN 46
# define LORA_RESET_PIN 7
# define LORA_DIO0_PIN 3
#define LORA_BYP_PIN 14
#define LORA_EN_PIN 17
#define LORA_BUSY_PIN 18
#define LORA_DIO1_PIN 1
#define LORA_DIO2_PIN 2

void setup() 
{

    // Begin serial communication
    Serial.begin(115200);

    delay(500);
    Serial.println("Starting setup!");

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, HIGH);

    pinMode(LORA_BYP_PIN, OUTPUT);
    digitalWrite(LORA_BYP_PIN, HIGH);

    pinMode(LORA_EN_PIN, OUTPUT);
    digitalWrite(LORA_EN_PIN, HIGH);

  delay(500);
  Serial.println("Starting setup!");
 
//
//  pinMode(ledTFT, OUTPUT);
 // digitalWrite(ledTFT, HIGH);
//
// while (!Serial){};
 pinMode (ledPin, OUTPUT);

 // Use this initializer if you're using a 1.8" TFT
 tft.init();   // initialize
 tft.setRotation(3); // Set the display image orientation to 0, 1, 2 or 3

 tft.fillScreen(TFT_BLACK);

 tft.setTextWrap(false);
 tft.fillScreen(TFT_BLACK);
 tft.setCursor(10, 10);
 tft.setTextColor(TFT_WHITE);
 tft.setTextSize(2);
 tft.println("LoRa Sender");


  LoRa.setPins(LORA_SS_PIN, LORA_RESET_PIN, LORA_DIO0_PIN);
  Serial.println("LoRa Sender");
 
  if (!LoRa.begin(868800000)) 
  {
    Serial.println("Starting LoRa failed!"); 
    while (1);
  }

LoRa.setTxPower(20,PA_OUTPUT_PA_BOOST_PIN); // Максимальная мощность передатчика
LoRa.setSpreadingFactor(7);                 // SF12 максимальная дальность
LoRa.enableCrc();                           // Контроль пакета 
LoRa.setOCP(240);                           // Установить максимальный ток передатчика.

//LoRa.dumpRegisters(Serial);


Serial.println("Setup END!");
}

void loop() {
 
 
  tft.setTextWrap(true, true);

  // Font and background colour, background colour is used for anti-alias blending
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
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
      tft.setCursor(0, 45);
      tft.println(" Received OK!");
      tft.drawString(" Received: ", 0, 80, 1);
    //  tft.fillRect(40, 38, 20, 12, TFT_BLACK);
      tft.drawString(recv.c_str(),140, 80);
      snprintf(buf, sizeof(buf), " RSSI:%i", LoRa.packetRssi());

      tft.drawString(buf,0, 110);
   

  }

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval)
  {
      // save the last time you blinked the LED
      previousMillis = currentMillis;

      Serial1.print("Sending packet: ");
      Serial1.println(counter);

      // send packet
      LoRa.beginPacket();
      LoRa.print("hello1 ");
      LoRa.print(counter);
      LoRa.endPacket();
      delay(100);
      tft.setCursor(0, 140);
      tft.println(" Transmitting: OK!");
      tft.drawString("Sending: hello", 10, 170, 1);
      //  tft.setTextColor(TFT_BLACK);
    //  tft.fillRect(47, 78, 20, 12, TFT_BLACK);
      tft.drawNumber(counter, 195, 170, 1);


      if (ledState == LOW) 
      {
          ledState = HIGH;
      }
      else 
      {
          ledState = LOW;
      }

      digitalWrite(ledPin, ledState);

      counter++;

  }

}
