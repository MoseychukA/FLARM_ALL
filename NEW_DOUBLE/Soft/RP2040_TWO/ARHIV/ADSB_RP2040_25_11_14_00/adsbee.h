#ifndef _ADS_BEE_H_
#define _ADS_BEE_H_

#include "aircraft_dictionary.h"
#include "bsp.h"
#include "data_structures.h"  // For PFBQueue.
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/watchdog.h"
#include "macros.h"  // For MAX / MIN.
#include "settings.h"
#include "stdint.h"
#include "transponder_packet.h"

class ADSBee {
   public:
    static constexpr uint16_t kTLMaxPWMCount = 5000;  // Clock is 125MHz, shoot for 25kHz PWM.
    static constexpr int kVDDMV = 3300;               // [mV] Voltage of positive supply rail.
    static constexpr int kTLMaxMV = 3300;             // [mV]
    static constexpr int kTLMinMV = 0;                // [mV]
    static constexpr uint32_t kStatusLEDOnMs = 10;

    static constexpr uint32_t kTLLearningIntervalMs         = 10000;  // [ms] Length of Simulated Annealing interval for learning trigger level.
    static constexpr uint16_t kTLLearningNumCycles          = 100;    // Number of simulated annealing cycles for learning trigger level.
    static constexpr uint16_t kTLLearningStartTemperatureMV = 1000;   // [mV] Starting value for simulated annealing temperature when learning triger level. This corresponds
                                                                      // to the maximum value that the trigger level could be moved (up or down) when exploring a neighbor state.

    static constexpr int32_t kNoiseFloorExpoFilterPercent    = 50;  // [%] Weight to use for low pass expo filter of noise floor ADC counts. 0 = no filter, 100 = hold value.
    static constexpr uint32_t kNoiseFloorADCSampleIntervalMs = 1;  // [ms] Interval between ADC samples to approximate noise floor value.

    struct ADSBeeConfig {
        PIO preamble_detector_pio = pio0;
        uint preamble_detector_demod_pin_irq = IO_IRQ_BANK0;
        PIO message_demodulator_pio = pio1;
        uint preamble_detector_demod_complete_irq = PIO0_IRQ_0;

        uint16_t r1090_led_pin = 15;
  
        uint16_t* pulses_pins = bsp.r1090_pulses_pins;         // 19, 22, 18 Чтение ADS-B на GPIO19. Будем искать сигнал DEMOD на GPIO20.
        uint16_t* demod_pins = bsp.r1090_demod_pins;           // 20, 23, 25

        // Use GPIO22 for the decode PIO program to output its recovered clock (for debugging only).
        uint16_t* recovered_clk_pins = bsp.r1090_recovered_clk_pins;  // Set RECOVERED_CLK to fake pin for high power preamble detector. Will be
                                           // overridden by higher priority (lower index) SM.
       
        //uint16_t tl_pwm_pin = bsp.r1090_tl_pwm_pin;             // GPIO 9  используются как выход ШИМ для установки пороговых напряжений аналогового компаратора.
        //uint16_t tl_adc_pin = bsp.r1090_tl_adc_pin;             // GPIO 27 используются как входы АЦП для считывания пороговых напряжений аналогового компаратора после ВЧ-фильтра.
        //uint16_t tl_adc_input = bsp.r1090_tl_adc_input;         // 1 ADC input for reading filtered Trigger Level.
        uint16_t rssi_adc_pin = bsp.r1090_rssi_adc_pin;         // GPIO 28 используется как вход АЦП для уровня мощности последнего декодированного пакета.
        uint16_t rssi_adc_input = bsp.r1090_rssi_adc_input;     // 2 ADC input for reading RSSI.
        uint32_t aircraft_dictionary_update_interval_ms = 1000; //
    };

    ADSBee(ADSBeeConfig config_in);
    bool Init();
    bool Update();

    /**
    * Вспомогательная функция Inlne, преобразующая милливольты на входе AD8313 в соответствующее значение в дБм, используя значения
    * из технического описания AD8313.
    * @param[in] mv Уровень напряжения в милливольтах.
    * @retval Соответствующий уровень мощности в дБм.
    */
    static inline int AD8313MilliVoltsTodBm(int mv)
    {
       
        static constexpr uint16_t kLNAGaindB = 44;    // Коэффициент усиления 2x LNA перед AD8313, полученный в ходе стендовых испытаний.
      //  Serial.print("***mv: "); Serial.println(60 * (mv - 1600) / 1000 - kLNAGaindB);
        return mv; // 60 * (mv - 1600) / 1000 - kLNAGaindB;  // AD8313 0dBm intercept at 1.6V, slope is 60dBm/V.
    }

    /**
    * Встроенная вспомогательная функция, преобразующая показания АЦП на RP2040 в милливольты.
    * @param[in] adc_counts Отсчеты АЦП, от 0 до 4095.
    * @retval Напряжение в милливольтах.
    */
    static inline int ADCCountsToMilliVolts(uint16_t adc_counts) { return 3300 * adc_counts / 0xFFF; }

    /**
   * Возвращает, включен ли смещенный тройник.
   * @retval True, если смещенный тройник включен, в противном случае false.
   */
    //bool BiasTeeIsEnabled() { return bias_tee_enabled_; }

    /**
     * Convenience function for temporarily disabling the watchdog without changing its timeout.
     */
    void DisableWatchdog() { watchdog_disable(); }

    /**
     * Convenience function for re-enabling the watchdog with the last used timeout.
     */
    void EnableWatchdog() { SetWatchdogTimeoutSec(watchdog_timeout_sec_); }

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
     * Returns the power level of the noise floor (signal strength sampled mostly during non-decode intervals and then
     * low-pass filtered).
     * @retval Power level of the noise floor, in dBm.
     */
    int GetNoiseFloordBm();

    /**
     * Get the current temperature used in learning trigger level (simulated annealing). A temperature of 0 means
     * learning has completed.
     * @retval Current temperature used for simulated annealing, in milliVolts.
     */
   // uint16_t GetTLLearningTemperatureMV();

    /**
     * Return the value of the low Minimum Trigger Level threshold in milliVolts.
     * @retval TL in milliVolts.
     */
    //int GetTLMilliVolts() { return tl_mv_; }

    inline uint32_t GetWatchdogTimeoutSec() { return watchdog_timeout_sec_; }

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

    /**
     * Returns the Receive Signal Strength Indicator (RSSI) of the signal currently provided by the RF power detector,
     * in mV.
     * @retval Voltage from the RF power detector, in mV.
     */
    inline int ReadSignalStrengthMilliVolts();

    /**
     * Returns the Receive Signal Strength Indicator (RSSI) of the message that is currently being provided by the RF
     * power detector, in dBm. makes use of ReadSignalStrengthMilliVolts().
     * @retval Voltage form the RF power detector converted to dBm using the chart in the AD8313 datasheet.
     */
    inline int ReadSignalStrengthdBm();

    /**
     * Read the low Minimum Trigger Level threshold via ADC.
     * @retval TL in milliVolts.
     */
    //int ReadTLMilliVolts();

    ///**
    // * Reboots the RP2040 via the watchdog.
    // * @param[in] delay_ms Milliseconds to wait befor rebooting. Defaults to 0 (immediate reboot).
    // */
    //inline void Reboot(uint16_t delay_ms = 0) { watchdog_reboot(0, 0, delay_ms); }

    ///**
    // * Returns whether ADS-B receiving is currently enabled.
    // * @retval True if enabled, false otherwise.
    // */
    //bool Receiver1090IsEnabled() { return r1090_enabled_; }

    ///**
    // * Enable or disable the bias tee to inject 3.3V at the RF IN connector.
    // * @param[in] is_enabled True to enable the bias tee, false otherwise.
    // */
    //inline void SetBiasTeeEnable(bool is_enabled) {
    //    bias_tee_enabled_ = is_enabled;
    //   // gpio_put(config_.bias_tee_enable_pin, !bias_tee_enabled_);
    //}

    ///**
    // * Enables or disables the ADS-B receiver by hogging the demodulation completed interrupt.
    // * @param[in] is_enabled True if ADS-B receiver should be enabled, false otherwise.
    // */
    //inline void SetReceiver1090Enable(bool is_enabled) {
    //    r1090_enabled_ = is_enabled;
    //    irq_set_enabled(config_.preamble_detector_demod_complete_irq, r1090_enabled_);
    //}



    /**
     * Sets the status LED to a given state. Does not record timestamps for turning off the LED.
     * @param[in] on True to turn on the LED, false to turn it off.
     */
    inline void SetStatusLED(bool on) { gpio_put(config_.r1090_led_pin, on ? 1 : 0); }

    ///**
    // * Set the Minimum Trigger Level (TL) at the AD8314 output in milliVolts.
    // * @param[in] tl_mv Voltage in milliVolts at the top of the pullup for the LEVEL net in the data slicer. Pull higher
    // * to accommodate a higher noise floor without false triggers.
    // * @retval True if succeeded, False if TL value was out of range.
    // */
    //bool SetTLMilliVolts(int tl_mv);

    /**
     * Sets the watchdog timer and enables it.
     * @param[in] watchdog_timeout_sec Maximum interval between PokeWatchdog() calls before watchdog times out and
     * triggers a reboot. 0 = watchodg is disabled. Note that this value is in seconds, to hopefully prevent an
     * unrecoverable loop where the watchdog timer gets set too short and causes the device to reboot before the setting
     * can be changed.
     * @retval True if set successfully, false if invalid watchdog value.
     */
    inline bool SetWatchdogTimeoutSec(uint32_t watchdog_timeout_sec) {
        if (watchdog_timeout_sec > UINT32_MAX / kMsPerSec) {
            return false;  // Watchdog timeout value too big.
        }
        watchdog_timeout_sec_ = watchdog_timeout_sec;
        if (watchdog_timeout_sec_ > 0) {
            watchdog_enable(watchdog_timeout_sec_ * kMsPerSec, true);  // Pause the watchdog timer during debug.
        } else {
            watchdog_disable();
        }
        return true;
    }

    /**
     * Start learning the trigger level through Simulated Annealing. Will begin kTLLearningNumCycles annealing cycles
     * with an annealing interval of kTLLearningIntervalMs milliseconds. Can be provided with maximum and minimum
     * trigger level bounds to allow a narrower search.
     * @param[in] tl_learning_num_cycles Number of cycles to use while annealing trigger level (sets the amount that the
     * annealing temperature is decreased each cycle). Optional, defaults to kTLLearningNumCycles.
     * @param[in] tl_learning_start_temperature_mv Annealing temperature to start with, in mV.
     * @param[in] tl_min_mv Minimum trigger level to use while learning, in milliVolts. Optional, defaults to full scale
     * (kTLMinMV).
     * @param[in] tl_max_mv Maximum trigger level to use while learning, in milliVolts. Optional, defaults to full scale
     * (kTLMaxMV).
     */
    //void StartTLLearning(uint16_t tl_learning_num_cycles = kTLLearningNumCycles,
    //                     uint16_t tl_learning_start_temperature_mv = kTLLearningStartTemperatureMV,
    //                     uint16_t tl_min_mv = kTLMinMV, uint16_t tl_max_mv = kTLMaxMV);

    PFBQueue<Raw1090Packet> raw_1090_packet_queue =  PFBQueue<Raw1090Packet>({.buf_len_num_elements = SettingsManager::Settings::kMaxNumTransponderPackets, .buffer = raw_1090_packet_queue_buffer_});

    AircraftDictionary aircraft_dictionary;
 
   private:
    ADSBeeConfig config_;
  
    uint32_t irq_wrapper_sm_ = 0;
    uint32_t preamble_detector_sm_[BSP::kMaxNumDemodStateMachines];
    uint32_t preamble_detector_offset_ = 0;

    uint32_t irq_wrapper_offset_ = 0;

    uint32_t message_demodulator_sm_[BSP::kMaxNumDemodStateMachines];
    uint32_t message_demodulator_offset_ = 0;

    uint32_t led_on_timestamp_ms_ = 0;

    //uint16_t tl_pwm_slice_ = 0;
    //uint16_t tl_pwm_chan_ = 0;

    //uint16_t tl_mv_ = SettingsManager::Settings::kDefaultTLMV;
    //uint16_t tl_pwm_count_ = 0;  // out of kTLMaxPWMCount

    //uint16_t tl_adc_counts_ = 0;

    //uint32_t tl_learning_cycle_start_timestamp_ms_ = 0;
    //uint16_t tl_learning_temperature_mv_ = 0;  // Don't learn automatically.
    //int16_t tl_learning_temperature_step_mv_ = 0;
    //uint16_t tl_learning_max_mv_ = kTLMaxMV;
    //uint16_t tl_learning_min_mv_ = kTLMinMV;
    //int16_t tl_learning_num_valid_packets_ = 0;
    //int16_t tl_learning_prev_num_valid_packets_ = 1;  // Set to 1 to avoid dividing by 0.
    //uint16_t tl_learning_prev_tl_mv_ = tl_mv_;

    uint64_t mlat_counter_wraps_ = 0;

    Raw1090Packet rx_packet_[BSP::kMaxNumDemodStateMachines];
    Raw1090Packet raw_1090_packet_queue_buffer_[SettingsManager::Settings::kMaxNumTransponderPackets];

    uint32_t last_aircraft_dictionary_update_timestamp_ms_ = 0;

    bool r1090_enabled_ = true;
    bool bias_tee_enabled_ = false;
    uint32_t watchdog_timeout_sec_ = SettingsManager::Settings::kDefaultWatchdogTimeoutSec * kMsPerSec;

    int32_t noise_floor_mv_;
    uint32_t noise_floor_last_sample_timestamp_ms_ = 0;

};

extern ADSBee adsbee;

#endif /* _ADS_BEE_HH_ */