#include <mutex>
#include "Arduino.h"
#include "pico/multicore.h"
#include "unit_conversions.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "transponder_packet.h"
#include "packet_decoder.h"
#include "hal.h"
#include "unit_conversions.h"
#include "bsp.h"
#include "awb_utils.h"
#include "decode_utils.h"
#include "object_dictionary.h"
#include "aircraft_dictionary.h"
#include "nasa_cpr.h"
#include "adsbee.h"
#include "beast_tables.h"
#include "data_structures.h"
#include "shared.h"

BSP bsp = BSP({});
ADSBee adsbee = ADSBee({});
SettingsManager settings_manager;
PacketDecoder decoder = PacketDecoder({ .enable_1090_error_correction = true });

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

    settings_manager.Load();    // Загрузить настройки по умолчанию. Нужно еще поработать с этой функцией.

    for (uint16_t i = 0; i < 5; i++)
    {
        adsbee.SetStatusLED(true);
        delay(300);
        adsbee.SetStatusLED(false);
        delay(300);
    }
    pinMode(ledPin, OUTPUT);

    Serial.println("Setup End");
}
void setup1() { pinMode(ledPin1, OUTPUT); }


void loop() 
{
    // decoder.UpdateLogLoop();   // Вывод сырых пакетов ().
     adsbee.Update();

    if (decode_message_available && decode_debug_message_out_queue.Length() > 0) {
        char* msg;
        if (decode_debug_message_out_queue.Pop(msg)) 
        {
            /*Serial.print("decode_debug_message: ");*/
            Serial.println(msg);
            free(msg); // освобождаем память после вывода
            if (decode_debug_message_out_queue.Length() == 0) {
                decode_message_available = false;
            }
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
    decoder.UpdateDecoderLoop();   //PacketDecoder 
    unsigned long currentMillis1 = millis();
    if (currentMillis1 - previousMillis1 >= interval1) 
    {
        previousMillis1 = currentMillis1;
        ledState1 = (ledState1 == LOW) ? HIGH : LOW;
        digitalWrite(ledPin1, ledState1);
    }
}






