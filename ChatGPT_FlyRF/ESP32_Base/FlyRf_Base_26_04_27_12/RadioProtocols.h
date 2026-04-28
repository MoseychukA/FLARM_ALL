/*
  Модуль RadioProtocols.h
  Назначение:
  - Общие идентификаторы радиопротоколов проекта.

  Что содержит файл:
  - Стандартные идентификаторы из basicmac/protocol.h.
  - Локальный идентификатор протокола MAVLink over LoRa,
    который используется для переключения радиоканала между OGNTP и MAVLink.
*/

#pragma once

#include <stdint.h>
#include <protocol.h>

#ifndef RF_PROTOCOL_MAVLINK
#define RF_PROTOCOL_MAVLINK 100U
#endif

