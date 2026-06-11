#include "ADSBReceiverESP32.h"

#include <esp_task_wdt.h>
#include <math.h>
#include <soc/gpio_reg.h>
#include <soc/soc.h>
#include <string.h>
#include <xtensa/core-macros.h>

#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

#if ADSB_ESP32_DMA_CAPTURE && __has_include(<driver/i2s.h>)
#include <driver/i2s.h>
#include <esp_intr_alloc.h>
#define ADSB_ESP32_HAS_I2S_DMA 1
#else
#define ADSB_ESP32_HAS_I2S_DMA 0
#endif

#include "Container.h"
#include "EEPROMRF.h"
#include "TrafficTypes.h"

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

namespace
{
    constexpr size_t kMaxSlots = 260;
    constexpr size_t kMaxAircraft = 48;
    constexpr uint32_t kCPRMaxPairAgeMs = 30000UL;
    constexpr uint16_t kPreambleSlots = 16;
    constexpr uint16_t kLongFrameDataSlots = 224;
    constexpr uint16_t kFullSampleSlots = kPreambleSlots + kLongFrameDataSlots;
    constexpr uint8_t kPhaseCount = 3;
    constexpr uint8_t kPhaseCenter = 1;
    constexpr uint8_t kPhaseSoft = 3;
    constexpr uint32_t kDmaBitRateHz = ADSB_ESP32_DMA_BIT_RATE_HZ;
    constexpr uint8_t kDmaSamplesPerHalfSlot = (kDmaBitRateHz / 2000000UL);
    constexpr size_t kDmaReadWords = 512;
    constexpr size_t kDmaMaxCandidatesPerChunk = 24;
    constexpr size_t kMaxPulseIntervals = 180;
    constexpr uint32_t kPollSliceUs = 2500UL;
    constexpr uint32_t kThresholdPwmFreqHz = 20000UL;
    constexpr uint8_t kThresholdPwmResolutionBits = 10;
    constexpr uint16_t kThresholdPwmMaxDuty = (1U << kThresholdPwmResolutionBits) - 1U;
    constexpr int32_t kThresholdMinMv = 400;
    constexpr int32_t kThresholdMaxMv = 1500;
    constexpr uint32_t kThresholdPwmHighMv = 3300UL;
    constexpr uint32_t kThresholdAmpRgOhm = 1000UL;
    constexpr uint32_t kThresholdAmpRfOhm = 510UL;
    constexpr uint8_t kCrcConfirmHits = ADSB_ESP32_CRC_CONFIRM_HITS;
    constexpr uint32_t kCrcConfirmWindowMs = 10000UL;
    constexpr uint32_t kApTrustWindowMs = 30000UL;
    constexpr uint8_t kThresholdPwmChannel = ADSB_ESP32_THRESHOLD_PWM_CHANNEL;

    struct DecodedModeSFrame
    {
        uint8_t bytes[14];
        uint8_t byteLen;
        uint8_t bitLen;
        uint8_t df;
        uint32_t address;
        uint32_t syndrome;
        bool crcSelfValid;
        bool addressParityValid;
        uint8_t phase;
        uint8_t preambleOffset;
        uint8_t preambleMismatches;
        uint8_t weakBits;
        uint8_t weakBitPositions[16];
        uint8_t weakBitPositionCount;
        bool softDecision;
    };

    struct PulseInterval
    {
        uint32_t riseCycle;
        uint32_t fallCycle;
    };

    struct CPRPacket
    {
        uint32_t lat;
        uint32_t lon;
        uint32_t timestampMs;
        bool valid;
    };

    struct AdsbAircraftState
    {
        uint32_t address;
        char callsign[8];
        int altitudeFt;
        int squawk;
        float speedKts;
        float courseDeg;
        int verticalRateFpm;
        float latitude;
        float longitude;
        CPRPacket even;
        CPRPacket odd;
        uint32_t lastSeenMs;
        bool hasCallsign;
        bool hasAltitude;
        bool hasSquawk;
        bool hasVelocity;
        bool hasPosition;
        bool crcConfirmed;
        uint8_t crcHitCount;
        uint32_t crcFirstSeenMs;
        uint32_t lastCrcMs;
    };

    static ADSBReceiverESP32Diag g_diag = {};
    static TaskHandle_t g_adsbTask = nullptr;
    static TaskHandle_t g_dmaTask = nullptr;
    static AdsbAircraftState g_aircraft[kMaxAircraft] = {};
    static volatile uint32_t g_cyclesPerUs = 240UL;
    static volatile uint32_t g_cyclesPerHalfUs = 120UL;
    static volatile uint32_t g_cyclesPerQuarterUs = 60UL;
    static volatile bool g_dmaCaptureActive = false;
    static bool g_thresholdPwmReady = false;
    static uint16_t g_lastThresholdPwmDuty = 0xFFFFU;

    static uint16_t thresholdSettingToDuty()
    {
        int32_t level = settings ? settings->threshold_level : 910;
        if (level < kThresholdMinMv) level = kThresholdMinMv;
        if (level > kThresholdMaxMv) level = kThresholdMaxMv;

        const uint64_t numerator = (uint64_t)level * kThresholdAmpRgOhm * kThresholdPwmMaxDuty;
        const uint64_t denominator = (uint64_t)kThresholdPwmHighMv * (kThresholdAmpRgOhm + kThresholdAmpRfOhm);
        uint32_t duty = (uint32_t)((numerator + (denominator / 2U)) / denominator);
        if (duty > kThresholdPwmMaxDuty)
        {
            duty = kThresholdPwmMaxDuty;
        }
        return (uint16_t)duty;
    }

    static bool setupThresholdPwm()
    {
        if (g_thresholdPwmReady)
        {
            return true;
        }

        pinMode(ADSB_ESP32_THRESHOLD_PWM_PIN, OUTPUT);
        digitalWrite(ADSB_ESP32_THRESHOLD_PWM_PIN, LOW);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        g_thresholdPwmReady = ledcAttachChannel(ADSB_ESP32_THRESHOLD_PWM_PIN, kThresholdPwmFreqHz, kThresholdPwmResolutionBits, kThresholdPwmChannel);
#else
        ledcSetup(kThresholdPwmChannel, kThresholdPwmFreqHz, kThresholdPwmResolutionBits);
        ledcAttachPin(ADSB_ESP32_THRESHOLD_PWM_PIN, kThresholdPwmChannel);
        g_thresholdPwmReady = true;
#endif
        return g_thresholdPwmReady;
    }

    static void writeThresholdPwm(uint16_t duty)
    {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcWriteChannel(kThresholdPwmChannel, duty);
#else
        ledcWrite(kThresholdPwmChannel, duty);
#endif
    }

    static const uint32_t crc24_table[256] = {
        0x0, 0xFFF409, 0x1C1B, 0xFFE812, 0x3836, 0xFFCC3F, 0x242D, 0xFFD024, 0x706C, 0xFF8465, 0x6C77, 0xFF987E, 0x485A,
        0xFFBC53, 0x5441, 0xFFA048, 0xE0D8, 0xFF14D1, 0xFCC3, 0xFF08CA, 0xD8EE, 0xFF2CE7, 0xC4F5, 0xFF30FC, 0x90B4, 0xFF64BD,
        0x8CAF, 0xFF78A6, 0xA882, 0xFF5C8B, 0xB499, 0xFF4090, 0x1C1B0, 0xFE35B9, 0x1DDAB, 0xFE29A2, 0x1F986, 0xFE0D8F,
        0x1E59D, 0xFE1194, 0x1B1DC, 0xFE45D5, 0x1ADC7, 0xFE59CE, 0x189EA, 0xFE7DE3, 0x195F1, 0xFE61F8, 0x12168, 0xFED561,
        0x13D73, 0xFEC97A, 0x1195E, 0xFEED57, 0x10545, 0xFEF14C, 0x15104, 0xFEA50D, 0x14D1F, 0xFEB916, 0x16932, 0xFE9D3B,
        0x17529, 0xFE8120, 0x38360, 0xFC7769, 0x39F7B, 0xFC6B72, 0x3BB56, 0xFC4F5F, 0x3A74D, 0xFC5344, 0x3F30C, 0xFC0705,
        0x3EF17, 0xFC1B1E, 0x3CB3A, 0xFC3F33, 0x3D721, 0xFC2328, 0x363B8, 0xFC97B1, 0x37FA3, 0xFC8BAA, 0x35B8E, 0xFCAF87,
        0x34795, 0xFCB39C, 0x313D4, 0xFCE7DD, 0x30FCF, 0xFCFBC6, 0x32BE2, 0xFCDFEB, 0x337F9, 0xFCC3F0, 0x242D0, 0xFDB6D9,
        0x25ECB, 0xFDAAC2, 0x27AE6, 0xFD8EEF, 0x266FD, 0xFD92F4, 0x232BC, 0xFDC6B5, 0x22EA7, 0xFDDAAE, 0x20A8A, 0xFDFE83,
        0x21691, 0xFDE298, 0x2A208, 0xFD5601, 0x2BE13, 0xFD4A1A, 0x29A3E, 0xFD6E37, 0x28625, 0xFD722C, 0x2D264, 0xFD266D,
        0x2CE7F, 0xFD3A76, 0x2EA52, 0xFD1E5B, 0x2F649, 0xFD0240, 0x706C0, 0xF8F2C9, 0x71ADB, 0xF8EED2, 0x73EF6, 0xF8CAFF,
        0x722ED, 0xF8D6E4, 0x776AC, 0xF882A5, 0x76AB7, 0xF89EBE, 0x74E9A, 0xF8BA93, 0x75281, 0xF8A688, 0x7E618, 0xF81211,
        0x7FA03, 0xF80E0A, 0x7DE2E, 0xF82A27, 0x7C235, 0xF8363C, 0x79674, 0xF8627D, 0x78A6F, 0xF87E66, 0x7AE42, 0xF85A4B,
        0x7B259, 0xF84650, 0x6C770, 0xF93379, 0x6DB6B, 0xF92F62, 0x6FF46, 0xF90B4F, 0x6E35D, 0xF91754, 0x6B71C, 0xF94315,
        0x6AB07, 0xF95F0E, 0x68F2A, 0xF97B23, 0x69331, 0xF96738, 0x627A8, 0xF9D3A1, 0x63BB3, 0xF9CFBA, 0x61F9E, 0xF9EB97,
        0x60385, 0xF9F78C, 0x657C4, 0xF9A3CD, 0x64BDF, 0xF9BFD6, 0x66FF2, 0xF99BFB, 0x673E9, 0xF987E0, 0x485A0, 0xFB71A9,
        0x499BB, 0xFB6DB2, 0x4BD96, 0xFB499F, 0x4A18D, 0xFB5584, 0x4F5CC, 0xFB01C5, 0x4E9D7, 0xFB1DDE, 0x4CDFA, 0xFB39F3,
        0x4D1E1, 0xFB25E8, 0x46578, 0xFB9171, 0x47963, 0xFB8D6A, 0x45D4E, 0xFBA947, 0x44155, 0xFBB55C, 0x41514, 0xFBE11D,
        0x4090F, 0xFBFD06, 0x42D22, 0xFBD92B, 0x43139, 0xFBC530, 0x54410, 0xFAB019, 0x5580B, 0xFAAC02, 0x57C26, 0xFA882F,
        0x5603D, 0xFA9434, 0x5347C, 0xFAC075, 0x52867, 0xFADC6E, 0x50C4A, 0xFAF843, 0x51051, 0xFAE458, 0x5A4C8, 0xFA50C1,
        0x5B8D3, 0xFA4CDA, 0x59CFE, 0xFA68F7, 0x580E5, 0xFA74EC, 0x5D4A4, 0xFA20AD, 0x5C8BF, 0xFA3CB6, 0x5EC92, 0xFA189B,
        0x5F089, 0xFA0480
    };

    static uint32_t crc24Bytes(const uint8_t* buffer, uint8_t len)
    {
        uint32_t crc = 0;
        for (uint8_t i = 0; i < len; ++i)
        {
            const uint8_t byte = buffer[i];
            crc = ((crc << 8) ^ crc24_table[((crc >> 16) ^ byte) & 0xFF]) & 0xFFFFFFUL;
        }
        return crc & 0xFFFFFFUL;
    }

    static uint32_t getBits(const uint8_t* bytes, uint16_t firstBit, uint8_t count)
    {
        uint32_t value = 0;
        for (uint8_t i = 0; i < count; ++i)
        {
            const uint16_t bitIndex = firstBit + i;
            const uint8_t b = bytes[bitIndex / 8];
            const uint8_t bit = (b >> (7 - (bitIndex & 7))) & 1U;
            value = (value << 1) | bit;
        }
        return value;
    }

    static bool isLongDf(uint8_t df)
    {
        return df == 16 || df == 17 || df == 18 || df == 19 || df == 20 || df == 21 || df == 24;
    }

    static bool isAddressParityDf(uint8_t df)
    {
        return df == 4 || df == 5 || df == 20 || df == 21;
    }

    static bool frameParityAccepted(const DecodedModeSFrame& frame)
    {
        return frame.crcSelfValid || frame.addressParityValid;
    }

    static int cprNL(float lat)
    {
        const float a = fabsf(lat);
        if (a < 10.47047130f) return 59;
        if (a < 14.82817437f) return 58;
        if (a < 18.18626357f) return 57;
        if (a < 21.02939493f) return 56;
        if (a < 23.54504487f) return 55;
        if (a < 25.82924707f) return 54;
        if (a < 27.93898710f) return 53;
        if (a < 29.91135686f) return 52;
        if (a < 31.77209708f) return 51;
        if (a < 33.53993436f) return 50;
        if (a < 35.22899598f) return 49;
        if (a < 36.85025108f) return 48;
        if (a < 38.41241892f) return 47;
        if (a < 39.92256684f) return 46;
        if (a < 41.38651832f) return 45;
        if (a < 42.80914012f) return 44;
        if (a < 44.19454951f) return 43;
        if (a < 45.54626723f) return 42;
        if (a < 46.86733252f) return 41;
        if (a < 48.16039128f) return 40;
        if (a < 49.42776439f) return 39;
        if (a < 50.67150166f) return 38;
        if (a < 51.89342469f) return 37;
        if (a < 53.09516153f) return 36;
        if (a < 54.27817472f) return 35;
        if (a < 55.44378444f) return 34;
        if (a < 56.59318756f) return 33;
        if (a < 57.72747354f) return 32;
        if (a < 58.84763776f) return 31;
        if (a < 59.95459277f) return 30;
        if (a < 61.04917774f) return 29;
        if (a < 62.13216659f) return 28;
        if (a < 63.20427479f) return 27;
        if (a < 64.26616523f) return 26;
        if (a < 65.31845310f) return 25;
        if (a < 66.36171008f) return 24;
        if (a < 67.39646774f) return 23;
        if (a < 68.42322022f) return 22;
        if (a < 69.44242631f) return 21;
        if (a < 70.45451075f) return 20;
        if (a < 71.45986473f) return 19;
        if (a < 72.45884545f) return 18;
        if (a < 73.45177442f) return 17;
        if (a < 74.43893416f) return 16;
        if (a < 75.42056257f) return 15;
        if (a < 76.39684391f) return 14;
        if (a < 77.36789461f) return 13;
        if (a < 78.33374083f) return 12;
        if (a < 79.29428225f) return 11;
        if (a < 80.24923213f) return 10;
        if (a < 81.19801349f) return 9;
        if (a < 82.13956981f) return 8;
        if (a < 83.07199445f) return 7;
        if (a < 83.99173563f) return 6;
        if (a < 84.89166191f) return 5;
        if (a < 85.75541621f) return 4;
        if (a < 86.53536998f) return 3;
        if (a < 87.00000000f) return 2;
        return 1;
    }

    static int positiveMod(int value, int divisor)
    {
        int result = value % divisor;
        return result < 0 ? result + divisor : result;
    }

    static float wrapLongitude(float lon)
    {
        while (lon > 180.0f) lon -= 360.0f;
        while (lon < -180.0f) lon += 360.0f;
        return lon;
    }

    static bool decodeGlobalCPR(const CPRPacket& even, const CPRPacket& odd, float& outLat, float& outLon)
    {
        if (!even.valid || !odd.valid)
        {
            return false;
        }

        const uint32_t pairAge = (even.timestampMs > odd.timestampMs)
            ? (even.timestampMs - odd.timestampMs)
            : (odd.timestampMs - even.timestampMs);
        if (pairAge > kCPRMaxPairAgeMs)
        {
            return false;
        }

        const float latEven = (float)even.lat / 131072.0f;
        const float latOdd = (float)odd.lat / 131072.0f;
        const int j = (int)floorf((59.0f * latEven) - (60.0f * latOdd) + 0.5f);

        float rlatEven = (360.0f / 60.0f) * ((float)positiveMod(j, 60) + latEven);
        float rlatOdd = (360.0f / 59.0f) * ((float)positiveMod(j, 59) + latOdd);
        if (rlatEven >= 270.0f) rlatEven -= 360.0f;
        if (rlatOdd >= 270.0f) rlatOdd -= 360.0f;
        if (cprNL(rlatEven) != cprNL(rlatOdd))
        {
            return false;
        }

        const bool useOdd = odd.timestampMs > even.timestampMs;
        const float lat = useOdd ? rlatOdd : rlatEven;
        const int nl = cprNL(lat);
        const int ni = useOdd ? ((nl > 1) ? (nl - 1) : 1) : ((nl > 0) ? nl : 1);
        const float lonEven = (float)even.lon / 131072.0f;
        const float lonOdd = (float)odd.lon / 131072.0f;
        const int m = (int)floorf(((float)(nl - 1) * lonEven) - ((float)nl * lonOdd) + 0.5f);
        const float lonCpr = useOdd ? lonOdd : lonEven;
        const float lon = (360.0f / (float)ni) * ((float)positiveMod(m, ni) + lonCpr);

        outLat = lat;
        outLon = wrapLongitude(lon);
        return true;
    }

    static int decodeAltitudeFt(uint16_t ac12)
    {
        if (ac12 == 0)
        {
            return 0;
        }

        const bool qBit = (ac12 & 0x0010U) != 0;
        const uint16_t n = (uint16_t)(((ac12 & 0x0FE0U) >> 1) | (ac12 & 0x000FU));
        return qBit ? ((int)n * 25 - 1000) : ((int)n * 25);
    }

    static int decodeAltitudeModeS13Ft(uint16_t ac13)
    {
        return decodeAltitudeFt(ac13 & 0x0FFFU);
    }

    static int decodeSquawkModeA(uint16_t id13)
    {
        auto bitFromLeft = [id13](uint8_t posFromLeft) -> uint8_t {
            return (id13 >> (12U - posFromLeft)) & 1U;
        };

        const int a = (int)bitFromLeft(1) | ((int)bitFromLeft(3) << 1) | ((int)bitFromLeft(5) << 2);
        const int b = (int)bitFromLeft(7) | ((int)bitFromLeft(9) << 1) | ((int)bitFromLeft(11) << 2);
        const int c = (int)bitFromLeft(0) | ((int)bitFromLeft(2) << 1) | ((int)bitFromLeft(4) << 2);
        const int d = (int)bitFromLeft(8) | ((int)bitFromLeft(10) << 1) | ((int)bitFromLeft(12) << 2);

        if (a > 7 || b > 7 || c > 7 || d > 7)
        {
            return -1;
        }
        return a * 1000 + b * 100 + c * 10 + d;
    }

    static char decodeCallsignChar(uint8_t code)
    {
        if (code >= 1 && code <= 26) return (char)('A' + code - 1);
        if (code >= 48 && code <= 57) return (char)('0' + code - 48);
        return ' ';
    }

    static AdsbAircraftState* findAircraftState(uint32_t address)
    {
        for (size_t i = 0; i < kMaxAircraft; ++i)
        {
            if (g_aircraft[i].address == address)
            {
                return &g_aircraft[i];
            }
        }
        return nullptr;
    }

    static AdsbAircraftState* getAircraftState(uint32_t address)
    {
        AdsbAircraftState* emptySlot = nullptr;
        AdsbAircraftState* oldest = &g_aircraft[0];

        for (size_t i = 0; i < kMaxAircraft; ++i)
        {
            if (g_aircraft[i].address == address)
            {
                return &g_aircraft[i];
            }
            if (g_aircraft[i].address == 0 && emptySlot == nullptr)
            {
                emptySlot = &g_aircraft[i];
            }
            if (g_aircraft[i].lastSeenMs < oldest->lastSeenMs)
            {
                oldest = &g_aircraft[i];
            }
        }

        AdsbAircraftState* slot = emptySlot ? emptySlot : oldest;
        memset(slot, 0, sizeof(*slot));
        slot->address = address;
        memset(slot->callsign, ' ', sizeof(slot->callsign));
        return slot;
    }

    static void pulseOkPin()
    {
        digitalWrite(ADSB_ESP32_OK_PULSE_PIN, LOW);
        delayMicroseconds(80);
        digitalWrite(ADSB_ESP32_OK_PULSE_PIN, HIGH);
    }

    static bool hasConfirmedLow20Alias(const AdsbAircraftState& aircraft)
    {
        if (aircraft.crcConfirmed)
        {
            return false;
        }

        const uint32_t low20 = aircraft.address & 0x0FFFFFUL;
        for (size_t i = 0; i < kMaxAircraft; ++i)
        {
            const AdsbAircraftState& other = g_aircraft[i];
            if (other.address != 0 && other.address != aircraft.address && other.crcConfirmed &&
                ((other.address & 0x0FFFFFUL) == low20))
            {
                return true;
            }
        }
        return false;
    }

    static void clearContainerAddress(uint32_t address)
    {
        const uint32_t maskedAddress = address & 0xFFFFFFUL;
        for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
        {
            if ((Container[i].addr & 0xFFFFFFUL) == maskedAddress)
            {
                Container[i] = EmptyFO;
            }
        }
    }

    static void clearUnconfirmedLow20Aliases(uint32_t confirmedAddress)
    {
        const uint32_t low20 = confirmedAddress & 0x0FFFFFUL;
        for (size_t i = 0; i < kMaxAircraft; ++i)
        {
            AdsbAircraftState& other = g_aircraft[i];
            if (other.address != 0 && other.address != confirmedAddress && !other.crcConfirmed &&
                ((other.address & 0x0FFFFFUL) == low20))
            {
                clearContainerAddress(other.address);
                memset(&other, 0, sizeof(other));
            }
        }
    }

    static bool noteCrcConfirmedFrame(AdsbAircraftState& aircraft, uint32_t nowMs)
    {
        aircraft.lastCrcMs = nowMs;
        if (aircraft.crcConfirmed)
        {
            return true;
        }

        if (hasConfirmedLow20Alias(aircraft))
        {
            ++g_diag.aliasSuppressed;
            clearContainerAddress(aircraft.address);
            memset(&aircraft, 0, sizeof(aircraft));
            return false;
        }

        if (aircraft.crcFirstSeenMs == 0U || (nowMs - aircraft.crcFirstSeenMs) > kCrcConfirmWindowMs)
        {
            aircraft.crcFirstSeenMs = nowMs;
            aircraft.crcHitCount = 1U;
        }
        else if (aircraft.crcHitCount < 255U)
        {
            ++aircraft.crcHitCount;
        }

        if (aircraft.crcHitCount >= kCrcConfirmHits)
        {
            aircraft.crcConfirmed = true;
            clearUnconfirmedLow20Aliases(aircraft.address);
            return true;
        }

        ++g_diag.crcPending;
        return false;
    }

    static bool aircraftAcceptsAddressParity(const AdsbAircraftState& aircraft, uint32_t nowMs)
    {
        return aircraft.crcConfirmed && aircraft.lastCrcMs != 0U && ((nowMs - aircraft.lastCrcMs) <= kApTrustWindowMs);
    }

    static void pushAircraftToContainer(const AdsbAircraftState& aircraft)
    {
        if (aircraft.address == 0)
        {
            return;
        }

        if (hasConfirmedLow20Alias(aircraft))
        {
            ++g_diag.aliasSuppressed;
            return;
        }

        fo = EmptyFO;
        fo.addr = aircraft.address & 0xFFFFFFUL;
        fo.squawk = aircraft.hasSquawk ? (uint16_t)aircraft.squawk : 0U;
        memcpy(fo.callsign, aircraft.callsign, sizeof(fo.callsign));
        fo.altitude = aircraft.hasAltitude ? ((float)aircraft.altitudeFt * 0.3048f) : 0.0f;
        fo.pressure_altitude = fo.altitude;
        fo.speed = aircraft.hasVelocity ? aircraft.speedKts : 0.0f;
        fo.course = aircraft.hasVelocity ? aircraft.courseDeg : 0.0f;
        fo.vert_rate = aircraft.hasVelocity ? aircraft.verticalRateFpm : 0;
        fo.latitude = aircraft.hasPosition ? aircraft.latitude : 0.0f;
        fo.longitude = aircraft.hasPosition ? aircraft.longitude : 0.0f;
        fo.rssi_rp2040 = 0;
        fo.signal_source = TRAFFIC_SOURCE_ADSB_DUMP1090;
        fo.aircraft_type = 1;
        fo.timestamp = (time_t)(millis() / 1000UL);
        fo.timemsg = fo.timestamp;
        Traffic_Update(&fo);
        if (Traffic_Add(&fo))
        {
            ++g_diag.containerUpdates;
        }
    }

    static uint8_t IRAM_ATTR readRxPinFast()
    {
#if ADSB_ESP32_RX_PIN < 32
        return (REG_READ(GPIO_IN_REG) & (1UL << ADSB_ESP32_RX_PIN)) ? 1U : 0U;
#else
        return (REG_READ(GPIO_IN1_REG) & (1UL << (ADSB_ESP32_RX_PIN - 32))) ? 1U : 0U;
#endif
    }

    static uint32_t IRAM_ATTR cpuCyclesNow()
    {
        return (uint32_t)XTHAL_GET_CCOUNT();
    }

    static bool IRAM_ATTR addPulseInterval(PulseInterval* pulses,
                                           uint16_t& pulseCount,
                                           uint32_t riseCycle,
                                           uint32_t fallCycle)
    {
        if ((int32_t)(fallCycle - riseCycle) <= 0)
        {
            return true;
        }
        if (pulseCount >= kMaxPulseIntervals)
        {
            return false;
        }

        pulses[pulseCount].riseCycle = riseCycle;
        pulses[pulseCount].fallCycle = fallCycle;
        ++pulseCount;
        return true;
    }

    static uint8_t IRAM_ATTR pulseStateAtCycle(const PulseInterval* pulses, uint16_t pulseCount, uint32_t sampleCycle)
    {
        for (uint16_t i = 0; i < pulseCount; ++i)
        {
            if ((int32_t)(sampleCycle - pulses[i].riseCycle) >= 0 &&
                (int32_t)(sampleCycle - pulses[i].fallCycle) < 0)
            {
                return 1U;
            }
            if ((int32_t)(pulses[i].riseCycle - sampleCycle) > 0)
            {
                break;
            }
        }
        return 0U;
    }

    static void IRAM_ATTR sampleSlotsFromPulseIntervals(uint32_t firstRiseCycle,
                                                        const PulseInterval* pulses,
                                                        uint16_t pulseCount,
                                                        uint8_t phaseSlots[kPhaseCount][kMaxSlots],
                                                        size_t slotCount)
    {
        uint32_t centerCycle = firstRiseCycle + g_cyclesPerQuarterUs;
        const uint32_t stepCycles = g_cyclesPerHalfUs;
        uint32_t phaseDelta = g_cyclesPerHalfUs / 4U;
        if (phaseDelta == 0U)
        {
            phaseDelta = 1U;
        }

        for (size_t i = 0; i < slotCount; ++i)
        {
            phaseSlots[0][i] = pulseStateAtCycle(pulses, pulseCount, centerCycle - phaseDelta);
            phaseSlots[kPhaseCenter][i] = pulseStateAtCycle(pulses, pulseCount, centerCycle);
            phaseSlots[2][i] = pulseStateAtCycle(pulses, pulseCount, centerCycle + phaseDelta);
            centerCycle += stepCycles;
        }
    }

    static bool IRAM_ATTR captureSlotsFromEdges(uint32_t firstRiseCycle,
                                                uint8_t phaseSlots[kPhaseCount][kMaxSlots],
                                                size_t slotCount)
    {
        PulseInterval pulses[kMaxPulseIntervals];
        uint16_t pulseCount = 0;
        uint8_t previous = 1U;
        bool highActive = true;
        uint32_t highStartCycle = firstRiseCycle;
        uint32_t previousCycle = firstRiseCycle;
        const uint32_t captureCycles = (uint32_t)(slotCount + 4U) * g_cyclesPerHalfUs;
        const uint32_t deadlineCycle = firstRiseCycle + captureCycles;

        while ((int32_t)(cpuCyclesNow() - deadlineCycle) < 0)
        {
            const uint8_t current = readRxPinFast();
            const uint32_t currentCycle = cpuCyclesNow();
            if (current != previous)
            {
                const uint32_t edgeCycle = previousCycle + ((uint32_t)(currentCycle - previousCycle) / 2U);
                if (current != 0U)
                {
                    highStartCycle = edgeCycle;
                    highActive = true;
                }
                else if (highActive)
                {
                    if (!addPulseInterval(pulses, pulseCount, highStartCycle, edgeCycle))
                    {
                        return false;
                    }
                    highActive = false;
                }
                previous = current;
            }
            previousCycle = currentCycle;
        }

        if (highActive)
        {
            if (!addPulseInterval(pulses, pulseCount, highStartCycle, deadlineCycle))
            {
                return false;
            }
        }

        if (pulseCount < 4U)
        {
            return false;
        }

        sampleSlotsFromPulseIntervals(firstRiseCycle, pulses, pulseCount, phaseSlots, slotCount);
        return true;
    }

    static uint8_t preambleMismatchesAt(const uint8_t* slots, size_t offset, size_t slotCount)
    {
        static const uint8_t kPreamble[16] = {
            1, 0, 1, 0, 0, 0, 0, 1,
            0, 1, 0, 0, 0, 0, 0, 0
        };

        if (offset + sizeof(kPreamble) > slotCount)
        {
            return 255U;
        }

        uint8_t mismatches = 0;
        for (size_t i = 0; i < sizeof(kPreamble); ++i)
        {
            if (slots[offset + i] != kPreamble[i])
            {
                ++mismatches;
            }
        }
        return mismatches;
    }

    static uint8_t phaseHighCount(uint8_t phaseSlots[kPhaseCount][kMaxSlots], size_t slot)
    {
        uint8_t count = 0;
        for (uint8_t phase = 0; phase < kPhaseCount; ++phase)
        {
            if (phaseSlots[phase][slot] != 0)
            {
                ++count;
            }
        }
        return count;
    }

    static uint8_t softPreambleRankAt(uint8_t phaseSlots[kPhaseCount][kMaxSlots], size_t offset, size_t slotCount, uint8_t* outHardErrors)
    {
        static const uint8_t kRequiredHigh[] = {0, 2, 7, 9};
        static const uint8_t kQuietLow[] = {4, 5, 6, 11, 12, 13, 14, 15};

        if (offset + kPreambleSlots > slotCount)
        {
            if (outHardErrors != nullptr)
            {
                *outHardErrors = 255U;
            }
            return 255U;
        }

        uint8_t hardErrors = 0;
        uint8_t weak = 0;
        for (uint8_t i = 0; i < sizeof(kRequiredHigh); ++i)
        {
            const uint8_t count = phaseHighCount(phaseSlots, offset + kRequiredHigh[i]);
            if (count == 0U)
            {
                ++hardErrors;
            }
            else if (count < 2U)
            {
                ++weak;
            }
        }

        for (uint8_t i = 0; i < sizeof(kQuietLow); ++i)
        {
            const uint8_t count = phaseHighCount(phaseSlots, offset + kQuietLow[i]);
            if (count >= 2U)
            {
                ++hardErrors;
            }
            else if (count != 0U)
            {
                ++weak;
            }
        }

        if (outHardErrors != nullptr)
        {
            *outHardErrors = hardErrors;
        }

        const uint16_t rank = (uint16_t)hardErrors * 20U + (uint16_t)weak * 3U;
        return (rank > 255U) ? 255U : (uint8_t)rank;
    }

    static int findSoftPreamble(uint8_t phaseSlots[kPhaseCount][kMaxSlots], size_t slotCount, uint8_t* outHardErrors, uint8_t* outRank)
    {
        if (slotCount < kPreambleSlots)
        {
            return -1;
        }

        int bestOffset = -1;
        uint8_t bestRank = 255U;
        uint8_t bestHardErrors = 255U;
        const size_t maxSearchOffset = ((slotCount - kPreambleSlots) > 32U) ? 32U : (slotCount - kPreambleSlots);
        for (size_t offset = 0; offset <= maxSearchOffset; ++offset)
        {
            uint8_t hardErrors = 0;
            const uint8_t rank = softPreambleRankAt(phaseSlots, offset, slotCount, &hardErrors);
            if (rank < bestRank)
            {
                bestRank = rank;
                bestOffset = (int)offset;
                bestHardErrors = hardErrors;
                if (rank == 0U)
                {
                    break;
                }
            }
        }

        if (bestOffset < 0 || bestHardErrors > 1U || bestRank > 26U)
        {
            return -1;
        }

        if (outHardErrors != nullptr)
        {
            *outHardErrors = bestHardErrors;
        }
        if (outRank != nullptr)
        {
            *outRank = bestRank;
        }
        return bestOffset;
    }

    static int findPreamble(const uint8_t* slots, size_t slotCount, uint8_t* outMismatches)
    {
        if (slotCount < kPreambleSlots)
        {
            return -1;
        }

        int bestOffset = -1;
        uint8_t bestMismatches = 255U;
        const size_t maxSearchOffset = ((slotCount - kPreambleSlots) > 32U) ? 32U : (slotCount - kPreambleSlots);
        for (size_t offset = 0; offset <= maxSearchOffset; ++offset)
        {
            const uint8_t mismatches = preambleMismatchesAt(slots, offset, slotCount);
            if (mismatches < bestMismatches)
            {
                bestMismatches = mismatches;
                bestOffset = (int)offset;
                if (mismatches == 0U)
                {
                    break;
                }
            }
        }

        if (bestMismatches > 1U)
        {
            return -1;
        }
        if (outMismatches != nullptr)
        {
            *outMismatches = bestMismatches;
        }
        return bestOffset;
    }

    static bool decodeBitPair(const uint8_t* slots, size_t bitSlot, bool& bit)
    {
        const uint8_t first = slots[bitSlot];
        const uint8_t second = slots[bitSlot + 1];
        if (first == 1 && second == 0)
        {
            bit = true;
            return true;
        }
        if (first == 0 && second == 1)
        {
            bit = false;
            return true;
        }
        return false;
    }

    static bool decodeBitsToBytes(const uint8_t* slots, size_t dataStartSlot, uint8_t bitLen, uint8_t* outBytes)
    {
        memset(outBytes, 0, 14);
        for (uint8_t i = 0; i < bitLen; ++i)
        {
            bool bit = false;
            if (!decodeBitPair(slots, dataStartSlot + (size_t)i * 2U, bit))
            {
                return false;
            }
            if (bit)
            {
                outBytes[i / 8] |= (uint8_t)(0x80U >> (i & 7));
            }
        }
        return true;
    }

    static bool decodeSoftBitPair(uint8_t phaseSlots[kPhaseCount][kMaxSlots], size_t bitSlot, bool& bit, bool& weak)
    {
        const int first = (int)phaseHighCount(phaseSlots, bitSlot);
        const int second = (int)phaseHighCount(phaseSlots, bitSlot + 1U);
        const int diff = first - second;
        weak = (diff == 1 || diff == -1);
        if (diff > 0)
        {
            bit = true;
            return true;
        }
        if (diff < 0)
        {
            bit = false;
            return true;
        }
        return false;
    }

    static bool decodeSoftBitsToBytes(uint8_t phaseSlots[kPhaseCount][kMaxSlots],
                                      size_t dataStartSlot,
                                      uint8_t bitLen,
                                      uint8_t* outBytes,
                                      uint8_t& weakBits,
                                      uint8_t* weakBitPositions,
                                      uint8_t& weakBitPositionCount)
    {
        memset(outBytes, 0, 14);
        weakBits = 0;
        weakBitPositionCount = 0;
        for (uint8_t i = 0; i < bitLen; ++i)
        {
            bool bit = false;
            bool weak = false;
            if (!decodeSoftBitPair(phaseSlots, dataStartSlot + (size_t)i * 2U, bit, weak))
            {
                return false;
            }
            if (weak)
            {
                ++weakBits;
                if (weakBitPositions != nullptr && weakBitPositionCount < 16U)
                {
                    weakBitPositions[weakBitPositionCount++] = i;
                }
            }
            if (bit)
            {
                outBytes[i / 8] |= (uint8_t)(0x80U >> (i & 7));
            }
        }
        return true;
    }

    static void updateFrameCrcAndAddress(DecodedModeSFrame& frame)
    {
        const uint32_t calculated = crc24Bytes(frame.bytes, frame.byteLen - 3U);
        const uint32_t parity = ((uint32_t)frame.bytes[frame.byteLen - 3U] << 16) |
                                ((uint32_t)frame.bytes[frame.byteLen - 2U] << 8) |
                                (uint32_t)frame.bytes[frame.byteLen - 1U];
        frame.syndrome = calculated ^ parity;
        frame.crcSelfValid = (frame.syndrome == 0);
        frame.addressParityValid = isAddressParityDf(frame.df) && frame.syndrome != 0;

        if (frame.addressParityValid)
        {
            frame.address = frame.syndrome & 0xFFFFFFUL;
        }
        else if (frame.df == 11)
        {
            frame.address = ((uint32_t)frame.bytes[1] << 16) | ((uint32_t)frame.bytes[2] << 8) | frame.bytes[3];
        }
        else if (frame.bitLen == 112)
        {
            frame.address = ((uint32_t)frame.bytes[1] << 16) | ((uint32_t)frame.bytes[2] << 8) | frame.bytes[3];
        }
        else
        {
            frame.address = frame.syndrome;
        }
    }

    static void flipFrameBit(DecodedModeSFrame& frame, uint8_t bitIndex)
    {
        frame.bytes[bitIndex / 8U] ^= (uint8_t)(0x80U >> (bitIndex & 7U));
    }

    static bool tryRepairOneWeakBit(DecodedModeSFrame& frame)
    {
        if (!frame.softDecision || frame.crcSelfValid || frame.bitLen != 112U ||
            (frame.df != 17 && frame.df != 18 && frame.df != 19) ||
            frame.weakBitPositionCount == 0U || frame.weakBitPositionCount > 12U)
        {
            return false;
        }

        for (uint8_t i = 0; i < frame.weakBitPositionCount; ++i)
        {
            const uint8_t bitIndex = frame.weakBitPositions[i];
            if (bitIndex < 5U)
            {
                continue;
            }

            flipFrameBit(frame, bitIndex);
            updateFrameCrcAndAddress(frame);
            if (frame.crcSelfValid)
            {
                ++g_diag.weakBitRepaired;
                return true;
            }
            flipFrameBit(frame, bitIndex);
        }

        updateFrameCrcAndAddress(frame);
        return false;
    }

    static bool tryRepairOneCrcBit(DecodedModeSFrame& frame)
    {
        if (frame.crcSelfValid || frame.bitLen != 112U ||
            (frame.df != 17 && frame.df != 18 && frame.df != 19))
        {
            return false;
        }

        uint8_t original[14];
        memcpy(original, frame.bytes, sizeof(original));
        for (uint8_t bitIndex = 5U; bitIndex < frame.bitLen; ++bitIndex)
        {
            flipFrameBit(frame, bitIndex);
            updateFrameCrcAndAddress(frame);
            if (frame.crcSelfValid)
            {
                ++g_diag.weakBitRepaired;
                return true;
            }
            memcpy(frame.bytes, original, sizeof(original));
        }

        updateFrameCrcAndAddress(frame);
        return false;
    }

    static bool decodeModeSFrame(const uint8_t* slots, size_t slotCount, uint8_t phase, DecodedModeSFrame& frame)
    {
        uint8_t preambleMismatches = 0;
        const int preambleOffset = findPreamble(slots, slotCount, &preambleMismatches);
        if (preambleOffset < 0)
        {
            return false;
        }

        const size_t dataStartSlot = (size_t)preambleOffset + 16U;
        if (dataStartSlot + 10U > slotCount)
        {
            return false;
        }

        uint8_t df = 0;
        for (uint8_t i = 0; i < 5; ++i)
        {
            bool bit = false;
            if (!decodeBitPair(slots, dataStartSlot + (size_t)i * 2U, bit))
            {
                return false;
            }
            df = (uint8_t)((df << 1) | (bit ? 1U : 0U));
        }

        const uint8_t bitLen = isLongDf(df) ? 112U : 56U;
        if (dataStartSlot + (size_t)bitLen * 2U > slotCount)
        {
            return false;
        }

        memset(&frame, 0, sizeof(frame));
        if (!decodeBitsToBytes(slots, dataStartSlot, bitLen, frame.bytes))
        {
            return false;
        }

        frame.df = df;
        frame.bitLen = bitLen;
        frame.byteLen = bitLen / 8U;
        frame.phase = phase;
        frame.preambleOffset = (uint8_t)preambleOffset;
        frame.preambleMismatches = preambleMismatches;
        updateFrameCrcAndAddress(frame);
        if (!frame.crcSelfValid)
        {
            tryRepairOneCrcBit(frame);
        }

        return true;
    }

    static bool decodeSoftModeSFrame(uint8_t phaseSlots[kPhaseCount][kMaxSlots], size_t slotCount, DecodedModeSFrame& frame)
    {
        uint8_t preambleHardErrors = 0;
        const int preambleOffset = findSoftPreamble(phaseSlots, slotCount, &preambleHardErrors, nullptr);
        if (preambleOffset < 0)
        {
            return false;
        }

        const size_t dataStartSlot = (size_t)preambleOffset + 16U;
        if (dataStartSlot + 10U > slotCount)
        {
            return false;
        }

        uint8_t df = 0;
        uint8_t weakDfBits = 0;
        for (uint8_t i = 0; i < 5; ++i)
        {
            bool bit = false;
            bool weak = false;
            if (!decodeSoftBitPair(phaseSlots, dataStartSlot + (size_t)i * 2U, bit, weak))
            {
                return false;
            }
            if (weak)
            {
                ++weakDfBits;
            }
            df = (uint8_t)((df << 1) | (bit ? 1U : 0U));
        }
        if (weakDfBits > 2U)
        {
            return false;
        }

        const uint8_t bitLen = isLongDf(df) ? 112U : 56U;
        if (dataStartSlot + (size_t)bitLen * 2U > slotCount)
        {
            return false;
        }

        memset(&frame, 0, sizeof(frame));
        uint8_t weakBits = 0;
        uint8_t weakBitPositionCount = 0;
        if (!decodeSoftBitsToBytes(phaseSlots, dataStartSlot, bitLen, frame.bytes, weakBits,
                                   frame.weakBitPositions, weakBitPositionCount))
        {
            return false;
        }

        frame.df = df;
        frame.bitLen = bitLen;
        frame.byteLen = bitLen / 8U;
        frame.phase = kPhaseSoft;
        frame.preambleOffset = (uint8_t)preambleOffset;
        frame.preambleMismatches = preambleHardErrors;
        frame.weakBits = (uint8_t)((weakBits > 240U) ? 240U : weakBits);
        frame.weakBitPositionCount = weakBitPositionCount;
        frame.softDecision = true;
        updateFrameCrcAndAddress(frame);
        if (!frame.crcSelfValid)
        {
            if (!tryRepairOneWeakBit(frame))
            {
                tryRepairOneCrcBit(frame);
            }
        }

        return true;
    }

    static uint8_t phasePenalty(uint8_t phase)
    {
        if (phase == kPhaseCenter)
        {
            return 0U;
        }
        return (phase == kPhaseSoft) ? 1U : 2U;
    }

    static uint16_t frameRank(const DecodedModeSFrame& frame)
    {
        uint16_t rank = frame.crcSelfValid ? 0U : (frame.addressParityValid ? 100U : 300U);
        rank += (uint16_t)frame.preambleMismatches * 10U;
        rank += (uint16_t)frame.weakBits;
        rank += phasePenalty(frame.phase);
        return rank;
    }

    static bool decodeBestModeSFrame(uint8_t phaseSlots[kPhaseCount][kMaxSlots], size_t slotCount, DecodedModeSFrame& frame)
    {
        static const uint8_t kPhaseOrder[kPhaseCount] = {kPhaseCenter, 0, 2};

        bool haveCandidate = false;
        DecodedModeSFrame best = {};
        uint16_t bestRank = 0xFFFFU;

        DecodedModeSFrame softCandidate = {};
        if (decodeSoftModeSFrame(phaseSlots, slotCount, softCandidate))
        {
            best = softCandidate;
            bestRank = frameRank(softCandidate);
            haveCandidate = true;
        }

        for (uint8_t i = 0; i < kPhaseCount; ++i)
        {
            const uint8_t phase = kPhaseOrder[i];
            DecodedModeSFrame candidate = {};
            if (!decodeModeSFrame(phaseSlots[phase], slotCount, phase, candidate))
            {
                continue;
            }

            const uint16_t rank = frameRank(candidate);
            if (!haveCandidate || rank < bestRank)
            {
                best = candidate;
                bestRank = rank;
                haveCandidate = true;
                if (candidate.crcSelfValid && candidate.preambleMismatches == 0U && candidate.phase == kPhaseCenter)
                {
                    break;
                }
            }
        }

        if (!haveCandidate)
        {
            return false;
        }

        frame = best;
        if (frame.phase != kPhaseCenter)
        {
            ++g_diag.multiPhaseUsed;
        }
        if (frame.softDecision)
        {
            ++g_diag.softDecisionUsed;
        }
        return true;
    }

    static void decodeAircraftId(AdsbAircraftState& aircraft, const DecodedModeSFrame& frame)
    {
        for (uint8_t i = 0; i < 8; ++i)
        {
            aircraft.callsign[i] = decodeCallsignChar((uint8_t)getBits(frame.bytes, 40U + (uint16_t)i * 6U, 6));
        }
        aircraft.hasCallsign = true;
    }

    static void decodeAirbornePosition(AdsbAircraftState& aircraft, const DecodedModeSFrame& frame, uint32_t nowMs)
    {
        const uint16_t altField = (uint16_t)getBits(frame.bytes, 40, 12);
        const int altFt = decodeAltitudeFt(altField);
        if (altFt != 0)
        {
            aircraft.altitudeFt = altFt;
            aircraft.hasAltitude = true;
        }

        const bool odd = getBits(frame.bytes, 53, 1) != 0;
        CPRPacket& cpr = odd ? aircraft.odd : aircraft.even;
        cpr.lat = getBits(frame.bytes, 54, 17);
        cpr.lon = getBits(frame.bytes, 71, 17);
        cpr.timestampMs = nowMs;
        cpr.valid = true;

        float lat = 0.0f;
        float lon = 0.0f;
        if (decodeGlobalCPR(aircraft.even, aircraft.odd, lat, lon))
        {
            aircraft.latitude = lat;
            aircraft.longitude = lon;
            aircraft.hasPosition = true;
        }
    }

    static void decodeAirborneVelocity(AdsbAircraftState& aircraft, const DecodedModeSFrame& frame)
    {
        const uint8_t subtype = (uint8_t)getBits(frame.bytes, 37, 3);
        if (subtype == 1 || subtype == 2)
        {
            const int ewPlusOne = (int)getBits(frame.bytes, 46, 10);
            const int nsPlusOne = (int)getBits(frame.bytes, 57, 10);
            if (ewPlusOne > 0 && nsPlusOne > 0)
            {
                float vx = (float)(ewPlusOne - 1);
                float vy = (float)(nsPlusOne - 1);
                if (getBits(frame.bytes, 45, 1) != 0) vx = -vx;
                if (getBits(frame.bytes, 56, 1) != 0) vy = -vy;
                if (subtype == 2)
                {
                    vx *= 4.0f;
                    vy *= 4.0f;
                }

                aircraft.speedKts = sqrtf(vx * vx + vy * vy);
                float course = atan2f(vx, vy) * 57.2957795131f;
                if (course < 0.0f) course += 360.0f;
                aircraft.courseDeg = course;
                aircraft.hasVelocity = true;
            }
        }
        else if (subtype == 3 || subtype == 4)
        {
            const int airspeedPlusOne = (int)getBits(frame.bytes, 57, 10);
            if (airspeedPlusOne > 0)
            {
                aircraft.speedKts = (float)(airspeedPlusOne - 1) * (subtype == 4 ? 4.0f : 1.0f);
                aircraft.courseDeg = (float)getBits(frame.bytes, 46, 10) * 360.0f / 1024.0f;
                aircraft.hasVelocity = true;
            }
        }

        const int verticalRatePlusOne = (int)getBits(frame.bytes, 69, 9);
        if (verticalRatePlusOne > 0)
        {
            int verticalRate = (verticalRatePlusOne - 1) * 64;
            if (getBits(frame.bytes, 68, 1) != 0)
            {
                verticalRate = -verticalRate;
            }
            aircraft.verticalRateFpm = verticalRate;
        }
    }

    static bool ingestExtendedSquitter(const DecodedModeSFrame& frame)
    {
        if (!frame.crcSelfValid || (frame.df != 17 && frame.df != 18 && frame.df != 19) || frame.address == 0)
        {
            return false;
        }

        AdsbAircraftState* aircraft = getAircraftState(frame.address);
        if (aircraft == nullptr)
        {
            return false;
        }

        const uint32_t nowMs = millis();
        aircraft->lastSeenMs = nowMs;
        const bool crcTrusted = noteCrcConfirmedFrame(*aircraft, nowMs);
        if (aircraft->address == 0)
        {
            return true;
        }
        const uint8_t typeCode = (uint8_t)getBits(frame.bytes, 32, 5);
        g_diag.lastTypeCode = typeCode;

        if (frame.df == 17)
        {
            ++g_diag.df17Accepted;
        }

        if (typeCode >= 1 && typeCode <= 4)
        {
            decodeAircraftId(*aircraft, frame);
        }
        else if ((typeCode >= 9 && typeCode <= 18) || (typeCode >= 20 && typeCode <= 22))
        {
            decodeAirbornePosition(*aircraft, frame, nowMs);
        }
        else if (typeCode == 19)
        {
            decodeAirborneVelocity(*aircraft, frame);
        }
        else
        {
            return true;
        }

        if (crcTrusted)
        {
            pushAircraftToContainer(*aircraft);
        }
        return true;
    }

    static bool ingestIdentityReply(const DecodedModeSFrame& frame)
    {
        if (!frame.addressParityValid || (frame.df != 5 && frame.df != 21) || frame.address == 0)
        {
            return false;
        }

        const int squawk = decodeSquawkModeA((uint16_t)getBits(frame.bytes, 19, 13));
        if (squawk < 0)
        {
            return false;
        }

        const uint32_t nowMs = millis();
        AdsbAircraftState* aircraft = findAircraftState(frame.address);
        if (aircraft == nullptr || !aircraftAcceptsAddressParity(*aircraft, nowMs))
        {
            ++g_diag.apSuppressed;
            return false;
        }

        aircraft->lastSeenMs = nowMs;
        aircraft->squawk = squawk;
        aircraft->hasSquawk = true;
        g_diag.lastSquawk = (uint16_t)squawk;
        ++g_diag.apAccepted;
        ++g_diag.squawkAccepted;
        pushAircraftToContainer(*aircraft);
        return true;
    }

    static bool ingestAltitudeReply(const DecodedModeSFrame& frame)
    {
        if (!frame.addressParityValid || (frame.df != 4 && frame.df != 20) || frame.address == 0)
        {
            return false;
        }

        const int altitudeFt = decodeAltitudeModeS13Ft((uint16_t)getBits(frame.bytes, 19, 13));
        if (altitudeFt == 0)
        {
            return false;
        }

        if (altitudeFt < -1200 || altitudeFt > 60000)
        {
            return false;
        }

        const uint32_t nowMs = millis();
        AdsbAircraftState* aircraft = findAircraftState(frame.address);
        if (aircraft == nullptr || !aircraftAcceptsAddressParity(*aircraft, nowMs))
        {
            ++g_diag.apSuppressed;
            return false;
        }

        aircraft->lastSeenMs = nowMs;
        aircraft->altitudeFt = altitudeFt;
        aircraft->hasAltitude = true;
        ++g_diag.apAccepted;
        pushAircraftToContainer(*aircraft);
        return true;
    }

    static bool processSampledSlots(uint8_t phaseSlots[kPhaseCount][kMaxSlots], size_t slotCount)
    {
        DecodedModeSFrame frame = {};
        if (!decodeBestModeSFrame(phaseSlots, slotCount, frame))
        {
            ++g_diag.packetsRejected;
            return false;
        }

        if (!frameParityAccepted(frame))
        {
            ++g_diag.crcRejected;
            ++g_diag.packetsRejected;
            return false;
        }

        bool accepted = false;
        if (frame.df == 17 || frame.df == 18 || frame.df == 19)
        {
            accepted = ingestExtendedSquitter(frame);
        }
        else if (frame.df == 5 || frame.df == 21)
        {
            accepted = ingestIdentityReply(frame);
        }
        else if (frame.df == 4 || frame.df == 20)
        {
            accepted = ingestAltitudeReply(frame);
        }
        else
        {
            accepted = frame.crcSelfValid;
        }

        if (!accepted)
        {
            ++g_diag.packetsRejected;
            return false;
        }

        g_diag.lastDf = frame.df;
        g_diag.lastPacketBits = frame.bitLen;
        g_diag.lastPhase = frame.phase;
        g_diag.lastAddress = frame.address & 0xFFFFFFUL;
        ++g_diag.packetsAccepted;
        g_diag.lastPacketMs = millis();
        pulseOkPin();
        return true;
    }

    static bool captureCandidateSlots(uint8_t phaseSlots[kPhaseCount][kMaxSlots], size_t slotCount)
    {
        uint8_t previous = readRxPinFast();
        const uint32_t startCycle = cpuCyclesNow();
        const uint32_t budgetCycles = g_cyclesPerUs * kPollSliceUs;
        uint32_t previousCycle = cpuCyclesNow();

        while (ADSBReceiverESP32_internalEnabled())
        {
            const uint8_t current = readRxPinFast();
            const uint32_t currentCycle = cpuCyclesNow();
            if (current != 0 && previous == 0)
            {
                const uint32_t riseCycle = previousCycle + ((uint32_t)(currentCycle - previousCycle) / 2U);
                if (captureSlotsFromEdges(riseCycle, phaseSlots, slotCount))
                {
                    return true;
                }
                previous = readRxPinFast();
                previousCycle = cpuCyclesNow();
                continue;
            }
            previous = current;
            previousCycle = currentCycle;

            if ((uint32_t)(currentCycle - startCycle) >= budgetCycles)
            {
                return false;
            }
        }

        return false;
    }

    static bool preambleLooksValid(const uint8_t* slots, size_t slotCount)
    {
        uint8_t preambleMismatches = 0;
        return findPreamble(slots, slotCount, &preambleMismatches) >= 0;
    }

    static bool preambleLooksValidAnyPhase(uint8_t phaseSlots[kPhaseCount][kMaxSlots], size_t slotCount)
    {
        if (findSoftPreamble(phaseSlots, slotCount, nullptr, nullptr) >= 0)
        {
            return true;
        }

        for (uint8_t phase = 0; phase < kPhaseCount; ++phase)
        {
            if (preambleLooksValid(phaseSlots[phase], slotCount))
            {
                return true;
            }
        }
        return false;
    }

#if ADSB_ESP32_HAS_I2S_DMA
    static uint8_t dmaSampleBit(const uint32_t* words, size_t sampleIndex, bool lsbFirst)
    {
        const uint32_t word = words[sampleIndex >> 5U];
        const uint8_t bitIndex = (uint8_t)(sampleIndex & 31U);
        const uint8_t shift = lsbFirst ? bitIndex : (uint8_t)(31U - bitIndex);
        return (word & (1UL << shift)) ? 1U : 0U;
    }

    static bool buildPhaseSlotsFromDma(const uint32_t* words,
                                       size_t sampleCount,
                                       size_t startSample,
                                       bool lsbFirst,
                                       uint8_t phaseSlots[kPhaseCount][kMaxSlots],
                                       size_t slotCount)
    {
        if (kDmaSamplesPerHalfSlot < 2U)
        {
            return false;
        }

        const size_t neededSamples = (slotCount + 1U) * (size_t)kDmaSamplesPerHalfSlot;
        if (startSample + neededSamples >= sampleCount)
        {
            return false;
        }

        const uint8_t center = kDmaSamplesPerHalfSlot / 2U;
        uint8_t delta = kDmaSamplesPerHalfSlot / 4U;
        if (delta == 0U)
        {
            delta = 1U;
        }

        for (size_t slot = 0; slot < slotCount; ++slot)
        {
            const size_t base = startSample + slot * (size_t)kDmaSamplesPerHalfSlot + center;
            phaseSlots[0][slot] = dmaSampleBit(words, base - delta, lsbFirst);
            phaseSlots[kPhaseCenter][slot] = dmaSampleBit(words, base, lsbFirst);
            phaseSlots[2][slot] = dmaSampleBit(words, base + delta, lsbFirst);
        }
        return true;
    }

    static uint8_t dmaPhaseHighCountAt(const uint32_t* words, size_t sampleCount, size_t startSample, size_t slot, bool lsbFirst)
    {
        const uint8_t center = kDmaSamplesPerHalfSlot / 2U;
        uint8_t delta = kDmaSamplesPerHalfSlot / 4U;
        if (delta == 0U)
        {
            delta = 1U;
        }

        const size_t base = startSample + slot * (size_t)kDmaSamplesPerHalfSlot + center;
        if (base < delta || base + delta >= sampleCount)
        {
            return 0U;
        }

        uint8_t count = 0;
        count += dmaSampleBit(words, base - delta, lsbFirst);
        count += dmaSampleBit(words, base, lsbFirst);
        count += dmaSampleBit(words, base + delta, lsbFirst);
        return count;
    }

    static bool dmaLooksLikeSoftPreamble(const uint32_t* words, size_t sampleCount, size_t startSample, bool lsbFirst)
    {
        static const uint8_t kRequiredHigh[] = {0, 2, 7, 9};
        static const uint8_t kQuietLow[] = {4, 5, 6, 11, 12, 13, 14, 15};

        if (kDmaSamplesPerHalfSlot < 2U ||
            startSample + (size_t)(kPreambleSlots + 1U) * (size_t)kDmaSamplesPerHalfSlot >= sampleCount)
        {
            return false;
        }

        uint8_t hardErrors = 0;
        uint8_t weak = 0;
        for (uint8_t i = 0; i < sizeof(kRequiredHigh); ++i)
        {
            const uint8_t count = dmaPhaseHighCountAt(words, sampleCount, startSample, kRequiredHigh[i], lsbFirst);
            if (count == 0U)
            {
                ++hardErrors;
            }
            else if (count < 2U)
            {
                ++weak;
            }
        }

        for (uint8_t i = 0; i < sizeof(kQuietLow); ++i)
        {
            const uint8_t count = dmaPhaseHighCountAt(words, sampleCount, startSample, kQuietLow[i], lsbFirst);
            if (count >= 2U)
            {
                ++hardErrors;
            }
            else if (count != 0U)
            {
                ++weak;
            }
        }

        return hardErrors <= 1U && ((uint16_t)hardErrors * 20U + (uint16_t)weak * 3U) <= 26U;
    }

    static size_t processDmaWords(const uint32_t* words, size_t wordCount, bool lsbFirst)
    {
        if (words == nullptr || wordCount == 0)
        {
            return 0;
        }

        const size_t sampleCount = wordCount * 32U;
        const size_t neededSamples = (size_t)kFullSampleSlots * (size_t)kDmaSamplesPerHalfSlot;
        if (sampleCount <= neededSamples + 2U)
        {
            return 0;
        }

        size_t acceptedCount = 0;
        size_t candidateCount = 0;
        uint8_t phaseSlots[kPhaseCount][kMaxSlots];
        uint8_t previous = dmaSampleBit(words, 0, lsbFirst);
        for (size_t sample = 1; sample + neededSamples + 2U < sampleCount; ++sample)
        {
            if ((sample & 0x7FFU) == 0U)
            {
                esp_task_wdt_reset();
            }

            const uint8_t current = dmaSampleBit(words, sample, lsbFirst);
            if (current != 0 && previous == 0)
            {
                if (!dmaLooksLikeSoftPreamble(words, sampleCount, sample, lsbFirst))
                {
                    previous = current;
                    continue;
                }

                ++candidateCount;
                if (candidateCount > kDmaMaxCandidatesPerChunk)
                {
                    break;
                }

                if (buildPhaseSlotsFromDma(words, sampleCount, sample, lsbFirst, phaseSlots, kFullSampleSlots))
                {
                    ++g_diag.sampledFrames;
                    g_diag.lastSampleSlots = kFullSampleSlots;
                    if (preambleLooksValidAnyPhase(phaseSlots, kFullSampleSlots))
                    {
                        if (processSampledSlots(phaseSlots, kFullSampleSlots))
                        {
                            ++acceptedCount;
                            sample += neededSamples;
                            previous = 0;
                            continue;
                        }
                    }
                    else
                    {
                        ++g_diag.preambleRejected;
                        ++g_diag.packetsRejected;
                    }
                }
            }
            previous = current;
        }
        return acceptedCount;
    }

    static bool setupDmaCapture()
    {
        if (kDmaSamplesPerHalfSlot < 2U)
        {
            return false;
        }

        i2s_config_t config = {};
        config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
        config.sample_rate = (int)(kDmaBitRateHz / 32UL);
        config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
        config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
        config.communication_format = (i2s_comm_format_t)1;
        config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
        config.dma_buf_count = 6;
        config.dma_buf_len = 256;
        config.use_apll = false;
        config.tx_desc_auto_clear = false;
        config.fixed_mclk = 0;

        const i2s_port_t port = (i2s_port_t)0;
        if (i2s_driver_install(port, &config, 0, nullptr) != ESP_OK)
        {
            return false;
        }

        i2s_pin_config_t pins;
        memset(&pins, 0xFF, sizeof(pins));
        pins.bck_io_num = ADSB_ESP32_DMA_BCLK_PIN;
        pins.ws_io_num = ADSB_ESP32_DMA_WS_PIN;
        pins.data_out_num = I2S_PIN_NO_CHANGE;
        pins.data_in_num = ADSB_ESP32_RX_PIN;
        if (i2s_set_pin(port, &pins) != ESP_OK)
        {
            i2s_driver_uninstall(port);
            return false;
        }

        i2s_zero_dma_buffer(port);
        return i2s_start(port) == ESP_OK;
    }

    static void adsbDmaTask(void* param)
    {
        (void)param;
        esp_task_wdt_add(NULL);
        const i2s_port_t port = (i2s_port_t)0;
        uint32_t words[kDmaReadWords];
        uint32_t startMs = millis();
        uint32_t acceptedAtStart = g_diag.packetsAccepted;

        for (;;)
        {
            esp_task_wdt_reset();

            if (!ADSBReceiverESP32_internalEnabled() || !g_dmaCaptureActive)
            {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            size_t bytesRead = 0;
            const esp_err_t rc = i2s_read(port, words, sizeof(words), &bytesRead, pdMS_TO_TICKS(50));
            if (rc != ESP_OK || bytesRead == 0)
            {
                if ((millis() - startMs) > 3000UL)
                {
                    i2s_stop(port);
                    i2s_driver_uninstall(port);
                    g_dmaCaptureActive = false;
                }
                continue;
            }

            const size_t wordCount = bytesRead / sizeof(words[0]);
            processDmaWords(words, wordCount, false);
            vTaskDelay(1);

            if ((millis() - startMs) > 15000UL && g_diag.packetsAccepted == acceptedAtStart)
            {
                i2s_stop(port);
                i2s_driver_uninstall(port);
                g_dmaCaptureActive = false;
            }
        }
    }
#endif

    static void adsbRxTask(void* param)
    {
        (void)param;
        esp_task_wdt_add(NULL);
        g_diag.taskRunning = true;
        uint8_t phaseSlots[kPhaseCount][kMaxSlots];

        for (;;)
        {
            esp_task_wdt_reset();

            if (!ADSBReceiverESP32_internalEnabled())
            {
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }

            if (!captureCandidateSlots(phaseSlots, kFullSampleSlots))
            {
                vTaskDelay(1);
                continue;
            }

            ++g_diag.sampledFrames;
            g_diag.lastSampleSlots = kFullSampleSlots;
            if (!preambleLooksValidAnyPhase(phaseSlots, kFullSampleSlots))
            {
                ++g_diag.preambleRejected;
                ++g_diag.packetsRejected;
                vTaskDelay(1);
                continue;
            }

            processSampledSlots(phaseSlots, kFullSampleSlots);
            vTaskDelay(1);
        }
    }

    static bool startGpioSamplerTask()
    {
        if (g_adsbTask != nullptr)
        {
            return true;
        }

        const BaseType_t rc = xTaskCreatePinnedToCore(adsbRxTask, "ADSB1090", 8192, nullptr, 4, &g_adsbTask, 0);
        if (rc != pdPASS)
        {
            g_diag.ready = false;
            g_adsbTask = nullptr;
            return false;
        }
        return true;
    }

    static bool startDmaCaptureTask()
    {
#if ADSB_ESP32_HAS_I2S_DMA
        if (g_dmaTask != nullptr)
        {
            g_dmaCaptureActive = true;
            return true;
        }

        if (!setupDmaCapture())
        {
            g_dmaCaptureActive = false;
            return false;
        }

        g_dmaCaptureActive = true;
        const BaseType_t rc = xTaskCreatePinnedToCore(adsbDmaTask, "ADSB1090DMA", 8192, nullptr, 2, &g_dmaTask, 0);
        if (rc != pdPASS)
        {
            g_dmaCaptureActive = false;
            g_dmaTask = nullptr;
            i2s_driver_uninstall((i2s_port_t)0);
            return false;
        }
        return true;
#else
        return false;
#endif
    }
}

bool ADSBReceiverESP32_internalEnabled()
{
    return settings != nullptr && settings->adsb_receiver == ADSB_RECEIVER_INTERNAL;
}

uint16_t ADSBReceiverESP32_thresholdPwmDuty()
{
    return thresholdSettingToDuty();
}

void ADSBReceiverESP32_applySettings()
{
    const uint16_t duty = thresholdSettingToDuty();
    if (!setupThresholdPwm())
    {
        Serial.println(F("[ADSB] threshold PWM attach failed on GPIO48"));
        return;
    }
    if (duty == g_lastThresholdPwmDuty)
    {
        return;
    }

    writeThresholdPwm(duty);
    g_lastThresholdPwmDuty = duty;
    g_diag.thresholdPwmDuty = duty;
    Serial.printf("[ADSB] threshold PWM GPIO%d channel=%u duty=%u level=%d mV\r\n",
                  ADSB_ESP32_THRESHOLD_PWM_PIN,
                  kThresholdPwmChannel,
                  duty,
                  (int)(settings ? settings->threshold_level : 910));
}

void ADSBReceiverESP32_setup()
{
    memset(&g_diag, 0, sizeof(g_diag));
    memset(g_aircraft, 0, sizeof(g_aircraft));

    const uint32_t cpuMHz = (uint32_t)ESP.getCpuFreqMHz();
    g_cyclesPerUs = (cpuMHz > 0U) ? cpuMHz : 240UL;
    g_cyclesPerHalfUs = (g_cyclesPerUs > 1U) ? (g_cyclesPerUs / 2U) : 120UL;
    if (g_cyclesPerHalfUs == 0)
    {
        g_cyclesPerHalfUs = 120UL;
    }
    g_cyclesPerQuarterUs = g_cyclesPerHalfUs / 2U;
    if (g_cyclesPerQuarterUs == 0)
    {
        g_cyclesPerQuarterUs = 1U;
    }
    pinMode(ADSB_ESP32_RX_PIN, INPUT_PULLDOWN);
    pinMode(ADSB_ESP32_OK_PULSE_PIN, OUTPUT);
    digitalWrite(ADSB_ESP32_OK_PULSE_PIN, HIGH);
    ADSBReceiverESP32_applySettings();

    g_diag.ready = true;
    if (!startDmaCaptureTask())
    {
        startGpioSamplerTask();
    }
}

void ADSBReceiverESP32_loop()
{
    static uint32_t lastSettingsApplyMs = 0;
    const uint32_t nowMs = millis();

    if ((nowMs - lastSettingsApplyMs) >= 500UL)
    {
        lastSettingsApplyMs = nowMs;
        ADSBReceiverESP32_applySettings();
    }

    if (ADSBReceiverESP32_internalEnabled() && !g_dmaCaptureActive && g_adsbTask == nullptr)
    {
        startGpioSamplerTask();
    }
}

bool ADSBReceiverESP32_getDiag(ADSBReceiverESP32Diag& outDiag)
{
    outDiag = g_diag;
    outDiag.thresholdPwmDuty = ADSBReceiverESP32_thresholdPwmDuty();
    return g_diag.ready;
}
