#include <SPI.h>
#include <LoRa.h>
 #define Serial SERIAL_PORT_USBVIRTUAL
int counter = 0;
const int ledPin =  28;//LED_BUILTIN;// the number of the LED pin
int ledState = LOW;             // ledState used to set the LED

// set pin for SAMD21E18A
//setPins(int ss = LORA_DEFAULT_SS_PIN, int reset = LORA_DEFAULT_RESET_PIN, int dio0 = LORA_DEFAULT_DIO0_PIN);
# define LORA_SS_PIN 23
# define LORA_RESET_PIN 4
# define LORA_DIO0_PIN 5

void setup() {
  Serial.begin(115200);
 // delay(1000);
// while (!Serial){};
 pinMode (ledPin, OUTPUT);
  LoRa.setPins(LORA_SS_PIN, LORA_RESET_PIN, LORA_DIO0_PIN);
  Serial.println("LoRa Sender");
 //if (!LoRa.begin(868E6)) 
  if (!LoRa.begin(868800000)) 
  {
    Serial.println("Starting LoRa failed!"); 
    while (1);
  }

LoRa.setTxPower(20,PA_OUTPUT_PA_BOOST_PIN);
  
}

void loop() {
 //  Serial.println("GET=DATETIME");
  Serial.print("Sending packet: ");
  Serial.println(counter);
   // send packet
  LoRa.beginPacket();
 // LoRa.print("GET=DATETIME");
  LoRa.print("hello ");
  LoRa.print(counter);
  LoRa.endPacket();

  
  Serial.print("REG_PA_CONFIG = ");
  Serial.println(LoRa.readRegister(0x09),HEX);
  Serial.println(LoRa.readRegister(0x09),BIN);
    
  Serial.print("REG_PA_DAC = ");
  Serial.println(LoRa.readRegister(0x4d),HEX);
  Serial.println(LoRa.readRegister(0x4d),BIN);
  
  Serial.print("REG_OCP = ");
  Serial.println(LoRa.readRegister(0x0b),HEX);
  Serial.println(LoRa.readRegister(0x0b),BIN);
  counter++;

    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }

    // set the LED with the ledState of the variable:
    digitalWrite(ledPin, ledState);

  delay(1500);
}
