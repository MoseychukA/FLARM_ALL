/*
  Модуль Baro.h
  Назначение:
  - Публичный интерфейс работы с барометрическим датчиком.

  Что описано в этом файле:
  - Функции запуска, обслуживания и остановки бародатчика.
  - Функции чтения текущих измерений и признака доступности датчика.
  - Точки доступа для остальных модулей, которым нужна барометрическая высота.
*/

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
