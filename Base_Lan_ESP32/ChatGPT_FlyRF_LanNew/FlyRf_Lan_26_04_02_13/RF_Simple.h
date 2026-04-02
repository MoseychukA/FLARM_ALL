#pragma once

#include <Arduino.h>
#include "ESP32RF.h"

// Упрощенный вариант LoRa-модуля на базе текущего проекта.
// В этой версии оставлены только: setup, channel, receive, decode, counters.

byte RF_Simple_setup(void);
void RF_Simple_loop(void);
void RF_Simple_parse(void);
void RF_Simple_getPacketCounters(uint32_t &tx, uint32_t &rx);
