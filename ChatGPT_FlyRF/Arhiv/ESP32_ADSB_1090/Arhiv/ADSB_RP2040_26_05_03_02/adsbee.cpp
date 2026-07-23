#include "adsbee.h"
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
#include "stdio.h"   
#include "capture_pio.h"
#include "hal.h"
#include "hardware/irq.h"
#include "packet_decoder.h"
#include "pico/binary_info.h"
#include <string.h>            
#include "comms.h"   


#define RSSI_V_MIN 700.0f      // мВ
#define RSSI_V_MAX 1300.0f     // мВ

#define MLAT_SYSTEM_CLOCK_RATIO 48 / 125
// Scales 125MHz system clock into a 48MHz counter.
static const uint32_t kMLATWrapCounterIncrement = (1 << 24) * MLAT_SYSTEM_CLOCK_RATIO;

constexpr float kPreambleDetectorFreq = 48e6;    // Running at 48MHz (24 clock cycles per half bit).
constexpr float kMessageDemodulatorFreq = 48e6;  // Run at 48 MHz to demodulate bits at 1Mbps.

constexpr float kInt16MaxRecip = 1.0f / INT16_MAX;

ADSBee *isr_access = nullptr;

/** Начало сквозных функций для публичного доступа **/
void on_systick_exception() { isr_access->OnSysTickWrap(); }

void on_demod_pin_change(uint gpio, uint32_t event_mask)
{
    switch (event_mask) 
    {
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

/** Завершение сквозных функций для публичного доступа **/

ADSBee::ADSBee(ADSBeeConfig config_in) 
{
    config_ = config_in;

    for (uint16_t sm_index = 0; sm_index < bsp.r1090_num_demod_state_machines; sm_index++)
    {
        preamble_detector_sm_[sm_index] = pio_claim_unused_sm(config_.preamble_detector_pio, true);
        message_demodulator_sm_[sm_index] = pio_claim_unused_sm(config_.message_demodulator_pio, true);
    }
    irq_wrapper_sm_ = pio_claim_unused_sm(config_.preamble_detector_pio, true);

    preamble_detector_offset_ = pio_add_program(config_.preamble_detector_pio, &preamble_detector_program);
    irq_wrapper_offset_ = pio_add_program(config_.preamble_detector_pio, &irq_wrapper_program);
    message_demodulator_offset_ = pio_add_program(config_.message_demodulator_pio, &message_demodulator_program);

    // Поместите параметры IRQ в глобальную область действия для ISR on_demod_complete.
    isr_access = this;
}

bool ADSBee::Init() 
{
    gpio_init(config_.r1090_led_pin);
    gpio_set_dir(config_.r1090_led_pin, GPIO_OUT);
    gpio_put(config_.r1090_led_pin, 0);

    // Initialize the trigger level bias ADC input.
    adc_init();
    adc_gpio_init(config_.rssi_adc_pin);
    noise_floor_mv_ = ReadSignalStrengthMilliVolts();

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
    irq_wrapper_program_init(config_.preamble_detector_pio, bsp.r1090_num_demod_state_machines, irq_wrapper_offset_, preamble_detector_div);
    for (uint16_t sm_index = 0; sm_index < bsp.r1090_num_demod_state_machines; sm_index++) 
    {
        // Заставить конечный автомат ждать запуска только в том случае, если он входит в циклическую группу правильно сформированных детекторов преамбул.
        bool make_sm_wait = sm_index > 0 && sm_index < bsp.r1090_high_power_demod_state_machine_index;
        // Инициализируем программу, используя вспомогательную функцию файла .pio
        preamble_detector_program_init(config_.preamble_detector_pio,                     // Use PIO block 0.
                                       preamble_detector_sm_[sm_index],                   // State machines 0-2
                                       preamble_detector_offset_ /* + starting_offset*/,  // Program startin offset.
                                       config_.pulses_pins[sm_index],                     // Pulses pin (input).
                                       config_.demod_pins[sm_index],                      // Demod pin (output).
                                       preamble_detector_div,                             // Clock divisor (for 48MHz).
                                       make_sm_wait                                       // Должен ли конечный автомат ждать начала IRQ.
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
    for (uint16_t sm_index = 0; sm_index < bsp.r1090_num_demod_state_machines; sm_index++) 
    {
        message_demodulator_program_init(config_.message_demodulator_pio, message_demodulator_sm_[sm_index],
                                         message_demodulator_offset_, 
                                         config_.pulses_pins[sm_index],
                                         config_.demod_pins[sm_index],
                                         config_.recovered_clk_pins[sm_index],
                                         message_demodulator_div);
    }

    // Set GPIO interrupts to be higher priority than the DEMOD interrupt to allow RSSI measurement.
    // irq_set_priority(config_.preamble_detector_demod_complete_irq, 1);
    irq_set_priority(config_.preamble_detector_demod_pin_irq, 0);

    // Set the last dictionary update timestamp.
    last_aircraft_dictionary_update_timestamp_ms_ = millis();

    // Enable the state machines.
    pio_sm_set_enabled(config_.preamble_detector_pio, irq_wrapper_sm_, true);
    // Need to enable the demodulator SMs first, since if the preamble detector trips the IRQ but the demodulator isn't
    // enabled, we end up in a deadlock (I think, this maybe should be verified again).
    for (uint16_t sm_index = 0; sm_index < bsp.r1090_num_demod_state_machines; sm_index++) 
    {
        pio_sm_set_enabled(config_.preamble_detector_pio, preamble_detector_sm_[sm_index], true);  //!! Расскомментировал
        pio_sm_set_enabled(config_.message_demodulator_pio, message_demodulator_sm_[sm_index], true);
    }

    // Включить циклический перебор детекторов правильно сформированных преамбул.
    // ПРИМЕЧАНИЕ: Их необходимо включить для работы детектора преамбул высокой мощности, поскольку они сбрасывают IRQ, на который опирается
    // детектор преамбул высокой мощности. Это связано с тем, что детектор преамбул высокой мощности использует
    // тот же код PIO, который обеспечивает циклический перебор для детекторов правильно сформированных преамбул.
    for (uint16_t sm_index = 0; sm_index < bsp.r1090_high_power_demod_state_machine_index; sm_index++) 
    {
        pio_sm_set_enabled(config_.preamble_detector_pio, preamble_detector_sm_[sm_index], true);
    }
    // Enable high power preamble detector.
    pio_sm_set_enabled(config_.preamble_detector_pio, preamble_detector_sm_[bsp.r1090_high_power_demod_state_machine_index], true);
    return true;
}

bool ADSBee::Update() 
{
    uint32_t timestamp_ms = millis();
    // Turn off the demod LED if it's been on for long enough.
    if (timestamp_ms - led_on_timestamp_ms_ > kStatusLEDOnMs) 
    {
        gpio_put(config_.r1090_led_pin, 0);
    }
    // Очистить словарь самолётов. Это необходимо сделать заранее, чтобы избежать отрицательной разницы во временных метках.
    // вызванной тем, что пакеты были получены позже, чем временная метка, которую мы берём в начале этой функции.
    if (timestamp_ms - last_aircraft_dictionary_update_timestamp_ms_ > config_.aircraft_dictionary_update_interval_ms) 
    {
        aircraft_dictionary.Update(timestamp_ms);
        last_aircraft_dictionary_update_timestamp_ms_ = timestamp_ms;
    }

    // Добавлять новые пакеты в словарь.
    // Raw1090Packet raw_packet;
    Decoded1090Packet decoded_packet;
    while (decoder.decoded_1090_packet_out_queue.Pop(decoded_packet) /*raw_1090_packet_queue.Pop(raw_packet)*/) 
    {

        // Decoded1090Packet decoded_packet = Decoded1090Packet(raw_packet);

        if (aircraft_dictionary.IngestDecoded1090Packet(decoded_packet))  
        {
            // Пакет был использован для обновления словаря или был молча проигнорирован (но предположительно действителен).
            FlashStatusLED();
            // ПРИМЕЧАНИЕ: помещение в очередь отчетов означает, что мы будем сообщать только о проверенных пакетах!
        }
        comms_manager.transponder_packet_reporting_queue.Push(decoded_packet);
    }
    // Occasionally sample the signal strength to approximate the noise floor.
    timestamp_ms = millis();
    if (timestamp_ms - noise_floor_last_sample_timestamp_ms_ > kNoiseFloorADCSampleIntervalMs) 
    {
        noise_floor_mv_ = ((noise_floor_mv_ * kNoiseFloorExpoFilterPercent) + ReadSignalStrengthMilliVolts() * (100 - kNoiseFloorExpoFilterPercent)) /100;
        noise_floor_last_sample_timestamp_ms_ = timestamp_ms;
    }
    return true;
}

void ADSBee::FlashStatusLED(uint32_t led_on_ms) 
{
    SetStatusLED(true);
    led_on_timestamp_ms_ = millis();
}

uint64_t ADSBee::GetMLAT48MHzCounts(uint16_t num_bits) 
{
    // Объединить счётчик переносов с текущим значением регистра SysTick и замаскировать до 48 бит.
   // Примечание: 24-битное значение SysTick вычитается из UINT_24_MAX, чтобы счёт шёл вверх, а не вниз.
    return (mlat_counter_wraps_ + ((0xFFFFFF - systick_hw->cvr) * MLAT_SYSTEM_CLOCK_RATIO)) & (UINT64_MAX >> (64 - num_bits));
}

uint64_t ADSBee::GetMLAT12MHzCounts(uint16_t num_bits) 
{
    // Piggyback off the higher resolution 48MHz timer function.
    return GetMLAT48MHzCounts(50) >> 2;  // Divide 48MHz counter by 4, widen the mask by 2 bits to compensate.
}

int ADSBee::GetNoiseFloordBm() { return AD8313MilliVoltsTodBm(noise_floor_mv_); }


void ADSBee::OnDemodBegin(uint gpio)
{
    // Read MLAT counter at the beginning to reduce jitter after interrupt.
    uint64_t mlat_48mhz_64bit_counts = GetMLAT48MHzCounts();
    uint16_t sm_index;
    for (sm_index = 0; sm_index < bsp.r1090_num_demod_state_machines; sm_index++) 
    {
        if (config_.demod_pins[sm_index] == gpio) 
        {
            break;
        }
    }
    if (sm_index >= bsp.r1090_num_demod_state_machines)
        return;  // Ignore; wasn't the start of a demod interval for a known SM.
    // Demodulation period is beginning! Store the MLAT counter and RSSI level.
    int16_t packet_rssi_dbm = static_cast<int16_t>(ReadSignalStrengthdBm());
    rx_packet_[sm_index].mlat_48mhz_64bit_counts = mlat_48mhz_64bit_counts;
    rx_packet_[sm_index].sigs_dbm = packet_rssi_dbm;
    rx_packet_[sm_index].sigq_db = packet_rssi_dbm - GetNoiseFloordBm();
}


float ADSBee::measureRSSI()
{
    return ReadSignalStrengthdBm();
}


void ADSBee::OnDemodComplete() 
{

    Aircraft1090 aircraft;

    for (uint16_t sm_index = 0; sm_index < bsp.r1090_num_demod_state_machines; sm_index++) 
    {
        if (!pio_interrupt_get(config_.preamble_detector_pio, sm_index)) 
        {
            continue;
        }
        pio_sm_set_enabled(config_.message_demodulator_pio, message_demodulator_sm_[sm_index], false);
        // RSSI is sampled in OnDemodBegin(), close to the packet reception moment.
        rx_packet_[sm_index].source = sm_index;  // Записываем этот конечный автомат как источник пакета.

        if (!pio_sm_is_rx_fifo_full(config_.message_demodulator_pio, message_demodulator_sm_[sm_index])) 
        {
            // Помещаем любое частично заполненное 32-битное слово в приемный буфер FIFO.
            pio_sm_exec_wait_blocking(config_.message_demodulator_pio, message_demodulator_sm_[sm_index], pio_encode_push(false, true));
        }

        // Очищаем буфер пакетов транспондера.
        memset((void *)rx_packet_[sm_index].buffer, 0x0, Raw1090Packet::kMaxPacketLenWords32);

        // Pull all words out of the RX FIFO.
        volatile uint16_t packet_num_words =  pio_sm_get_rx_fifo_level(config_.message_demodulator_pio, message_demodulator_sm_[sm_index]);
        if (packet_num_words > Raw1090Packet::kMaxPacketLenWords32) 
        {
            // Недопустимая длина пакета; сбросьте всё и попробуйте снова в следующий раз.
            // Включайте эту печать только для отладки! Печать из прерывания приводит к сбою библиотеки USB.
            // CONSOLE_WARNING("ADSBee::OnDemodComplete", "Received a packet with %d 32-bit words, expected maximum of
            // %d.",
            //                 packet_num_words, Raw1090Packet::kExtendedSquitterPacketNumWords32);
            // pio_sm_clear_fifos(config_.message_demodulator_pio, message_demodulator_sm_);
            packet_num_words = Raw1090Packet::kMaxPacketLenWords32;
        }
        // Отслеживаем, что мы попытались что-то демодулировать.
        aircraft_dictionary.Record1090Demod();
        // Создайте Raw1090Packet и поместите его в очередь.
        for (uint16_t i = 0; i < packet_num_words; i++) 
        {
            rx_packet_[sm_index].buffer[i] =   pio_sm_get(config_.message_demodulator_pio, message_demodulator_sm_[sm_index]);
            if (i == packet_num_words - 1) 
            {
                // Маскируем и выравниваем по левому краю конечное слово на основе длины в битах.
                switch (packet_num_words) 
                {
                    case Raw1090Packet::kSquitterPacketNumWords32:
                        aircraft_dictionary.Record1090RawSquitterFrame();
                        rx_packet_[sm_index].buffer[i] = (rx_packet_[sm_index].buffer[i] & 0xFFFFFF) << 8;
                        rx_packet_[sm_index].buffer_len_bits = Raw1090Packet::kSquitterPacketLenBits;
                        raw_1090_packet_queue.Push(rx_packet_[sm_index]);
                        decoder.raw_1090_packet_in_queue.Push(rx_packet_[sm_index]); 
                        break;
                    case Raw1090Packet::kExtendedSquitterPacketNumWords32:
                        aircraft_dictionary.Record1090RawExtendedSquitterFrame();
                        rx_packet_[sm_index].buffer[i] = (rx_packet_[sm_index].buffer[i] & 0xFFFF) << 16;
                        rx_packet_[sm_index].buffer_len_bits = Raw1090Packet::kExtendedSquitterPacketLenBits;
                        raw_1090_packet_queue.Push(rx_packet_[sm_index]);
                        decoder.raw_1090_packet_in_queue.Push(rx_packet_[sm_index]);
                        break;
                    default:
                        break;
                }
            }
        }

        // Очищаем FIFO, выталкивая частичное слово из ISR, не блокируя, если FIFO заполнен (а этого быть не должно).
        pio_sm_exec_wait_blocking(config_.message_demodulator_pio, message_demodulator_sm_[sm_index], pio_encode_push(false, false));
        while (!pio_sm_is_rx_fifo_empty(config_.message_demodulator_pio, message_demodulator_sm_[sm_index])) 
        {
            pio_sm_get(config_.message_demodulator_pio, message_demodulator_sm_[sm_index]);
        }

        // Сбросить конечный автомат демодулятора для ожидания следующего интервала декодирования, затем включить его.
        pio_sm_restart(config_.message_demodulator_pio, message_demodulator_sm_[sm_index]);  // Reset FIFOs, ISRs, etc.
        // Высокомощный демодулятор имеет другой начальный адрес, что обусловлено тем, что индекс его вывода DEMOD
        // отличается. Это имеет значение только для начального ожидания программы, последующие проверки демодулятора выполняются на
        // полном входном регистре GPIO.
        uint demodulator_program_start =
            sm_index == bsp.r1090_high_power_demod_state_machine_index
                ? message_demodulator_offset_ + message_demodulator_offset_high_power_initial_entry
                : message_demodulator_offset_ + message_demodulator_offset_initial_entry;
        pio_sm_exec_wait_blocking(config_.message_demodulator_pio, message_demodulator_sm_[sm_index],
                                  pio_encode_jmp(demodulator_program_start));  // Jump to beginning of program.
        pio_sm_set_enabled(config_.message_demodulator_pio, message_demodulator_sm_[sm_index], true);

        // Выводим детектор преамбулы из состояния ожидания.
        if (sm_index == bsp.r1090_high_power_demod_state_machine_index) 
        {
            // Конечный автомат высокой мощности работает автономно и не требует ожидания завершения других SM. Он
            // обычно активируется одним из чередующихся обновлений конечных автоматов детектора правильно сформированных преамбул, но
            // если это сделать здесь, он активируется немного быстрее и позволяет перехватить последующий пакет высокой мощности, если он
            // поступит быстро.
            pio_sm_exec_wait_blocking(
                config_.preamble_detector_pio, preamble_detector_sm_[sm_index],
                pio_encode_jmp(preamble_detector_offset_ + preamble_detector_offset_waiting_for_first_edge));
        }

        pio_interrupt_clear(config_.preamble_detector_pio, sm_index);
       //!! message_ESP = true;
    }
}

void ADSBee::OnSysTickWrap() { mlat_counter_wraps_ += kMLATWrapCounterIncrement; }

int ADSBee::ReadSignalStrengthMilliVolts()
{
    adc_select_input(config_.rssi_adc_input);
    int rssi_adc_counts = adc_read();
    return rssi_adc_counts * 3300 / 4095;
}

int ADSBee::ReadSignalStrengthdBm() { return AD8313MilliVoltsTodBm(ReadSignalStrengthMilliVolts()); }

