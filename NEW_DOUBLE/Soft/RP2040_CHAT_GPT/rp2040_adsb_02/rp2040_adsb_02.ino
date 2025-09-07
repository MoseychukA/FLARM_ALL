#include <Arduino.h>
#include "config.h"
#include "adsb_decoder.h"
#include "adsb_crc.h"
#include "adsb_cpr.h"
#include "adsb_gillham.h"

void blinkCore0() {
    pinMode(LED_CORE0, OUTPUT);
    while (true) {
        digitalWrite(LED_CORE0, !digitalRead(LED_CORE0));
        delay(1000);
    }
}

void blinkCore1() {
    pinMode(LED_CORE1, OUTPUT);
    while (true) {
        digitalWrite(LED_CORE1, !digitalRead(LED_CORE1));
        delay(500);
    }
}


void setup() {
  //Serial.begin(USB_BAUD);            // USB лог+CLI
  //Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX, UART_TX); // к ESP32S3

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

    Serial2.setTX(UART_TX);
    Serial2.setRX(UART_RX);
    Serial2.begin(921600);


  pinMode(LED_CORE0, OUTPUT);
  pinMode(LED_CORE1, OUTPUT);
  pinMode(PIN_PRE1, OUTPUT);
  pinMode(PIN_PRE2, OUTPUT);
  pinMode(PIN_PRE3, OUTPUT);

  adsb_gillham_init();
  adsb_decoder_init();

  multicore_launch_core1(blinkCore1); // запустить на ядре 1

  Serial.println("RP2040 ADS-B Receiver Ready");
}

static void print_json(const DecodedADSB& fo) 
{
  Serial.printf("{\"icao\":\"%06X\",\"flt\":\"%s\",\"lat\":%.6f,\"lon\":%.6f,"
                "\"alt\":%.0f,\"spd\":%.1f,\"trk\":%.1f,\"vr\":%d,\"sq\":%d}\n",
                fo.addr, fo.flight, fo.latitude, fo.longitude,
                fo.altitude, fo.speed, fo.course, fo.vert_rate, fo.Squawk);
}

void loop() {
  AdsbPacket pkts[MAX_PACKETS_PER_FETCH];
  int got = 0;
  while (got < MAX_PACKETS_PER_FETCH && adsb_decoder_fetch(&pkts[got])) got++;

  for (int i=0;i<got;i++) {
    auto &p = pkts[i];
    // Индикаторы преамбулы по каналам (краткий импульс)
    if (p.channel==1) { gpio_put(PIN_PRE1, 1); delayMicroseconds(10); gpio_put(PIN_PRE1,0); }
    if (p.channel==2) { gpio_put(PIN_PRE2, 1); delayMicroseconds(10); gpio_put(PIN_PRE2,0); }
    if (p.channel==3) { gpio_put(PIN_PRE3, 1); delayMicroseconds(10); gpio_put(PIN_PRE3,0); }

    // Проверка CRC здесь уже пройдена/исправлена в декодере.
    DecodedADSB fo = adsb_decode_packet(p);

    // Вывод в USB
    print_json(fo);

    // Отправка бинарно на ESP32S3
    Serial2.write((const uint8_t*)&fo, sizeof(fo));
  }
}
