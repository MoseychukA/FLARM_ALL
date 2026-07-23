#include "System.h"
#include <Arduino.h>
#include "HardwareConfig.h"
#include "DeviceInfo.h"
#include "EEPROMRF.h"
#include "Container.h"
#include "WiFiRF.h"
#include "WebRF.h"
#include "RS485Display.h"
#include "ESP32RF.h"
#include "Bluetooth.h"

static uint32_t g_lastLoopMs = 0;
static bool g_powerWasDisabled = false;
static bool g_otaMode = false;

static const uint32_t POWER_OFF_PULSE_MS = 6000UL;
static const uint32_t POWER_OFF_DEBOUNCE_MS = 30UL;

void SystemSetup()
{
    Serial.println();
    Serial.print(F("[SETUP] Version: "));
    Serial.println(DeviceInfo_programVersion());

    pinMode(POWER_ON_PIN, OUTPUT);
    digitalWrite(POWER_ON_PIN, HIGH);
    pinMode(POWER_TRACKER_PIN, OUTPUT);
    digitalWrite(POWER_TRACKER_PIN, LOW);
    pinMode(POWER_IN_PIN, INPUT);
#if BUTTON_2_PIN >= 0
    pinMode(BUTTON_2_PIN, INPUT_PULLUP);
#endif
#if LED_PIN >= 0
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
#endif
#ifdef LED_LCD_PIN
    pinMode(LED_LCD_PIN, OUTPUT);
    digitalWrite(LED_LCD_PIN, HIGH);
#endif

    EEPROM_setup();
    TrafficDB.init();
    RS485Display_setup();
    WiFi_setup();
    Web_setup();
    Bluetooth_setup();
    Display_setup();
    Serial.println(F("[SETUP] READY"));
}

void SystemLoop()
{
    if (g_otaMode)
    {
        WiFi_loop();
        Web_loop();
        delay(1);
        return;
    }

    if (!g_powerWasDisabled && digitalRead(POWER_IN_PIN) == LOW)
    {
        digitalWrite(POWER_ON_PIN, HIGH);
        delay(POWER_OFF_DEBOUNCE_MS);
        if (digitalRead(POWER_IN_PIN) == LOW)
        {
            g_powerWasDisabled = true;
            Display_powerOff();
            digitalWrite(POWER_TRACKER_PIN, HIGH);
            delay(POWER_OFF_PULSE_MS);
            digitalWrite(POWER_TRACKER_PIN, LOW);
            delay(100);
            digitalWrite(POWER_ON_PIN, LOW);
        }
        return;
    }

    RS485Display_loop();
    WiFi_loop();
    Web_loop();
    Bluetooth_loop();
    Traffic_loop();
    Display_loop();

    const uint32_t now = millis();
    if ((uint32_t)(now - g_lastLoopMs) > 500)
    {
        g_lastLoopMs = now;
#if LED_PIN >= 0
        digitalWrite(LED_PIN, (RS485Display_lastRxMs() && (now - RS485Display_lastRxMs() < 3000)) ? HIGH : LOW);
#endif
    }
}

void SystemEnterOtaMode()
{
    if (g_otaMode) return;
    g_otaMode = true;
    Bluetooth_fini();
    Serial.println(F("[OTA] Display, RS485 and Bluetooth processing paused"));
}

bool SystemOtaMode()
{
    return g_otaMode;
}

float SystemDisplayCourseDeg()
{
    return ThisAircraft.course;
}
