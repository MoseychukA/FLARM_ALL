/*
  Модуль Tracker.h
  Назначение:
  - Публичный интерфейс подсистемы внешнего трекера.

  Что содержит файл:
  - Объявления функций запуска, циклического обслуживания и завершения работы трекера.
*/

#pragma once

#include <stdint.h>

struct TrackerDiagnostics
{
    uint32_t rxFrames;
    uint32_t txFrames;
    uint32_t rxBytes;
    uint32_t txBytes;
    uint32_t lastRxAgeMs;
    uint32_t lastTxAgeMs;
    bool linkActive;
    char lastRxText[391];
    char lastTxText[391];
};

void Tracker_setup();
void Tracker_loop();
void Tracker_fini();
void Tracker_getDiagnostics(TrackerDiagnostics& diagnostics);

// Есть ли активное текстовое сообщение трекера для вывода на TFT и RS485.
bool Tracker_hasActiveTextMessage();

// Получить текст активного сообщения с префиксом времени прихода.
const char* Tracker_getActiveTextMessage();
// Подтвердить прочтение активного текстового сообщения трекера.
// Отправляет ответ в трекер и удаляет сообщение с экрана/RS485.
bool Tracker_confirmActiveTextMessage();
