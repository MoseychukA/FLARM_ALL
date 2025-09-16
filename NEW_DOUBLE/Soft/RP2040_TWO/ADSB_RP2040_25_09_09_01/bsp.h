#pragma once

#include "hardware/pio.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "stdint.h"

class BSP 
{
   public:
    static const uint16_t kMaxNumDemodStateMachines = 4;

    uint16_t comms_uart_tx_pin = 4;
    uint16_t comms_uart_rx_pin = 5;

    PIO preamble_detector_pio = pio0;
    uint preamble_detector_demod_pin_irq = IO_IRQ_BANK0;
    PIO message_demodulator_pio = pio1;
    uint preamble_detector_demod_complete_irq = PIO0_IRQ_0;

    uint16_t r1090_led_pin = 25;
    uint16_t r1090_num_demod_state_machines = 3;  //
    uint16_t r1090_high_power_demod_state_machine_index = 2;
    uint16_t r1090_pulses_pins[kMaxNumDemodStateMachines] = {19, 22, 19 };
    uint16_t r1090_demod_pins[kMaxNumDemodStateMachines] = {20, 23, 23 };
    uint16_t r1090_recovered_clk_pins[kMaxNumDemodStateMachines] = {21, 24, 26 }; // Установить RECOVERED_CLK на фальшивый вывод для детектора преамбулы высокой мощности. Будет переопределено
                                                                                 // higher priority (lower index) SM.
    uint16_t r1090_tl_pwm_pin = 9;             // Pin for Trigger Level PWM output.
    uint16_t r1090_tl_adc_pin = 27;            // Pin for reading filtered Trigger Level.
    uint16_t r1090_tl_adc_input = 1;           // ADC input for reading filtered Trigger Level.
    uint16_t r1090_rssi_adc_pin = 28;          // Pin for reading RSSI.
    uint16_t r1090_rssi_adc_input = 2;         // ADC input for reading RSSI.
 };

extern BSP bsp;