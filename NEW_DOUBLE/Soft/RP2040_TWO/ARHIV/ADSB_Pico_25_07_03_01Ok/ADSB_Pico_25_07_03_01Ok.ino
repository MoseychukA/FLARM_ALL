#include "Arduino.h"
#include <mutex>
#include "comms.h"
#include "packet_decoder.h"
#include "pico/multicore.h"
#include "transponder_packet.h"
#include "unit_conversions.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hal.h"
#include "unit_conversions.h"
#include "eepromPico.h"
#include "bsp.h"
#include "awb_utils.h"
#include "decode_utils.h"
#include "object_dictionary.h"
#include "aircraft_dictionary.h"
#include "nasa_cpr.h"
#include "adsbee.h"
#include "beast_tables.h"
#include "flash_utils.h"
#include "firmware_update.h"


const uint16_t kStatusLEDBootupBlinkPeriodMs = 200; 
const uint32_t kESP32BootupTimeoutMs         = 10000;
const uint32_t kESP32BootupCommsRetryMs      = 500;

// Здесь можно переопределить параметры конфигурации по умолчанию.
EEPROMPICO eeprom_pico = EEPROMPICO({});
// BSP настраивается по-разному, если подключена или нет EEPROM. Попытаемся инициализировать EEPROM, чтобы выяснить,
// какую конфигурацию платы следует загрузить (настройки во флэш-памяти или настройки в EEPROM).
BSP bsp = BSP({}); //(eeprom_pico.Init());  // Загружаю по умолчанию
 
ADSBee adsbee = ADSBee({});
SettingsManager settings_manager;
ObjectDictionary object_dictionary;
PacketDecoder decoder = PacketDecoder({ .enable_1090_error_correction = true });



void setup() 
{
    bi_decl(bi_program_description("ADSBee 1090 ADSB Receiver"));


    Serial.begin(115200);
    unsigned long t0 = millis(); while (!Serial && !Serial.dtr() && (millis() - t0) < 8000) delay(10);
    delay(2000);
    comms_manager.Init();  //Сначала настроим вывод в КОМ порт
    sleep_ms(500);
    adsbee.Init();

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
        sleep_ms(kStatusLEDBootupBlinkPeriodMs / 2);
        adsbee.SetStatusLED(false);
        sleep_ms(kStatusLEDBootupBlinkPeriodMs / 2);
    }
    Serial.println("Setup End\r\n");
   
}


void setup1()
{
   
}




void loop() 
{

    decoder.UpdateLogLoop();   // Вывод сырых пакетов ().
    comms_manager.Update();    // Вывод расшифрованных пакетов.
    adsbee.Update();
}


void loop1()
{

    decoder.UpdateDecoderLoop();

}


inline void StopCore1() { multicore_reset_core1(); }
inline void StartCore1() { multicore_launch_core1(loop1); }