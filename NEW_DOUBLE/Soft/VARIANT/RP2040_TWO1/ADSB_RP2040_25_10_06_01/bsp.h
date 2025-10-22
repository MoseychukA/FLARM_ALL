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

    BSP(bool has_eeprom_in) : has_eeprom(has_eeprom_in) 
	{

        // Overrides for non-EEPROM version of ADSBee: ADSBee 1090U, ADSBee m1090.

        gnss_pps_pin = 2;
        gnss_enable_pin = 3;

        r1090_num_demod_state_machines = 3;
        for (uint16_t i = 0; i < r1090_num_demod_state_machines; i++) 
		{
            r1090_pulses_pins[i] = 19;
            r1090_demod_pins[i] = 20 + i;
            // Set RECOVERED_CLK to fake pin for high power preamble detector. Will be overridden by
            // higher priority (lower index) SM.
            r1090_recovered_clk_pins[i] = UINT16_MAX;  // Set to UINT16_MAX to indicate not connected.
        }
        r1090_recovered_clk_pins[0] = 24;  // Connect SM0 to recovered_clk output for debugging.
        r1090_tl_pwm_pin = 26;
        r1090_tl_adc_pin = 27;
        r1090_tl_adc_input = 1;
        r1090_rssi_adc_pin = 28;
        r1090_rssi_adc_input = 2;

    }

    bool has_eeprom = false;

    uint16_t gnss_uart_tx_pin = 0;
    uint16_t gnss_uart_rx_pin = 1;
    uint16_t gnss_pps_pin = 26;
    uint16_t gnss_enable_pin = UINT16_MAX;  // Set to UINT16_MAX to indicate not connected.

    uint16_t comms_uart_tx_pin = 4;
    uint16_t comms_uart_rx_pin = 5;

    PIO preamble_detector_pio = pio0;
    uint preamble_detector_demod_pin_irq = IO_IRQ_BANK0;
    PIO message_demodulator_pio = pio1;
    uint preamble_detector_demod_complete_irq = PIO0_IRQ_0;

    uint16_t r1090_led_pin = 15;
    uint16_t r1090_num_demod_state_machines = 3;
    uint16_t r1090_high_power_demod_state_machine_index = 2;
    uint16_t r1090_pulses_pins[kMaxNumDemodStateMachines] = {19, 22, 19};
    uint16_t r1090_demod_pins[kMaxNumDemodStateMachines] = {20, 23, 29};
    uint16_t r1090_recovered_clk_pins[kMaxNumDemodStateMachines] = {21, 24, 26};  // These pin values are only for old hardware.
    uint16_t r1090_tl_pwm_pin = 25;                                       // Pin for Trigger Level PWM output.
    uint16_t r1090_tl_adc_pin = 27;                                       // Pin for reading filtered Trigger Level.
    uint16_t r1090_tl_adc_input = 1;    // ADC input for reading filtered Trigger Level.
    uint16_t r1090_rssi_adc_pin = 28;   // Pin for reading RSSI.
    uint16_t r1090_rssi_adc_input = 2;  // ADC input for reading RSSI.
    uint16_t r1090_bias_tee_enable_pin = 18;

};

extern BSP bsp;