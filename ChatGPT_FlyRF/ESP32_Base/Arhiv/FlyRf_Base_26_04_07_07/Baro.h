#pragma once
#include <Arduino.h>

void Baro_setup();
void Baro_loop();
void Baro_fini();

bool Baro_available();
float Baro_temperatureC();
int32_t Baro_pressurePa();
float Baro_altitudeMeters();
uint32_t Baro_lastUpdateMs();
