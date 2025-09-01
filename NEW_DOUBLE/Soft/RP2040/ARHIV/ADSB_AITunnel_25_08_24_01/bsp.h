#pragma once

#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "stdint.h"

class BSP 
{
   public:
    static const uint16_t kMaxNumDemodStateMachines = 4;

    PIO preamble_detector_pio = pio0;
    uint preamble_detector_demod_pin_irq = IO_IRQ_BANK0;
    PIO message_demodulator_pio = pio1;
    uint preamble_detector_demod_complete_irq = PIO0_IRQ_0;

    uint16_t r1090_num_demod_state_machines = 3;  //
    uint16_t r1090_high_power_demod_state_machine_index = 2;
    uint16_t r1090_pulses_pins[kMaxNumDemodStateMachines] = {19, 22, 18 };
    uint16_t r1090_demod_pins[kMaxNumDemodStateMachines] = {20, 23, 17 };
    uint16_t r1090_recovered_clk_pins[kMaxNumDemodStateMachines] = {21, 24, 26 }; 
 };

extern BSP bsp;