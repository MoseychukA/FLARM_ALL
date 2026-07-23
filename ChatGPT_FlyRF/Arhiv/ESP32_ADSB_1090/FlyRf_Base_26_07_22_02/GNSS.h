/*
  Модуль GNSS.h
  Назначение:
  - Публичный интерфейс навигационного модуля GNSS.

  Что содержит файл:
  - Назначение пинов и скорости порта GNSS.
  - Функции setup/loop/fini.
  - Функции чтения координат, времени, высоты, числа спутников и статусов фикса.
*/

#pragma once
#include <Arduino.h>

#ifndef PIN_GNSS_RX
#define PIN_GNSS_RX 15
#endif

#ifndef PIN_GNSS_TX
#define PIN_GNSS_TX 16
#endif

#ifndef GNSS_SERIAL_BAUD
#define GNSS_SERIAL_BAUD 9600UL
#endif

void GNSS_setup();
void GNSS_loop();
void GNSS_fini();

bool GNSS_coordinatesValid();
bool GNSS_timeValid();
uint8_t GNSS_timeCentisecond();
bool GNSS_hasFix();
bool GNSS_altitudeValid();
bool GNSS_satellitesValid();
uint32_t GNSS_lastFixMs();
float GNSS_latitude();
float GNSS_longitude();
float GNSS_altitudeMeters();
uint8_t GNSS_satellites();

bool GNSS_waitingForInitialFix();
bool GNSS_waitingForRecovery();
bool GNSS_noDataTimeout();
void GNSS_applyCurrentStateToThisAircraft();
