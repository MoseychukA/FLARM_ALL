#include <mutex>
#include "Arduino.h"
#include "pico/multicore.h"
#include "adsbee.h"
#include "comms.h"
#include "hal.h"
#include "mode_s_packet.h"
#include "mode_s_packet_decoder.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "unit_conversions.h"
#include "bsp.h"
#include "hardware/watchdog.h"
#include "hardware/gpio.h"
#include "composite_array.h"

BSP bsp = BSP({});
ADSBee adsbee = ADSBee({});
CommsManager comms_manager = CommsManager({});
SettingsManager settings_manager;
ModeSPacketDecoder decoder = ModeSPacketDecoder({.enable_1090_error_correction = true});

void setup()
{
    bi_decl(bi_program_description("ADSBee 1090 ADSB Receiver"));

    comms_manager.Init();  //Сначала настроим вывод в КОМ порт
    sleep_ms(500);
    adsbee.Init();
    Serial.begin(115200);
    unsigned long t0 = millis(); while (!Serial && !Serial.dtr() && (millis() - t0) < 8000) delay(10);
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
    uint32_t timeout_ms = 4000;
    watchdog_enable(timeout_ms, /* pause_on_debug = */ false);

    Serial.println("WDT включён на 4 сек."); //WDT is on for 4 sec.

    Serial.println("Setup End\r\n");
    comms_manager.console_printf("\r\nSetup End\r\n");
}


void setup1()
{

}


void loop()
{

    //  decoder.UpdateLogLoop();   // Вывод сырых пакетов ().
    comms_manager.Update();    // Вывод расшифрованных пакетов.
    adsbee.Update();
    // Периодически сбрасываем WDT, иначе будет рестарт микроконтроллера
    watchdog_update();
   // adsbee.PokeWatchdog();
}


void loop1()
{
    decoder.UpdateDecoderLoop();   //PacketDecoder 

}






//int main() {
//    bi_decl(bi_program_description("ADSBee 1090 ADSB Receiver"));
//
//    // Initialize coprocessor SPI bus.
//    // ESP32 SPI pins.
//    gpio_set_function(bsp.copro_spi_clk_pin, GPIO_FUNC_SPI);
//    gpio_set_function(bsp.copro_spi_mosi_pin, GPIO_FUNC_SPI);
//    gpio_set_function(bsp.copro_spi_miso_pin, GPIO_FUNC_SPI);
//    gpio_set_drive_strength(bsp.copro_spi_clk_pin, bsp.copro_spi_drive_strength);
//    gpio_set_drive_strength(bsp.copro_spi_mosi_pin, bsp.copro_spi_drive_strength);
//    gpio_set_pulls(bsp.copro_spi_clk_pin, bsp.copro_spi_pullup, bsp.copro_spi_pulldown);   // Clock pin pulls.
//    gpio_set_pulls(bsp.copro_spi_mosi_pin, bsp.copro_spi_pullup, bsp.copro_spi_pulldown);  // MOSI pin pulls.
//    gpio_set_pulls(bsp.copro_spi_miso_pin, bsp.copro_spi_pullup, bsp.copro_spi_pulldown);  // MISO pin pulls.
//    adsbee.Init();
//    comms_manager.Init();
//    comms_manager.console_printf("ADSBee 1090\r\nSoftware Version %d.%d.%d\r\n",
//                                 object_dictionary.kFirmwareVersionMajor, object_dictionary.kFirmwareVersionMinor,
//                                 object_dictionary.kFirmwareVersionPatch);
//
//    settings_manager.Load();
//
//    // Blink the LED a few times to indicate a successful startup.
//    for (uint16_t i = 0; i < num_status_led_blinks; i++) 
//    {
//        adsbee.SetStatusLED(true);
//        sleep_ms(kStatusLEDBootupBlinkPeriodMs / 2);
//        adsbee.SetStatusLED(false);
//        sleep_ms(kStatusLEDBootupBlinkPeriodMs / 2);
//    }
//
//    multicore_reset_core1();
//    multicore_launch_core1(main_core1);
//
//    uint32_t esp32_last_heartbeat_timestamp_ms = 0;
//
//    while (true) 
//    {
//        // Loop forever.
//        decoder.UpdateLogLoop();
//        comms_manager.Update();
//        adsbee.Update();
//        adsbee.PokeWatchdog();
//    }
//}