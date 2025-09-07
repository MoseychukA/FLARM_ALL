#pragma once
#include <stdint.h>
#include <stddef.h>

void   pio_dma_init();
void   pio_dma_start();
void   pio_dma_stop();
size_t pio_dma_read_ch(int ch, uint32_t *dst_words, size_t max_words);