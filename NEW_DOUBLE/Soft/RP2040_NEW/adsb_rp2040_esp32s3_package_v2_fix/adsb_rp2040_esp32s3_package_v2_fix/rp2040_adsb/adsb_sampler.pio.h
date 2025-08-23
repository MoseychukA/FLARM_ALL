#pragma once
#include <hardware/pio.h>

static const uint16_t adsb_sampler_program_instructions[] = {
    0x4001 // IN PINS, 1
};

static const pio_program_t adsb_sampler_program = {
    .instructions = adsb_sampler_program_instructions,
    .length = 1,
    .origin = -1
};

static inline void adsb_sampler_program_init(PIO pio, uint sm, int offset, uint pin, uint32_t sample_freq_hz){
    gpio_init(pin); gpio_set_dir(pin, GPIO_IN); gpio_pull_down(pin);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_in_pins(&c, pin);
    sm_config_set_in_shift(&c, true, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    float div=(float)clock_get_hz(clk_sys)/(float)sample_freq_hz;
    sm_config_set_clkdiv(&c, div);
    sm_config_set_wrap(&c, offset, offset + 1 - 1); // offset..offset
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}
