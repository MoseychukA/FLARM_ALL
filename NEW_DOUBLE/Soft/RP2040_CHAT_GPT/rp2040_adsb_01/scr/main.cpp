#include <Arduino.h>
#include "config.h"
#include "adsb_decoder.h"
#include "adsb_crc.h"
#include "adsb_cpr.h"
#include "adsb_gillham.h"

// Задачи FreeRTOS на ядрах
TaskHandle_t tBlink0, tBlink1, tProc;

void blinkTaskCore0(void *param) {
  pinMode(LED_CORE0, OUTPUT);
  for (;;) {
    gpio_put(LED_CORE0, !gpio_get(LED_CORE0));
    vTaskDelay(pdMS_TO_TICKS(CORE0_LED_PERIOD_MS));
  }
}
void blinkTaskCore1(void *param) {
  pinMode(LED_CORE1, OUTPUT);
  for (;;) {
    gpio_put(LED_CORE1, !gpio_get(LED_CORE1));
    vTaskDelay(pdMS_TO_TICKS(CORE1_LED_PERIOD_MS));
  }
}

void processorTask(void *param) {
  for (;;) {
    // обработка очереди собранных пакетов
    AdsbPacket p;
    while (adsb_dma_fetch(&p)) {
      // Отладочный вывод RAW до CRC (опционально — у нас CRC уже проверен в сборке)
      if (OUTPUT_RAW_BEFORE_CRC) {
        Serial1.print("RAW["); Serial1.print(p.bits); Serial1.print("]: ");
        int nBytes = (p.bits+7)/8;
        for (int i=0;i<nBytes;i++) {
          if (i) Serial1.print(' ');
          Serial1.printf("%02X", p.raw[i]);
        }
        Serial1.println();
      }

      DecodedADSB fo{};
      if (adsb_parse(p, &fo)) {
        // USB вывод
        if (OUTPUT_JSON) {
          Serial1.printf("{\"icao\":\"%06X\",\"flt\":\"%s\",\"lat\":%.6f,\"lon\":%.6f,\"alt_ft\":%.1f,"
                         "\"spd_kt\":%.1f,\"trk_deg\":%.1f,\"vr_fpm\":%d,\"sqwk\":%d}\n",
                         fo.addr, fo.flight, fo.latitude, fo.longitude, fo.altitude/0.3048f,
                         fo.speed, fo.course, fo.vert_rate, fo.Squawk);
        }
        // Отправка бинарно в ESP32S3:
        Serial2.write((uint8_t*)&fo, sizeof(fo));
      }
    }
    // Обработать каналы (в идеале — в другой задаче/ядре, но у нас pio+dma работают сами)
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void setup() {
  // USB Serial
  Serial1.setTX(0); // USB CDC в Arduino core на RP2040 — Serial
  Serial1.begin(USB_BAUD);
  Serial1.println("RP2040 ADS-B Receiver Boot");

  // UART2 к ESP32S3
  Serial2.begin(UART2_BAUD, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);

  pinMode(RSSI_PIN, INPUT);

  // Инициализации
  adsb_gillham_init();
  cpr_reset();
  adsb_correlator_init();
  adsb_dma_init();

  // Задачи
  xTaskCreatePinnedToCore(blinkTaskCore0, "blink0", 2048, NULL, 1, &tBlink0, 0);
  xTaskCreatePinnedToCore(blinkTaskCore1, "blink1", 2048, NULL, 1, &tBlink1, 1);
  xTaskCreatePinnedToCore(processorTask, "proc", 4096, NULL, 2, &tProc, 0);

  Serial1.println("Init done.");
}

void loop() {
  // «Приём на ядре 1 / обработка на ядре 0» достигается задачами; тут только сервис
  // Фоновая параллельная обработка трёх каналов:
  // Для простоты дергаем сервис тут:
  // (Можно вынести в отдельную задачу на core1 и разнести correlator/assembler.)
}
