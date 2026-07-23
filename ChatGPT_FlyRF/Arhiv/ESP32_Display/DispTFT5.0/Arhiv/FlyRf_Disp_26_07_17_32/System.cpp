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
#include "ProjectHardware.h"

static uint32_t g_lastLoopMs = 0;
static bool g_powerWasDisabled = false;

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
    pinMode(POWER_IN_PIN, INPUT_PULLUP);
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

#if !(PROJECT_USE_GL050001C0_40 && PROJECT_GL050001C0_40_STATIC_IMAGE)
    RS485Display_setup();
#if !PROJECT_DIAGNOSTIC_DISABLE_WIFI_WEB
    WiFi_setup();
    Web_setup();
#endif
#endif

    Display_setup();
    Serial.println(F("[SETUP] READY"));
}

void SystemLoop()
{
#if PROJECT_USE_GL050001C0_40 && PROJECT_GL050001C0_40_STATIC_IMAGE
    delay(1000);
    return;
#endif

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
#if !PROJECT_DIAGNOSTIC_DISABLE_WIFI_WEB
    WiFi_loop();
    Web_loop();
#endif
    Traffic_loop();
    Display_loop();

    const uint32_t now = millis();
    if ((uint32_t)(now - g_lastLoopMs) > 500)
    {
        g_lastLoopMs = now;
#if LED_PIN >= 0
        digitalWrite(LED_PIN, RS485Display_baseConnected() ? HIGH : LOW);
#endif
    }
}

float SystemDisplayCourseDeg()
{
    return ThisAircraft.course;
}
