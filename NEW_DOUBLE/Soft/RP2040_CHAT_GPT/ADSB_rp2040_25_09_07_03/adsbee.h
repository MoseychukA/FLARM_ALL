#ifndef _ADS_BEE_H_
#define _ADS_BEE_H_

#include "bsp.h"
#include "hardware/watchdog.h"
#include "stdint.h"
#include "adsb_packet.h"
//#include "raw1090packet.h"

constexpr int kMaxNumDemodStateMachines = 3;
constexpr int kMaxNumTransponderPackets = 32;

struct AdsbeeContext {
    Raw1090Packet rx_packet_[kMaxNumDemodStateMachines];
    Raw1090Packet raw_1090_packet_queue_buffer_[kMaxNumTransponderPackets];
};

void OnDemodComplete(int sm_index, const Raw1090Packet& pkt);

class ADSBee {
 
public:
 
    static constexpr uint32_t kTLLearningIntervalMs          = 10000;  // [ms] Length of Simulated Annealing interval for learning trigger level.
    static constexpr uint16_t kTLLearningNumCycles           = 100;    // Number of simulated annealing cycles for learning trigger level.
    static constexpr uint16_t kTLLearningStartTemperatureMV  = 1000;   // [mV] Starting value for simulated annealing temperature when learning triger level. This corresponds
    static constexpr int32_t kNoiseFloorExpoFilterPercent    = 50;     // [%] Weight to use for low pass expo filter of noise floor ADC counts. 0 = no filter, 100 = hold value.
    static constexpr uint32_t kNoiseFloorADCSampleIntervalMs = 1;      // [ms] Interval between ADC samples to approximate noise floor value.

    struct ADSBeeConfig {
        PIO preamble_detector_pio = pio0;
        uint preamble_detector_demod_pin_irq = IO_IRQ_BANK0;
        PIO message_demodulator_pio = pio1;
        uint preamble_detector_demod_complete_irq = PIO0_IRQ_0;

        uint16_t* pulses_pins        = bsp.r1090_pulses_pins;
        uint16_t* demod_pins         = bsp.r1090_demod_pins;
        uint16_t* recovered_clk_pins = bsp.r1090_recovered_clk_pins;      // Set RECOVERED_CLK to fake pin for high power preamble detector.
                                           
    };

    ADSBee(ADSBeeConfig config_in);
    bool Init();
    bool Update();
    void HandleReceivedPacket(int sm_index, Raw1090Packet& pkt);

 
     /**
     * Creates a composite timestamp using the current value of the SysTick timer (running at 125MHz) and the SysTick
     * wrap counter to simulate a timer running at 48MHz (which matches the frequency of the preamble detector PIO).
     * @param[in] num_bits Number of bits to mask the counter value to. Defaults to full resolution.
     * @retval 48MHz counter value.
     */
    uint64_t GetMLAT48MHzCounts(uint16_t num_bits = 64);

    /**
     * Creates a composite timestamp using the current value of the SysTick timer (running at 125MHz) and the SysTick
     * wrap counter to simulate a timer running at 12MHz, which matches existing decoders that use the Mode S Beast
     * protocol.
     * @param[in] num_bits Number of bits to mask the counter value to. Defaults to 48 bits (6 Bytes) to match Mode S
     * Beast protocol.
     * @retval 48MHz counter value.
     */
    uint64_t GetMLAT12MHzCounts(uint16_t num_bits = 48);

    /**
     * ISR for GPIO interrupts.
     */
    void OnDemodBegin(uint gpio);

    /**
     * ISR triggered by DECODE completing, via PIO0 IRQ0.
     */
    void OnDemodComplete();

    /**
     * ISR triggered by SysTick interrupt. Used to wrap the MLAT counter.
     */
    void OnSysTickWrap();

    /**
     * Resets the watchdog counter to the value set in SetWatchdogTimeoutSec().
     */
    inline void PokeWatchdog() { watchdog_update(); }

    static constexpr uint16_t kMaxNumTransponderPackets = 100;  // Определяет размер кольцевого буфера ADSBPacket (PFBQueue).
   // PFBQueue<Raw1090Packet> raw_1090_packet_queue =  PFBQueue<Raw1090Packet>({.buf_len_num_elements =kMaxNumTransponderPackets, .buffer = raw_1090_packet_queue_buffer_});

   private:
    ADSBeeConfig config_;
  
    uint32_t irq_wrapper_sm_ = 0;
    uint32_t preamble_detector_sm_[BSP::kMaxNumDemodStateMachines];
    uint32_t preamble_detector_offset_ = 0;

    uint32_t irq_wrapper_offset_ = 0;

    uint32_t message_demodulator_sm_[BSP::kMaxNumDemodStateMachines];
    uint32_t message_demodulator_offset_ = 0;
  
    uint64_t mlat_counter_wraps_ = 0;

    Raw1090Packet rx_packet_[BSP::kMaxNumDemodStateMachines];
    Raw1090Packet raw_1090_packet_queue_buffer_[kMaxNumTransponderPackets];

    uint32_t last_aircraft_dictionary_update_timestamp_ms_ = 0;

};

extern ADSBee adsbee;

#endif /* _ADS_BEE_HH_ */