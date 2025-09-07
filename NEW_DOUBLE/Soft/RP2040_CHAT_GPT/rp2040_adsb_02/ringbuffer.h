#pragma once
#include <Arduino.h>

// Кольцевой буфер 32-бит слов (для DMA)
struct DMARing {
  volatile uint32_t *base;  // malloc'ed
  uint32_t words;
  volatile uint32_t widx;
  volatile uint32_t ridx;
};

bool ring_init(DMARing &rb, uint32_t words);
inline uint32_t ring_avail(const DMARing &rb) { return (rb.widx - rb.ridx) & (rb.words-1); }
inline bool ring_pop(DMARing &rb, uint32_t &word) {
  if (rb.ridx == rb.widx) return false;
  word = rb.base[rb.ridx];
  rb.ridx = (rb.ridx + 1) & (rb.words-1);
  return true;
}
inline volatile uint32_t* ring_write_ptr(DMARing &rb) { return &rb.base[rb.widx]; }
inline void ring_commit(DMARing &rb) { rb.widx = (rb.widx + 1) & (rb.words-1); }
