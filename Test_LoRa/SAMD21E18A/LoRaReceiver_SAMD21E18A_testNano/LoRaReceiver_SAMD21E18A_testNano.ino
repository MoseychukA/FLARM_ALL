#include <SPI.h>
#include <LoRa.h>

#define Serial SERIAL_PORT_USBVIRTUAL   // Подключаем USB порт в качестве COM порта
const int ledPin =  LED_BUILTIN;// the number of the LED pin
//const int ledPin10 = 10;// the number of the LED pin
// Variables will change :
int ledState = LOW;             // ledState used to set the LED

// set pin for SAMD21E18A
//setPins(int ss = LORA_DEFAULT_SS_PIN, int reset = LORA_DEFAULT_RESET_PIN, int dio0 = LORA_DEFAULT_DIO0_PIN);
# define LORA_SS_PIN 23
# define LORA_RESET_PIN 4
# define LORA_DIO0_PIN 5

int s[40];

void setup() {
  Serial.begin(115200);
  delay(5000);
 // while (!Serial);                     // Ожидаем включение СОМ порта, иначе информация будет выводиться сразу
  pinMode (ledPin, OUTPUT);
  //  pinMode (ledPin10, OUTPUT);
  LoRa.setPins(LORA_SS_PIN, LORA_RESET_PIN, LORA_DIO0_PIN);

  Serial.println("LoRa Receiver");

  if (!LoRa.begin(868E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
    Serial.println("Starting OK!");
}

void loop() {
  // try to parse packet
 // Serial.println("LoRa.parsePacket!");
  int packetSize = LoRa.parsePacket();

  if (packetSize) {
    Serial.print("packetSize '");
    Serial.println(packetSize);
    // received a packet
    Serial.print("Received packet '");
    // read packet
    int i=0;
    while (LoRa.available()) {

      Serial.print(LoRa.read());
    //s[i] = LoRa.read();
      // Serial.print(s[i]);
   }
   /*
  for(int x=0; i<packetSize;i++)
  {
       Serial.print(s[i]);
       s[i]=0;
  }
*/
    // print RSSI of packet
    Serial.print("' with RSSI ");
    Serial.println(LoRa.packetRssi());
    if (ledState == LOW)
    {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }
   Serial.flush();
    // set the LED with the ledState of the variable:
    digitalWrite(ledPin, ledState);
  }
  delay(100);
}
