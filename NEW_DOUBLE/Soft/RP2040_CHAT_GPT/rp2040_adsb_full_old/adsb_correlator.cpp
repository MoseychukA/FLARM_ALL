#include "adsb_correlator.h"
#include "config.h"
#include <string.h>

static CorrConfig g_cfg{ 14, 2, 10, PRE_SAMPLES + MAX_BITS*BIT_SAMPLES };

void correlator_init() {}
void correlator_set_cfg(const CorrConfig &cfg){ g_cfg=cfg; }
CorrConfig correlator_get_cfg(){ return g_cfg; }

size_t unpack_samples(const uint32_t *words, size_t nwords, uint8_t *out_bits, size_t max_bits) {
  size_t produced=0;
  for (size_t i=0;i<nwords;i++){
    uint32_t w=words[i];
    for (int b=31;b>=0;b--){
      if (produced>=max_bits) return produced;
      out_bits[produced++] = (w>>b)&1u;
    }
  }
  return produced;
}

// Окна преамбулы (80 отсчётов, 0.5 мкс шаг): импульсы у ~0, 0.5, 1.5, 2.0 мкс
static uint8_t pos_mask[PRE_SAMPLES];
static bool mask_init=false;
static void init_mask(){
  if(mask_init) return; mask_init=true;
  memset(pos_mask,0,sizeof(pos_mask));
  auto mark=[&](int c){ for(int k=-2;k<=+2;k++){int i=c+k; if(i>=0 && i<PRE_SAMPLES) pos_mask[i]=1;} };
  mark(0);   // ~0 мкс
  mark(1);   // ~0.5 мкс
  mark(3);   // ~1.5 мкс
  mark(4);   // ~2.0 мкс
}

bool correlator_detect_preamble(const uint8_t* s, size_t n, size_t &t0_out, int &score_out) {
  init_mask();
  if (n < PRE_SAMPLES) return false;
  int best=-9999; size_t best_t=0;

  for (size_t t=0; t+PRE_SAMPLES<=n; ++t) {
    int posHits=0, negHits=0, longPulse=0, run=0;
    for (int i=0;i<PRE_SAMPLES;i++) {
      uint8_t v = s[t+i];
      run = v ? run+1 : 0;
      if (pos_mask[i]) { if (v) posHits++; } else { if (v) negHits++; }
      if (run>3) longPulse=1; // >0.7 мкс подряд вне ожидаемых окн — штраф
    }
    int score = posHits - g_cfg.neg_weight*negHits - (longPulse?5:0);
    if (posHits >= g_cfg.pos_min_hits && score > best) { best=score; best_t=t; }
  }
  if (best >= g_cfg.pre_thresh) { t0_out=best_t; score_out=best; return true; }
  return false;
}

int assemble_bits_after_preamble(const uint8_t *s, size_t n, size_t t0, uint8_t *out_bytes, int want_bits) {
  size_t start = t0 + PRE_SAMPLES;
  size_t need  = (size_t)want_bits * BIT_SAMPLES;
  if (start + need > n) return 0;
  memset(out_bytes,0,(want_bits+7)/8);
  for (int i=0;i<want_bits;i++){
    size_t idx=start + i*BIT_SAMPLES;
    uint8_t a = s[idx+0];
    uint8_t b = s[idx+1];
    int bit = (a>0 && b==0) ? 1 : 0;  // импульс в первой половине = 1
    out_bytes[i>>3] |= (bit << (7-(i&7)));
  }
  return want_bits;
}

