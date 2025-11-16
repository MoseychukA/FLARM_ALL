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

    /*   while (Serial1.available()) {
        uint8_t b = Serial1.read();*/


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
// 
//// Глобальные переменные:
//uint16_t received_values[256];
//uint8_t  received_count = 0;
//bool     new_packet_available = false;
//
//void receiveFromESP32()
//{
//    static uint8_t step = 0;
//    static uint8_t count = 0;
//    static uint8_t recvCount = 0;
//    static uint16_t values[256];
//    static uint8_t buf[2];
//    static uint8_t bufidx = 0;
//
//    while (uart_is_readable(uart1)) 
//    {
//        uint8_t b = uart_getc(uart1);
//        switch (step) 
//        {
//        case 0: // Ждём count
//            count = b;
//            recvCount = 0;
//            step = 1;
//            bufidx = 0;
//            break;
//        case 1: // Читаем значения по 2 байта
//            buf[bufidx++] = b;
//            if (bufidx == 2) {
//                // !!! Здесь главная правка !!!
//                uint16_t v = (buf[0] << 8) | buf[1]; // КОРРЕКТНО для big-endian
//                values[recvCount++] = v;
//                bufidx = 0;
//                if (recvCount == count) step = 2;
//            }
//            break;
//        case 2: // Ждём маркер конца (0xAAAA)
//            buf[bufidx++] = b;
//            if (bufidx == 2) 
//            {
//                uint16_t m = (buf[0] << 8) | buf[1];
//                if (m == 0xAAAA) {
//                    // Копируем принятые значения в доступный массив
//                    memcpy(received_values, values, count * sizeof(uint16_t));
//                    received_count = count;
//                    new_packet_available = true; // Сигнал, что новое сообщение принято!
//                }
//                // Готовим всё для нового пакета
//                step = 0;
//                bufidx = 0;
//            }
//            break;
//        }
//    }
//}
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
    uint32_t timeout_ms = 4000;
    watchdog_enable(timeout_ms, false);

    Serial.println("WDT включён на 4 сек."); //WDT is on for 4 sec.

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
 
   // receiveFromESP32(); // Всегда вызывайте приём

   //// Если новое сообщение пришло:
   // if (new_packet_available) 
   // {
   //     // Обработка (например, выводим по UART0/Serial):
   //     Serial.print("Received ");
   //     Serial.print(received_count);
   //     Serial.println(" values:");
   //     for (uint8_t i = 0; i < received_count; i++) {
   //         Serial.print(" [");
   //         Serial.print(i);
   //         Serial.print("]=");
   //         Serial.println(received_values[i]);
   //     }

   //     // После обработки обязательно сбросьте флаг:
   //     new_packet_available = false;
   // }

    // Периодически сбрасываем WDT, иначе будет рестарт микроконтроллера
    watchdog_update();

}


void loop1()
{
   decoder.UpdateDecoderLoop();   //PacketDecoder 
   receiveFromESP32(); // Всегда вызывайте приём

// Если новое сообщение пришло:
   if (new_packet_available)
   {
       // Обработка (например, выводим по UART0/Serial):
      /* Serial.print("Received ");
       Serial.print(received_count);
       Serial.println(" values:");
       for (uint8_t i = 0; i < received_count; i++)  
       {
           Serial.print(" [");
           Serial.print(i);
           Serial.print("]=");
           Serial.println(received_values[i]);
       }*/

       // После обработки обязательно сбросьте флаг:
       new_packet_available = false;
   }
   watchdog_update();
}
