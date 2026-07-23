#include "OtaRollback.h"

#include <Arduino.h>
#include <esp_ota_ops.h>

static constexpr uint32_t OTA_CONFIRM_DELAY_MS = 30000UL;
static constexpr uint32_t OTA_MIN_HEALTHY_LOOPS = 100UL;

static bool g_pendingVerification = false;
static bool g_bootConfirmed = true;
static bool g_systemReady = false;
static uint32_t g_readyAtMs = 0;
static uint32_t g_lastConfirmAttemptMs = 0;
static uint32_t g_healthyLoops = 0;

// Arduino Core вызывает эту функцию до setup(). Запрещаем автоматическое
// подтверждение, чтобы приложение подтвердило запуск только после самопроверки.
extern "C" bool verifyRollbackLater(void)
{
    return true;
}

void OtaRollback_begin()
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    g_pendingVerification =
        running != nullptr &&
        esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY;
    g_bootConfirmed = !g_pendingVerification;

    if (g_pendingVerification)
    {
        Serial.println(F("[OTA] New firmware is pending verification"));
        Serial.println(F("[OTA] Rollback protection is active"));
    }
    else
    {
        Serial.println(F("[OTA] Running firmware is already confirmed"));
    }
}

void OtaRollback_setSystemReady()
{
    g_systemReady = true;
    g_readyAtMs = millis();
    g_healthyLoops = 0;
}

void OtaRollback_loop()
{
    if (!g_pendingVerification || !g_systemReady) return;

    if (g_healthyLoops < OTA_MIN_HEALTHY_LOOPS) ++g_healthyLoops;
    if ((uint32_t)(millis() - g_readyAtMs) < OTA_CONFIRM_DELAY_MS) return;
    if (g_healthyLoops < OTA_MIN_HEALTHY_LOOPS) return;

    const uint32_t now = millis();
    if ((uint32_t)(now - g_lastConfirmAttemptMs) < 1000UL) return;
    g_lastConfirmAttemptMs = now;

    const esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
    if (result == ESP_OK)
    {
        g_pendingVerification = false;
        g_bootConfirmed = true;
        Serial.println(F("[OTA] Startup confirmed; rollback cancelled"));
    }
    else
    {
        Serial.printf("[OTA] Failed to confirm startup: %s\n", esp_err_to_name(result));
    }
}

bool OtaRollback_pendingVerification()
{
    return g_pendingVerification;
}

bool OtaRollback_bootConfirmed()
{
    return g_bootConfirmed;
}
