#include "settings.h"

//    ,      settings.h,   .
// ESP32  RP2040    settings.cpp,        .
const char SettingsManager::kConsoleLogLevelStrs[SettingsManager::LogLevel::kNumLogLevels]
                                                [SettingsManager::kConsoleLogLevelStrMaxLen] = {"SILENT", "ERRORS",
                                                                                                "WARNINGS", "INFO"};
const char SettingsManager::kSerialInterfaceStrs[SettingsManager::SerialInterface::kNumSerialInterfaces]
                                                [SettingsManager::kSerialInterfaceStrMaxLen] = {"CONSOLE", "COMMS_UART"
                                                                                                };
