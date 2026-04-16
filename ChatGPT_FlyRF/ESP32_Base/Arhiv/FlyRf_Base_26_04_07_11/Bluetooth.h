#pragma once
#include <Arduino.h>

#define BLUETOOTH_OFF 0U
#define BLUETOOTH_LE  1U

void Bluetooth_setup();
void Bluetooth_loop();
void Bluetooth_fini();

size_t Bluetooth_write(const uint8_t* buffer, size_t size);
bool Bluetooth_connected();
bool Bluetooth_active();
bool Bluetooth_supported();
String Bluetooth_name();
