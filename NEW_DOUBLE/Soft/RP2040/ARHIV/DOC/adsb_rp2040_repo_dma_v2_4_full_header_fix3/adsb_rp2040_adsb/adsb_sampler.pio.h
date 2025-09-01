
#ifndef ADSB_SAMPLER_PIO_H
#define ADSB_SAMPLER_PIO_H
#include "hardware/pio.h"

#define adsb_sampler_wrap_target 0
#define adsb_sampler_wrap 1

static const uint16_t adsb_sampler_program_instructions[] = {
    (uint16_t)pio_encode_in(pio_pins, 1),
    (uint16_t)pio_encode_jmp(0)
};

static const struct pio_program adsb_sampler_program = {
    .instructions = adsb_sampler_program_instructions,
    .length = 2,
    .origin = -1,
};

static inline pio_sm_config adsb_sampler_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + adsb_sampler_wrap_target, offset + adsb_sampler_wrap);
    sm_config_set_in_shift(&c, true, true, 32);
    return c;
}

#endif // ADSB_SAMPLER_PIO_H
