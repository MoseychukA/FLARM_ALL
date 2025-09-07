#pragma once
#include <Arduino.h>

// ======= GPIOs =======
#define PIN_CH1   18
#define PIN_CH2   19
#define PIN_CH3   22

#define PIN_PRE1  17
#define PIN_PRE2  20
#define PIN_PRE3  23

#define PIN_RSSI  26

// UART (RP2040 -> ESP32S3)
#define UART_TX   4
#define UART_RX   5
#define UART_BAUD 115200

// USB log & CLI — стандартный Serial на 115200
#define USB_BAUD  115200

// Blink pins
#define LED_CORE0 15
#define LED_CORE1 25

// ======= Приём ADS-B =======
#define SAMPLE_PERIOD_US     0.5f   // целевой шаг дискретизации 0.5 мкс
#define PIO_TARGET_HZ        (uint32_t)(1.0f / (SAMPLE_PERIOD_US*1e-6f)) // 2 MHz

// Коррелятор преамбулы
#define PRE_WIN_SAMPLES      80     // 8 мкс @ 0.1 мкс условн. шкала — у нас нормировано ниже
#define DEAD_TIME_US         8       // 6–8 мкс
#define DEAD_TIME_SAMPLES    (uint32_t)(DEAD_TIME_US / SAMPLE_PERIOD_US)

#define PRE_MASK_LONG_US     0.7f
#define PRE_MIN_POSHITS      2       // min hits per window

// CPR таймаут между even/odd (глобальная декодировка)
#define CPR_PAIR_WINDOW_MS   10000

// Очереди
#define PACKET_QUEUE_LEN     64
#define DMA_RING_WORDS       4096    // по 32-бит словам на канал
#define MAX_PACKETS_PER_FETCH 16

// Профили шума
enum Profile {
  PROFILE_NORMAL,
  PROFILE_HIGH_EMI,
  PROFILE_URBAN,
  PROFILE_REMOTE
};

//// === Profiles ===
//enum ProfileMode { PROFILE_NORMAL, PROFILE_HIGH_EMI, PROFILE_URBAN, PROFILE_REMOTE };



struct RuntimeConfig {
  Profile profile = PROFILE_NORMAL;
  float   base_thr = 6.5f;
  float   k_neg    = 0.25f;
  bool    digital_filter_on = true;
  uint32_t pio_clk_hz = PIO_TARGET_HZ;
};

// NVS (LittleFS) файл
#define CONFIG_FILE "/config.txt"

// Включить отладочный RAW/CSV поток (до CRC)
#define DEBUG_DUMP_RAW 1
