#ifndef _ADS_BEE_H_
#define _ADS_BEE_H_



#include "hardware/pio.h"
#include "hardware/watchdog.h"
#include "bsp.h"



class Raw1090Packet {
public:
    static const uint16_t kMaxPacketLenWords32 = 4;
    static const uint16_t kSquitterPacketLenBits = 56;
    static const uint16_t kSquitterPacketNumWords32 = 2;  // 56 bits = 1.75 words, round up to 2.
    static const uint16_t kExtendedSquitterPacketLenBits = 112;
    static const uint16_t kExtendedSquitterPacketNumWords32 = 4;  // 112 bits = 3.5 words, round up to 4.

    Raw1090Packet(char* rx_string, int16_t source_in = -1, uint64_t mlat_48mhz_64bit_counts = 0);

    Raw1090Packet(uint32_t rx_buffer[kMaxPacketLenWords32], uint16_t rx_buffer_len_words32, int16_t source_in = -1, uint64_t mlat_48mhz_64bit_counts = 0);
    /**
     * Default constructor.
     */
    Raw1090Packet()
    {
        for (uint16_t i = 0; i < kMaxPacketLenWords32; i++)
        {
            buffer[i] = 0;
        }
    }

    /**
    * Вспомогательная функция, возвращающая временную метку в секундах. Используется для абстрагирования разрешения временной метки MLAT для
    * функций, которым это не нужно.
    * @return Временная метка в секундах.
    */
    uint64_t GetTimestampMs() { return mlat_48mhz_64bit_counts / 48'000; }

    /**
    * Вывести содержимое буфера в строку.
    * @param[in] buf Буфер для печати.
    * @param[in] buf_len_bytes Длина буфера в символах.
    * @return Количество символов, записанных в буфер.
    */
    //uint16_t PrintBuffer(char* buf, uint16_t buf_len_bytes) const;

    uint32_t buffer[kMaxPacketLenWords32] = { 0 };
    uint16_t buffer_len_bits = 0;
    int8_t source = -1;                    // Источник пакета ADS-B (номер конечного автомата PIO).
    uint64_t mlat_48mhz_64bit_counts = 0;  // Счетчик MLAT высокого разрешения.
};


class ADSBee {
   public:
       static constexpr uint32_t kStatusLEDOnMs = 10;

    struct ADSBeeConfig {
        PIO preamble_detector_pio = pio0;
        uint preamble_detector_demod_pin_irq = IO_IRQ_BANK0;
        PIO message_demodulator_pio = pio1;
        uint preamble_detector_demod_complete_irq = PIO0_IRQ_0;

         // Reading ADS-B on GPIO19. Will look for DEMOD signal on GPIO20.
        uint16_t* pulses_pins = bsp.r1090_pulses_pins;
        uint16_t* demod_pins = bsp.r1090_demod_pins;
        // Use GPIO22 for the decode PIO program to output its recovered clock (for debugging only).
        uint16_t* recovered_clk_pins =  bsp.r1090_recovered_clk_pins;  // Set RECOVERED_CLK to fake pin for high power preamble detector. Will be
    };

    ADSBee(ADSBeeConfig config_in);
    bool Init();
 /*   bool Update();*/

 
    /**
     * Blinks the status LED for a given number of milliseconds. Non-blocking.
     * @param[in] led_on_ms Optional parameter specifying number of milliseconds to turn on for. Defaults to
     * kStatusLEDOnMs.
     */
    void FlashStatusLED(uint32_t led_on_ms = kStatusLEDOnMs);

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

};

extern ADSBee adsbee;

#endif /* _ADS_BEE_HH_ */