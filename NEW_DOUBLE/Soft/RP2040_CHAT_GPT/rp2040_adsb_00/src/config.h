#pragma once

// ===== Serial =====
#define USB_BAUD            115200
#define ESP32_BAUD          115200

// ===== UART with ESP32-S3 (Serial2) =====
#define ESP32_UART          Serial2
#define ESP32_TX_PIN        4
#define ESP32_RX_PIN        5

// ===== ADS-B input pins (three independent channels) =====
#define PIN_CH1             18
#define PIN_CH2             19
#define PIN_CH3             22

// ===== Preamble indicator LEDs =====
#define LED_PRE1            17
#define LED_PRE2            20
#define LED_PRE3            23

// ===== Heartbeat LEDs =====
#define LED_CORE0           15  // core 0, 1000 ms
#define LED_CORE1           25  // core 1,  500 ms

// ===== RSSI analog input =====
#define PIN_RSSI            26

// ===== PIO/DMA =====
// 0.5 µs resolution => >= 2 MHz sample rate. We'll use 4 MHz (0.25 µs/ts)
#define SAMPLE_HZ           4000000u

// ring buffer sizes (per channel)
#define RING_BYTES          (4096u)

// Max packets buffered
#define PACKET_QUEUE_DEPTH  64

// ===== Detection / filtering params =====
#define DEAD_TIME_US        7     // 6–8 µs recommended
#define PREAMBLE_SAMPLES    80    // 80 samples (20 µs @ 0.25 µs/ts)
#define BIT_TS              2     // 0.5 µs per bit = 2 samples @ 0.25 µs
#define DF_SHORT_BITS       56
#define DF_LONG_BITS        112

// digital filter thresholds (can be changed at runtime / profiles)
struct FilterConfig {
  bool   enabled = true;
  float  min_us = 0.3f;  // drop < 0.3 µs
  float  norm_us = 0.5f; // normalize length to 0.5 µs
  float  max_us = 0.7f;  // drop > 0.7 µs
};

enum Profile { PROFILE_NORMAL=0, PROFILE_HIGH_EMI=1, PROFILE_URBAN=2, PROFILE_REMOTE=3 };

// dynamic correlator thresholds (base + k*EMA)
struct CorrConfig {
  float base_thr = 8.0f;
  float k_ema    = 2.5f;
  uint8_t posHitsMin = 2;      // min hits in windows
  bool tighten_deltas_in_noise = true; // ±1 stricter under noise
};

extern FilterConfig g_filter;
extern CorrConfig   g_corr;
extern Profile      g_profile;

// Reference point for local CPR (degrees)
extern volatile float g_ref_lat;
extern volatile float g_ref_lon;

// Enable RAW dump to USB (pre-CRC)
extern volatile bool g_rawDump;

// Controls & persistence
void loadConfig();
void saveConfig();
void showConfig();
void setProfile(Profile p);

// Fast GPIO macros (no digitalWrite)
#include "hardware/gpio.h"
#define GPIO_SET(p)   gpio_put(p, 1)
#define GPIO_CLR(p)   gpio_put(p, 0)
#define GPIO_TGL(p)   gpio_put(p, !gpio_get_out_level(p))