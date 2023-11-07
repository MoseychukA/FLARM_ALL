
#include <LoRa.h>
#include "boards.h"
int counter = 0;

unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 3000;           // interval at which to blink (milliseconds)



void setup()
{
    initBoard();
    // When the power is turned on, a delay is required.
    delay(1500);

    Serial.println("LoRa Receiver");

    LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);
    if (!LoRa.begin(LoRa_frequency)) {
        Serial.println("Starting LoRa failed!");
        while (1);
    }

    LoRa.setTxPower(20, PA_OUTPUT_PA_BOOST_PIN);

    u8g2->clearBuffer();
}

void loop()
{
    // try to parse packet
    int packetSize = LoRa.parsePacket();
    if (packetSize) 
    {
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
          //  u8g2->clearBuffer();
            char buf[256];
            u8g2->drawStr(0, 12, "Received OK!");
            u8g2->setDrawColor(0);// Black
            u8g2->drawBox(0, 14, 80, 12);
            u8g2->drawBox(0, 28, 80, 12);
            u8g2->setDrawColor(1);
            u8g2->drawStr(0, 26, recv.c_str());
            snprintf(buf, sizeof(buf), "RSSI:%i", LoRa.packetRssi());
            u8g2->drawStr(0, 40, buf);
          /*  snprintf(buf, sizeof(buf), "SNR:%.1f", LoRa.packetSnr());
            u8g2->drawStr(0, 56, buf);*/
            u8g2->sendBuffer();
        }
#endif
    }

    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval)
    {
        // save the last time you blinked the LED
        previousMillis = currentMillis;

        Serial.print("Sending packet: ");
        Serial.println(counter);

        // send packet
        LoRa.beginPacket();
        LoRa.print("hello3 ");
        LoRa.print(counter);
        LoRa.endPacket();

        Serial.print("REG_PA_CONFIG = ");
        Serial.println(LoRa.readRegister(0x09), HEX);
        Serial.println(LoRa.readRegister(0x09), BIN);

        Serial.print("REG_PA_DAC = ");
        Serial.println(LoRa.readRegister(0x4d), HEX);
        Serial.println(LoRa.readRegister(0x4d), BIN);

        Serial.print("REG_OCP = ");
        Serial.println(LoRa.readRegister(0x0b), HEX);
        Serial.println(LoRa.readRegister(0x0b), BIN);
#ifdef HAS_DISPLAY
        if (u8g2) {
            char buf[256];
            u8g2->drawStr(0, 52, "Transmitting: OK!");
            snprintf(buf, sizeof(buf), "Sending: %d", counter);
            u8g2->setDrawColor(0);// Black
            u8g2->drawBox(64, 53, 30, 11);
            u8g2->setDrawColor(1);
            u8g2->drawStr(0, 64, buf);
            u8g2->sendBuffer();
        }
#endif
        counter++;

    }

}
