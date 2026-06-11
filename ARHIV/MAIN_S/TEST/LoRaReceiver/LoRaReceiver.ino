
#include <LoRa.h>
#include "boards.h"

#include "I2C_AXP192.h"


I2C_AXP192 axp192(I2C_AXP192_DEFAULT_ADDRESS, Wire1);
const int btnPin = 37;

void setup()
{
    initBoard();
    // When the power is turned on, a delay is required.
    delay(1000);

    Wire1.begin(21, 22);

    I2C_AXP192_InitDef initDef = {
      .EXTEN = true,
      .BACKUP = true,
      .DCDC1 = 3300,
      .DCDC2 = 0,
      .DCDC3 = 3300,
      .LDO2 = 3300,
      .LDO3 = 3300,
      .GPIO0 = 2800,
      .GPIO1 = -1,
      .GPIO2 = -1,
      .GPIO3 = -1,
      .GPIO4 = -1,
    };
    axp192.begin(initDef);

    pinMode(btnPin, INPUT);



    Serial.println("LoRa Receiver");

    LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);
    if (!LoRa.begin(LoRa_frequency)) {
        Serial.println("Starting LoRa failed!");
        while (1);
    }
    else
    {
        Serial.println("Starting LoRa Ok!");
    }
}

void loop()
{
    // try to parse packet
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
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
#ifdef HAS_DISPLAY
        if (u8g2) {
            u8g2->clearBuffer();
            char buf[256];
            u8g2->drawStr(0, 12, "Received OK!");
            u8g2->drawStr(0, 26, recv.c_str());
            snprintf(buf, sizeof(buf), "RSSI:%i", LoRa.packetRssi());
            u8g2->drawStr(0, 40, buf);
            snprintf(buf, sizeof(buf), "SNR:%.1f", LoRa.packetSnr());
            u8g2->drawStr(0, 56, buf);
            u8g2->sendBuffer();
        }
#endif
    }
}
