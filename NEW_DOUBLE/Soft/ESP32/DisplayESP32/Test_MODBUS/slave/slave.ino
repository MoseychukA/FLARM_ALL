/*
  ModbusRTU ESP8266/ESP32
  Simple slave example

  (c)2019 Alexander Emelianov (a.m.emelianov@gmail.com)
  https://github.com/emelianov/modbus-esp8266

  modified 13 May 2020
  by brainelectronics

  This code is licensed under the BSD New License. See LICENSE.txt for more info.
*/

#include <ModbusRTU.h>
#include <HardwareSerial.h>

#define DE_RE_PIN 40
#define RX_PIN 38
#define TX_PIN 39
#define BUTTON_S1_PIN 45
#define BUTTON_S2_PIN 48
#define REGN 10
#define SLAVE_ID 1

HardwareSerial rs485Serial(1);

ModbusRTU mb;

void setup() {
  Serial.begin(9600, SERIAL_8N1);

  pinMode(BUTTON_S1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_S2_PIN, INPUT_PULLUP);

  // Настройка RS485
  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, LOW);

  // Инициализация последовательного порта
  rs485Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  mb.begin(&rs485Serial, DE_RE_PIN);

  mb.slave(SLAVE_ID);
  mb.addHreg(REGN);
  mb.Hreg(REGN, 100);
}

void loop() 
{


  mb.task();


  yield();
}