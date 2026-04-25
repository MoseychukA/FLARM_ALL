/*
  Модуль RP2040Bridge.cpp
  Назначение:
  - Встроенный прием и декодирование ADS-B 1090 на ESP32-S3 вместо внешнего RP2040.
  - Захват импульсов с компаратора по GPIO4: новый RMT API на Arduino-ESP32 3.x, быстрый GPIO sampler на Arduino-ESP32 2.x.
  - Декодирование Extended Squitter и запись целей в Container через штатный путь проекта.
*/

#include "RP2040Bridge.h"

#include <Arduino.h>
#include <esp_idf_version.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include <driver/rmt_rx.h>
#else
#include <driver/rmt.h>
#include <freertos/ringbuf.h>
#endif
#include <freertos/queue.h>
#include <esp_task_wdt.h>
#include <driver/gpio.h>
#include <soc/gpio_reg.h>
#include <xtensa/core-macros.h>
#include <math.h>
#include <string.h>

#include "Container.h"
#include "DeviceInfo.h"
#include "packet_decoder.h"
#include "aircraft_dictionary.h"
#include "crc.h"
#include "buffer_utils.h"
#include "EEPROMRF.h"

#ifndef GPIO_ADSB_1090_IN
#define GPIO_ADSB_1090_IN 4
#endif

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#ifndef ADSB_RMT_CHANNEL
#define ADSB_RMT_CHANNEL RMT_CHANNEL_2
#endif

#ifndef ADSB_RMT_MEM_BLOCKS
#define ADSB_RMT_MEM_BLOCKS 1
#endif

#ifndef ADSB_RMT_RINGBUF_BYTES
#define ADSB_RMT_RINGBUF_BYTES 32768
#endif
#endif

#ifndef ADSB_RMT_RESOLUTION_HZ
#define ADSB_RMT_RESOLUTION_HZ 80000000UL
#endif

#ifndef ADSB_RMT_SYMBOLS
#define ADSB_RMT_SYMBOLS 256U
#endif

#ifndef ADSB_HALF_US_TICKS
#define ADSB_HALF_US_TICKS 40U   // 0.5 us / 12.5 ns при clk_div=1
#endif

#ifndef ADSB_TICKS_TOLERANCE
#define ADSB_TICKS_TOLERANCE 18U
#endif

#ifndef ADSB_MAX_HALF_BITS
#define ADSB_MAX_HALF_BITS 512U
#endif

#ifndef ADSB_SAMPLER_HALF_BITS
#define ADSB_SAMPLER_HALF_BITS 280U
#endif

#ifndef ADSB_OS_PER_US
#define ADSB_OS_PER_US 8U      // 8 выборок на 1 мкс = шаг 0.125 мкс
#endif

#ifndef ADSB_OS_PER_HALF
#define ADSB_OS_PER_HALF (ADSB_OS_PER_US / 2U)
#endif

#ifndef ADSB_OS_TOTAL_US
#define ADSB_OS_TOTAL_US 128U  // 8 мкс преамбула + 112 мкс данных + запас
#endif

#ifndef ADSB_OS_SAMPLES
#define ADSB_OS_SAMPLES (ADSB_OS_TOTAL_US * ADSB_OS_PER_US)
#endif

#ifndef ADSB_SAMPLER_WAIT_IDLE_US
#define ADSB_SAMPLER_WAIT_IDLE_US 250U
#endif

#ifndef ADSB_SAMPLER_IDLE_LOW_US
#define ADSB_SAMPLER_IDLE_LOW_US 20U
#endif

#ifndef ADSB_SAMPLER_SAMPLES_PER_HALF
#define ADSB_SAMPLER_SAMPLES_PER_HALF 3U
#endif

#ifndef ADSB_INPUT_INVERT
#define ADSB_INPUT_INVERT 0
#endif

#ifndef ADSB_DIAG_PRINT_MS
#define ADSB_DIAG_PRINT_MS 5000UL
#endif

#ifndef ADSB_PACKET_QUEUE_LEN
#define ADSB_PACKET_QUEUE_LEN 24U
#endif

#ifndef ADSB_INTERNAL_TASK_STACK
#define ADSB_INTERNAL_TASK_STACK 12288U
#endif

#ifndef ADSB_INTERNAL_TASK_PRIO
#define ADSB_INTERNAL_TASK_PRIO 20U
#endif

#ifndef ADSB_PACKET_INDICATOR_PIN
#define ADSB_PACKET_INDICATOR_PIN 2
#endif

#ifndef ADSB_PACKET_INDICATOR_PULSE_US
#define ADSB_PACKET_INDICATOR_PULSE_US 60U
#endif

#ifndef ADSB_THRESHOLD_PWM_PIN
#define ADSB_THRESHOLD_PWM_PIN 48
#endif

#ifndef ADSB_THRESHOLD_PWM_MIN_MV
#define ADSB_THRESHOLD_PWM_MIN_MV 300
#endif

#ifndef ADSB_THRESHOLD_PWM_MAX_MV
#define ADSB_THRESHOLD_PWM_MAX_MV 1400
#endif

#ifndef ADSB_THRESHOLD_PWM_MIN_DUTY
#define ADSB_THRESHOLD_PWM_MIN_DUTY 10
#endif

#ifndef ADSB_THRESHOLD_PWM_MAX_DUTY
#define ADSB_THRESHOLD_PWM_MAX_DUTY 130
#endif

#ifndef ADSB_THRESHOLD_PWM_FREQ
#define ADSB_THRESHOLD_PWM_FREQ 20000
#endif

#ifndef ADSB_THRESHOLD_PWM_CHANNEL
#define ADSB_THRESHOLD_PWM_CHANNEL 7
#endif

#ifndef ADSB_THRESHOLD_PWM_RES_BITS
#define ADSB_THRESHOLD_PWM_RES_BITS 8
#endif

static RP2040BridgeDiag g_diag = {};
static TaskHandle_t g_adsbTaskHandle = nullptr;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static rmt_channel_handle_t g_rmtRxChannel = nullptr;
struct AdsbRmtRxEvent { size_t num_symbols; };
static QueueHandle_t g_rmtDoneQueue = nullptr;
static rmt_symbol_word_t g_rmtRxSymbols[ADSB_RMT_SYMBOLS];
static rmt_receive_config_t g_rmtReceiveConfig = {};
#else
static volatile bool g_legacySamplerMode = true;
#endif
static QueueHandle_t g_ufoQueue = nullptr;
static bool g_ready = false;
static uint32_t g_lastDictUpdateMs = 0;
static uint32_t g_lastDiagPrintMs = 0;
static uint32_t g_lastPreamblePrintMs = 0;
static uint32_t g_lastPreamblePrintCount = 0;
static uint8_t g_streamHalfBits[ADSB_MAX_HALF_BITS] = {0};
static size_t g_streamHalfBitLen = 0;
static PacketDecoder::PacketDecoderConfig g_decoderConfig = [](){ PacketDecoder::PacketDecoderConfig c; c.enable_1090_error_correction = true; c.max_1090_error_correction_num_bits = 1; return c; }();
PacketDecoder decoder(g_decoderConfig);
static AircraftDictionary g_adsbDictionary;

static void printPreambleDetectedOncePerInterval()
{
    const uint32_t nowMs = millis();
    if ((g_lastPreamblePrintCount == 0UL) || ((nowMs - g_lastPreamblePrintMs) >= 1000UL))
    {
        g_lastPreamblePrintMs = nowMs;
        g_lastPreamblePrintCount = g_diag.preambleFound;
        Serial.printf("[ADSB] PREAMBLE: total=%lu off=%u df=%u len=%u phase=%d inv=%u crcClass=%u raw=%lu crc0=%lu crc1=%lu crcbad=%lu dec=%lu cont=%lu calc=%06lX par=%06lX syn=%06lX w=%08lX%08lX%08lX%08lX\r\n",
                      (unsigned long)g_diag.preambleFound,
                      (unsigned)g_diag.lastPreambleOffset,
                      (unsigned)g_diag.lastDf,
                      (unsigned)g_diag.lastFrameLenBits,
                      (int)g_diag.lastPhase,
                      (unsigned)g_diag.lastInvert,
                      (unsigned)g_diag.lastCrcClass,
                      (unsigned long)g_diag.rawFrames,
                      (unsigned long)g_diag.crcOkFrames,
                      (unsigned long)g_diag.crcFixFrames,
                      (unsigned long)g_diag.crcBadFrames,
                      (unsigned long)g_diag.decodedFrames,
                      (unsigned long)g_diag.dictionaryFrames,
                      (unsigned long)g_diag.lastCalcCrc,
                      (unsigned long)g_diag.lastParity,
                      (unsigned long)g_diag.lastSyndrome,
                      (unsigned long)g_diag.lastWord0,
                      (unsigned long)g_diag.lastWord1,
                      (unsigned long)g_diag.lastWord2,
                      (unsigned long)g_diag.lastWord3);
    }
}

static inline uint16_t ticksToHalfBits(uint16_t ticks)
{
    return (uint16_t)((ticks + (ADSB_HALF_US_TICKS / 2U)) / ADSB_HALF_US_TICKS);
}

static inline void pulseDecodedPacketIndicator()
{
    digitalWrite(ADSB_PACKET_INDICATOR_PIN, LOW);
    delayMicroseconds(ADSB_PACKET_INDICATOR_PULSE_US);
    digitalWrite(ADSB_PACKET_INDICATOR_PIN, HIGH);
}

static int clampAdsbThresholdMv(int mv)
{
    if (mv < ADSB_THRESHOLD_PWM_MIN_MV)
    {
        mv = ADSB_THRESHOLD_PWM_MIN_MV;
    }
    if (mv > ADSB_THRESHOLD_PWM_MAX_MV)
    {
        mv = ADSB_THRESHOLD_PWM_MAX_MV;
    }
    return mv;
}

static uint8_t adsbThresholdMvToDuty(int mv)
{
    mv = clampAdsbThresholdMv(mv);
    const long duty = map((long)mv,
                          (long)ADSB_THRESHOLD_PWM_MIN_MV,
                          (long)ADSB_THRESHOLD_PWM_MAX_MV,
                          (long)ADSB_THRESHOLD_PWM_MIN_DUTY,
                          (long)ADSB_THRESHOLD_PWM_MAX_DUTY);
    if (duty < 0L)
    {
        return 0U;
    }
    if (duty > 255L)
    {
        return 255U;
    }
    return (uint8_t)duty;
}

static void applyAdsbThresholdPwm(int mv, bool printLog)
{
    const int clampedMv = clampAdsbThresholdMv(mv);
    const uint8_t duty = adsbThresholdMvToDuty(clampedMv);

    pinMode(ADSB_THRESHOLD_PWM_PIN, OUTPUT);

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    // Arduino-ESP32 3.x: LEDC привязывается напрямую к pin.
    ledcAttach(ADSB_THRESHOLD_PWM_PIN, ADSB_THRESHOLD_PWM_FREQ, ADSB_THRESHOLD_PWM_RES_BITS);
    ledcWrite(ADSB_THRESHOLD_PWM_PIN, duty);
#else
    // Arduino-ESP32 2.x: используется отдельный номер LEDC-канала.
    ledcSetup(ADSB_THRESHOLD_PWM_CHANNEL, ADSB_THRESHOLD_PWM_FREQ, ADSB_THRESHOLD_PWM_RES_BITS);
    ledcAttachPin(ADSB_THRESHOLD_PWM_PIN, ADSB_THRESHOLD_PWM_CHANNEL);
    ledcWrite(ADSB_THRESHOLD_PWM_CHANNEL, duty);
#endif

    ThisAircraft.rp2040_gain = clampedMv;
    if (settings != nullptr)
    {
        settings->threshold_level = clampedMv;
    }

    if (printLog)
    {
        Serial.printf("[ADSB] Threshold PWM GPIO%d: %d mV, duty=%u\r\n",
                      ADSB_THRESHOLD_PWM_PIN, clampedMv, (unsigned)duty);
    }
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
typedef rmt_symbol_word_t adsb_rmt_item_t;
static bool IRAM_ATTR adsbRmtRxDoneCallback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t* edata, void* user_ctx)
{
    (void)channel;
    BaseType_t highTaskWoken = pdFALSE;
    AdsbRmtRxEvent ev = {};
    ev.num_symbols = (edata != nullptr) ? edata->num_symbols : 0U;
    xQueueSendFromISR((QueueHandle_t)user_ctx, &ev, &highTaskWoken);
    return highTaskWoken == pdTRUE;
}
#else
typedef rmt_item32_t adsb_rmt_item_t;
#endif

static bool buildHalfBitWaveform(const adsb_rmt_item_t* items, size_t count, uint8_t* wave, size_t& waveLen)
{
    waveLen = 0;
    if (items == nullptr || wave == nullptr || count == 0U)
    {
        return false;
    }

    for (size_t i = 0; i < count; ++i)
    {
        const uint16_t durs[2] = { items[i].duration0, items[i].duration1 };
        const uint8_t levels[2] = { (uint8_t)items[i].level0, (uint8_t)items[i].level1 };

        for (int part = 0; part < 2; ++part)
        {
            if (durs[part] == 0U)
            {
                continue;
            }
            uint16_t hb = ticksToHalfBits(durs[part]);
            if (hb == 0U)
            {
                hb = 1U;
            }
            while (hb-- > 0U)
            {
                if (waveLen >= ADSB_MAX_HALF_BITS)
                {
                    return true;
                }
                wave[waveLen++] = levels[part] ? 1U : 0U;
            }
        }
    }
    return waveLen > 0U;
}

static bool findPreambleOffset(const uint8_t* wave, size_t waveLen, size_t& offset)
{
    // Схема взята из RP2040 ADSBee: детектор преамбулы PIO работает по
    // 15 half-bit шаблону 101000010100000. Последний ноль не используется
    // для старта демодулятора, но данные ADS-B всё равно начинаются через
    // 16 half-bit = 8 us от начала преамбулы.
    static const uint8_t preambleWellFormed[15] = { 1,0,1,0,0,0,0,1,0,1,0,0,0,0,0 };
    static const uint8_t preambleHighPower[15]  = { 1,1,1,0,0,0,0,1,0,1,0,0,0,0,0 };

    if (wave == nullptr || waveLen < 240U)
    {
        return false;
    }

    const size_t maxOffset = (waveLen > 280U) ? 40U : (waveLen - 240U);
    for (size_t start = 0; start <= maxOffset; ++start)
    {
        uint8_t errWell = 0U;
        uint8_t errHigh = 0U;
        for (size_t i = 0; i < 15U; ++i)
        {
            const uint8_t v = wave[start + i] ? 1U : 0U;
            if (v != preambleWellFormed[i]) ++errWell;
            if (v != preambleHighPower[i]) ++errHigh;
        }

        // RP2040 PIO детектор строгий. На ESP32 оставляем только 1 half-bit
        // допуска для программного sampler; старый допуск 2 давал много ложных
        // преамбул и crcbad без единого корректного кадра.
        if (errWell <= 1U || errHigh <= 1U)
        {
            offset = start;
            return true;
        }
    }
    return false;
}
static inline void writePacketBit(uint32_t* words, uint16_t bitIndex, bool value)
{
    if (!value || words == nullptr || bitIndex >= 112U)
    {
        return;
    }
    const uint16_t wordIndex = bitIndex / 32U;
    const uint16_t bitInWord = 31U - (bitIndex % 32U);
    words[wordIndex] |= (1UL << bitInWord);
}

static uint16_t modeSFrameLenBitsFromDf(uint8_t df)
{
    switch (df)
    {
        case 0U:
        case 4U:
        case 5U:
        case 11U:
            return Raw1090Packet::kSquitterPacketLenBits;
        case 16U:
        case 17U:
        case 18U:
        case 19U:
        case 20U:
        case 21U:
        case 24U:
            return Raw1090Packet::kExtendedSquitterPacketLenBits;
        default:
            return Raw1090Packet::kExtendedSquitterPacketLenBits;
    }
}

static uint32_t calcModeSCrcSyndromeBits(const uint32_t* words, uint16_t lenBits)
{
    if (lenBits == Raw1090Packet::kSquitterPacketLenBits)
    {
        uint8_t rawBytes[Raw1090Packet::kSquitterPacketLenBits / 8U] = {0};
        WordBufferToByteBuffer(words, rawBytes, sizeof(rawBytes));
        return crc24_syndrome(rawBytes, sizeof(rawBytes));
    }
    uint8_t rawBytes[Raw1090Packet::kExtendedSquitterPacketLenBits / 8U] = {0};
    WordBufferToByteBuffer(words, rawBytes, sizeof(rawBytes));
    return crc24_syndrome(rawBytes, sizeof(rawBytes));
}

static uint32_t calcModeSCrcSyndrome112(const uint32_t* words)
{
    return calcModeSCrcSyndromeBits(words, Raw1090Packet::kExtendedSquitterPacketLenBits);
}

static void recordRp2040StyleCrcDiag(const uint32_t* words, uint16_t lenBits)
{
    if (words == nullptr) return;
    const uint16_t lenBytes = lenBits / 8U;
    uint8_t rawBytes[Raw1090Packet::kExtendedSquitterPacketLenBits / 8U] = {0};
    WordBufferToByteBuffer(words, rawBytes, lenBytes);

    // Полностью совпадает с ADSBee/RP2040:
    // calculated_crc = crc24(payload_without_last_3_crc_bytes)
    // parity = последние 24 бита кадра
    // syndrome = calculated_crc ^ parity
    const uint32_t calc = crc24(rawBytes, (uint16_t)(lenBytes - 3U));
    const uint32_t parity = ((uint32_t)rawBytes[lenBytes - 3U] << 16) |
                            ((uint32_t)rawBytes[lenBytes - 2U] << 8) |
                            ((uint32_t)rawBytes[lenBytes - 1U]);

    g_diag.lastCalcCrc = calc & 0x00FFFFFFUL;
    g_diag.lastParity = parity & 0x00FFFFFFUL;
    g_diag.lastSyndrome = (calc ^ parity) & 0x00FFFFFFUL;
    g_diag.lastWord0 = words[0];
    g_diag.lastWord1 = words[1];
    g_diag.lastWord2 = words[2];
    g_diag.lastWord3 = words[3];
}

static inline uint16_t countOsHigh(const uint8_t* wave, size_t waveLen, int32_t start, uint16_t len)
{
    if (wave == nullptr || len == 0U) return 0U;
    uint16_t c = 0U;
    for (uint16_t i = 0U; i < len; ++i)
    {
        const int32_t idx = start + (int32_t)i;
        if (idx >= 0 && (size_t)idx < waveLen && wave[idx]) ++c;
    }
    return c;
}

static bool findPreambleOffsetOs(const uint8_t* wave, size_t waveLen, size_t& offset)
{
    static const uint8_t preambleWellFormed[16] = { 1,0,1,0,0,0,0,1,0,1,0,0,0,0,0,0 };
    static const uint8_t preambleHighPower[16]  = { 1,1,1,0,0,0,0,1,0,1,0,0,0,0,0,0 };
    if (wave == nullptr || waveLen < (16U * ADSB_OS_PER_HALF + 112U * ADSB_OS_PER_US)) return false;
    const size_t minNeeded = 16U * ADSB_OS_PER_HALF + 112U * ADSB_OS_PER_US;
    const size_t maxStart = (waveLen > minNeeded) ? (waveLen - minNeeded) : 0U;
    const size_t searchMax = (maxStart > (ADSB_OS_PER_US * 2U)) ? (ADSB_OS_PER_US * 2U) : maxStart;
    uint16_t bestScore = 0xFFFFU;
    size_t bestStart = 0U;
    bool found = false;
    for (size_t start = 0U; start <= searchMax; ++start)
    {
        for (uint8_t variant = 0U; variant < 2U; ++variant)
        {
            const uint8_t* pat = (variant == 0U) ? preambleWellFormed : preambleHighPower;
            uint16_t score = 0U, goodHigh = 0U, goodLow = 0U;
            for (uint8_t hb = 0U; hb < 16U; ++hb)
            {
                const uint16_t h = countOsHigh(wave, waveLen, (int32_t)(start + (size_t)hb * ADSB_OS_PER_HALF), ADSB_OS_PER_HALF);
                if (pat[hb])
                {
                    if (h >= (ADSB_OS_PER_HALF / 2U)) ++goodHigh;
                    else score += (uint16_t)((ADSB_OS_PER_HALF / 2U) - h + 4U);
                }
                else
                {
                    if (h <= 1U) ++goodLow;
                    else score += (uint16_t)(h + 1U);
                }
            }
            if (goodHigh >= 4U && goodLow >= 9U && score < bestScore)
            {
                bestScore = score; bestStart = start; found = true;
            }
        }
    }
    if (found) { offset = bestStart; return true; }
    return false;
}

static bool decodeOversampledWaveToRawPacket(const uint8_t* wave, size_t waveLen, Raw1090Packet& outPacket)
{
    size_t preambleOffset = 0U;
    if (!findPreambleOffsetOs(wave, waveLen, preambleOffset)) return false;
    ++g_diag.preambleFound;
    g_diag.lastPreambleOffset = (uint8_t)((preambleOffset < 255U) ? preambleOffset : 255U);
    uint32_t bestWords[Raw1090Packet::kMaxPacketLenWords32] = {0, 0, 0, 0};
    uint8_t bestDf = 0U, bestCrcClass = 3U;
    uint16_t bestFrameLenBits = Raw1090Packet::kExtendedSquitterPacketLenBits;
    uint16_t bestScore = 0xFFFFU;
    int16_t bestFixBit = -1;
    int8_t bestPhase = 0;
    uint8_t bestInvert = 0U;
    bool haveCandidate = false;
    const int8_t phaseCandidates[] = { 0, -1, 1, -2, 2, -3, 3, -4, 4, -5, 5, -6, 6, -7, 7, -8, 8 };
    for (uint8_t invert = 0U; invert < 2U; ++invert)
    {
        for (uint8_t phaseIndex = 0U; phaseIndex < (uint8_t)(sizeof(phaseCandidates) / sizeof(phaseCandidates[0])); ++phaseIndex)
        {
            const int32_t dataStart = (int32_t)preambleOffset + (int32_t)(8U * ADSB_OS_PER_US) + phaseCandidates[phaseIndex];
            if (dataStart < 0) continue;
            if ((size_t)(dataStart + (int32_t)(112U * ADSB_OS_PER_US)) > waveLen) { ++g_diag.rawShortFrames; continue; }
            uint32_t words[Raw1090Packet::kMaxPacketLenWords32] = {0, 0, 0, 0};
            uint16_t ambiguous = 0U, energy = 0U;
            for (uint16_t bit = 0U; bit < 112U; ++bit)
            {
                const int32_t bitStart = dataStart + (int32_t)bit * (int32_t)ADSB_OS_PER_US;
                uint16_t first = countOsHigh(wave, waveLen, bitStart, ADSB_OS_PER_HALF);
                uint16_t second = countOsHigh(wave, waveLen, bitStart + (int32_t)ADSB_OS_PER_HALF, ADSB_OS_PER_HALF);
                if (invert) { first = ADSB_OS_PER_HALF - first; second = ADSB_OS_PER_HALF - second; }
                energy += first + second;
                if (first > second) { writePacketBit(words, bit, true); if ((first - second) <= 1U) ++ambiguous; }
                else if (second > first) { writePacketBit(words, bit, false); if ((second - first) <= 1U) ++ambiguous; }
                else ++ambiguous;
            }
            const uint8_t df = (uint8_t)((words[0] >> 27) & 0x1FU);
            const uint16_t frameLenBits = modeSFrameLenBitsFromDf(df);
            const bool dfIsUseful = (df == 0U || df == 4U || df == 5U || df == 11U || df == 16U || df == 17U || df == 18U || df == 19U || df == 20U || df == 21U || df == 24U);
            int16_t fixBit = -1;
            uint8_t crcClass = 3U;
            if (dfIsUseful)
            {
                const uint32_t syndrome = calcModeSCrcSyndromeBits(words, frameLenBits);
                if (syndrome == 0U) crcClass = 0U;
                else { fixBit = crc24_find_single_bit_error(syndrome, frameLenBits); crcClass = (fixBit >= 0) ? 1U : 2U; }
            }
            uint16_t score = (uint16_t)(crcClass * 1000U + ambiguous * 3U);
            if (!dfIsUseful) score = (uint16_t)(score + 800U);
            if (invert) score = (uint16_t)(score + 30U);
            if (energy < 80U || energy > 650U) score = (uint16_t)(score + 200U);
            score = (uint16_t)(score + phaseIndex);
            if (!haveCandidate || score < bestScore)
            {
                memcpy(bestWords, words, sizeof(bestWords)); bestScore = score; bestDf = df; bestFrameLenBits = frameLenBits; bestFixBit = fixBit; bestCrcClass = crcClass; bestPhase = phaseCandidates[phaseIndex]; bestInvert = invert; haveCandidate = true;
            }
        }
    }
    if (!haveCandidate) return false;
    g_diag.lastDf = bestDf;
    g_diag.lastFrameLenBits = bestFrameLenBits;
    g_diag.lastCrcClass = bestCrcClass;
    g_diag.lastPhase = bestPhase;
    g_diag.lastInvert = bestInvert;
    recordRp2040StyleCrcDiag(bestWords, bestFrameLenBits);
    printPreambleDetectedOncePerInterval();
    if (bestCrcClass == 0U) ++g_diag.crcOkFrames;
    else if (bestCrcClass == 1U && bestFixBit >= 0) { flip_bit(bestWords, (uint16_t)bestFixBit); ++g_diag.crcFixFrames; }
    else {
        ++g_diag.crcBadFrames;
        ++g_diag.rawPairErrors;
        // В ESP32 sampler нет лишнего PIO-бита, поэтому НЕ применяем >>= 1 к последнему слову.
        // Как в ADSBee, сырой кадр передаётся дальше в PacketDecoder без предварительного отбрасывания по CRC.
    }
    memset(outPacket.buffer, 0, sizeof(outPacket.buffer));
    memcpy(outPacket.buffer, bestWords, sizeof(bestWords));
    outPacket.buffer_len_bits = bestFrameLenBits;
    ++g_diag.rawFrames;
    return true;
}

static bool decodeWaveToRawPacket(const uint8_t* wave, size_t waveLen, Raw1090Packet& outPacket)
{
    size_t preambleOffset = 0;
    if (!findPreambleOffset(wave, waveLen, preambleOffset))
    {
        return false;
    }

    ++g_diag.preambleFound;
    g_diag.lastPreambleOffset = (uint8_t)((preambleOffset < 255U) ? preambleOffset : 255U);

    // Выбираем кандидата по CRC Mode-S, а не только по DF/количеству пар.
    // Это важно для GPIO sampler: raw кадры уже собираются, но без CRC-фильтра
    // легко выбрать неверную фазу и получить ok=0.
    const int8_t phaseCandidates[] = { 0, -1, 1, -2, 2, -3, 3, -4, 4 };

    uint32_t bestWords[Raw1090Packet::kMaxPacketLenWords32] = {0, 0, 0, 0};
    uint8_t bestDf = 0U;
    uint16_t bestScore = 0xFFFFU;
    int16_t bestFixBit = -1;
    uint8_t bestCrcClass = 3U; // 0=CRC OK, 1=один бит исправим, 2=DF полезный, 3=мусор
    bool haveCandidate = false;

    for (uint8_t invert = 0U; invert < 2U; ++invert)
    {
        for (uint8_t phaseIndex = 0; phaseIndex < (uint8_t)(sizeof(phaseCandidates) / sizeof(phaseCandidates[0])); ++phaseIndex)
        {
            const int32_t dataStartSigned = (int32_t)preambleOffset + 16 + phaseCandidates[phaseIndex];
            if (dataStartSigned < 0)
            {
                continue;
            }
            const size_t dataStart = (size_t)dataStartSigned;
            if ((dataStart + 224U) > waveLen)
            {
                ++g_diag.rawShortFrames;
                continue;
            }

            uint32_t words[Raw1090Packet::kMaxPacketLenWords32] = {0, 0, 0, 0};
            uint16_t ambiguous = 0U;

            for (uint16_t i = 0; i < 112U; ++i)
            {
                uint8_t a = wave[dataStart + (size_t)i * 2U];
                uint8_t b = wave[dataStart + (size_t)i * 2U + 1U];
                if (invert)
                {
                    a ^= 1U;
                    b ^= 1U;
                }

                if (a > b)
                {
                    writePacketBit(words, i, true);
                }
                else if (b > a)
                {
                    writePacketBit(words, i, false);
                }
                else
                {
                    ++ambiguous;
                    if (a != 0U)
                    {
                        writePacketBit(words, i, true);
                    }
                }
            }

            const uint8_t df = (uint8_t)((words[0] >> 27) & 0x1FU);
            const bool dfIsUseful = (df == 17U || df == 18U || df == 19U || df == 20U || df == 21U);

            int16_t fixBit = -1;
            uint8_t crcClass = 3U;
            if (dfIsUseful)
            {
                const uint32_t syndrome = calcModeSCrcSyndrome112(words);
                if (syndrome == 0U)
                {
                    crcClass = 0U;
                }
                else
                {
                    fixBit = crc24_find_single_bit_error(syndrome, Raw1090Packet::kExtendedSquitterPacketLenBits);
                    if (fixBit > 0)
                    {
                        crcClass = 1U;
                    }
                    else
                    {
                        crcClass = 2U;
                    }
                }
            }

            uint16_t score = (uint16_t)(ambiguous * 2U);
            score = (uint16_t)(score + (uint16_t)crcClass * 1000U);
            if (!dfIsUseful)
            {
                score = (uint16_t)(score + 800U);
            }
            if (invert)
            {
                score = (uint16_t)(score + 20U);
            }
            score = (uint16_t)(score + (uint16_t)(phaseIndex & 0x03U));

            if (!haveCandidate || score < bestScore)
            {
                memcpy(bestWords, words, sizeof(bestWords));
                bestScore = score;
                bestDf = df;
                bestFixBit = fixBit;
                bestCrcClass = crcClass;
                haveCandidate = true;
            }
        }
    }

    if (!haveCandidate)
    {
        return false;
    }

    g_diag.lastDf = bestDf;
    recordRp2040StyleCrcDiag(bestWords, Raw1090Packet::kExtendedSquitterPacketLenBits);

    if (bestCrcClass == 0U)
    {
        ++g_diag.crcOkFrames;
    }
    else if (bestCrcClass == 1U && bestFixBit > 0)
    {
        flip_bit(bestWords, (uint16_t)bestFixBit);
        ++g_diag.crcFixFrames;
    }
    else
    {
        ++g_diag.crcBadFrames;
        ++g_diag.rawPairErrors;
        return false;
    }

    memset(outPacket.buffer, 0, sizeof(outPacket.buffer));
    memcpy(outPacket.buffer, bestWords, sizeof(bestWords));
    outPacket.buffer_len_bits = Raw1090Packet::kExtendedSquitterPacketLenBits;

    g_diag.rawFrames++;
    return true;
}
static uint8_t mapCategoryToAircraftType(const Aircraft1090& ac)
{
    switch (ac.category)
    {
        case Aircraft1090::kCategoryGliderSailplane: return AIRCRAFT_TYPE_GLIDER;
        case Aircraft1090::kCategoryRotorcraft: return AIRCRAFT_TYPE_HELICOPTER;
        case Aircraft1090::kCategoryParachutistSkydiver: return AIRCRAFT_TYPE_PARACHUTE;
        case Aircraft1090::kCategoryUltralightHangGliderParaglider: return AIRCRAFT_TYPE_PARAGLIDER;
        case Aircraft1090::kCategoryUnmannedAerialVehicle: return AIRCRAFT_TYPE_UAV;
        case Aircraft1090::kCategoryLight:
        case Aircraft1090::kCategoryMedium1:
        case Aircraft1090::kCategoryMedium2:
        case Aircraft1090::kCategoryHighVortexAircraft:
        case Aircraft1090::kCategoryHeavy:
        case Aircraft1090::kCategoryHighPerformance:
            return AIRCRAFT_TYPE_JET;
        default:
            return AIRCRAFT_TYPE_POWERED;
    }
}

static void aircraftToUfo(const Aircraft1090& ac, ufo_t& dst)
{
    dst = EmptyFO;
    dst.valid = true;
    dst.protocol = 0xAD;
    dst.addr = ac.icao_address & 0x00FFFFFFUL;
    dst.addr_type = ADDR_TYPE_ICAO;
    dst.signal_source = TRAFFIC_SOURCE_ADSB_DUMP1090;
    dst.aircraft_type = mapCategoryToAircraftType(ac);

    const time_t nowSec = (time_t)(millis() / 1000UL);
    dst.timestamp = nowSec;
    dst.timemsg = nowSec;
    dst.seen = nowSec;

    if (ac.callsign[0] != '\0' && ac.callsign[0] != '?')
    {
        memset(dst.callsign, 0, sizeof(dst.callsign));
        const size_t copyLen = (sizeof(dst.callsign) < (size_t)8U) ? sizeof(dst.callsign) : (size_t)8U;
        memcpy(dst.callsign, ac.callsign, copyLen);
    }

    dst.squawk = (int)ac.squawk;

    if (ac.HasBitFlag(Aircraft1090::kBitFlagPositionValid))
    {
        dst.latitude = ac.latitude_deg;
        dst.longitude = ac.longitude_deg;
    }

    if (ac.HasBitFlag(Aircraft1090::kBitFlagBaroAltitudeValid))
    {
        dst.altitude = (float)ac.baro_altitude_ft * 0.3048f;
        dst.pressure_altitude = dst.altitude;
    }
    else if (ac.HasBitFlag(Aircraft1090::kBitFlagGNSSAltitudeValid))
    {
        dst.altitude = (float)ac.gnss_altitude_ft * 0.3048f;
        dst.pressure_altitude = dst.altitude;
    }

    if (ac.HasBitFlag(Aircraft1090::kBitFlagDirectionValid))
    {
        dst.course = ac.direction_deg;
    }

    if (ac.HasBitFlag(Aircraft1090::kBitFlagHorizontalVelocityValid))
    {
        dst.speed = ac.velocity_kts;
    }

    if (ac.HasBitFlag(Aircraft1090::kBitFlagVerticalVelocityValid))
    {
        dst.vert_rate = ac.vertical_rate_fpm;
        dst.vs = (float)ac.vertical_rate_fpm;
    }

    dst.rssi_rp2040 = (int8_t)constrain((int)ac.last_message_signal_strength_dbm, -128, 127);
    dst.rssi = dst.rssi_rp2040;
}

static void queueAircraftToMainLoop(const Aircraft1090& ac)
{
    if (g_ufoQueue == nullptr)
    {
        return;
    }

    ufo_t msg = {};
    aircraftToUfo(ac, msg);
    if (xQueueSend(g_ufoQueue, &msg, 0) != pdTRUE)
    {
        ufo_t drop = {};
        (void)xQueueReceive(g_ufoQueue, &drop, 0);
        (void)xQueueSend(g_ufoQueue, &msg, 0);
    }
}

static void ingestDecodedPackets()
{
    Decoded1090Packet decoded;
    while (decoder.decoded_1090_packet_out_queue.Pop(decoded))
    {
        ++g_diag.decodedFrames;
        const uint32_t icao = decoded.GetICAOAddress() & 0x00FFFFFFUL;
        if (!g_adsbDictionary.IngestDecoded1090Packet(decoded))
        {
            continue;
        }

        Aircraft1090* ac = g_adsbDictionary.GetAircraftPtr(icao);
        if (ac == nullptr)
        {
            continue;
        }
        queueAircraftToMainLoop(*ac);
        ++g_diag.dictionaryFrames;
        ++g_diag.packetsAccepted;
        pulseDecodedPacketIndicator();
        g_diag.lastPacketMs = millis();
        g_diag.lastAddress = icao;
        g_diag.lastRssi = (int)ac->last_message_signal_strength_dbm;
    }
}

static void submitRaw1090Packet(const Raw1090Packet& rawIn)
{
    Raw1090Packet raw = rawIn;
    raw.source = 0;
    raw.sigs_dbm = 0;
    raw.sigq_db = 0;
    raw.mlat_48mhz_64bit_counts = 0;

    ++g_diag.packetsReceived;
    if (!decoder.raw_1090_packet_in_queue.Push(raw))
    {
        ++g_diag.overflowCount;
        return;
    }

    decoder.UpdateDecoderLoop();
    ingestDecodedPackets();
}

static void trimStreamHalfBits(size_t removeCount)
{
    if (removeCount == 0U || g_streamHalfBitLen == 0U)
    {
        return;
    }
    if (removeCount >= g_streamHalfBitLen)
    {
        g_streamHalfBitLen = 0U;
        return;
    }
    memmove(g_streamHalfBits, g_streamHalfBits + removeCount, g_streamHalfBitLen - removeCount);
    g_streamHalfBitLen -= removeCount;
}

static bool tryDecodeStreamPacket()
{
    if (g_streamHalfBitLen < 240U)
    {
        return false;
    }

    size_t preambleOffset = 0;
    if (!findPreambleOffset(g_streamHalfBits, g_streamHalfBitLen, preambleOffset))
    {
        // Оставляем хвост, достаточный для преамбулы на границе следующего RMT-фрагмента.
        if (g_streamHalfBitLen > 48U)
        {
            trimStreamHalfBits(g_streamHalfBitLen - 48U);
        }
        return false;
    }

    if ((preambleOffset + 240U) > g_streamHalfBitLen)
    {
        if (preambleOffset > 0U)
        {
            trimStreamHalfBits(preambleOffset);
        }
        return false;
    }

    Raw1090Packet raw;
    if (!decodeWaveToRawPacket(g_streamHalfBits + preambleOffset, g_streamHalfBitLen - preambleOffset, raw))
    {
        trimStreamHalfBits(preambleOffset + 1U);
        ++g_diag.packetsRejected;
        return false;
    }

    submitRaw1090Packet(raw);
    trimStreamHalfBits(preambleOffset + 240U);
    return true;
}

static void appendHalfBitToStream(uint8_t level)
{
    if (g_streamHalfBitLen >= ADSB_MAX_HALF_BITS)
    {
        trimStreamHalfBits(g_streamHalfBitLen - 48U);
    }
    g_streamHalfBits[g_streamHalfBitLen++] = level ? 1U : 0U;
}

static void processRmtItems(const adsb_rmt_item_t* items, size_t count)
{
    if (items == nullptr || count == 0U)
    {
        ++g_diag.packetsRejected;
        return;
    }

    for (size_t i = 0; i < count; ++i)
    {
        const uint16_t durs[2] = { items[i].duration0, items[i].duration1 };
        const uint8_t levels[2] = { (uint8_t)items[i].level0, (uint8_t)items[i].level1 };

        for (int part = 0; part < 2; ++part)
        {
            if (durs[part] == 0U)
            {
                continue;
            }

            uint16_t hb = ticksToHalfBits(durs[part]);
            if (hb == 0U)
            {
                hb = 1U;
            }
            if (hb > 64U)
            {
                // Длинная пауза между кадрами. Сбрасываем поток, чтобы не склеивать разные сообщения.
                g_streamHalfBitLen = 0U;
                continue;
            }

            while (hb-- > 0U)
            {
                appendHalfBitToStream(levels[part]);
                while (tryDecodeStreamPacket())
                {
                    ;
                }
            }
        }
    }

    if (g_streamHalfBitLen > 400U)
    {
        trimStreamHalfBits(g_streamHalfBitLen - 240U);
    }
}

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
static inline uint32_t adsbCyclesPerHalfBit()
{
    uint32_t mhz = getCpuFrequencyMhz();
    if (mhz == 0U) mhz = 240U;
    return (mhz / 2U); // 0.5 us
}

static inline uint8_t adsbReadInputFast()
{
#if GPIO_ADSB_1090_IN < 32
    uint8_t v = (REG_READ(GPIO_IN_REG) & (1UL << GPIO_ADSB_1090_IN)) ? 1U : 0U;
#else
    uint8_t v = (uint8_t)gpio_get_level((gpio_num_t)GPIO_ADSB_1090_IN);
#endif
#if ADSB_INPUT_INVERT
    v ^= 1U;
#endif
    return v;
}

static bool captureAdsbSamplerBurst(uint8_t* wave, size_t& waveLen)
{
    waveLen = 0U;
    if (wave == nullptr)
    {
        return false;
    }

    const uint32_t waitStartUs = micros();

    // ВАЖНО:
    // Раньше sampler запускался от любого следующего фронта. На реальном
    // потоке 1090 это часто был фронт внутри уже идущего пакета или шумовая
    // пачка, поэтому преамбулы находились, но CRC всегда был плохой.
    //
    // Теперь сначала требуем устойчивую паузу LOW перед стартом пакета.
    // Только после такой паузы следующий HIGH считается кандидатом на начало
    // преамбулы ADS-B.
    bool idleStarted = false;
    uint32_t idleStartUs = waitStartUs;

    for (;;)
    {
        const uint8_t v = adsbReadInputFast();
        const uint32_t nowUs = micros();

        if (v == 0U)
        {
            if (!idleStarted)
            {
                idleStarted = true;
                idleStartUs = nowUs;
            }

            if ((uint32_t)(nowUs - idleStartUs) >= ADSB_SAMPLER_IDLE_LOW_US)
            {
                break;
            }
        }
        else
        {
            idleStarted = false;
            idleStartUs = nowUs;
        }

        if ((uint32_t)(nowUs - waitStartUs) > ADSB_SAMPLER_WAIT_IDLE_US)
        {
            return false;
        }
    }

    // Ждём первый активный фронт после подтверждённой паузы LOW.
    const uint32_t edgeWaitStartUs = micros();
    uint32_t edgeCycle = 0U;
    for (;;)
    {
        if (adsbReadInputFast() != 0U)
        {
            // Фиксируем CCOUNT сразу на обнаруженном фронте.
            // В версии 33 base брался позже, после служебного кода,
            // что сдвигало выборку на микросекунды и давало только crcbad.
            edgeCycle = XTHAL_GET_CCOUNT();
            break;
        }
        if ((uint32_t)(micros() - edgeWaitStartUs) > ADSB_SAMPLER_WAIT_IDLE_US)
        {
            return false;
        }
    }

    ++g_diag.samplerBursts;

    uint32_t mhz = getCpuFrequencyMhz();
    if (mhz == 0U) mhz = 240U;
    const uint32_t stepCycles = mhz / ADSB_OS_PER_US; // 0.125 us при ADSB_OS_PER_US=8 и CPU 240 MHz

    // Снимаем oversampling-окно около 128 us. Это ближе к ADSBee PIO.
    portDISABLE_INTERRUPTS();

    const uint32_t base = edgeCycle;
    uint8_t prev = 1U;
    uint32_t edges = 0U;

    for (size_t i = 0; i < ADSB_OS_SAMPLES; ++i)
    {
        const uint32_t t = base + (uint32_t)i * stepCycles;
        while ((int32_t)(XTHAL_GET_CCOUNT() - t) < 0) { ; }
        const uint8_t v = adsbReadInputFast();
        wave[i] = v ? 1U : 0U;
        if (i > 0U && wave[i] != prev) { ++edges; }
        prev = wave[i];
    }

    portENABLE_INTERRUPTS();

    g_diag.samplerEdges += edges;
    waveLen = ADSB_OS_SAMPLES;
    return true;
}
#endif

static void adsbTask(void* param)
{
    (void)param;
    esp_task_wdt_add(NULL);
    g_diag.taskRunning = true;

    for (;;)
    {
        esp_task_wdt_reset();

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        AdsbRmtRxEvent ev = {};
        if (g_rmtDoneQueue != nullptr && xQueueReceive(g_rmtDoneQueue, &ev, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            const size_t count = ev.num_symbols;
            if (count > 0U)
            {
                g_diag.bytesReceived += (uint32_t)(count * sizeof(adsb_rmt_item_t));
                g_diag.lastPacketSize = (uint8_t)((count < (size_t)255U) ? count : (size_t)255U);
                processRmtItems(g_rmtRxSymbols, count);
            }
            if (g_rmtRxChannel != nullptr)
            {
                esp_err_t rxErr = rmt_receive(g_rmtRxChannel, g_rmtRxSymbols, sizeof(g_rmtRxSymbols), &g_rmtReceiveConfig);
                if (rxErr != ESP_OK)
                {
                    ++g_diag.overflowCount;
                }
            }
        }
#else
        uint8_t samplerWave[ADSB_OS_SAMPLES];
        size_t samplerLen = 0U;
        if (captureAdsbSamplerBurst(samplerWave, samplerLen))
        {
            g_diag.bytesReceived += (uint32_t)samplerLen;
            g_diag.lastPacketSize = (uint8_t)((samplerLen < (size_t)255U) ? samplerLen : (size_t)255U);
            Raw1090Packet raw;
            if (decodeOversampledWaveToRawPacket(samplerWave, samplerLen, raw))
            {
                submitRaw1090Packet(raw);
            }
            else
            {
                ++g_diag.packetsRejected;
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
#endif

        const uint32_t nowMs = millis();
        if ((nowMs - g_lastDictUpdateMs) >= 1000UL)
        {
            g_adsbDictionary.Update(nowMs);
            g_lastDictUpdateMs = nowMs;
        }

        if ((nowMs - g_lastDiagPrintMs) >= ADSB_DIAG_PRINT_MS)
        {
            g_lastDiagPrintMs = nowMs;
            Serial.printf("[ADSB] diag bursts=%lu edges=%lu pre=%lu raw=%lu crc0=%lu crc1=%lu crcbad=%lu dec=%lu cont=%lu pair=%lu short=%lu ok=%lu rej=%lu df=%u len=%u cls=%u ph=%d inv=%u off=%u calc=%06lX par=%06lX syn=%06lX last=%06lX\r\n",
                          (unsigned long)g_diag.samplerBursts,
                          (unsigned long)g_diag.samplerEdges,
                          (unsigned long)g_diag.preambleFound,
                          (unsigned long)g_diag.rawFrames,
                          (unsigned long)g_diag.crcOkFrames,
                          (unsigned long)g_diag.crcFixFrames,
                          (unsigned long)g_diag.crcBadFrames,
                          (unsigned long)g_diag.decodedFrames,
                          (unsigned long)g_diag.dictionaryFrames,
                          (unsigned long)g_diag.rawPairErrors,
                          (unsigned long)g_diag.rawShortFrames,
                          (unsigned long)g_diag.packetsAccepted,
                          (unsigned long)g_diag.packetsRejected,
                          (unsigned)g_diag.lastDf,
                          (unsigned)g_diag.lastFrameLenBits,
                          (unsigned)g_diag.lastCrcClass,
                          (int)g_diag.lastPhase,
                          (unsigned)g_diag.lastInvert,
                          (unsigned)g_diag.lastPreambleOffset,
                          (unsigned long)g_diag.lastCalcCrc,
                          (unsigned long)g_diag.lastParity,
                          (unsigned long)g_diag.lastSyndrome,
                          (unsigned long)g_diag.lastAddress);
        }
    }
}

void RP2040Bridge_setGainValues(const uint16_t* data, uint8_t count)
{
    (void)data;
    (void)count;
}

void RP2040Bridge_sendGainNow()
{
    const int mv = settings ? settings->threshold_level : 910;
    applyAdsbThresholdPwm(mv, true);
    ++g_diag.gainPacketsSent;
}

void RP2040Bridge_sendGainSingle(uint16_t gain)
{
    applyAdsbThresholdPwm((int)gain, true);
    ++g_diag.gainPacketsSent;
}

bool unpack_ToDUMP1090(const ToDUMP1090_RAW* inRaw, ToDUMP1090* packet)
{
    if (inRaw == nullptr || packet == nullptr)
    {
        return false;
    }
    memset(packet, 0, sizeof(*packet));
    return false;
}

bool RP2040Bridge_processPacket(const uint8_t* data, size_t len)
{
    (void)data;
    (void)len;
    return false;
}

void receiveRP2040(void* param)
{
    adsbTask(param);
}

void RP2040Bridge_setup()
{
    memset(&g_diag, 0, sizeof(g_diag));
    g_diag.ready = false;
    g_diag.restartCount++;

    decoder.Init();
    g_adsbDictionary.Init();

    pinMode(ADSB_PACKET_INDICATOR_PIN, OUTPUT);
    digitalWrite(ADSB_PACKET_INDICATOR_PIN, HIGH);

    pinMode(GPIO_ADSB_1090_IN, INPUT);
    applyAdsbThresholdPwm(settings ? settings->threshold_level : 910, true);

    if (g_ufoQueue == nullptr)
    {
        g_ufoQueue = xQueueCreate(ADSB_PACKET_QUEUE_LEN, sizeof(ufo_t));
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    if (g_rmtDoneQueue == nullptr)
    {
        g_rmtDoneQueue = xQueueCreate(4, sizeof(AdsbRmtRxEvent));
    }
    if (g_rmtDoneQueue == nullptr)
    {
        Serial.println(F("[ADSB] RMT event queue error"));
        return;
    }

    if (g_rmtRxChannel == nullptr)
    {
        rmt_rx_channel_config_t rxConfig = {};
        rxConfig.gpio_num = (gpio_num_t)GPIO_ADSB_1090_IN;
        rxConfig.clk_src = RMT_CLK_SRC_DEFAULT;
        rxConfig.resolution_hz = ADSB_RMT_RESOLUTION_HZ;
        rxConfig.mem_block_symbols = ADSB_RMT_SYMBOLS;
        rxConfig.flags.invert_in = false;
        rxConfig.flags.with_dma = false;

        esp_err_t err = rmt_new_rx_channel(&rxConfig, &g_rmtRxChannel);
        if (err != ESP_OK || g_rmtRxChannel == nullptr)
        {
            Serial.printf("[ADSB] RMT new RX channel error: %d\r\n", (int)err);
            return;
        }

        rmt_rx_event_callbacks_t callbacks = {};
        callbacks.on_recv_done = adsbRmtRxDoneCallback;
        err = rmt_rx_register_event_callbacks(g_rmtRxChannel, &callbacks, g_rmtDoneQueue);
        if (err != ESP_OK)
        {
            Serial.printf("[ADSB] RMT callback error: %d\r\n", (int)err);
            return;
        }

        err = rmt_enable(g_rmtRxChannel);
        if (err != ESP_OK)
        {
            Serial.printf("[ADSB] RMT enable error: %d\r\n", (int)err);
            return;
        }
    }

    g_rmtReceiveConfig = {};
    g_rmtReceiveConfig.signal_range_min_ns = 50;       // отсечь очень короткий шум
    g_rmtReceiveConfig.signal_range_max_ns = 20000;    // пауза >20 us завершает пачку

    esp_err_t rxErr = rmt_receive(g_rmtRxChannel, g_rmtRxSymbols, sizeof(g_rmtRxSymbols), &g_rmtReceiveConfig);
    if (rxErr != ESP_OK)
    {
        Serial.printf("[ADSB] RMT receive start error: %d\r\n", (int)rxErr);
        return;
    }

    Serial.printf("[ADSB] RMT RX OK GPIO%d, resolution=%lu Hz\r\n", GPIO_ADSB_1090_IN, (unsigned long)ADSB_RMT_RESOLUTION_HZ);
#else
    // Arduino-ESP32 2.x / ESP-IDF 4.x: legacy RMT RX на ESP32-S3 запускается,
    // но переполняется на реальном потоке 1090. Используем быстрый GPIO sampler на core 0.
    gpio_set_direction((gpio_num_t)GPIO_ADSB_1090_IN, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)GPIO_ADSB_1090_IN, GPIO_FLOATING);
    g_legacySamplerMode = true;
    Serial.printf("[ADSB] GPIO oversampler RX OK GPIO%d, step=0.125us, idle=%uus, ADSBee-window, no-trim, crc-rp2040-diag, core=0\r\n", GPIO_ADSB_1090_IN, (unsigned)ADSB_SAMPLER_IDLE_LOW_US);
#endif

    if (g_adsbTaskHandle == nullptr)
    {
        xTaskCreatePinnedToCore(receiveRP2040, "ADSB1090", ADSB_INTERNAL_TASK_STACK, nullptr,
                                ADSB_INTERNAL_TASK_PRIO, &g_adsbTaskHandle, 0);
    }

    g_ready = true;
    g_diag.ready = true;
    Serial.println(F("=== SERIAL MODE: ADSB INTERNAL ==="));
}

void RP2040Bridge_loop()
{
    if (!g_ready || g_ufoQueue == nullptr)
    {
        return;
    }

    ufo_t decoded = {};
    while (xQueueReceive(g_ufoQueue, &decoded, 0) == pdTRUE)
    {
        fo = decoded;
        Traffic_Update(&fo);
        Traffic_Add(&fo);
    }
}

bool RP2040Bridge_isReady()
{
    return g_ready;
}

bool RP2040Bridge_getDiag(RP2040BridgeDiag& outDiag)
{
    outDiag = g_diag;
    return g_ready;
}
