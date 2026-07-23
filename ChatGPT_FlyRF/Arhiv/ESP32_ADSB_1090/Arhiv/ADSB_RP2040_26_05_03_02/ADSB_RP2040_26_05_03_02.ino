#include <mutex>
#include "Arduino.h"
#include "pico/multicore.h"
#include "unit_conversions.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "comms.h"
#include "transponder_packet.h"
#include "packet_decoder.h"
#include "hal.h"
#include "unit_conversions.h"
#include "bsp.h"
#include "awb_utils.h"
#include "decode_utils.h" 
#include "aircraft_dictionary.h"
#include "nasa_cpr.h"
#include "adsbee.h"
#include "beast_tables.h"
#include "data_structures.h"
#include "hardware/watchdog.h"

const int pwmPin = 9;
const int rssiPin = 28;

// Автоматическая регулировка усиления: RSSI GPIO28 -> PWM GPIO9.
const bool useAutomaticGainControl = true;
const bool pwmDutyIncreasesGain = true;  // Поставьте false, если больший PWM уменьшает усиление.

const uint32_t pwmFrequencyHz = 10000;  // Частота PWM уменьшена в 2 раза: 20 kHz -> 10 kHz.
const int pwmReferenceMv = 5900;
const int pwmStartOutputMv = 1000;
const int pwmMaxOutputMv = 1500;        // Ограничение, чтобы выход RC не уходил к 1.8 V.

const int minDuty = 10;     // Нижний предел управляющего PWM.
const int maxDuty = (pwmMaxOutputMv * 255 + pwmReferenceMv / 2) / pwmReferenceMv;
const int startDuty = (pwmStartOutputMv * 255 + pwmReferenceMv / 2) / pwmReferenceMv;

const int rssiMinMv = 700;
const int rssiMaxMv = 1300;
const int rssiTargetMv = (rssiMinMv + rssiMaxMv) / 2;
const int rssiDeadbandMv = 25;

const uint32_t agcUpdateIntervalMs = 20;
const int agcFilterNewSamplePercent = 25;
const int agcMaxStepDuty = 4;

int agcDuty = startDuty;
int agcFilteredRssiMv = rssiTargetMv;
uint32_t agcLastUpdateMs = 0;

int clampInt(int value, int minValue, int maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

void writeGainPwmDuty(int duty)
{
    agcDuty = clampInt(duty, minDuty, maxDuty);
    analogWrite(pwmPin, agcDuty);
}

int readAveragedRssiMilliVolts()
{
    const uint8_t sampleCount = 8;
    uint32_t sumMv = 0;

    for (uint8_t i = 0; i < sampleCount; i++)
    {
        sumMv += adsbee.ReadSignalStrengthMilliVolts();
    }

    return sumMv / sampleCount;
}

void updateAutomaticGainControl()
{
    uint32_t nowMs = millis();
    if (nowMs - agcLastUpdateMs < agcUpdateIntervalMs)
    {
        return;
    }
    agcLastUpdateMs = nowMs;

    int rssiMv = readAveragedRssiMilliVolts();
    agcFilteredRssiMv =
        (agcFilteredRssiMv * (100 - agcFilterNewSamplePercent) + rssiMv * agcFilterNewSamplePercent) / 100;

    int errorMv = rssiTargetMv - agcFilteredRssiMv;
    int absErrorMv = abs(errorMv);
    if (absErrorMv <= rssiDeadbandMv)
    {
        return;
    }

    int stepDuty = clampInt((absErrorMv - rssiDeadbandMv) / 75 + 1, 1, agcMaxStepDuty);
    if (errorMv > 0)
    {
        writeGainPwmDuty(agcDuty + (pwmDutyIncreasesGain ? stepDuty : -stepDuty));
    }
    else
    {
        writeGainPwmDuty(agcDuty + (pwmDutyIncreasesGain ? -stepDuty : stepDuty));
    }
}



//====================== Прием числа установки ШИМ из ESP32S3 =====================================
#define START_MARK 0x55
#define END_MARK   0xAAAA

uint16_t received_values[256];
uint8_t  received_count = 0;
bool     new_packet_available = false;



void receiveFromESP32() 
{
    static enum { WAIT_STX, WAIT_COUNT, WAIT_DATA, WAIT_MARKER } state = WAIT_STX;
    static uint8_t count = 0, recvCount = 0, buf[2], bufidx = 0;
    static uint16_t values[256];
    static uint8_t marker_buf[2];

    while (uart_is_readable(uart1)) 
    {
        uint8_t b = uart_getc(uart1);
        switch (state) 
        {
        case WAIT_STX:
            if (b == START_MARK) state = WAIT_COUNT;
            break;
        case WAIT_COUNT:
            count = b;
            recvCount = 0;
            state = WAIT_DATA;
            bufidx = 0;
            break;
        case WAIT_DATA:
            buf[bufidx++] = b;
            if (bufidx == 2) {
                uint16_t v = ((uint16_t)buf[0] << 8) | buf[1];
                values[recvCount++] = v;
                bufidx = 0;
                if (recvCount == count) {
                    state = WAIT_MARKER;
                    bufidx = 0;
                }
            }
            break;
        case WAIT_MARKER:
            marker_buf[bufidx++] = b;
            if (bufidx == 2) {
                uint16_t m = ((uint16_t)marker_buf[0] << 8) | marker_buf[1];
                if (m == END_MARK) {
                    memcpy(received_values, values, count * sizeof(uint16_t));
                    received_count = count;
                    new_packet_available = true;
                }
                state = WAIT_STX;
                bufidx = 0;
            }
            break;
        }
    }
}

//=================================================================================================
 
BSP bsp = BSP({});
ADSBee adsbee = ADSBee({});
SettingsManager settings_manager;
PacketDecoder decoder = PacketDecoder({ .enable_1090_error_correction = true });

void setup() 
{
    bi_decl(bi_program_description("ADSBee 1090 ADSB Receiver"));

    comms_manager.Init();  //Сначала настроим вывод в КОМ порт
    sleep_ms(500);
    adsbee.Init();
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && !Serial.dtr() && (millis() - t0) < 8000) delay(10);
    delay(1000);
    Serial.print("Software ");
    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    ver_soft.remove(val_srt);
    Serial.println(ver_soft);
    comms_manager.console_printf(ver_soft.c_str());
    settings_manager.Load();    // Загрузить настройки по умолчанию. Нужно еще поработать с этой функцией.

    for (uint16_t i = 0; i < 4; i++)
    {
        adsbee.SetStatusLED(true);
        delay(200);
        adsbee.SetStatusLED(false);
        delay(200);
    }

    // Устанавливаем WDT на 4 секунды:
    //uint32_t timeout_ms = 4000;
    //watchdog_enable(timeout_ms, false);

    //Serial.println("WDT включён на 4 сек."); //WDT is on for 4 sec.


    pinMode(pwmPin, OUTPUT);
    analogWriteResolution(8); // 8-битное разрешение PWM
    analogWriteFreq(pwmFrequencyHz); // частота PWM ~10 kHz
    writeGainPwmDuty(startDuty); // стартовый уровень
    Serial.print("AGC ");
    Serial.println(useAutomaticGainControl ? "enabled" : "manual PWM control ready");
    Serial.print("RSSI GPIO");
    Serial.print(rssiPin);
    Serial.print(" target ");
    Serial.print(rssiTargetMv);
    Serial.print(" mV, PWM GPIO");
    Serial.println(pwmPin);

    Serial.println("Setup End\r\n");
    comms_manager.console_printf("\r\nSetup End\r\n");
}


void setup1()
{ 
    
}


void loop() 
{

  //  decoder.UpdateLogLoop();   // Вывод сырых пакетов. Отключен
    comms_manager.Update();    // Вывод расшифрованных пакетов.
    adsbee.Update();
    receiveFromESP32(); // Всегда вызывайте приём

    if (useAutomaticGainControl)
    {
        updateAutomaticGainControl();
        new_packet_available = false;
    }
// Если новое сообщение пришло:
    else if (new_packet_available)
    {
        // Обработка (например, выводим по UART0/Serial):
 /*       Serial.print("Received ");
        Serial.print(received_count);
        Serial.println(" values:");
        for (uint8_t i = 0; i < received_count; i++)
        {
            Serial.print(" [");
            Serial.print(i);
            Serial.print("]=");
            Serial.println(received_values[i]);
        }*/


        long mv = received_values[0]; // читает следующую цифру последовательности

        // Защита:ограничиваем диапазон
        if (mv < 300) mv = 300;
        if (mv > 1400) mv = 1400;

        // Преобразуем mv (300..1400) -> duty (minDuty..maxDuty)
        int duty = map((int)mv, 300, 1400, minDuty, maxDuty);
        writeGainPwmDuty(duty);
 /*       Serial.print("duty: ");
        Serial.println(duty);*/
        // После обработки обязательно сбросьте флаг:
        new_packet_available = false;
    }
    // Периодически сбрасываем WDT, иначе будет рестарт микроконтроллера
    //watchdog_update();

}


void loop1()
{
   decoder.UpdateDecoderLoop();   //PacketDecoder 
   //watchdog_update();
}
