#include "adsb_pio_dma.h"
#include "adsb_sampler.pio.h"
#include "config.h"
#include <hardware/pio.h>
#include <hardware/dma.h>
#include <hardware/irq.h>
#include <string.h>

static PIO  g_pio = pio0;
static int  g_sm [3] = {-1,-1,-1};
static uint g_ofs = 0;
static int  g_dma_ch[3] = {-1,-1,-1};

static uint32_t g_buf1[RING_WORDS];
static uint32_t g_buf2[RING_WORDS];
static uint32_t g_buf3[RING_WORDS];

struct RingDesc {
  volatile uint32_t *buf;
  volatile size_t head_seg;
  volatile size_t tail_seg;
};
static RingDesc g_ring[3];
static volatile size_t g_dma_seg_idx[3] = {0,0,0};

static inline void dma_start_segment(int ch) {
  RingDesc &r = g_ring[ch];
  size_t seg = g_dma_seg_idx[ch] % RING_SEGMENTS;
  volatile uint32_t *dst = r.buf + seg*SEGMENT_WORDS;
  dma_channel_set_read_addr (g_dma_ch[ch], &g_pio->rxf[g_sm[ch]], false);
  dma_channel_set_write_addr(g_dma_ch[ch], (void*)dst, false);
  dma_channel_set_trans_count(g_dma_ch[ch], SEGMENT_WORDS, true);
}

static void __isr dma_irq0() {
  for (int ch=0; ch<3; ++ch) {
    int d = g_dma_ch[ch]; if (d<0) continue;
    if (dma_hw->ints0 & (1u<<d)) {
      dma_hw->ints0 = (1u<<d);
      RingDesc &r = g_ring[ch];
      r.head_seg = (r.head_seg + 1) % RING_SEGMENTS;
      g_dma_seg_idx[ch] = (g_dma_seg_idx[ch] + 1) % RING_SEGMENTS;
      dma_start_segment(ch);
    }
  }
}

void pio_dma_init() {
  // Буферы
  g_ring[0] = { g_buf1, 0, 0 };
  g_ring[1] = { g_buf2, 0, 0 };
  g_ring[2] = { g_buf3, 0, 0 };

  // PIO program
  g_ofs = pio_add_program(g_pio, &adsb_sampler_program);
  for (int i=0;i<3;i++) g_sm[i] = pio_claim_unused_sm(g_pio, true);

  const uint pins[3] = { PIO_PIN_CH1, PIO_PIN_CH2, PIO_PIN_CH3 };
  for (int i=0;i<3;i++) adsb_sampler_program_init(g_pio, g_sm[i], g_ofs, pins[i]);

  // DMA
  for (int i=0;i<3;i++) {
    g_dma_ch[i] = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(g_dma_ch[i]);
    channel_config_set_read_increment (&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, pio_get_dreq(g_pio, g_sm[i], false));
    dma_channel_configure(
      g_dma_ch[i], &c,
      (void*)(g_ring[i].buf + 0),      // dst
      &g_pio->rxf[g_sm[i]],            // src
      SEGMENT_WORDS, false
    );
  }

  irq_set_exclusive_handler(DMA_IRQ_0, dma_irq0);
  irq_set_enabled(DMA_IRQ_0, true);
  for (int i=0;i<3;i++) {
    dma_hw->ints0 = (1u<<g_dma_ch[i]);
    dma_channel_set_irq0_enabled(g_dma_ch[i], true);
  }
}

void pio_dma_start() {
  for (int i=0;i<3;i++) { pio_sm_set_enabled(g_pio, g_sm[i], true); }
  for (int i=0;i<3;i++) {
    g_ring[i].head_seg = g_ring[i].tail_seg = 0;
    g_dma_seg_idx[i]=0;
    dma_start_segment(i);
  }
}
void pio_dma_stop() {
  for (int i=0;i<3;i++) {
    if (g_dma_ch[i]>=0) dma_channel_abort(g_dma_ch[i]);
    if (g_sm[i]>=0)     pio_sm_set_enabled(g_pio, g_sm[i], false);
  }
}

size_t pio_dma_read_ch(int ch, uint32_t *dst_words, size_t max_words) {
  RingDesc &r = g_ring[ch];
  size_t avail = (r.head_seg + RING_SEGMENTS - r.tail_seg) % RING_SEGMENTS;
  if (!avail) return 0;
  size_t copied=0;
  while (avail && copied + SEGMENT_WORDS <= max_words) {
    size_t seg = r.tail_seg;
    memcpy(dst_words+copied, (const void*)(r.buf + seg*SEGMENT_WORDS), SEGMENT_WORDS*sizeof(uint32_t));
    copied += SEGMENT_WORDS;
    r.tail_seg = (r.tail_seg+1) % RING_SEGMENTS;
    avail--;
  }
  return copied;
}

