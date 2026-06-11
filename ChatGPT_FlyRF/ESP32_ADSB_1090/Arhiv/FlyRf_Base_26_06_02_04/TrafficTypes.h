/*
  Модуль TrafficTypes.h
  Назначение:
  - Базовые типы данных для промежуточного представления сторонней воздушной цели.

  Что содержит файл:
  - Перечень источников трафика.
  - Структуру TrafficCandidate, которая используется декодерами до записи данных в Container.
*/

#pragma once

#include <Arduino.h>

enum TrafficSource : uint8_t
{
    TRAFFIC_SOURCE_UNKNOWN = 0,
    TRAFFIC_SOURCE_FLARM_LORA = 1,
    TRAFFIC_SOURCE_ADSB_DUMP1090 = 2
};

struct TrafficCandidate
{
    uint32_t address;
    float lat;
    float lon;
    int altitude;
    float speed;
    float course;
    uint32_t timestampMs;
    int rssi;  // Параметр радиоканала или протокола: описывает частоту, мощность, профиль, режим передачи или текущее состояние RF.
    float snr;  // Параметр радиоканала или протокола: описывает частоту, мощность, профиль, режим передачи или текущее состояние RF.
    TrafficSource source;
    bool valid;
};
