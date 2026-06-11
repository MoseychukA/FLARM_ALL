/*
  Модуль FlyRfSpi.h
  Назначение:
  - Публичный интерфейс управления доступом к SPI-шине.

  Что содержит файл:
  - Объявления функций инициализации, блокировки и разблокировки SPI.
  - RAII-обертку FlyRfSpiGuard для безопасного удержания шины в пределах области.
*/

#pragma once
#include <Arduino.h>

extern "C" void FlyRfSpiSetup();
extern "C" bool FlyRfSpiLock(uint32_t timeoutMs = 50);
extern "C" void FlyRfSpiUnlock();

struct FlyRfSpiGuard
{
    bool locked;
    explicit FlyRfSpiGuard(uint32_t timeoutMs = 50) : locked(FlyRfSpiLock(timeoutMs)) {}
    ~FlyRfSpiGuard() { if (locked) FlyRfSpiUnlock(); }
    operator bool() const { return locked; }
};
