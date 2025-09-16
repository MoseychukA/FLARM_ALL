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
#include "decode_utils.h"
#include "adsbee.h"
#include "data_structures.h"
#include <math.h>
#include <map>
#include <cstring>


// Serial config
#define USB_BAUD 115200
#define UART2_BAUD 921600
#define UART2_TX_PIN 4
#define UART2_RX_PIN 5

const uint16_t kStatusLEDBootupBlinkPeriodMs = 200; 
const uint32_t kESP32BootupTimeoutMs         = 10000;
const uint32_t kESP32BootupCommsRetryMs      = 500;
 
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

    Serial.begin(921600);
    unsigned long t0 = millis(); while (!Serial && !Serial.dtr() && (millis() - t0) < 8000) delay(10);
    delay(2000);
    Serial.print("Software ");
    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    ver_soft.remove(val_srt);
    Serial.println(ver_soft);
 
    settings_manager.Load();    // Загрузить настройки по умолчанию. Нужно еще поработать с этой функцией.

    for (uint16_t i = 0; i < 5; i++)
    {
        adsbee.SetStatusLED(true);
        delay(300);
        adsbee.SetStatusLED(false);
        delay(300);
    }
    Serial.println("Setup End\r\n");
}


void setup1()
{ 
    
}


void loop() 
{

    decoder.UpdateLogLoop();   // Вывод сырых пакетов ().
   // comms_manager.Update();    // Вывод расшифрованных пакетов.
    adsbee.Update();


}


void loop1()
{
   decoder.UpdateDecoderLoop();   //PacketDecoder 

}
