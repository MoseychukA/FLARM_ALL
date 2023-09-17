#include <SPI.h>
#include <LoRa.h>
#include <TFT_eSPI.h> // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

int counter = 0;
const int ledPin =  2;//LED_BUILTIN;// the number of the LED pin
const int ledTFT = 5; 
int ledState = LOW;             // ledState used to set the LED

unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 3000;           // interval at which to blink (milliseconds)

//setPins(int ss = LORA_DEFAULT_SS_PIN, int reset = LORA_DEFAULT_RESET_PIN, int dio0 = LORA_DEFAULT_DIO0_PIN);
# define LORA_SS_PIN 22
# define LORA_RESET_PIN 15
# define LORA_DIO0_PIN -1

void setup() {
  Serial.begin(115200);


  pinMode(ledTFT, OUTPUT);
  digitalWrite(ledTFT, HIGH);

// while (!Serial){};
 pinMode (ledPin, OUTPUT);

 // Use this initializer if you're using a 1.8" TFT
 tft.init();   // initialize
 tft.setRotation(2); // Set the display image orientation to 0, 1, 2 or 3

 tft.fillScreen(TFT_BLACK);

 tft.setTextWrap(false);
 tft.fillScreen(TFT_BLACK);
 tft.setCursor(0, 10);
 tft.setTextColor(TFT_WHITE);
 tft.setTextSize(1);
 tft.println("LoRa Sender");


  LoRa.setPins(LORA_SS_PIN, LORA_RESET_PIN, LORA_DIO0_PIN);
  Serial.println("LoRa Sender");
 
  if (!LoRa.begin(868800000)) 
  {
    Serial.println("Starting LoRa failed!"); 
    while (1);
  }

LoRa.setTxPower(20,PA_OUTPUT_PA_BOOST_PIN);
  
}

void loop() {
 
 
  tft.setTextWrap(true, true);

  // Font and background colour, background colour is used for anti-alias blending
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
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
      tft.setCursor(0, 30);
      tft.println("Received OK!");
      tft.drawString("Received: ", 0, 40, 1);
    //  tft.fillRect(40, 38, 20, 12, TFT_BLACK);
      tft.drawString(recv.c_str(),60, 40);
      snprintf(buf, sizeof(buf), "RSSI:%i", LoRa.packetRssi());

      tft.drawString(buf,0, 50);
   

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


      if (ledState == LOW) {
          ledState = HIGH;
      }
      else {
          ledState = LOW;
      }

      // set the LED with the ledState of the variable:
      digitalWrite(ledPin, ledState);

      counter++;

  }















}
