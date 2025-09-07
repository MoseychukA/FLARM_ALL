#pragma once
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "config.h"

// Простейшая программа: IN PINS, JMP ., автосдвиг на 32 бита
static inline uint16_t enc_in_pins_1() { return (0b010 << 13) | (1<<0); }
static inline uint16_t enc_jmp(uint8_t addr){ return (0b000 << 13) | (addr & 0x1F); }

static const uint16_t adsb_sampler_program_insts[] = {
  // 0:
  (uint16_t)((PIO_IN_INSTRUCTION_BITS) + 1), // заменим ниже правильной сборкой
  // 1:
  0
};
// Поскольку Arduino-pico не предоставляет макрос PIO assembler здесь — создадим вручную:
#undef PIO_IN_INSTRUCTION_BITS
#define PIO_IN_INSTRUCTION_BITS ((0b010 << 13) | (0/*source ignored here — fixed by sm_config_set_in_pins*/))
static const uint16_t adsb_sampler_program_fixed[] = {
  (uint16_t)((0b010 << 13) | 1), // in pins,1
  (uint16_t)((0b000 << 13) | 0), // jmp 0
};
static const struct pio_program adsb_sampler_program = {
  .instructions = adsb_sampler_program_fixed,
  .length = 2,
  .origin = -1,
};

static inline void adsb_sampler_program_init(PIO pio, uint sm, uint offset, uint pin) {
  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_in_pins(&c, pin);
  sm_config_set_in_shift(&c, true, true, 32); // autopush=32
  float div = (float)clock_get_hz(clk_sys) / (SAMPLE_RATE_HZ*2.0f); // 2 инструкции/цикл
  sm_config_set_clkdiv(&c, div);
  pio_sm_init(pio, sm, offset + 0, &c);
  pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
  pio_sm_set_enabled(pio, sm, false);
}
