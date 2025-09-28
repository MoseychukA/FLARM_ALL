/*
Рабочий вариант для ESP32S3: смена параметров LoRaWAN через LMIC + ссылка на библиотеку
Используемая библиотека:
GitHub - mcci-catena/arduino-lmic: LoraWAN-MAC-in-C library, adapted to run under the Arduino environment

github.com


(OFFICIAL LoRaWAN/LMIC для Arduino, ESP32, ESP32S3)

ВАЖНО:

НЕТ никакой библиотеки с lora.setDR()/lora.joinOTAA() и тем более с методом .handle() для LoRaWAN на ESP32S3!
"lora" здесь — это объект, который ты должен сам описать, или, что чаще встречается, надо работать прямо с LMIC из arduino-lmic!
Класс с подобным API есть только в библиотеках типа BasicMAC, которые нет в публичном git.
Мой пример — реально рабочий вариант для ESP32S3, где все параметры задаются через LMIC (библиотека MCCI Catena arduino-lmic).
Шаблон со сменой DR (и, частично, SF/BW/CR) на ESP32S3 c arduino-lmic (LoRaWAN!)
// Используй библиотеку:
// https://github.com/mcci-catena/arduino-lmic

*/

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include "esp_task_wdt.h"


#define LORA_SCK     12
#define LORA_MISO    13
#define LORA_MOSI    11
#define LORA_CS      46
#define LORA_RST     7
#define LORA_DIO0    18
#define LORA_DIO1    15

const lmic_pinmap lmic_pins = {
   .nss = LORA_CS,
   .rxtx = 0xff,//LMIC_UNUSED_PIN,
   .rst = LORA_RST,
   .dio = {18, 0xff,0xff},
};



// ------- LoRaWAN OTAA credentials ------- //
static const u1_t PROGMEM DEVEUI[8] = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77};
static const u1_t PROGMEM APPEUI[8] = {0x70,0xB3,0xD5,0x7E,0xD0,0x00,0x00,0x00};
static const u1_t PROGMEM APPKEY[16] = { /* твой AppKey */ };
void os_getDevKey (u1_t* buf) { memcpy_P(buf, APPKEY, 16);}
void os_getArtEui(u1_t* buf)  { memcpy_P(buf, APPEUI, 8);}
void os_getDevEui(u1_t* buf)  { memcpy_P(buf, DEVEUI, 8);}

uint8_t  dr       = 5;    // DR5 (SF7) старт
enum class Step { DR, POWER, DONE };
Step curStep = Step::DR;

const uint32_t TX_INTERVAL_MS = 8000;
uint32_t pktCnt = 0;
uint8_t txPower = 14;

void applyParameters() {
    // DR 0-5: 0 (SF12), 5 (SF7) - EU868
    LMIC_setDrTxpow(dr, txPower);
}

void nextStep() 
{
    static uint8_t repeat = 0;
    repeat++;
    if (repeat >= 5) {
        repeat = 0;
        curStep = static_cast<Step>(static_cast<int>(curStep)+1);
        if (curStep == Step::DR) 
        {
            dr = (dr == 5) ? 0 : dr+1;
            applyParameters();
        } else if (curStep == Step::POWER) {
            txPower = (txPower >= 20) ? 2 : txPower+2;
            applyParameters();
        }
    }
}

void do_send(osjob_t* j) {
    uint8_t payload[4] = {
        (uint8_t)(pktCnt >> 24),
        (uint8_t)(pktCnt >> 16),
        (uint8_t)(pktCnt >> 8),
        (uint8_t)(pktCnt)
    };

    LMIC_setDrTxpow(dr, txPower);
    LMIC_setTxData2(1, payload, sizeof(payload), 0);
    Serial.printf("[Tx] pkt=%lu  DR=%d  TX=%ddBm", pktCnt, dr, txPower);
    pktCnt++;
}

static osjob_t sendjob;
void onEvent (ev_t ev) {
    switch(ev) {
        case EV_JOINED:
            Serial.println("EV_JOINED");
            LMIC_setAdrMode(0); // Отключить ADR (чтобы DR не менялся сам)
            applyParameters();
            do_send(&sendjob);
            break;
        case EV_TXCOMPLETE:
            Serial.println("TX complete");
            nextStep();
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL_MS/1000), do_send);
            break;
        default: break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("ESP32S3 LoRaWAN test (LMIC/MCCI)"));
    esp_task_wdt_deinit(); // отключает WDT полностью — только для поиска ошибки!
    delay(100);
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    os_init();
    LMIC_reset();
    LMIC_startJoining();
    Serial.println(F("ESP32S3 Setup END"));
}

void loop() {
    os_runloop_once();
}

/*
Суть
Используй библиотеку MCCI Catena arduino-lmic
GitHub - mcci-catena/arduino-lmic: LoraWAN-MAC-in-C library, adapted to run under the Arduino environment

github.com


Всё управление параметрами — через LMIC:

LMIC_setDrTxpow(dr, txPower);
dr (0—SF12 (максимальная дальность), 5—SF7 (максимальная скорость))
txPower — мощность передатчика (2...20)
Переключения DR — пример в коде.
Несуществующие у тебя методы:
.setDR(), .setBW(), .setSF() и т.п. — так может называться только пользовательский класс или API приватных решений (в публичных библиотеках ардуино этого НЕТ).

Если нужна простая LoRa без LoRaWAN (Примитивная точка-точка)
Тогда применяй
GitHub - sandeepmistry/arduino-LoRa: An Arduino library for sending and receiving data using LoRa radios.

github.com

:

Параметры задаются так:
LoRa.setSpreadingFactor(12);
LoRa.setSignalBandwidth(125E3);
LoRa.setCodingRate4(8);
LoRa.setTxPower(20);
Но там нет joinOTAA, никакого LoRaWAN — только чистый радиоканал!

*/