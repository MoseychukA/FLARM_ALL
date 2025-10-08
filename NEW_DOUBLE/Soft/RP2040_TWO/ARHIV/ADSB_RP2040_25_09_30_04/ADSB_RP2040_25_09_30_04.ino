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

 
BSP bsp = BSP({});
ADSBee adsbee = ADSBee({});
SettingsManager settings_manager;
PacketDecoder decoder = PacketDecoder({ .enable_1090_error_correction = true });
AircraftDictionary send_dictionary;

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
   // send_dictionary.transmitBaseAllAircrafts();
}


void loop1()
{
   decoder.UpdateDecoderLoop();   //PacketDecoder 

}
