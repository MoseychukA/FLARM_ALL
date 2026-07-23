#pragma once
#include <Arduino.h>
#define Log_println(x) Serial.println(x)
#define Log_printf(...) Serial.printf(__VA_ARGS__)
