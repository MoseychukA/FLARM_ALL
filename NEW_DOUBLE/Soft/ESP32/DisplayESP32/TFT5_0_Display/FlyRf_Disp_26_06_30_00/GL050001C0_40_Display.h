#pragma once

#include <Arduino.h>

bool GL050001C0_40_setup();
void GL050001C0_40_showStartup(const String &version);
void GL050001C0_40_showStatus(uint32_t rxPackets, uint32_t txPackets);
void GL050001C0_40_showPowerOff();
void GL050001C0_40_loop();
