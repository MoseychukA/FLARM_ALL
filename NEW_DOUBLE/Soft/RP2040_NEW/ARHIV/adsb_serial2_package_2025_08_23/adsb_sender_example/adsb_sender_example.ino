// adsb_sender_example.ino
// Example of sending FlightObject packets over Serial2
// Integrate this file's relevant parts (protocol_send_flight and FlightObject) into your ADS-B sketch.

#include <Arduino.h>
#include "../common/protocol.h"

// Choose your baudrate
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 230400
#endif

// For ESP32-based sender you can set pins here, otherwise default mapping is used
#if defined(ARDUINO_ARCH_ESP32)
#define SENDER_UART_RX -1  // unused on sender
#define SENDER_UART_TX -1  // unused on sender
#endif

HardwareSerial &LINK = Serial2; // adjust if your board uses a different port

// Simulated receivedPacket structure for demo purposes only
struct DemoPacket {
  uint32_t addr; char squawk[8]; char flight[9];
  int32_t altitude; int32_t speed; uint16_t track; int32_t vert_rate;
  float lat_msg; float lon_msg; uint32_t seen_time;
};

static uint32_t fakeNow() { return millis()/1000; }

void setup() {
  Serial.begin(115200);
  delay(1000);
#if defined(ARDUINO_ARCH_ESP32)
  LINK.begin(SERIAL_BAUD); // pins can be set if needed: LINK.begin(SERIAL_BAUD, SERIAL_8N1, SENDER_UART_RX, SENDER_UART_TX);
#else
  LINK.begin(SERIAL_BAUD);
#endif
  Serial.println("Sender started");
}

void loop() {
  // DEMO: send a test packet every 2 seconds
  static uint32_t t0 = 0; if (millis()-t0 < 2000) return; t0 = millis();

  DemoPacket receivedPacket = {};
  receivedPacket.addr = 0xABCDEF;
  strcpy(receivedPacket.squawk, "1200");
  strcpy(receivedPacket.flight, "SU1234");
  receivedPacket.altitude = 1200;
  receivedPacket.speed = 720;
  receivedPacket.track = 270;
  receivedPacket.vert_rate = 300;
  receivedPacket.lat_msg = 55.7558f;
  receivedPacket.lon_msg = 37.6176f;
  receivedPacket.seen_time = millis()/1000;

  FlightObject fo = {};
  fo.addr = receivedPacket.addr; // ICAO address
  fo.squawk = (uint16_t)atoi(receivedPacket.squawk);
  memset(fo.flight, 0, sizeof(fo.flight));
  strncpy(fo.flight, receivedPacket.flight, sizeof(fo.flight)-1);
  fo.altitude = receivedPacket.altitude; // meters
  fo.pressure_altitude = receivedPacket.altitude; // meters
  fo.speed = (uint16_t)receivedPacket.speed; // km/h
  fo.course = receivedPacket.track; // degrees
  fo.vert_rate = receivedPacket.vert_rate; // m/min
  fo.latitude = receivedPacket.lat_msg;
  fo.longitude = receivedPacket.lon_msg;
  fo.seen = receivedPacket.seen_time;
  fo.timestamp = fakeNow();
  fo.signal_source = 1; // DUMP1090
  fo.aircraft_type = 9;

  protocol_send_flight(LINK, fo);
  Serial.println("Sent FlightObject frame");
}
