#include <SX126x.h>
#include <SX127x.h>

#define SX126X
//#define SX127X

#if defined(SX126X)
SX126x LoRa;
#elif defined(SX127X)
SX127x LoRa;
#endif

// gateway ID and node ID
uint8_t gatewayId = 0xCC;
uint8_t nodeId = 0x77;

// Message structure to transmit
struct dataObject {
  uint8_t gatewayId;
  uint8_t nodeId;
  uint16_t messageId;
  uint32_t time;
  int32_t data;
};
dataObject message;
uint8_t messageLen = sizeof(dataObject);


int counter = 0;
const int ledPin = 4;// the number of the LED pin

int ledState = LOW;             // ledState used to set the LED 

unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 3000;           // interval at which to blink (milliseconds)

#define LMIC_UNUSED_PIN 255
#define LORA_SS_PIN 46
#define LORA_RESET_PIN 7
#define LORA_DIO0_PIN 3
#define LORA_BYP_PIN 14
#define LORA_EN_PIN 17
#define LORA_BUSY_PIN 18
#define LORA_DIO1_PIN 1
#define LORA_DIO2_PIN 2




void setup() {

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

#if defined(SX126X)
  // Begin LoRa radio and set NSS, reset, busy, IRQ, txen, and rxen pin with connected arduino pins
  Serial.println("Begin LoRa radio");
  int8_t nssPin = 46, resetPin = 7, busyPin = 18, irqPin = 42, txenPin = -1, rxenPin = -1;
  if (!LoRa.begin(nssPin, resetPin, busyPin, irqPin, txenPin, rxenPin))
  {
    Serial.println("Something wrong, can't begin LoRa radio");
    while(1);
  }
  // Configure TCXO used in RF module
  Serial.println("Set RF module to use TCXO as clock reference");
  LoRa.setDio3TcxoCtrl(SX126X_DIO3_OUTPUT_1_8, SX126X_TCXO_DELAY_10);
#elif defined(SX127X)
  // Begin LoRa radio and set NSS, reset, IRQ, txen, and rxen pin with connected arduino pins
  Serial.println("Begin LoRa radio");
  int8_t nssPin = 10, resetPin = 9, irqPin = 2, txenPin = 8, rxenPin = 7;
  if (!LoRa.begin(nssPin, resetPin, irqPin, txenPin, rxenPin)){
    Serial.println("Something wrong, can't begin LoRa radio");
    while(1);
  }
#endif

  // Set frequency to 868 Mhz
  Serial.println("Set frequency to 868 Mhz");
  LoRa.setFrequency(868800000);

  // Set TX power to 17 dBm
  Serial.println("Set TX power to +22 dBm");
  LoRa.setTxPower(17);

  // Configure modulation parameter including spreading factor (SF), bandwidth (BW), and coding rate (CR)
  Serial.println("Set modulation parameters:\n\tSpreading factor = 7\n\tBandwidth = 125 kHz\n\tCoding rate = 4/5");
  uint8_t sf = 7;
  uint32_t bw = 125000;
  uint8_t cr = 5;
  LoRa.setLoRaModulation(sf, bw, cr);

  // Configure packet parameter including header type, preamble length, payload length, and CRC type
  Serial.println("Set packet parameters:\n\tImplicit header type\n\tPreamble length = 12\n\tPayload Length = message length\n\tCRC on");
  uint8_t headerType = LORA_HEADER_IMPLICIT;
  uint16_t preambleLength = 12;
  uint8_t payloadLength = messageLen;
  bool crcType = true;
  LoRa.setLoRaPacket(headerType, preambleLength, payloadLength, crcType);

  // Set syncronize word for private network (0x3444)
  Serial.println("Set syncronize word to 0x3444");
  LoRa.setSyncWord(0x3444);

  Serial.println("\n-- LORA NODE --\n");
  
  // Assign gateway Id and node Id to message object
  message.gatewayId = gatewayId;
  message.nodeId = nodeId;
  message.messageId = 0;

}

void loop() {

    // Присвоить данным случайное значение и время текущее время
  message.data = random(-1073741824, 1073741824);
  message.time = millis();
  message.messageId++;
  digitalWrite(ledPin, LOW);
  // Передача объекта сообщения
  LoRa.beginPacket();
  LoRa.put(message);
  LoRa.endPacket();

  // Print message in serial
  Serial.print("Gateway ID    : 0x");
  if (message.gatewayId < 0x10) Serial.print("0");
  Serial.println(message.gatewayId, HEX);
  Serial.print("Node ID       : 0x");
  if (message.nodeId < 0x10) Serial.print("0");
  Serial.println(message.nodeId, HEX);
  Serial.print("Message ID    : ");
  Serial.println(message.messageId);
  Serial.print("Time          : ");
  Serial.println(message.time);
  Serial.print("Data          : ");
  Serial.println(message.data);

  // Дождитесь окончания процесса модуляции для передачи пакета
  LoRa.wait();
  //delay(5);
  // Print transmit time
  Serial.print("Transmit time : ");
  Serial.print(LoRa.transmitTime());
  Serial.println(" ms");
  Serial.println();
  digitalWrite(ledPin, HIGH);
  // Перевести радиочастотный модуль в спящий режим через несколько секунд
  LoRa.sleep();
  delay(3000);
  LoRa.wake();

}
