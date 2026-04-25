/*
  Модуль FlarmDecoder.h
  Назначение:
  - Объявление интерфейса декодера FLARM и его диагностических счетчиков.

  Что содержит файл:
  - Структуру статистики FlarmDecodeDiag.
  - Функции декодирования, получения статистики и ее сброса.
*/

#pragma once

#include <Arduino.h>
#include "TrafficTypes.h"

struct FlarmDecodeDiag
{
    uint32_t okCount;  // Счетчик, индекс, позиция или номер элемента.
    uint32_t rejectCount;  // Счетчик, индекс, позиция или номер элемента.
    uint32_t shortPacketCount;  // Счетчик, индекс, позиция или номер элемента.
    uint32_t emptyPatternCount;  // Счетчик, индекс, позиция или номер элемента.
    uint32_t invalidFieldCount;  // Логический флаг состояния, разрешения или наличия данных.
    uint32_t candidateCount;  // Счетчик, индекс, позиция или номер элемента.
    uint32_t positionClampCount;  // Счетчик, индекс, позиция или номер элемента.
    uint32_t lastIcao;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    uint8_t lastLength;  // Параметр геометрии, координаты, размера или угла.
    int lastRssi;  // Буфер, текстовая строка или рабочее сообщение.
    float lastSnr;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    uint32_t lastDecodeMs;  // Временная отметка, интервал или значение тайм-аута.
};

bool FlarmDecoder_process(const uint8_t* data, size_t len, int rssi, float snr);
bool FlarmDecoder_decodeCandidate(const uint8_t* data, size_t len, int rssi, float snr, TrafficCandidate& outCandidate);
uint32_t FlarmDecoder_decodeCount();
uint32_t FlarmDecoder_rejectCount();
bool FlarmDecoder_getDiag(FlarmDecodeDiag& outDiag);
void FlarmDecoder_resetStats();
