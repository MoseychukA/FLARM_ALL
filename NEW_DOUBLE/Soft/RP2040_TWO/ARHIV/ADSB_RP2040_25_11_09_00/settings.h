#ifndef SETTINGS_HH_
#define SETTINGS_HH_

#include <stdlib.h>
#include <cstdint>
#include <cstring>     // for memset
#include <functional>  // for strtoull

#include "macros.h"
#include "stdio.h"


class SettingsManager 
{
   public:
    // Serial Interface enum and string conversion array.
    enum SerialInterface : uint16_t { kConsole = 0, kCommsUART, kNumSerialInterfaces };
    static constexpr uint16_t kSerialInterfaceStrMaxLen = 30;
    static const char kSerialInterfaceStrs[SerialInterface::kNumSerialInterfaces][kSerialInterfaceStrMaxLen];

    enum LogLevel : uint16_t { kSilent = 0, kErrors, kWarnings, kInfo, kNumLogLevels };
    static constexpr uint16_t kConsoleLogLevelStrMaxLen = 30;
    static const char kConsoleLogLevelStrs[LogLevel::kNumLogLevels][kConsoleLogLevelStrMaxLen];

    enum EnableState : int8_t {
        kEnableStateExternal = -1,  // Enable GPIO pin is high impedance.
        kEnableStateDisabled = 0,
        kEnableStateEnabled = 1
    };

    // Эта структура содержит неизменяемые настройки, которые должны сохраняться при перезагрузках, но могут быть перезаписаны во время
    // обновления прошивки, если формат структуры настроек изменится.
    struct Settings {
        static constexpr int kDefaultTLMV = 1300;  // [mV]
        static constexpr uint16_t kMaxNumTransponderPackets = 100;  // Определяет размер кольцевого буфера ADSBPacket (PFBQueue).
        static constexpr uint32_t kDefaultWatchdogTimeoutSec = 10;
        static constexpr uint32_t kDefaultCommsUARTBaudrate = 115200;
 
        // ADSBee settings
        bool receiver_enabled = true;
        int tl_mv = kDefaultTLMV;
        bool bias_tee_enabled = false;
        uint32_t watchdog_timeout_sec = kDefaultWatchdogTimeoutSec;

        // Настройки менеджера коммуникаций
        LogLevel log_level = LogLevel::kWarnings;
        uint32_t comms_uart_baud_rate = 115200;

    };

 
    bool Apply();

    /**
    * Загружает настройки из EEPROM. Предполагается, что настройки хранятся по адресу 0x0, и не выполняет проверку целостности.
    * @retval True в случае успеха, в противном случае false.
    */
    bool Load();

         /**
     * Saves settings to EEPROM. Stores settings at address 0x0 and performs no integrity check.
     * @retval True if succeeded, false otherwise.
     */
    bool Save();

    Settings settings;

   private:
};

extern SettingsManager settings_manager;

#endif 