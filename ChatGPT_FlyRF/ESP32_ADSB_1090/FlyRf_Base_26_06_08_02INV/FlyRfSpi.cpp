/*
  Модуль FlyRfSpi.cpp
  Назначение:
  - Заглушка/обертка для синхронизации доступа к SPI-шине проекта.

  Основные задачи модуля:
  - Предоставить единый интерфейс блокировки SPI.
  - Позволить безопасно использовать общий SPI-ресурс из разных модулей.
  - В текущем проекте служить простой совместимой прослойкой без сложной логики.
*/

#include "FlyRfSpi.h"

extern "C" void FlyRfSpiSetup()
{
}

extern "C" bool FlyRfSpiLock(uint32_t timeoutMs)
{
    (void)timeoutMs;
    return true;
}

extern "C" void FlyRfSpiUnlock()
{
}
