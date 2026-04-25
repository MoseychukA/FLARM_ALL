/*
  Модуль Mavlink.h
  Назначение:
  - Публичный интерфейс подсистемы MAVLink.

  Основные задачи модуля:
  - Формировать бинарный поток MAVLink v1 для нашего самолета и сторонних целей.
  - Отправлять данные в Serial и/или RS485 согласно настройкам WEB-интерфейса.
  - Предоставлять кодирование и декодирование MAVLink-пакетов для радиоканала LMIC LoRa.
  - Не требовать внешней библиотеки MAVLink и работать как автономный модуль проекта.
*/

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <protocol.h>
#include "RadioProtocols.h"
#include "Container.h"

#ifndef MAVLINK_LORA_PAYLOAD_SIZE
#define MAVLINK_LORA_PAYLOAD_SIZE 64U
#endif

extern const rf_proto_desc_t mavlink_lora_proto_desc;

void Mavlink_setup();
void Mavlink_loop();
void Mavlink_fini();

size_t mavlink_lora_encode(void* pkt, ufo_t* this_aircraft);
bool mavlink_lora_decode(void* pkt, ufo_t* this_aircraft, ufo_t* fop);

