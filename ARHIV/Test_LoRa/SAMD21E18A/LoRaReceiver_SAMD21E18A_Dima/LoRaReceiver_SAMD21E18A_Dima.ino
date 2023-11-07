#include <SPI.h>
#include <LoRa.h>

  #define Serial SERIAL_PORT_USBVIRTUAL
  
void setup() {
  Serial.begin(57600);
 
  while (!Serial);
  LoRa.setPins(23,4,2);         // Только для  ATMEL_SAMD21E18A
  Serial.println("LoRa Receiver");

  if (!LoRa.begin(868E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  else
  {
    Serial.println("Starting Ok!");
  }
}

void loop() {
  // try to parse packet
  int packetSize = LoRa.parsePacket();
   //  Serial.println(packetSize);
  if (packetSize) {
    // received a packet
    Serial.print("Received packet '");

    // read packet
    while (LoRa.available()) {
      Serial.print((char)LoRa.read());
    }

    // print RSSI of packet
    Serial.print("' with RSSI ");
    Serial.println(LoRa.packetRssi());
 }

 delay(100);
}
