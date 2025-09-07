#pragma once
#include <Arduino.h>
#include "config.h"

// Вход: окно сырых уровней 0/1 (после порога) — по каналу.
// Весовая корреляция по 80 отсчётам, с маской подавления ложных срабатываний,
// dead-time и динамическим порогом.
struct CorrState {
  float ema_neg = 0.f;   // ЭМА шума
  uint32_t last_t0 = 0;  // позиция последнего t0 (индексы сэмплов)
};

struct CorrProfile {
  float base_thr;
  float k_neg;
  int   min_pos_hits;
  int   dead_time_samples;
  float long_pulse_max_us; // >0.7 us => reject
};

void corr_profile_from_runtime(const RuntimeConfig&, CorrProfile&);

PreambleHit corr_preamble_and_get_t0(const uint8_t *samples, uint32_t samples_len,
                                     uint32_t start_index, CorrState &st,
                                     const CorrProfile &prof);

// Быстрый порог/фильтр по длительности импульсов (0.3–0.7 мкс) + нормализация.
void digital_filter_03us(uint8_t *samples, uint32_t n, bool enabled);
