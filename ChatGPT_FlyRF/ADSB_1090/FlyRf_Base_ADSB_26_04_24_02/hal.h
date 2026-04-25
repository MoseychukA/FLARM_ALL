#ifndef HAL_HH_
#define HAL_HH_

#include <stdint.h>

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
inline uint64_t get_time_since_boot_us() { return (uint64_t)esp_timer_get_time(); }
inline uint32_t get_time_since_boot_ms() { return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS); }
#else
#include "hal_god_powers.h"
extern uint64_t time_since_boot_us;
inline uint64_t get_time_since_boot_us() { return time_since_boot_us; }
inline uint32_t get_time_since_boot_ms() { return (uint32_t)(time_since_boot_us / 1000ULL); }
#endif

#endif /* HAL_HH_ */
