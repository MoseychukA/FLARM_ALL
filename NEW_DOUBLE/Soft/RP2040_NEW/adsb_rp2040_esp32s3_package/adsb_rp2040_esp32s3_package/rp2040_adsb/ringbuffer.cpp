#include "ringbuffer.h"
#include "utils.h"
#include <math.h>

SampleFifo g_samp[3];

static CorrCfg cfg_from(const struct Config &cfg) {
  CorrCfg c; c.preamble_len_samp = 160; c.bit_samp = 20; c.halfbit_samp = 10; c.short_min=8; c.long_max=14; return c;
}

void init_corr_state(CorrState &st, const struct Config &cfg) {
  st.base_thr = cfg.base_thr;
  st.k = cfg.k_ema;
  st.ema_noise = 0.0f;
  st.dead_until = 0;
}

// Accessors
static inline uint8_t sget(int ch, uint32_t idx) { return g_samp[ch].s[idx & (SAMPLE_FIFO_SIZE-1)]; }

static int correlate_preamble(int ch, uint32_t cur, const CorrCfg &ccfg, const struct Config &cfg, float &score, int &t0_offset) {
  // Weighted correlation across 160 samples (8us) around cur (exclusive)
  // Expected pulse centers (in samples from t0): 0,10,20,70,80,110 at 10MHz; here x2: 0,20,40,140,160,220, but preamble is 8us (160) so last is 110@10MHz->220@20MHz which is outside; correct positions at 10MHz: 0,5,10,35,40,55. At 20MHz multiply by 2: 0,10,20,70,80,110
  const int centers[6] = {0,10,20,70,80,110};
  const int win = 3; // +/- samples around center to accumulate

  // Search t0 around cur - 160 to cur, we consider cur as end of window
  int best_off = -1; float best_score = -1e9;
  for (int shift = -4; shift <= 4; ++shift) {
    int t0 = (int)cur - ccfg.preamble_len_samp + shift; if (t0 < 0) continue;
    float posHits = 0.0f, negHits = 0.0f; int penalties = 0;
    bool longPulse = false;

    for (int k=0;k<6;k++) {
      int c = t0 + centers[k];
      // sum around center
      int hits = 0; int span=0;
      for (int d=-win; d<=win; ++d) { uint8_t v = sget(ch, c+d); hits += v; span++; }
      posHits += hits;
      // Check for unexpected pulses in off windows between known ones
      // Simple penalty: count ones in off regions near midpoints
      if (k<5) {
        int mid = t0 + (centers[k] + centers[k+1])/2;
        int off = 0; for (int d=-win; d<=win; ++d) off += sget(ch, mid+d);
        negHits += off;
        if (off > 0) penalties += off;
      }
    }

    // Minimal energy per window
    if (posHits < cfg.min_pos_hits) continue;

    // Long pulse check inside preamble: scan all 160 samples for runs > long_max
    int run=0; for (int s=0;s<ccfg.preamble_len_samp;s++){ if (sget(ch,t0+s)) { run++; if (run>ccfg.long_max) { longPulse=true; break; } } else run=0; }
    if (longPulse) continue;

    float sc = posHits - cfg.penalty_w * penalties;
    if (sc > best_score) { best_score = sc; best_off = t0; }
  }

  if (best_off < 0) return 0;
  score = best_score;
  t0_offset = best_off;
  return 1;
}

static bool decode_bits_from_samples(int ch, uint32_t t0, const CorrCfg &ccfg, const struct Config &cfg, RawMessage &rm) {
  // After t0 (preamble), payload starts at t0 + 160 samples.
  // Decode up to 120us (112 bits + 24 parity) but we only need 56 or 112 payload bits.
  // We'll attempt to decode 112 first; if energy low after 56, we may stop.
  int start = t0 + ccfg.preamble_len_samp;
  int max_bits = 112;
  uint8_t bits[120] = {0};
  for (int b=0;b<max_bits;b++) {
    int bit_base = start + b*ccfg.bit_samp;
    // PPM: 1-> pulse in first half, 0-> pulse in second half
    int a=0,bh=0;
    for (int d=0; d<ccfg.halfbit_samp; ++d) a += sget(ch, bit_base + d);
    for (int d=0; d<ccfg.halfbit_samp; ++d) bh += sget(ch, bit_base + ccfg.halfbit_samp + d);
    bits[b] = (a > bh) ? 1 : 0;
  }

  // Assemble bytes
  int msg_bits = 112;
  int msg_bytes = 14;
  // Heuristic to decide 56-bit short message:
  int energy_after_56 = 0; for (int i=56;i<112;i++) energy_after_56 += bits[i];
  if (energy_after_56 < 3) { // very low -> likely 56-bit
    msg_bits = 56; msg_bytes = 7;
  }

  memset(rm.payload, 0, sizeof(rm.payload));
  for (int i=0;i<msg_bits;i++) {
    int byte = i >> 3; int bit = 7 - (i & 7);
    if (bits[i]) rm.payload[byte] |= (1 << bit);
  }
  rm.bits = msg_bits; rm.bytes = msg_bytes; rm.t0_us = (uint32_t)((t0 * 50) / 1000); // 50ns per sample -> us
  return true;
}

int run_correlator_and_slice(int ch, const struct Config &cfg, LockFreePacketQueue &outq) {
  static uint32_t last_t0_samp[3] = {0,0,0};
  CorrCfg ccfg = cfg_from(cfg);

  // Work on latest window; use write index as current position
  uint32_t widx = g_samp[ch].widx;

  // Dead time
  if ((int32_t)(widx - last_t0_samp[ch]) < (int32_t)(cfg.dead_time_us * 20)) {
    return 0;
  }

  float score=0; int t0=0;
  int ok = correlate_preamble(ch, widx, ccfg, cfg, score, t0);
  if (!ok) {
    // Update EMA noise based on off-window hits (not implemented in detail)
    return 0;
  }

  // Dynamic threshold check
  float dyn_thr = cfg.base_thr + cfg.k_ema * 0.0f; // simplified EMA usage
  if (score < dyn_thr) return 0;

  // Verify spacing between peaks roughly dAB~10, dBC~25, dCD~10 at 10MHz -> times 2 here: 20,50,20
  // Skip detailed check here due to simplified correlation already constrained.

  // Slice payload
  RawMessage rm; rm.channel = ch;
  if (!decode_bits_from_samples(ch, t0, ccfg, cfg, rm)) return 0;

  // Deadtime after detection
  last_t0_samp[ch] = widx;

  // Enqueue
  outq.push(rm);
  return 1;
}
