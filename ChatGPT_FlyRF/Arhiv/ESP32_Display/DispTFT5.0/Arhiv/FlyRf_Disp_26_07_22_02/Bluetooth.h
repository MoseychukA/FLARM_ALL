#pragma once

#include <Arduino.h>

void Bluetooth_setup();
void Bluetooth_loop();
void Bluetooth_fini();
bool Bluetooth_connected();
bool Bluetooth_supported();
String Bluetooth_name();
