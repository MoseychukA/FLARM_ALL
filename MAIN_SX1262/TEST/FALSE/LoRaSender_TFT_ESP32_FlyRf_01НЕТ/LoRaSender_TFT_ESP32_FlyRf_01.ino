#include <SPI.h>
#include <LoRa.h>
#include <TFT_eSPI.h> // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

int counter = 0;
const int ledPin = 4;// the number of the LED pin

int ledState = LOW;             // ledState used to set the LED 

unsigned long previousMillis = 0;        // will store last time LED was updated
// constants won't change:
const long interval = 3000;           // interval at which to blink (milliseconds)


# define LORA_SS_PIN 46
# define LORA_RESET_PIN 7
# define LORA_DIO0_PIN 18
#define LORA_BYP_PIN 14
#define LORA_EN_PIN 17
#define LORA_BUSY_PIN 18
#define LORA_DIO1_PIN 1
#define LORA_DIO2_PIN 2



void setup() {
  
  Serial.begin(115200);

  delay(500);
  Serial.println("Starting setup!");

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  pinMode(LORA_BYP_PIN, OUTPUT);
  digitalWrite(LORA_BYP_PIN, HIGH);

  pinMode(LORA_EN_PIN, OUTPUT);
  digitalWrite(LORA_EN_PIN, HIGH);


 //// Use this initializer if you're using a 1.8" TFT
 //tft.init();   // initialize
 //tft.setRotation(3); // Set the display image orientation to 0, 1, 2 or 3

 //tft.fillScreen(TFT_BLACK);

 //tft.setTextWrap(false);
 //tft.fillScreen(TFT_BLACK);
 //tft.setCursor(0, 10);
 //tft.setTextColor(TFT_WHITE);
 //tft.setTextSize(2);
 //tft.println("LoRa Sender");


  LoRa.setPins(LORA_SS_PIN, LORA_RESET_PIN, LORA_DIO0_PIN);
  Serial.println("LoRa Sender");
 
  if (!LoRa.begin(868800000)) 
  {
    Serial.println("Starting LoRa failed!"); 
    while (1);
  }

LoRa.setTxPower(20,PA_OUTPUT_PA_BOOST_PIN);
  
Serial.println("Setup END!");
Serial1.println("Setup END!");


}

void loop() {
 
 
  //tft.setTextWrap(true, true);

  //// Font and background colour, background colour is used for anti-alias blending
  //tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  /*Serial.print("REG_PA_CONFIG = ");
  Serial.println(LoRa.readRegister(0x09),HEX);
  Serial.println(LoRa.readRegister(0x09),BIN);
    
  Serial.print("REG_PA_DAC = ");
  Serial.println(LoRa.readRegister(0x4d),HEX);
  Serial.println(LoRa.readRegister(0x4d),BIN);
  
  Serial.print("REG_OCP = ");
  Serial.println(LoRa.readRegister(0x0b),HEX);
  Serial.println(LoRa.readRegister(0x0b),BIN);*/


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
    //  tft.setCursor(0, 30);
    //  tft.println("Received OK!");
    //  tft.drawString("Received: ", 0, 60, 1);
    ////  tft.fillRect(40, 38, 20, 12, TFT_BLACK);
    //  tft.drawString(recv.c_str(),60, 60);
    //  snprintf(buf, sizeof(buf), "RSSI:%i", LoRa.packetRssi());

    //  tft.drawString(buf,0, 50);
   

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
      tft.setCursor(10, 70);
      tft.println(" Transmitting: OK!");
      tft.drawString("Sending: hello", 10, 120, 1);
      //  tft.setTextColor(TFT_BLACK);
    //  tft.fillRect(47, 78, 20, 12, TFT_BLACK);
      tft.drawNumber(counter, 195, 120, 1);


      if (ledState == LOW) {
          ledState = HIGH;
      }
      else {
          ledState = LOW;
      }

      // set the LED with the ledState of the variable:
      //digitalWrite(ledPin, ledState);

      counter++;

  }

}
