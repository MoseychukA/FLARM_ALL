// Auto-generated minimal header for adsb_sampler.pio
#pragma once
#include <hardware/pio.h>

static const uint16_t adsb_sampler_program_instructions[] = {
    0xa0bf, // set x, 31
    0x4001, // in  pins, 1
    0x0041, // jmp x--, 1
    0x8020, // push block
};

static const struct pio_program adsb_sampler_program = {
    .instructions = adsb_sampler_program_instructions,
    .length = 4,
    .origin = -1,
};

static inline pio_sm_config adsb_sampler_program_get_default_config(uint offset) {
  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_in_pins(&c, 0);                 // перенастраивается позже
  sm_config_set_in_shift(&c, true, true, 32);   // shift right, autopush=1, threshold 32
  sm_config_set_clkdiv(&c, 1.0f);               // задаётся позже точно
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX); // не используем TX, но ок
  return c;
}
