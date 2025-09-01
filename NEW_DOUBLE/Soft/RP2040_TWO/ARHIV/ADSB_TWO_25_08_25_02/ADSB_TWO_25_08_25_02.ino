#include "Arduino.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "adsbee.h"
#include "bsp.h"
#include "data_structures.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include "decode.h"
#include "shared.h"


BSP bsp = BSP({});
ADSBee adsbee = ADSBee({});

/* Тест-индикаторы */
const int ledPin = 15;
int ledState = LOW;
unsigned long previousMillis = 0;
const long interval = 1000;

const int ledPin1 = 25;
int ledState1 = LOW;
unsigned long previousMillis1 = 0;
const long interval1 = 300;
static constexpr uint8_t UART2_TX = 4;
static constexpr uint8_t UART2_RX = 5;
//============================================================================================================

bool receive_packet(Raw1090Packet* pkt) 
{
    static uint32_t counter = 0;
    if (counter > 10) return false; // имитация окончания
    counter++;

    // Заполняем набором данных
    for (int i = 0; i < Raw1090Packet::kMaxPacketLenWords32; i++) 
    {
        pkt->buffer[i] = i + counter;
    }
    pkt->buffer_len_bits = 8 * 12; // 12 слов
    return true;
}


void dump_buffer_in_hex(const uint8_t* data, size_t len_bytes) {
    for (size_t i = 0; i < len_bytes; i++) {
        if (i > 0) Serial.print(" ");
        if (data[i] < 16) Serial.print("0");
        Serial.print(data[i], HEX);
    }
    Serial.println();
}



//=============================================================================================================

/* setup/loop */
void setup() {
    bi_decl(bi_program_description("ADSBee 1090 ADSB Receiver"));
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
   
    Serial2.setTX(UART2_TX);
    Serial2.setRX(UART2_RX);
    Serial2.begin(921600);

    adsbee.Init();
    pinMode(ledPin, OUTPUT);

    Serial.println("Setup End");
}
void setup1() { pinMode(ledPin1, OUTPUT); }

void dump_packet(const Raw1090Packet& pkt)
{
    Serial.println("Package contents:");
    uint16_t total_bytes = (pkt.buffer_len_bits + 7) / 8; // число байт
    uint8_t* data = (uint8_t*)pkt.buffer;
    for (uint16_t i = 0; i < total_bytes; i++) {
        if (i > 0) Serial.print(" ");
        if (data[i] < 16) Serial.print("0");
        Serial.print(data[i], HEX);
    }
    Serial.println();
}

void loop() 
{
    if (raw_packet_available) {
        noInterrupts();
        Raw1090Packet pkt = shared_raw_packet; // копируем
        raw_packet_available = false;
        interrupts();

        dump_packet(pkt); // выводим
    }

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) 
    {
        previousMillis = currentMillis;
        ledState = (ledState == LOW) ? HIGH : LOW;
        digitalWrite(ledPin, ledState);
    }
}
void loop1() 
{
    unsigned long currentMillis1 = millis();
    if (currentMillis1 - previousMillis1 >= interval1) 
    {
        previousMillis1 = currentMillis1;
        ledState1 = (ledState1 == LOW) ? HIGH : LOW;
        digitalWrite(ledPin1, ledState1);
    }
}






