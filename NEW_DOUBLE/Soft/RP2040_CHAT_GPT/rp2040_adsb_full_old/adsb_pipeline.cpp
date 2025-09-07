#include "adsb_pipeline.h"
#include "adsb_pio_dma.h"
#include "adsb_correlator.h"
#include "adsb_crc.h"
#include "adsb_decoder.h"
#include "adsb_gillham.h"
#include "adsb_cpr.h"
#include "config.h"
#include <string.h>

struct ChanState {
  static const size_t SBUF = 32768; // ~16.384 мс @2 МГц
  uint8_t  s[SBUF];
  size_t   n = 0;
  int      dead = 0;
};
static ChanState g_cs[3];
static frame_cb_t g_cb = nullptr;

static void handle_words(int ch, const uint32_t *w, size_t nw) {
  ChanState &cs = g_cs[ch];
  static uint8_t tmp[SEGMENT_WORDS*32];
  size_t nb = unpack_samples(w, nw, tmp, sizeof(tmp));
  if (!nb) return;

  if (cs.n + nb > ChanState::SBUF) {
    size_t over = cs.n + nb - ChanState::SBUF;
    memmove(cs.s, cs.s + over, cs.n - over);
    cs.n -= over;
  }
  memcpy(cs.s + cs.n, tmp, nb);
  cs.n += nb;

  if (cs.dead > 0) {
    cs.dead -= nb;
    if (cs.dead < 0) cs.dead = 0;
    return;
  }

  size_t t0; int score;
  if (correlator_detect_preamble(cs.s, cs.n, t0, score)) {
    AdsbPacket pkt{}; pkt.rssi=score; pkt.timestamp_ms=millis(); pkt.ch=ch;
    // пробуем 112, затем 56
    const int tries[2]={112,56};
    bool ok=false; int used_bits=0;
    for (int k=0;k<2;k++){
      int want = tries[k];
      int got = assemble_bits_after_preamble(cs.s, cs.n, t0, pkt.raw, want);
      if (got==want && adsb_crc_check(pkt.raw, want)) { ok=true; used_bits=want; break; }
    }
    cs.dead = PRE_SAMPLES + MAX_BITS*BIT_SAMPLES;
    size_t cut = t0 + PRE_SAMPLES + MAX_BITS*BIT_SAMPLES;
    if (cut < cs.n) { size_t keep = cs.n - cut; memmove(cs.s, cs.s + cut, keep); cs.n = keep; }
    else cs.n = 0;

    if (ok) {
      pkt.bits = used_bits;
      DecodedADSB d = adsb_decode_packet(pkt);
      if (g_cb) g_cb(ch, d);
    }
  } else {
    if (cs.n > ChanState::SBUF/2) { size_t drop=cs.n-ChanState::SBUF/2; memmove(cs.s, cs.s+drop, ChanState::SBUF/2); cs.n=ChanState::SBUF/2; }
  }
}

void pipeline_init(frame_cb_t cb) {
  g_cb = cb;
  correlator_init();
  adsb_gillham_init();
  cpr_init();
  pio_dma_init();
  pio_dma_start();
}

void pipeline_poll_once() {
  uint32_t tmp[RING_WORDS];
  for (int ch=0; ch<3; ++ch) {
    size_t n = pio_dma_read_ch(ch, tmp, RING_WORDS);
    if (n) handle_words(ch, tmp, n);
  }
}

