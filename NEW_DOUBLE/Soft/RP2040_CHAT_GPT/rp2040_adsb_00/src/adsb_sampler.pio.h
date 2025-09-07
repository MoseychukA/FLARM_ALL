// AUTO-GENERATED (simplified) from adsb_sampler.pio
#pragma once
#include "hardware/pio.h"

static inline pio_sm_config adsb_sampler_program_get_default_config(uint offset) {
  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_in_pins(&c, 0);
  sm_config_set_in_shift(&c, true, true, 32);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
  sm_config_set_clkdiv_int_frac(&c, 1, 0);
  sm_config_set_wrap(&c, offset, offset+6);
  return c;
}

static inline int adsb_sampler_program_init(PIO pio, uint pin) {
  static const uint16_t program_insts[] = {
    0xa0a7, // pull block
    0xb03f, // mov y, ~null
    0xe03f, // set x,31
    0x4001, // in pins,1
    0x0045, // jmp x--, +5
    0x8080, // push block
    0x0081, // jmp y--, start
  };
  uint offset = pio_add_program_at_offset(pio, (const pio_program){program_insts, 7}, pio_get_free_sm(pio));
  int sm = pio_claim_unused_sm(pio, true);
  pio_sm_config c = adsb_sampler_program_get_default_config(offset);
  sm_config_set_in_pins(&c, pin);
  pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
  pio_sm_init(pio, sm, offset, &c);
  // Clock divider to SAMPLE_HZ*32 will be set by caller
  return sm;
}