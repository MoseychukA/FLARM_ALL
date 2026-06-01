#include <Arduino.h>
#include <Wire.h>
#include "src/ProjectPins.h"
#include "src/R820T2Controller.h"
#include "src/AdsbReceiver.h"
#include "src/ScreenView.h"

#define Serial_SPEED 115200
#define DEBUG_PORT Serial1

R820T2Controller radio;
AdsbReceiver receiver;
ScreenView screen;

static String projectName()
{
  String name = __FILE__;
  int pos = name.lastIndexOf('\\');
  if (pos < 0) pos = name.lastIndexOf('/');
  name.remove(0, pos + 1);
  pos = name.lastIndexOf('.');
  if (pos > 0) name.remove(pos);
  return name;
}

void setup()
{
  DEBUG_PORT.begin(Serial_SPEED);
  delay(100);
  DEBUG_PORT.flush();
  delay(1000);

  DEBUG_PORT.println("Start system");
  DEBUG_PORT.println("Serial output: UART1 PA9(TX) PA10(RX)");
  DEBUG_PORT.println();

  String ver_soft = projectName();
  DEBUG_PORT.println(ver_soft);

  pinMode(PIN_SOUNDER, OUTPUT);
  digitalWrite(PIN_SOUNDER, LOW);

  Wire.begin();
  screen.begin(ver_soft);
  receiver.begin(PIN_ADSB_SIGNAL);

  bool tunerOk = radio.begin();
  radio.setFrequency(1090000000UL);
  radio.setManualGain(14, 15, 8);

  DEBUG_PORT.print("R820T2: ");
  DEBUG_PORT.println(tunerOk ? "OK" : "not found");
  DEBUG_PORT.println("AD8310 input PA0");
  DEBUG_PORT.println("Start system END");
}

void loop()
{
  receiver.poll();

  if (receiver.available()) {
    const AdsbMessage &msg = receiver.lastMessage();
    screen.showMessage(receiver.stats(), msg);

    DEBUG_PORT.print(msg.crcOk ? "ADS-B OK " : "ADS-B CRC ");
    DEBUG_PORT.print("DF=");
    DEBUG_PORT.print(msg.df);
    DEBUG_PORT.print(" ICAO=");
    if (msg.icao < 0x100000UL) DEBUG_PORT.print('0');
    if (msg.icao < 0x010000UL) DEBUG_PORT.print('0');
    if (msg.icao < 0x001000UL) DEBUG_PORT.print('0');
    if (msg.icao < 0x000100UL) DEBUG_PORT.print('0');
    if (msg.icao < 0x000010UL) DEBUG_PORT.print('0');
    DEBUG_PORT.print(msg.icao, HEX);
    DEBUG_PORT.print(" TC=");
    DEBUG_PORT.print(msg.typeCode);
    DEBUG_PORT.print(" RSSI=");
    DEBUG_PORT.print(msg.signal);
    DEBUG_PORT.print(" RAW=");
    for (uint8_t i = 0; i < msg.bitCount / 8; i++) {
      if (msg.raw[i] < 16) DEBUG_PORT.print('0');
      DEBUG_PORT.print(msg.raw[i], HEX);
    }
    DEBUG_PORT.println();

    if (msg.crcOk) {
      digitalWrite(PIN_SOUNDER, HIGH);
      delay(8);
      digitalWrite(PIN_SOUNDER, LOW);
    }
  }

  screen.update(receiver.stats(), receiver.lastMessage(), receiver.hasMessage());
  yield();
}
