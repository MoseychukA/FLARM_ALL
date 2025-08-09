/*
  ModbusRTU ESP8266/ESP32
  Read multiple coils from slave device example

  (c)2019 Alexander Emelianov (a.m.emelianov@gmail.com)
  https://github.com/emelianov/modbus-esp8266

  modified 13 May 2020
  by brainelectronics

  This code is licensed under the BSD New License. See LICENSE.txt for more info.
*/

#include <ModbusRTU.h>
#include <HardwareSerial.h>

#define DE_RE_PIN 21
#define RX_PIN 17
#define TX_PIN 18
#define SLAVE_ID 1

HardwareSerial rs485Serial(1);

ModbusRTU mb;

bool cbWrite(Modbus::ResultCode event, uint16_t transactionId, void* data) {
#ifdef ESP8266
  Serial.printf_P("Request result: 0x%02X, Mem: %d\n", event, ESP.getFreeHeap());
#elif ESP32
  Serial.printf_P("Request result: 0x%02X, Mem: %d\n", event, ESP.getFreeHeap());
#else
  Serial.print("Request result: 0x");
  Serial.print(event, HEX);
#endif
  return true;
}

void setup() {
  Serial.begin(115200);
  // Настройка RS485
  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, LOW);

  // Инициализация последовательного порта
  rs485Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  mb.begin(&rs485Serial, DE_RE_PIN);
  mb.master();
}

bool coils[20];

void loop() 
{
  if (!mb.slave())
  {
    mb.readCoil(1, 1, coils, 20, cbWrite);
  }
  mb.task();
  yield();
}