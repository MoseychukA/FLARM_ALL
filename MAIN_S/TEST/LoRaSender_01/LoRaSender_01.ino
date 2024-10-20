#include <LoRa.h>
#include "boards.h"
#include "I2C_AXP192.h"


I2C_AXP192 axp192(I2C_AXP192_DEFAULT_ADDRESS, Wire1);
const int btnPin = 37;

int counter = 0;

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
      .GPIO0 = 3300,
      .GPIO1 = -1,
      .GPIO2 = -1,
      .GPIO3 = -1,
      .GPIO4 = -1,
    };
    axp192.begin(initDef); 

    pinMode(btnPin, INPUT);

    Serial.println("LoRa Sender");
    LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);
    if (!LoRa.begin(LoRa_frequency)) {
        Serial.println("Starting LoRa failed!");
        while (1);
    }
}

void loop()
{
    Serial.print("Sending packet: ");
    Serial.println(counter);

    // send packet
    LoRa.beginPacket();
    LoRa.print("Hello ");
    LoRa.print(counter);
    LoRa.endPacket();

#ifdef HAS_DISPLAY
    if (u8g2) {
        char buf[256];
        u8g2->clearBuffer();
        u8g2->drawStr(0, 12, "Transmitting: OK!");
        snprintf(buf, sizeof(buf), "Sending: %d", counter);
        u8g2->drawStr(0, 30, buf);
        u8g2->sendBuffer();
    }
#endif
    counter++;
    delay(2000);
}
