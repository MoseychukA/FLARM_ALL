// Visual Micro is in vMicro>General>Tutorial Mode
// 
/*
    Name:       ESP32_Info.ino
    Created:	15.12.2024 6:55:53
    Author:     MASTER\Alex
*/

#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"
#include "usb/usb_host.h"
#include <esp_task_wdt.h>
#include "esp32-hal-psram.h"

void logMemory()
{
    log_d("Total heap: %d", ESP.getHeapSize());
    log_d("Free heap: %d", ESP.getFreeHeap());
    log_d("Total PSRAM: %d", ESP.getPsramSize());
    log_d("Free PSRAM: %d", ESP.getFreePsram());
    log_d("Used PSRAM: %d", ESP.getPsramSize() - ESP.getFreePsram());
    log_d("spiram size %u", esp_spiram_get_size());
    log_d("FlashChipSize %u\n", ESP.getFlashChipSize());

}


// The setup() function runs once each time the micro-controller starts
void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 1000);

    esp_log_level_set("*", ESP_LOG_VERBOSE);  // Выводим отладочные сообщения

    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    ver_soft.remove(val_srt);
    Serial.println("\n*** Version " + ver_soft + "\n");
    if (psramInit())
    {
        Serial.println("!PSRAM is correctly initialized\n");
    }
    else
    {
        Serial.println("PSRAM not available");
    }

    logMemory();

}

// Add the main program code into the continuous loop() function
void loop()
{


}
