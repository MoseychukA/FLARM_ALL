#include <mutex>
#include "Arduino.h"
#include "adsbee.h"
#include "comms.h"
#include "hal.h"
#include "mode_s_packet.h"
#include "mode_s_packet_decoder.h"

#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "unit_conversions.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"


#define ON_PICO

const uint16_t kStatusLEDBootupBlinkPeriodMs = 200;
const uint32_t kESP32BootupTimeoutMs = 10000;
const uint32_t kESP32BootupCommsRetryMs = 500;

BSP bsp = BSP({});

ADSBee adsbee = ADSBee({});
CommsManager comms_manager = CommsManager({});
SettingsManager settings_manager;
//ObjectDictionary object_dictionary;


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

    // Add a test aircraft to start.
    // Aircraft1090 test_aircraft;
    // test_aircraft.category = Aircraft1090::Category::kCategorySpaceTransatmosphericVehicle;
    // strcpy(test_aircraft.callsign, "TST1234");
    // test_aircraft.latitude_deg = 20;
    // test_aircraft.longitude_deg = 140;
    // test_aircraft.baro_altitude_ft = 10000;
    // test_aircraft.vertical_rate_fpm = -5;
    // test_aircraft.altitude_source = Aircraft1090::AltitudeSource::kAltitudeSourceBaro;
    // test_aircraft.direction_deg = 100;
    // test_aircraft.velocity_kts = 200;
    // adsbee.aircraft_dictionary.InsertAircraft(test_aircraft);

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

}


void loop1()
{
   decoder.UpdateDecoderLoop();   //PacketDecoder 

}



/*

int main() {
    bi_decl(bi_program_description("ADSBee 1090 ADSB Receiver"));

    // Initialize coprocessor SPI bus.
    // ESP32 SPI pins.
    gpio_set_function(bsp.copro_spi_clk_pin, GPIO_FUNC_SPI);
    gpio_set_function(bsp.copro_spi_mosi_pin, GPIO_FUNC_SPI);
    gpio_set_function(bsp.copro_spi_miso_pin, GPIO_FUNC_SPI);
    gpio_set_drive_strength(bsp.copro_spi_clk_pin, bsp.copro_spi_drive_strength);
    gpio_set_drive_strength(bsp.copro_spi_mosi_pin, bsp.copro_spi_drive_strength);
    gpio_set_pulls(bsp.copro_spi_clk_pin, bsp.copro_spi_pullup, bsp.copro_spi_pulldown);   // Clock pin pulls.
    gpio_set_pulls(bsp.copro_spi_mosi_pin, bsp.copro_spi_pullup, bsp.copro_spi_pulldown);  // MOSI pin pulls.
    gpio_set_pulls(bsp.copro_spi_miso_pin, bsp.copro_spi_pullup, bsp.copro_spi_pulldown);  // MISO pin pulls.
    // Initialize SPI Peripheral.
    spi_init(bsp.copro_spi_handle, bsp.copro_spi_clk_freq_hz);
    // The CC1312 straight up does not work with CPOL = 0 and CPHA = 0 (only sends one Byte per transaction then
    // explodes). The ESP32 doesn't seem to care either way (in fact it interprets CPOL = 1 CPHA = 1 as CPOL = 0 CPHA =
    // 0 just fine), so we stick with CPOL = 1 CPHA = 1.
    // I briefly tried switching the SPI format back and forth continutously in SPIBeginTransaction(), but this was
    // causing crashes.
    spi_set_format(bsp.copro_spi_handle,
                   8,           // Bits per transfer.
                   SPI_CPOL_1,  // Polarity (CPOL).
                   SPI_CPHA_1,  // Phase (CPHA).
                   SPI_MSB_FIRST);

    adsbee.Init();
    comms_manager.Init();
    comms_manager.console_printf("ADSBee 1090\r\nSoftware Version %d.%d.%d\r\n",
                                 object_dictionary.kFirmwareVersionMajor, object_dictionary.kFirmwareVersionMinor,
                                 object_dictionary.kFirmwareVersionPatch);

    settings_manager.Load();

    uint16_t num_status_led_blinks = FirmwareUpdateManager::AmWithinFlashPartition(0) ? 1 : 2;
    // Blink the LED a few times to indicate a successful startup.
    for (uint16_t i = 0; i < num_status_led_blinks; i++) {
        adsbee.SetStatusLED(true);
        sleep_ms(kStatusLEDBootupBlinkPeriodMs / 2);
        adsbee.SetStatusLED(false);
        sleep_ms(kStatusLEDBootupBlinkPeriodMs / 2);
    }

    // If WiFi is enabled, try establishing communication with the ESP32 and maybe update its firmware.
    if (esp32.IsEnabled()) {
        adsbee.DisableWatchdog();  // Disable watchdog while setting up ESP32, in case kESP32BootupTimeoutMs >=
                                   // watchdog timeout, and to avoid watchdog reboot during ESP32 programming.

        // Try reading from the ESP32 till it finishes turning on.
        uint32_t esp32_firmware_version = 0x0;
        bool flash_esp32 = true;
        uint32_t esp32_comms_start_timestamp_ms = get_time_since_boot_ms();
        uint32_t esp32_comms_last_try_timestamp_ms = 0;
        while (get_time_since_boot_ms() - esp32_comms_start_timestamp_ms < kESP32BootupTimeoutMs) {
            // Wait until the next retry interval to avoid spamming the ESP32 continuously.
            if (get_time_since_boot_ms() - esp32_comms_last_try_timestamp_ms < kESP32BootupCommsRetryMs) {
                continue;
            }
            esp32_comms_last_try_timestamp_ms = get_time_since_boot_ms();
            // Try reading the firmware version from the ESP32. If the read succeeds, confirm that the firmware
            // version matches ours.
            if (!esp32.Read(ObjectDictionary::Address::kAddrFirmwareVersion, esp32_firmware_version)) {
                // Couldn't read firmware version from ESP32. Try again later.
                CONSOLE_ERROR("main", "Unable to read firmware version from ESP32.");
            } else if (esp32_firmware_version != object_dictionary.kFirmwareVersion) {
                // ESP32 firmware version doesn't match ours. Flash the ESP32.
                CONSOLE_ERROR("main",
                              "Incorrect firmware version detected on ESP32. Pico is running %d.%d.%d but ESP32 is "
                              "running %d.%d.%d",
                              object_dictionary.kFirmwareVersionMajor, object_dictionary.kFirmwareVersionMinor,
                              object_dictionary.kFirmwareVersionPatch, esp32_firmware_version >> 16,
                              (esp32_firmware_version >> 8) & 0xFF, esp32_firmware_version & 0xFF);
                break;
            } else {
                // Firmware checks out, all good! Don't flash the ESP32.
                flash_esp32 = false;
                break;
            }
        }
        adsbee.EnableWatchdog();  // Restore watchdog.
#ifndef DEBUG_DISABLE_ESP32_FLASH
        // If we never read from the ESP32, or read a different firmware version, try writing to it.
        if (flash_esp32) {
            adsbee.DisableWatchdog();  // Disable watchdog while flashing.
            if (!esp32.DeInit()) {
                CONSOLE_ERROR("main", "Error while de-initializing ESP32 before flashing.");
            } else if (!esp32_flasher.FlashESP32()) {
                CONSOLE_ERROR("main", "Error while flashing ESP32. Disabling.");
                esp32.SetEnable(false);  // Disable ESP32 if flashing failed.
            } else if (!esp32.Init()) {
                CONSOLE_ERROR("main", "Error while re-initializing ESP32 after flashing.");
            }
            adsbee.EnableWatchdog();  // Restore watchdog after flashing.
        }
#endif
    }

    multicore_reset_core1();
    multicore_launch_core1(main_core1);

    uint32_t esp32_last_heartbeat_timestamp_ms = 0;

    while (true) {
        // Loop forever.
        decoder.UpdateLogLoop();
        comms_manager.Update();
        adsbee.Update();

        esp32.Update();

        // Poke the watchdog to keep things alive if the ESP32 is responding or if it's disabled.
        uint32_t old_esp32_last_heartbeat_timestamp_ms = esp32_last_heartbeat_timestamp_ms;
        esp32_last_heartbeat_timestamp_ms = esp32.GetLastHeartbeatTimestampMs();
        if (esp32_last_heartbeat_timestamp_ms != old_esp32_last_heartbeat_timestamp_ms || !esp32.IsEnabled()) {
            // Don't need to talk to the ESP32, or it acknowledged a heartbeat just now: poke the watchdog since nothing
            // seems amiss.
            adsbee.PokeWatchdog();
        }
    }
}
*/