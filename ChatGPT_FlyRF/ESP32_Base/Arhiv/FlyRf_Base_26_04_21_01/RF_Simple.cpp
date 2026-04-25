/*
  Модуль RF_Simple.cpp
  Назначение:
  - Упрощенный вариант использования радиочасти проекта.

  Основные задачи модуля:
  - Дать минимальную обертку вокруг RF_setup/RF_loop/ParseData.
  - Использоваться как простая точка старта для отладки и дальнейшего упрощения радиочасти.
*/

#include "RF_Simple.h"
#include "RF.h"
#include "Container.h"
#include "Log.h"

// Вариант проще: использует уже существующие RF_setup/RF_loop/ParseData,
// но без тайм-слотов, без TX-планировщика и без лишней логики вокруг Web/NMEA.
// Этот файл оставлен как стартовая основа для дальнейшего упрощения RF.cpp.

byte RF_Simple_setup(void)
{
    return RF_setup();
}

void RF_Simple_loop(void)
{
    // Только прием и последующий разбор принятого пакета.
    RF_loop();
    ParseData();
}

void RF_Simple_parse(void)
{
    ParseData();
}

void RF_Simple_getPacketCounters(uint32_t &tx, uint32_t &rx)
{
    RF_GetPacketCounters(tx, rx);
}
