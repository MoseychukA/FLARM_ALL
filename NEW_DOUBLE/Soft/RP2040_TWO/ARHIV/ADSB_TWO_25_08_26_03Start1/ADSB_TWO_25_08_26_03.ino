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
#include "object_dictionary.h"
#include "aircraft_dictionary.h"
#include "nasa_cpr.h"
#include "adsbee.h"
#include "beast_tables.h"
#include "data_structures.h"
#include "shared.h"

const uint16_t kStatusLEDBootupBlinkPeriodMs = 200; 
const uint32_t kESP32BootupTimeoutMs         = 10000;
const uint32_t kESP32BootupCommsRetryMs      = 500;
 
BSP bsp = BSP({});
ADSBee adsbee = ADSBee({});
SettingsManager settings_manager;
PacketDecoder decoder = PacketDecoder({ .enable_1090_error_correction = true });

////Raw1090Packet rx_packet_[BSP::kMaxNumDemodStateMachines];
//Raw1090Packet raw_1090_packet_queue_buffer_[SettingsManager::Settings::kMaxNumTransponderPackets];
//PFBQueue<Raw1090Packet> raw_1090_packet_queue = PFBQueue<Raw1090Packet>({ .buf_len_num_elements = SettingsManager::Settings::kMaxNumTransponderPackets, .buffer = raw_1090_packet_queue_buffer_ });
//
//Raw1090Packet raw_packet;

//void dump_bytes_in_hex(const uint8_t* data, size_t len) {
//    for (size_t i = 0; i < len; i++) {
//        if (i > 0) Serial.print(" ");
//        if (data[i] < 16) Serial.print("0");
//        Serial.print(data[i], HEX);
//    }
//    Serial.println();
//}

void dump_bytes_in_hex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        //if (i > 0) Serial.print(" ");
        //if (data[i] < 16) Serial.print("0");
        Serial.print((char)data[i]);
    }
    Serial.println();
}


void setup() 
{
    bi_decl(bi_program_description("ADSBee 1090 ADSB Receiver"));

    comms_manager.Init();  //Сначала настроим вывод в КОМ порт
    sleep_ms(500);
    adsbee.Init();

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
   // comms_manager.Update();    // Вывод расшифрованных пакетов.
    adsbee.Update();

    //// вывод на ESP32S3
    //if (message_ready) 
    //{
    //    size_t len = message_len_bytes;
    //    uint8_t* data = message_buffer;

    //    dump_bytes_in_hex(data, len);

    //    message_ready = false; // сбрасываем флаг
    //}

 //   decoded_1090_packet_out_queue.Push(decoded_packet);

    if (decode_message_available && decode_debug_message_out_queue.Length() > 0) 
    {
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

}


void loop1()
{
   decoder.UpdateDecoderLoop();   //PacketDecoder 
}
