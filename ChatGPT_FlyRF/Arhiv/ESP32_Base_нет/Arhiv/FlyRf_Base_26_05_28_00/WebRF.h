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
bool Web_otaInProgress(void);

extern String TxDataTemplate;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
extern WiFiClient client;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
