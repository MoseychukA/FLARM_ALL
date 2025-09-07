#pragma once
#include <Arduino.h>

// ====== ПИНЫ ======
#define UART2_TX_PIN   4   // к ESP32S3
#define UART2_RX_PIN   5
#define USB_BAUD       115200
#define UART2_BAUD     115200

// Каналы приёма ADS-B (цифровой демодулятор/детектор — внешний)
#define CH1_PIN_IN     18
#define CH2_PIN_IN     19
#define CH3_PIN_IN     22

// Индикаторы preamble (по каналам)
#define CH1_PRE_LED    17
#define CH2_PRE_LED    20
#define CH3_PRE_LED    23

// Индикация по ядрам
#define LED_CORE0      15
#define LED_CORE1      25

// RSSI аналоговый вход
#define RSSI_PIN       26

// ====== ПАРАМЕТРЫ ОБРАБОТКИ ======
// Дискретизация (отсчётов/мкс) — 2 => шаг 0.5 мкс (бит = 1 мкс)
#define SAMPLES_PER_US       2
#define SAMPLE_RATE_HZ       (SAMPLES_PER_US * 1000000) // 2 MHz

// Параметры преамбулы: 8 мкс, пульсы в позициях (мкс) 0,1,3,4,6,7
// В отсчётах по 0.5 мкс => 0,2,6,8,12,14 (центры окон). Веса/маски ниже.
#define PRE_SAMPLES          (8 * SAMPLES_PER_US) // 16
#define BIT_US               1
#define BIT_SAMPLES          (BIT_US * SAMPLES_PER_US) // 2
#define LONG_BITS            112
#define SHORT_BITS           56

// Дедтайм после детекции (не проверять заново «хвост того же пакета»)
#define PREAMBLE_DEADTIME_SAMPLES (8 * SAMPLES_PER_US) // 16

// Длина окна корреляции преамбулы (взяли 80 полуотсчетов = 40 отсчетов при 0.5 мкс?)
// Здесь используем 80 * 0.1us? — упрощаем: весовая корреляция по 80 «семплам»
// но наш семпл 0.5мкс → эквивалентно 16…20 пунктам. Оставим 80 как «субокна».
#define PRE_WINDOW            80

// Динамический порог (EMA шума)
#define EMA_ALPHA_NUM         1
#define EMA_ALPHA_DEN         8
#define BASE_THR              4
#define K_THR                 2

// Маски подавления: запрещённые окна (между ожидаемыми импульсами)
#define MASK_UNEXPECTED_PENALTY  2

// Фильтр импульсов: длительность 0.4–0.7 мкс (при 0.5мкс семпле → допустимо 1–2 полусемпла)
// Здесь – цифровой фильтр на сырых семплах (можно отключать).
#define DIGIFLT_ENABLE_DEFAULT   true
#define MIN_PULSE_SAMPLES        1
#define MAX_PULSE_SAMPLES        2

// Профили среды помех
enum Profile {
  PROFILE_NORMAL,
  PROFILE_HIGH_EMI,
  PROFILE_URBAN,
  PROFILE_REMOTE
};
extern Profile g_profile;

// Опорная точка (локальный CPR) — скорректируйте под вашу позицию
#define REF_LAT       55.7558f
#define REF_LON       37.6173f

// DMA/кольца
#define DMA_RING_WORDS        4096   // на канал, 32-бит слов
#define MAX_PACKETS_QUEUE     64

// Форматы вывода
#define OUTPUT_RAW_BEFORE_CRC true
#define OUTPUT_JSON           true
#define OUTPUT_CSV            false
#define OUTPUT_NMEA           false

// NVS (LittleFS)
#define LFS_ENABLE            true
#define LFS_PATH              "/config.txt"

// Тайминги LED ядра
#define CORE0_LED_PERIOD_MS   1000
#define CORE1_LED_PERIOD_MS   500

// UART2 к ESP32 — формат бинарной структуры DecodedADSB (см. adsb_decoder.h)
