/*
    Основная точка входа для FlyRf_Base_26_06_08_02.
    Отображаемая версия прошивки определяется именем этого файла.
    
    Настройки компиляции.
    IDE: Arduino 1.6/1.8
    Board: ESP32S3 Dev Module (esp32_esp32s3)

    Option 1: Upload Speed: 921600
    Option 2: USB Mode: Hardware CDC and JTAG
    Option 3: USB CDC On Boot: Disabled
    Option 4: USB Firmware MSC On Boot: Disabled
    Option 5: USB DFU On Boot: Disabled
    Option 6: Upload Mode: UART0 / Hardware CDC
    Option 7: CPU Frequency: 240MHz (WiFi)
    Option 8: Flash Mode: DIO 80MHz
    Option 9: Flash Size: 16MB (128Mb)
    Option 10: Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)
    Option 11: Core Debug Level: None
    Option 12: PSRAM: OPI PSRAM
    Option 13: Arduino Runs On: Core 1
    Option 14: Events Run On: Core 1
    Option 15: Erase All Flash Before Sketch Upload: Disabled
    Option 16: JTAG Adapter: Disabled
    Option 17: FileSystem Upload Tool: SPIFFS
    Option 18: Automatic FS Upload: Off
*/

#include <Arduino.h>
#include "System.h"
#include "DeviceInfo.h"

void setup()
{
    Serial.begin(115200);
    delay(200);
    DeviceInfo_setProgramVersion(DeviceInfo_programVersionFromFile(__FILE__));
    SystemSetup();
}

void loop()
{
    SystemLoop();
}
