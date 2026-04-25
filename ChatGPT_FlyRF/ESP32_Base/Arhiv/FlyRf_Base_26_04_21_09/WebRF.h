/*
  Модуль WebRF.h
  Назначение:
  - Публичный интерфейс WEB-сервера проекта.

  Что содержит файл:
  - Объявления функций запуска, обслуживания и остановки WEB-интерфейса.
  - Внешние объекты, используемые совместно с сетевой частью.
*/

#pragma once
#include <Arduino.h>
#include <WiFiClient.h>

void Web_setup(void);
void Web_loop(void);
void Web_fini(void);

extern String TxDataTemplate;
extern WiFiClient client;
