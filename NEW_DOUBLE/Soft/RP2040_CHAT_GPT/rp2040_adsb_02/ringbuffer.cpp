#include "ringbuffer.h"
#include <stdlib.h>

bool ring_init(DMARing &rb, uint32_t words) {
  // Требуем степень двойки
  if ((words & (words-1)) != 0) return false;
  rb.base = (volatile uint32_t*)malloc(words * sizeof(uint32_t));
  if (!rb.base) return false;
  rb.words = words;
  rb.widx = rb.ridx = 0;
  return true;
}
