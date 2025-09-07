#pragma once
#include <stdint.h>
#include <stddef.h>
#include "adsb_types.h"

// Параметры коррелятора (переключаются профилями/CLI)
struct CorrConfig {
  int pre_thresh;        // минимальный score преамбулы
  int neg_weight;        // штраф «неожиданных» импульсов
  int pos_min_hits;      // минимум попаданий в окна
  int dead_time_samples; // запрет повторной детекции
};

void correlator_init();
void correlator_set_cfg(const CorrConfig &cfg);
CorrConfig correlator_get_cfg();

size_t unpack_samples(const uint32_t *words, size_t nwords, uint8_t *out_bits, size_t max_bits);
bool   correlator_detect_preamble(const uint8_t* s, size_t n, size_t &t0_out, int &score_out);
int    assemble_bits_after_preamble(const uint8_t *s, size_t n, size_t t0, uint8_t *out_bytes, int want_bits);
