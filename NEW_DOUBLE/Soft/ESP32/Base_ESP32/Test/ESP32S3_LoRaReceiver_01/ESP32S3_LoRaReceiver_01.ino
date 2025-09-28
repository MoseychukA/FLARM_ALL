//4. Пример приемника
#include <SPI.h>
#include <LoRa.h>


// Переопредели пины на свои!
#define LORA_SCK    12
#define LORA_MISO   13
#define LORA_MOSI   11
#define LORA_CS     46
#define LORA_RST    7
#define LORA_IRQ    18

// Параметры LoRa
long FREQUENCY = 8688E5;       // 868 МГц
int SPREADING_FACTOR = 8;     // ДОЛЖЕН совпадать с передатчиком!
long BANDWIDTH = 125E3;
int CODING_RATE = 5;
int TX_POWER = 14;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);

  if (!LoRa.begin(FREQUENCY)) {
    Serial.println("LoRa init failed!");
    while (1);
  }

  LoRa.setSpreadingFactor(SPREADING_FACTOR);
  LoRa.setSignalBandwidth(BANDWIDTH);
  LoRa.setCodingRate4(CODING_RATE);
  LoRa.setTxPower(TX_POWER);

  Serial.println("LoRa Receiver started!");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print("Received packet: ");
    while (LoRa.available()) {
      Serial.print((char)LoRa.read());
    }
    Serial.print(" RSSI: ");
    Serial.println(LoRa.packetRssi());
  }
}
//5. Как менять параметры “на лету”
//Для тестов вставь в loop такой цикл (на передатчике):

//for (int sf=7; sf<=12; sf++) {
//  LoRa.setSpreadingFactor(sf);
//  Serial.print("SF="); Serial.println(sf);
//  for (int i=0; i<5; i++) {
//    LoRa.beginPacket();
//    LoRa.print("SF: "); LoRa.print(sf);
//    LoRa.print(" packet #"); LoRa.print(i);
//    LoRa.endPacket();
//    delay(1000);
//  }
//}