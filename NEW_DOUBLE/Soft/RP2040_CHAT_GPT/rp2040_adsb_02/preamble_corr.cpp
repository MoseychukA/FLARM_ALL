#include "preamble_corr.h"
#include <string.h>

static inline int sat(int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); }

void corr_profile_from_runtime(const RuntimeConfig& rc, CorrProfile& p) {
  p.base_thr = rc.base_thr;
  p.k_neg    = rc.k_neg;
  p.min_pos_hits = PRE_MIN_POSHITS;
  p.dead_time_samples = DEAD_TIME_SAMPLES;
  p.long_pulse_max_us = PRE_MASK_LONG_US;
}

static const uint8_t preamble_template[16] = {
  // Эталонные окна (условная, 0.5us дискретизация): 1,0,1,0,1,0,0,0 (и т.д.)
  // Мы коррелируем по окнам с шагом 0.5мкс — здесь ядро веса по подокнам.
  1,0,1,0,1,0,0,0,  1,0,1,0,1,0,0,0
};

PreambleHit corr_preamble_and_get_t0(const uint8_t *samples, uint32_t samples_len,
                                     uint32_t start_index, CorrState &st,
                                     const CorrProfile &prof) {
  PreambleHit hit{false,0,0.f};
  if (start_index + PRE_WIN_SAMPLES >= samples_len) return hit;

  // dead-time
  if (start_index - st.last_t0 < (uint32_t)prof.dead_time_samples) {
    return hit;
  }

  // весовая корреляция + маска длинных импульсов
  int posHits=0, negHits=0;
  float score=0.f, penalty=0.f;
  uint32_t longPulseMaxSamples = (uint32_t)(prof.long_pulse_max_us / SAMPLE_PERIOD_US);

  // Проверка "длинных" импульсов в пределах окна
  uint32_t run=0;
  for (uint32_t i=0;i<PRE_WIN_SAMPLES;i++) {
    uint8_t v = samples[start_index + i] & 1u;
    if (v) { run++; if (run>longPulseMaxSamples){ hit.ok=false; return hit; } }
    else run=0;
  }

  // Корреляция по окнам (грубо 16 подокон по 5 отсчётов = 80)
  for (int w=0; w<16; ++w) {
    int cnt=0;
    for (int j=0;j<5;j++) {
      cnt += (samples[start_index + w*5 + j] & 1u);
    }
    if (cnt >= 3) { // "единичное" окно
      posHits++;
      if (preamble_template[w]) score += 1.0f; else penalty += 1.0f;
    } else {
      negHits++;
      if (!preamble_template[w]) score += 1.0f; else penalty += 1.0f;
    }
  }

  float dynamic_thr = prof.base_thr + prof.k_neg * st.ema_neg;
  float finalScore = score - 0.5f*penalty;

  if (posHits < prof.min_pos_hits) {
    // обновляем ЭМА шума
    st.ema_neg = 0.95f*st.ema_neg + 0.05f*negHits;
    return hit;
  }

  if (finalScore >= dynamic_thr) {
    hit.ok = true;
    hit.t0_index = start_index; // упростим — начало окна ~ начало преамбулы
    hit.score = finalScore;
    st.last_t0 = start_index;
    // лёгкая разрядка ЭМА
    st.ema_neg = 0.9f*st.ema_neg;
    return hit;
  } else {
    st.ema_neg = 0.95f*st.ema_neg + 0.05f*negHits;
  }
  return hit;
}

void digital_filter_03us(uint8_t *s, uint32_t n, bool enabled) {
  if (!enabled || n<3) return;
  // Уберём слишком короткие импульсы (<0.3us) и нормализуем к 0.5us:
  // простая морфология: удаляем одиночные "1" и одиночные "0"
  for (uint32_t i=1;i<n-1;i++) {
    if (s[i]==1 && s[i-1]==0 && s[i+1]==0) s[i]=0; // короткая вспышка
    if (s[i]==0 && s[i-1]==1 && s[i+1]==1) s[i]=1; // короткая провал
  }
}
