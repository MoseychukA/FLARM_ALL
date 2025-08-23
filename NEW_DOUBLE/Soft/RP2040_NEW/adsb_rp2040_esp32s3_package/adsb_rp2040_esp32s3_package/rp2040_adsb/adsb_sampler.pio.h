// adsb_sampler.pio.h - precompiled header for Arduino build without pioasm
#pragma once
#include <hardware/pio.h>

// Compiled program (manually filled minimal structure)
typedef struct {
    uint16_t instructions[2];
    uint8_t length;
    int8_t origin;
} pio_program_t;

static const pio_program_t adsb_sampler_program = {
    // This is a placeholder. We'll actually load assembly at runtime using the standard API in Arduino core.
    // However, Arduino RP2040 core requires a pio_program struct; we provide minimal stub.
    .instructions = {0x4001, 0x80a0},
    .length = 2,
    .origin = -1,
};

static inline void adsb_sampler_program_init(PIO pio, uint sm, int offset, uint pin, uint32_t sample_freq_hz) {
    // Configure pin as input
    gpio_init(pin); gpio_set_dir(pin, GPIO_IN); gpio_set_pulls(pin, true, false);

    // Build state machine config
    pio_sm_config c = pio_get_default_sm_config();
    // Map IN to the specified pin
    sm_config_set_in_pins(&c, pin);
    // Shift right, autopush every 32 bits
    sm_config_set_in_shift(&c, true, true, 32);
    // Join RX FIFO to increase depth
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    // Calculate clock divider: SM executes one instruction per cycle; we need one sample per cycle
    float div = (float)clock_get_hz(clk_sys) / (float)sample_freq_hz;
    sm_config_set_clkdiv(&c, div);

    // Set wrap to two-instruction loop
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}
