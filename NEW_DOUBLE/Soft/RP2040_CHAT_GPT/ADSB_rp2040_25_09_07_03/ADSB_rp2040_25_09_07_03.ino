#include "Arduino.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "adsbee.h"
#include "bsp.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
//#include "adsb_packet.h"
#include "adsb_decoder.h"


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

AdsbeeContext g_ctx;

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

void loop() {
 
    for (int sm = 0; sm < kMaxNumDemodStateMachines; sm++) {
        Raw1090Packet pkt = g_ctx.rx_packet_[sm];
        if (pkt.buffer_len_bits > 0) {
            if (adsb_check_crc(pkt)) {
                Serial.println("[ADS-B] CRC OK");
                char buf[128];
                pkt.PrintBuffer(buf, sizeof(buf));
                Serial.println(buf);

                DecodedAdsb d = adsb_decode(pkt);
                Serial.print(" DF="); Serial.print(d.df);
                Serial.print(" ICAO="); Serial.print(d.icao, HEX);
                Serial.print(" TC="); Serial.print(d.tc);
                if (d.callsign[0]) {
                    Serial.print(" CALLSIGN="); Serial.print(d.callsign);
                }
                if (d.alt != 0) {
                    Serial.print(" ALT="); Serial.print(d.alt);
                }
                Serial.println();
            }
            else {
                Serial.println("[ADS-B] CRC ERROR");
            }
            g_ctx.rx_packet_[sm] = Raw1090Packet(); // очистка
        }
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






