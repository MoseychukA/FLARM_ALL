//3. Пример передатчика с гибкими параметрами
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
int  SPREADING_FACTOR = 8;    // 7...12
long BANDWIDTH = 125E3;       // 125E3, 250E3, 500E3
int  CODING_RATE = 5;         // 5=4/5, 6=4/6, 7=4/7, 8=4/8
int  TX_POWER = 20;           // 2..20 дБм

void setup() {
  Serial.begin(115200);
  while (!Serial);
  delay(500);
  // Инициализация LoRa (на своих пинах)
  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);

  if (!LoRa.begin(FREQUENCY)) {
    Serial.println("LoRa init failed!");
    while (1);
  }
  // Установим SF, BW, CR, и мощность передатчика
  LoRa.setSpreadingFactor(SPREADING_FACTOR);
  LoRa.setSignalBandwidth(BANDWIDTH);
  LoRa.setCodingRate4(CODING_RATE);
  LoRa.setTxPower(TX_POWER);

  Serial.print("LoRa init OK! Params: ");
  Serial.print("SF="); Serial.print(SPREADING_FACTOR);
  Serial.print(" BW="); Serial.print(BANDWIDTH/1000); Serial.print("kHz");
  Serial.print(" CR=4/"); Serial.print(CODING_RATE);
  Serial.print(" TxPwr="); Serial.print(TX_POWER); Serial.println("dBm");
}

void loop() {
  // отправляем тестовое сообщение каждую секунду
  static unsigned long cnt = 0;
  LoRa.beginPacket();
  LoRa.print("LoRa test packet #");
  LoRa.print(cnt);
  LoRa.endPacket();
  Serial.print("Sent packet #"); Serial.println(cnt++);
  delay(1000);
}