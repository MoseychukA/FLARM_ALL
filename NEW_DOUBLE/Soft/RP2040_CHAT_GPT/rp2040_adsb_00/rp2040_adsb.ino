#include "adsb_structs.h"
#include "adsb_cpr.h"
#include "adsb_crc.h"
#include "adsb_gillham.h"
#include "adsb_correlator.h"
#include "adsb_dma.h"

#include "TestSuite.cpp"

#define LED_CORE0 15
#define LED_CORE1 25

void blinkTaskCore0() {
  while (true) {
    gpio_put(LED_CORE0, !gpio_get(LED_CORE0));
    sleep_ms(1000);
  }
}

void blinkTaskCore1() {
  while (true) {
    gpio_put(LED_CORE1, !gpio_get(LED_CORE1));
    sleep_ms(500);
  }
}

void setup() {
  Serial1.begin(115200);  // USB Serial
  Serial2.begin(115200, SERIAL_8N1, 5, 4); // UART2 к ESP32S3

  gpio_init(LED_CORE0); gpio_set_dir(LED_CORE0, GPIO_OUT);
  gpio_init(LED_CORE1); gpio_set_dir(LED_CORE1, GPIO_OUT);

  adsb_gillham_init();
  adsb_dma_init();         // DMA+PIO для трёх каналов
  adsb_correlator_init();  // кореллятор

  Serial1.println("RP2040 ADS-B Receiver Start");

  // Запуск тасков
  multicore_launch_core1(blinkTaskCore1);
  xTaskCreatePinnedToCore([](void*) {
    blinkTaskCore0();
  }, "blink0", 2048, NULL, 1, NULL, 0);

  // Тесты
  runGillhamTests();
}

void loop() {
  // Основной цикл: извлекаем пакеты из DMA-очередей, обрабатываем CRC, CPR и отправляем
  AdsbPacket pkt;
  while (adsb_dma_fetch(&pkt)) {
    if (!adsb_crc_check(pkt.raw, pkt.bits)) {
      continue;
    }
    DecodedADSB fo = adsb_decode_packet(pkt);

    // Отправляем на ESP32S3
    Serial2.write((uint8_t*)&fo, sizeof(fo));

    // Отладка в USB
    Serial1.printf("ICAO=%06X FLT=%s ALT=%dft LAT=%.5f LON=%.5f SPD=%.1f\n",
      fo.addr, fo.flight, fo.altitude, fo.latitude, fo.longitude, fo.speed);
  }
}