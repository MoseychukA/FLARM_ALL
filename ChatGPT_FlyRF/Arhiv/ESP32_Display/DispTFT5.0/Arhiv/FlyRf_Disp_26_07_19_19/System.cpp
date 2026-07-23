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
    // Bounce-буферы RGB требуют крупные непрерывные блоки DMA-SRAM.
    // Выделяем их до запуска Wi-Fi, WEB и буферов UART RS485.
    Display_setup();
    RS485Display_setup();
    WiFi_setup();
    Web_setup();
    Serial.println(F("[SETUP] READY"));
}

void SystemLoop()
{
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

    // Кадр дисплея всегда строится только после полного приёма, проверки CRC
    // и расшифровки RS485-пакета в ThisAircraft/Container/Remote state.
    RS485Display_loop();
    WiFi_loop();
    Web_loop();
    Traffic_loop();
    Display_loop();
    // Сразу после возможной полной перерисовки забираем кадры, которые UART
    // накопил за время работы дисплея. Выполняется в той же loop-задаче.
    RS485Display_loop();

    const uint32_t now = millis();
    if ((uint32_t)(now - g_lastLoopMs) > 500)
    {
        g_lastLoopMs = now;
#if LED_PIN >= 0
        digitalWrite(LED_PIN, (RS485Display_lastRxMs() && (now - RS485Display_lastRxMs() < 3000)) ? HIGH : LOW);
#endif
    }
}

float SystemDisplayCourseDeg()
{
    return ThisAircraft.course;
}
