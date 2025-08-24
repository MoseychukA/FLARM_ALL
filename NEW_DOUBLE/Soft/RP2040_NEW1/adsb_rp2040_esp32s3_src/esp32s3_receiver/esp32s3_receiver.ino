// esp32s3_receiver.ino
// Board: ESP32S3 Dev Module
// UART: TX GPIO41, RX GPIO42
// Receives PackedFO + CRC32 and prints to Serial

#include <Arduino.h>

static const int UART_TX = 41;
static const int UART_RX = 42;
static const uint32_t UART_BAUD = 921600;

#pragma pack(push, 1)
struct PackedFO {
  uint32_t magic;       // 'FOv1'
  uint32_t addr;
  uint16_t Squawk;
  char     flight[9];
  int32_t  altitude;
  int32_t  pressure_altitude;
  uint16_t speed;
  uint16_t course;
  int16_t  vert_rate;
  int32_t  lat_mdeg;
  int32_t  lon_mdeg;
  uint32_t seen;
  uint32_t timestamp;
  uint8_t  signal_source;
  uint8_t  aircraft_type;
};
#pragma pack(pop)

uint32_t crc32_ieee(const uint8_t *data, size_t len){
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i=0;i<len;i++){
    uint8_t b = data[i];
    crc ^= b;
    for (int k=0;k<8;k++){
      uint32_t mask = -(crc & 1u);
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc;
}

void setup(){
  Serial.begin(115200);
  Serial1.begin(UART_BAUD, SERIAL_8N1, UART_RX, UART_TX);
  Serial.println("ESP32S3 Receiver start");
}

void loop(){
  static uint8_t buf[sizeof(PackedFO)+4];
  static size_t got=0; size_t need = sizeof(buf);
  while (Serial1.available() && got<need){ buf[got++] = Serial1.read(); }
  if (got==need){
    PackedFO pkt; memcpy(&pkt, buf, sizeof(pkt));
    uint32_t rcrc; memcpy(&rcrc, buf+sizeof(pkt), 4);
    uint32_t ccrc = crc32_ieee((const uint8_t*)&pkt, sizeof(pkt));
    if (ccrc==rcrc && pkt.magic==0x3141764F){
      Serial.print("ADDR="); Serial.print(pkt.addr, HEX);
      Serial.print(" FLT="); Serial.print(pkt.flight);
      Serial.print(" LAT="); Serial.print(pkt.lat_mdeg/1e6,6);
      Serial.print(" LON="); Serial.print(pkt.lon_mdeg/1e6,6);
      Serial.print(" SPD="); Serial.print(pkt.speed);
      Serial.print(" ALT="); Serial.println(pkt.altitude);
      Serial.println("OK");
    } else {
      Serial.println("CRC FAIL or MAGIC");
    }
    got=0;
  }
}
