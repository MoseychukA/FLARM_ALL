#include <SPI.h>
#include <LoRa.h>

int counter = 0;
# define ledPin 4
//const int ledPin = 4;
int ledState = LOW;             // ledState used to set the LED


//setPins(int ss = LORA_DEFAULT_SS_PIN, int reset = LORA_DEFAULT_RESET_PIN, int dio0 = LORA_DEFAULT_DIO0_PIN);
# define LORA_SS_PIN 46
# define LORA_RESET_PIN 7
# define LORA_DIO0_PIN -1

void setup() 
{
  Serial.begin(115200);
 // delay(1000);
// while (!Serial){};
 pinMode (ledPin, OUTPUT);
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
 //  Serial.println("GET=DATETIME");
  Serial.print("Sending packet: ");
  Serial.println(counter);
   // send packet
  LoRa.beginPacket();
 // LoRa.print("GET=DATETIME");
  LoRa.print("SX1276 ");
  LoRa.print(counter);
  LoRa.endPacket();

  
  counter++;

    if (ledState == LOW) 
    {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }

    // set the LED with the ledState of the variable:
    digitalWrite(ledPin, ledState);

  delay(4300);
}
