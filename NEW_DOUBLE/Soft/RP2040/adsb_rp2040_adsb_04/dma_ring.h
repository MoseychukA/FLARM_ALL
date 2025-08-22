
#pragma once
#include <Arduino.h>
#include "hardware/pio.h"
#include "hardware/dma.h"
struct DmaRing { uint32_t *buf; size_t words; volatile uint32_t widx; uint32_t ridx; int dma_ch; PIO pio; uint sm; };
bool dma_ring_init(DmaRing &r, PIO pio, uint sm, size_t words);
void dma_ring_deinit(DmaRing &r);
size_t dma_ring_available(const DmaRing &r);
bool dma_ring_get_word(DmaRing &r, uint32_t &w);
