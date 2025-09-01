// esp32s3_receiver.ino
// ESP32S3 receiver of FlightObject frames over Serial (UART)
// Pins: UART_TX_PIN GPIO41, UART_RX_PIN GPIO42 (per request)

#include <Arduino.h>
#include "../common/protocol.h"

// Configure UART pins and baud
#define UART_TX_PIN 41
#define UART_RX_PIN 42
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 230400
#endif

HardwareSerial &LINK = Serial2;
ProtocolReceiver rx;

void printFlight(const FlightObject &fo) {
  Serial.printf("addr: %06X\n", (unsigned)fo.addr);
  Serial.printf("squawk: %u\n", (unsigned)fo.squawk);
  Serial.printf("flight: %s\n", fo.flight);
  Serial.printf("altitude(m): %ld, pressure_altitude(m): %ld\n", (long)fo.altitude, (long)fo.pressure_altitude);
  Serial.printf("speed(kmh): %u, course(deg): %u\n", (unsigned)fo.speed, (unsigned)fo.course);
  Serial.printf("vert_rate(m/min): %ld\n", (long)fo.vert_rate);
  Serial.printf("lat: %.6f, lon: %.6f\n", fo.latitude, fo.longitude);
  Serial.printf("seen: %lu, timestamp: %lu\n", (unsigned long)fo.seen, (unsigned long)fo.timestamp);
  Serial.printf("signal_source: %u, aircraft_type: %u\n\n", (unsigned)fo.signal_source, (unsigned)fo.aircraft_type);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  // Initialize Serial2 on requested pins
  LINK.begin(SERIAL_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  Serial.println("ESP32S3 receiver started");
}

void loop() {
  FlightObject fo;
  if (rx.poll(LINK, fo)) {
    printFlight(fo);
  }
}
