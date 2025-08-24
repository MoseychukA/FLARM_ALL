// adsb_sampler.pio.h - pre-generated minimal sampler
// Generated offline, included to avoid pioasm at build time
#pragma once
#include "hardware/pio.h"

// Program binary
static const uint16_t adsb_sampler_program_instructions[] = {
    0x4001, // in pins, 1
};

static const struct pio_program adsb_sampler_program = {
    .instructions = adsb_sampler_program_instructions,
    .length = 1,
    .origin = -1,
};

// Init: set pin as input, configure clock divider for target sample rate, autopush threshold
static inline void adsb_sampler_program_init(PIO pio, uint sm, uint offset, uint pin, uint32_t sample_hz) {
    // Configure pin
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);

    // SM config
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_in_pins(&c, pin);
    sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / (float)sample_hz);
    sm_config_set_in_shift(&c, true, true, 32); // shift right, autopush, threshold 32

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}
