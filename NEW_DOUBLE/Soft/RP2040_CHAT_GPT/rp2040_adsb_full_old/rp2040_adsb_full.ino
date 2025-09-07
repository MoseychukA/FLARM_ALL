/*
Структура проекта rp2040_adsb_full
rp2040_adsb_full/
│
├── rp2040_adsb_full.ino     // Главный файл Arduino (setup, loop)
│
├── adsb_structs.h           // Общие структуры данных (DecodedADSB, ToDUMP1090)
├── adsb_packet.h            // Определения сырых ADS-B пакетов, обработка бит
├── adsb_packet.cpp
│
├── adsb_cpr.h               // CPR-декодер (широта/долгота из even/odd сообщений)
├── adsb_cpr.cpp
│
├── adsb_correlator.h        // Коррелятор и фильтры
├── adsb_correlator.cpp
│
├── adsb_sampler.pio         // PIO-программа для выборки сигналов
├── adsb_sampler.h           // Интерфейс работы с PIO
├── adsb_sampler.cpp
│
├── adsb_dma.h               // Настройка DMA-кольца
├── adsb_dma.cpp
│
├── adsb_decoder.h           // Высокоуровневый парсер сообщений
├── adsb_decoder.cpp
│
├── adsb_tx.h                // Передача данных в ESP32S3
├── adsb_tx.cpp
│
└── utils.h                  // Вспомогательные функции (CRC, таймеры и т.п.)
    utils.cpp

Роли файлов
rp2040_adsb_full.ino — инициализация, запуск FreeRTOS-задач (или loop), вызов пайплайна (PIO → DMA → коррелятор → декодер → CPR → отправка в ESP32 через Serial2).
adsb_structs.h — структура DecodedADSB (внутреннее представление) и структура ToDUMP1090 (для отправки в ESP32).
adsb_packet.* — работа с битами пакета: хранение raw-данных, проверка CRC, парсинг полей.
adsb_cpr.* — декодирование координат (Compact Position Reporting, глобальный/локальный режим).
adsb_correlator.* — фильтрация, корреляция сигналов, suppression mask, dead-time.
adsb_sampler.pio/.h/.cpp — PIO-программа для захвата сигналов ADS-B с GPIO.
adsb_dma.* — настройка DMA-кольца для PIO-сэмплов.
adsb_decoder.* — высокоуровневая сборка: берёт сэмплы, гонит через коррелятор, собирает пакеты.
adsb_tx.* — упаковка в ToDUMP1090 и немедленная передача через Serial2.
utils.* — функции CRC, таймеры, отладка.

*/


#include <Arduino.h>
#include "config.h"
#include "adsb_types.h"
#include "adsb_pipeline.h"
#include "cli.h"
#include "profiles.h"
#include "filters.h"
#include "outputs.h"
#include "storage.h"
#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "adsb_gillham.h"
#include "adsb_packet.h"
#include "adsb_packet.h"
#include "adsb_tx.h"
#include "adsb_structs.h"

//// эта функция вызывается, когда успешно принят и декодирован ADS-B пакет
//void onDecodedPacket(const DecodedADSB& rx) {
//    ToDUMP1090 pkt{};
//    pkt.addr = rx.addr;
//    pkt.squawk = rx.Squawk;     // заглавная S
//    strncpy(pkt.flight, rx.flight, sizeof(pkt.flight));
//    pkt.altitude = rx.altitude;
//    pkt.speed = rx.speed;
//    pkt.track = rx.course;     // заменили track → course
//    pkt.vert_rate = rx.vert_rate;
//    pkt.lat = rx.lat_msg;    // заменили lat → lat_msg
//    pkt.lon = rx.lon_msg;    // заменили lon → lon_msg
//    pkt.seen_time = 0; //rx.seen_time;
//    pkt.endOfPacket[0] = (char)0xFF;
//    pkt.endOfPacket[1] = (char)0xFF;
//    pkt.endOfPacket[2] = (char)0xFF;
//
//    Serial2.write(reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt));
//}



//!!extern void uartInit();
//!!extern void uartSendPacket(const ToDUMP1090& pkt);



static void core1_entry() {
  gpio_init(LED_CORE1); gpio_set_dir(LED_CORE1, GPIO_OUT);
  uint32_t t0 = to_ms_since_boot(get_absolute_time());
  bool lvl=false;
  for(;;){
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - t0 >= 500) { t0=now; lvl=!lvl; gpio_put(LED_CORE1,lvl); }
    tight_loop_contents();
  }
}

static void frame_cb(int ch, const DecodedADSB& d){
  // Фильтрация
  if (!filtersAllow(d)) return;
  // Вывод в USB (в формате, заданном CLI)
  outputsPrint(d);
  // Бинарная отправка в ESP32S3 по UART1
  Serial1.write(reinterpret_cast<const uint8_t*>(&d), sizeof(d));
  // Вспышка светодиода канала
  uint pin = (ch==0?PREAMBLE_LED1:(ch==1?PREAMBLE_LED2:PREAMBLE_LED3));
  gpio_put(pin,1); busy_wait_us_32(80); gpio_put(pin,0);
}

void setup() {

    Serial.begin(115200);
    unsigned long t0 = millis(); while (!Serial && !Serial.dtr() && (millis() - t0) < 8000) delay(10);
    delay(2000);
    Serial.print("Software ");
    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    ver_soft.remove(val_srt);
    Serial.println(ver_soft);
/*
  // UART1 -> ESP32S3
  Serial1.setTX(UART2_TX_PIN);
  Serial1.setRX(UART2_RX_PIN);
  Serial1.begin(UART2_BAUDRATE);
  */
  uartInit();
    Serial.println("RP2040 ADS-B TX ready");

  // GPIO индикации
  gpio_init(LED_CORE0); gpio_set_dir(LED_CORE0, GPIO_OUT); gpio_put(LED_CORE0,0);
  gpio_init(PREAMBLE_LED1); gpio_set_dir(PREAMBLE_LED1, GPIO_OUT); gpio_put(PREAMBLE_LED1,0);
  gpio_init(PREAMBLE_LED2); gpio_set_dir(PREAMBLE_LED2, GPIO_OUT); gpio_put(PREAMBLE_LED2,0);
  gpio_init(PREAMBLE_LED3); gpio_set_dir(PREAMBLE_LED3, GPIO_OUT); gpio_put(PREAMBLE_LED3,0);

  // RSSI (опционально)
  analogReadResolution(12);
  pinMode(RSSI_PIN, INPUT);

  gillhamInit();  // построение таблицы Q=0
  Serial.println("Gillham таблица готова!");

  // Хранилище (LittleFS): загрузить настройки
  storageInit();
  storageLoad();

  // Инициализация подсистем
  profilesInit();   // установит профиль по умолчанию/из flash
  outputsInit();    // формат вывода
  filtersInit();    // фильтры

  // Пайплайн приема/декода (PIO+DMA+коррелятор+декодер)
  pipeline_init(frame_cb);

  // Ядро 1 — индикация
  multicore_launch_core1(core1_entry);

  Serial.println("{\"msg\":\"RP2040 ADS-B Receiver ready\"}");
}

void loop() {
  // Неблокирующая обработка входных колец DMA
  pipeline_poll_once();

  // CLI командами можно управлять параметрами/профилями/выводом/фильтрами
  cliTask();

  // Ядро 0: моргание
  static uint32_t t0=millis(); static bool lvl=false;
  if (millis()-t0 >= 1000) { t0=millis(); lvl=!lvl; gpio_put(LED_CORE0,lvl); }

  // быстрый опрос RSSI (если подключено)
  (void)analogRead(RSSI_PIN);
  tight_loop_contents();
    
}

