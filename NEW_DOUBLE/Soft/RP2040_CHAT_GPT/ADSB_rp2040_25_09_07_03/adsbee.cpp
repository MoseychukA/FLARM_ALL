#include "adsbee.h"
#include "Arduino.h"
#include <hardware/structs/systick.h>
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/exception.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "pico/rand.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "stdio.h"  // for printing
#include "capture_pio.h"
#include "hardware/irq.h"
#include "pico/binary_info.h"
#include <string.h>  // for strcat
//#include "adsb_packet.h"
#include "adsb_decoder.h"

extern Raw1090Packet rx_packet_[3];
extern volatile bool packet_ready_[3];


#define MLAT_SYSTEM_CLOCK_RATIO 48 / 125
// Scales 125MHz system clock into a 48MHz counter.
static const uint32_t kMLATWrapCounterIncrement = (1 << 24) * MLAT_SYSTEM_CLOCK_RATIO;

constexpr float kPreambleDetectorFreq = 48e6;    // Running at 48MHz (24 clock cycles per half bit).
constexpr float kMessageDemodulatorFreq = 48e6;  // Run at 48 MHz to demodulate bits at 1Mbps.

constexpr float kInt16MaxRecip = 1.0f / INT16_MAX;

ADSBee *isr_access = nullptr;

/** Begin pass-through functions for public access **/
void on_systick_exception() { isr_access->OnSysTickWrap(); }

void on_demod_pin_change(uint gpio, uint32_t event_mask) {
    switch (event_mask) {
        case GPIO_IRQ_EDGE_RISE:
            isr_access->OnDemodBegin(gpio);
            break;
        case GPIO_IRQ_EDGE_FALL:
            break;
        case GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL:
            break;
    }
    gpio_acknowledge_irq(gpio, event_mask);
}

void on_demod_complete() { isr_access->OnDemodComplete(); }

/** End pass-through functions for public access **/

ADSBee::ADSBee(ADSBeeConfig config_in) {
    config_ = config_in;

    for (uint16_t sm_index = 0; sm_index < bsp.r1090_num_demod_state_machines; sm_index++) {
        preamble_detector_sm_[sm_index] = pio_claim_unused_sm(config_.preamble_detector_pio, true);
        message_demodulator_sm_[sm_index] = pio_claim_unused_sm(config_.message_demodulator_pio, true);
    }
    irq_wrapper_sm_ = pio_claim_unused_sm(config_.preamble_detector_pio, true);

    preamble_detector_offset_ = pio_add_program(config_.preamble_detector_pio, &preamble_detector_program);
    irq_wrapper_offset_ = pio_add_program(config_.preamble_detector_pio, &irq_wrapper_program);
    message_demodulator_offset_ = pio_add_program(config_.message_demodulator_pio, &message_demodulator_program);

    // Put IRQ parameters into the global scope for the on_demod_complete ISR.
    isr_access = this;
}

bool ADSBee::Init() {
 

    // Enable the MLAT timer using the 24-bit SysTick timer connected to the 125MHz processor clock.
    // SysTick Control and Status Register
    systick_hw->csr = 0b110;  // Source = Processor Clock, TickInt = Enabled, Counter = Disabled.
    // SysTick Reload Value Register
    systick_hw->rvr = 0xFFFFFF;  // Use the full 24 bit span of the timer value register.
    // 0xFFFFFF = 16777215 counts @ 125MHz = approx. 0.134 seconds.
    // Call the OnSysTickWrap function every time the SysTick timer hits 0.
    //!!exception_set_exclusive_handler(SYSTICK_EXCEPTION, on_systick_exception);
    // Let the games begin!
    systick_hw->csr |= 0b1;  // Enable the counter.

    /** PREAMBLE DETECTOR PIO **/
    // Calculate the PIO clock divider.
    float preamble_detector_div = (float)clock_get_hz(clk_sys) / kPreambleDetectorFreq;
    irq_wrapper_program_init(config_.preamble_detector_pio, bsp.r1090_num_demod_state_machines, irq_wrapper_offset_,
                             preamble_detector_div);
    for (uint16_t sm_index = 0; sm_index < bsp.r1090_num_demod_state_machines; sm_index++) 
    {
        // Only make the state machine wait to start if it's part of the round-robin group of well formed preamble
        // detectors.
        bool make_sm_wait = sm_index > 0 && sm_index < bsp.r1090_high_power_demod_state_machine_index;
        // Initialize the program using the .pio file helper function
        preamble_detector_program_init(config_.preamble_detector_pio,                     // Use PIO block 0.
                                       preamble_detector_sm_[sm_index],                   // State machines 0-2
                                       preamble_detector_offset_ /* + starting_offset*/,  // Program startin offset.
                                       config_.pulses_pins[sm_index],                     // Pulses pin (input).
                                       config_.demod_pins[sm_index],                      // Demod pin (output).
                                       preamble_detector_div,                             // Clock divisor (for 48MHz).
                                       make_sm_wait  // Whether state machine should wait for an IRQ to begin.
        );

        // Handle GPIO interrupts (for marking beginning of demod interval).
        gpio_set_irq_enabled_with_callback(config_.demod_pins[sm_index], GPIO_IRQ_EDGE_RISE /* | GPIO_IRQ_EDGE_FALL */,   true, on_demod_pin_change);

        // Set the preamble sequnence into the ISR: ISR: 0b101000010100000(0)
        // Last 0 removed from preamble sequence to allow the demodulator more time to start up.
        // mov isr null ; Clear ISR.
        pio_sm_exec(config_.preamble_detector_pio, preamble_detector_sm_[sm_index], pio_encode_mov(pio_isr, pio_null));
        // Fill start of preamble pattern with different bits if the state machine is intended to sense high power
        // preambles.
        if (sm_index == bsp.r1090_high_power_demod_state_machine_index) 
        {
            // High power preamble.
            // set x 0b111  ; ISR = 0b00000000000000000000000000000000
            pio_sm_exec(config_.preamble_detector_pio, preamble_detector_sm_[sm_index], pio_encode_set(pio_x, 0b111));
            // in x 3       ; ISR = 0b00000000000000000000000000000111
            pio_sm_exec(config_.preamble_detector_pio, preamble_detector_sm_[sm_index], pio_encode_in(pio_x, 3));
            // set x 0b101  ; ISR = 0b00000000000000000000000000000000
            pio_sm_exec(config_.preamble_detector_pio, preamble_detector_sm_[sm_index], pio_encode_set(pio_x, 0b101));
        } 
        else 
        {
            // Well formed preamble.
            // set x 0b101  ; ISR = 0b00000000000000000000000000000000
            pio_sm_exec(config_.preamble_detector_pio, preamble_detector_sm_[sm_index], pio_encode_set(pio_x, 0b101));
            // in x 3       ; ISR = 0b00000000000000000000000000000101
            pio_sm_exec(config_.preamble_detector_pio, preamble_detector_sm_[sm_index], pio_encode_in(pio_x, 3));
        }
        // in null 4    ; ISR = 0b00000000000000000000000001?10000
        pio_sm_exec(config_.preamble_detector_pio, preamble_detector_sm_[sm_index], pio_encode_in(pio_null, 4));
        // in x 3       ; ISR = 0b00000000000000000000001?10000101
        pio_sm_exec(config_.preamble_detector_pio, preamble_detector_sm_[sm_index], pio_encode_in(pio_x, 3));
        // in null 4    ; ISR = 0b0000000000000000001?100001010000
        // Note: this is shorter than the real tail but we need extra time for the demodulator to start up.
        pio_sm_exec(config_.preamble_detector_pio, preamble_detector_sm_[sm_index], pio_encode_in(pio_null, 4));
        // mov x null   ; Clear scratch x.
        pio_sm_exec(config_.preamble_detector_pio, preamble_detector_sm_[sm_index], pio_encode_mov(pio_x, pio_null));

        // Use this instruction to verify preamble was formed correctly (pushes ISR to RX FIFO).
        // pio_sm_exec(config_.preamble_detector_pio, preamble_detector_sm_[sm_index], pio_encode_push(false, true));
    }

    // Enable the DEMOD interrupt on PIO1_IRQ_0.
    pio_set_irq0_source_enabled(config_.preamble_detector_pio, pis_interrupt0, true);  // PIO0 state machine 0
    pio_set_irq0_source_enabled(config_.preamble_detector_pio, pis_interrupt1, true);  // PIO0 state machine 1
    pio_set_irq0_source_enabled(config_.preamble_detector_pio, pis_interrupt2, true);  // PIO0 state machine 2

    // Handle PIO0 IRQ0.
    irq_set_exclusive_handler(config_.preamble_detector_demod_complete_irq, on_demod_complete);
    irq_set_enabled(config_.preamble_detector_demod_complete_irq, true);

    /** MESSAGE DEMODULATOR PIO **/
    float message_demodulator_div = (float)clock_get_hz(clk_sys) / kMessageDemodulatorFreq;
    for (uint16_t sm_index = 0; sm_index < bsp.r1090_num_demod_state_machines; sm_index++) {
        message_demodulator_program_init(config_.message_demodulator_pio, message_demodulator_sm_[sm_index],
                                         message_demodulator_offset_, config_.pulses_pins[sm_index],
                                         config_.demod_pins[sm_index], config_.recovered_clk_pins[sm_index],
                                         message_demodulator_div);
    }

    // Set GPIO interrupts to be higher priority than the DEMOD interrupt to allow RSSI measurement.
    // irq_set_priority(config_.preamble_detector_demod_complete_irq, 1);
    irq_set_priority(config_.preamble_detector_demod_pin_irq, 0);

    // Set the last dictionary update timestamp.
    last_aircraft_dictionary_update_timestamp_ms_ = millis();//!! get_time_since_boot_ms();

    // Enable the state machines.
    pio_sm_set_enabled(config_.preamble_detector_pio, irq_wrapper_sm_, true);
    // Need to enable the demodulator SMs first, since if the preamble detector trips the IRQ but the demodulator isn't
    // enabled, we end up in a deadlock (I think, this maybe should be verified again).
    for (uint16_t sm_index = 0; sm_index < bsp.r1090_num_demod_state_machines; sm_index++) {
        // pio_sm_set_enabled(config_.preamble_detector_pio, preamble_detector_sm_[sm_index], true);
        pio_sm_set_enabled(config_.message_demodulator_pio, message_demodulator_sm_[sm_index], true);
    }
    // Enable round robin well formed preamble detectors.
    // NOTE: These need to be enable to allow the high power preamble detector to run, since they reset the IRQ that the
    // high power preamble detector relies on. This is a vestige of the fact that the high power preamble detector uses
    // the same PIO code that does round-robin for the well formed preamble detectors.
    for (uint16_t sm_index = 0; sm_index < bsp.r1090_high_power_demod_state_machine_index; sm_index++) {
        pio_sm_set_enabled(config_.preamble_detector_pio, preamble_detector_sm_[sm_index], true);
    }
    // Enable high power preamble detector.
    pio_sm_set_enabled(config_.preamble_detector_pio, preamble_detector_sm_[bsp.r1090_high_power_demod_state_machine_index], true);


    return true;
}

bool ADSBee::Update() 
{
 
    return true;
}


uint64_t ADSBee::GetMLAT48MHzCounts(uint16_t num_bits) {
    // Combine the wrap counter with the current value of the SysTick register and mask to 48 bits.
    // Note: 24-bit SysTick value is subtracted from UINT_24_MAX to make it count up instead of down.
    return (mlat_counter_wraps_ + ((0xFFFFFF - systick_hw->cvr) * MLAT_SYSTEM_CLOCK_RATIO)) &
           (UINT64_MAX >> (64 - num_bits));
}

uint64_t ADSBee::GetMLAT12MHzCounts(uint16_t num_bits) {
    // Piggyback off the higher resolution 48MHz timer function.
    return GetMLAT48MHzCounts(50) >> 2;  // Divide 48MHz counter by 4, widen the mask by 2 bits to compensate.
}

void ADSBee::OnDemodBegin(uint gpio) {
    // Read MLAT counter at the beginning to reduce jitter after interrupt.
    uint64_t mlat_48mhz_64bit_counts = GetMLAT48MHzCounts();
    uint16_t sm_index;
    for (sm_index = 0; sm_index < bsp.r1090_num_demod_state_machines; sm_index++) {
        if (config_.demod_pins[sm_index] == gpio) {
            break;
        }
    }
    if (sm_index >= bsp.r1090_num_demod_state_machines)
        return;  // Ignore; wasn't the start of a demod interval for a known SM.
    // Demodulation period is beginning! Store the MLAT counter.
    rx_packet_[sm_index].mlat_48mhz_64bit_counts = mlat_48mhz_64bit_counts;
}


void ADSBee::HandleReceivedPacket(int sm_index, Raw1090Packet& pkt) 
{
    // === Проверка CRC ===
    if (adsb_check_crc(pkt)) 
    {
        Serial.print("[ADS-B] CRC OK SM=");
        Serial.println(sm_index);

        char buf[128];
        pkt.PrintBuffer(buf, sizeof(buf));
        Serial.println(buf);

        DecodedAdsb d = adsb_decode(pkt);
        Serial.print(" DF="); Serial.print(d.df);
        Serial.print(" ICAO="); Serial.print(d.icao, HEX);
        Serial.print(" TC="); Serial.print(d.tc);
        if (d.callsign[0]) {
            Serial.print(" CALLSIGN="); Serial.print(d.callsign);
        }
        if (d.alt != 0) {
            Serial.print(" ALT="); Serial.print(d.alt);
        }
        Serial.println();

    }
    else 
    {
 /*       Serial.print("[ADS-B] CRC ERROR SM=");
        Serial.println(sm_index);*/
    }
}



void ADSBee::OnDemodComplete() {
    for (uint16_t sm_index = 0; sm_index < bsp.r1090_num_demod_state_machines; sm_index++) {
        if (!pio_interrupt_get(config_.preamble_detector_pio, sm_index)) {
            continue;
        }
        pio_sm_set_enabled(config_.message_demodulator_pio, message_demodulator_sm_[sm_index], false);
        // Read the RSSI level of the current packet.
        rx_packet_[sm_index].sigs_dbm = 0;
        rx_packet_[sm_index].sigq_db = 0;
        rx_packet_[sm_index].source = sm_index;  // Record this state machine as the source of the packet.
        if (!pio_sm_is_rx_fifo_full(config_.message_demodulator_pio, message_demodulator_sm_[sm_index])) 
        {
            // Push any partially complete 32-bit word onto the RX FIFO.
            pio_sm_exec_wait_blocking(config_.message_demodulator_pio, message_demodulator_sm_[sm_index], pio_encode_push(false, true));
        }

        // Clear the transponder packet buffer.
        memset((void *)rx_packet_[sm_index].buffer, 0x0, Raw1090Packet::kMaxPacketLenWords32);

        // Pull all words out of the RX FIFO.
        volatile uint16_t packet_num_words =  pio_sm_get_rx_fifo_level(config_.message_demodulator_pio, message_demodulator_sm_[sm_index]);
        if (packet_num_words > Raw1090Packet::kMaxPacketLenWords32) {
            // Packet length is invalid; dump everything and try again next time.
            // Only enable this print for debugging! Printing from the interrupt causes the USB library to crash.
            // CONSOLE_WARNING("ADSBee::OnDemodComplete", "Received a packet with %d 32-bit words, expected maximum of
            // %d.",
            //                 packet_num_words, Raw1090Packet::kExtendedSquitterPacketNumWords32);
            // pio_sm_clear_fifos(config_.message_demodulator_pio, message_demodulator_sm_);
            packet_num_words = Raw1090Packet::kMaxPacketLenWords32;
        }

        // Create a Raw1090Packet and push it onto the queue.
        for (uint16_t i = 0; i < packet_num_words; i++) 
        {
            rx_packet_[sm_index].buffer[i] =   pio_sm_get(config_.message_demodulator_pio, message_demodulator_sm_[sm_index]);
            if (i == packet_num_words - 1) 
            {
                // Trim off extra ingested bit from last word in the packet.
                // Mask and left align final word based on bit length.
                switch (packet_num_words) {
                    case Raw1090Packet::kSquitterPacketNumWords32:
                        rx_packet_[sm_index].buffer[i] = (rx_packet_[sm_index].buffer[i] & 0xFFFFFF) << 8;
                        rx_packet_[sm_index].buffer_len_bits = Raw1090Packet::kSquitterPacketLenBits;
                        HandleReceivedPacket(sm_index, rx_packet_[sm_index]);

  
                        break;
                    case Raw1090Packet::kExtendedSquitterPacketNumWords32:
                        rx_packet_[sm_index].buffer[i] = (rx_packet_[sm_index].buffer[i] & 0xFFFF) << 16;
                        rx_packet_[sm_index].buffer_len_bits = Raw1090Packet::kExtendedSquitterPacketLenBits;
                        HandleReceivedPacket(sm_index, rx_packet_[sm_index]);
                        break;
                    default:
                        break;
                }
            }
        }



        // Clear the FIFO by pushing partial word from ISR, not bothering to block if FIFO is full (it shouldn't be).
        pio_sm_exec_wait_blocking(config_.message_demodulator_pio, message_demodulator_sm_[sm_index],
                                  pio_encode_push(false, false));
        while (!pio_sm_is_rx_fifo_empty(config_.message_demodulator_pio, message_demodulator_sm_[sm_index])) {
            pio_sm_get(config_.message_demodulator_pio, message_demodulator_sm_[sm_index]);
        }

        // Reset the demodulator state machine to wait for the next decode interval, then enable it.
        pio_sm_restart(config_.message_demodulator_pio, message_demodulator_sm_[sm_index]);  // Reset FIFOs, ISRs, etc.
        // The high power demodulator has a different start address to account for the fact that the index of its DEMOD
        // pin is different. This only matters for the initial program wait, subsequent demod checks are done on the
        // full GPIO input register.
        uint demodulator_program_start =
            sm_index == bsp.r1090_high_power_demod_state_machine_index
                ? message_demodulator_offset_ + message_demodulator_offset_high_power_initial_entry
                : message_demodulator_offset_ + message_demodulator_offset_initial_entry;
        pio_sm_exec_wait_blocking(config_.message_demodulator_pio, message_demodulator_sm_[sm_index],
                                  pio_encode_jmp(demodulator_program_start));  // Jump to beginning of program.
        pio_sm_set_enabled(config_.message_demodulator_pio, message_demodulator_sm_[sm_index], true);

        // Release the preamble detector from its wait state.
        if (sm_index == bsp.r1090_high_power_demod_state_machine_index) {
            // High power state machine operates alone and doesn't need to wait for any other SM to complete. It would
            // normally be enabled by one of the interleaved well formed preamble detector state machines refreshin, but
            // doing it here brings it up a little quicker and allows it to catch a subsequent high power packet if it
            // comes in quickly.
            pio_sm_exec_wait_blocking(
                config_.preamble_detector_pio, preamble_detector_sm_[sm_index],
                pio_encode_jmp(preamble_detector_offset_ + preamble_detector_offset_waiting_for_first_edge));
        }

        pio_interrupt_clear(config_.preamble_detector_pio, sm_index);
    }
}

void ADSBee::OnSysTickWrap() { mlat_counter_wraps_ += kMLATWrapCounterIncrement; }

