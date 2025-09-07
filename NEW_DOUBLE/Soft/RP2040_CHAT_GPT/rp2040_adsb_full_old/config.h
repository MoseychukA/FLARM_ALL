#pragma once

// USB
#define USB_BAUDRATE       115200

// UART1 -> ESP32S3
#define UART2_TX_PIN       4
#define UART2_RX_PIN       5
#define UART2_BAUDRATE     115200

// ADS-B входы (три канала)
#define PIO_PIN_CH1        18
#define PIO_PIN_CH2        19
#define PIO_PIN_CH3        22

// Индикация преамбул
#define PREAMBLE_LED1      17
#define PREAMBLE_LED2      20
#define PREAMBLE_LED3      23

// Индикация ядер
#define LED_CORE0          15
#define LED_CORE1          25

// RSSI (ADC0) — GPIO26
#define RSSI_PIN           26

// Частота дискретизации: 2 МГц (0.5 µs/сэмпл)
#define SAMPLE_RATE_HZ     2000000.0f

// DMA кольца: 8 сегментов * 512 слов (по 32 бита)
#define RING_SEGMENTS      8
#define SEGMENT_WORDS      512
#define RING_WORDS         (RING_SEGMENTS * SEGMENT_WORDS)

// Коррелятор/демодуляция
#define PRE_SAMPLES        80         // 8 мкс окно преамбулы
#define BIT_SAMPLES        2          // 1 бит = 1 мкс = 2 отсчёта
#define MAX_BITS           112        // максимум (56/112)
#define CPR_PAIR_WINDOW_MS 10000      // окно even/odd 10 c

// Профили по умолчанию
#define DEFAULT_PROFILE    0          // 0=Normal, 1=Urban, 2=HighEMI, 3=Remote

// Хранилище
#define FS_FILENAME        "/config.txt"

// Вывод
#define DEFAULT_OUTPUT_FMT 1          // 0=RAW,1=JSON,2=CSV,3=NMEA
