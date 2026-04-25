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
    uint32_t okCount;             
    uint32_t rejectCount;         
    uint32_t shortPacketCount;    
    uint32_t emptyPatternCount;   
    uint32_t invalidFieldCount;   
    uint32_t candidateCount;  
    uint32_t positionClampCount;  
    uint32_t lastIcao;            
    uint8_t lastLength;  
    int lastRssi;  
    float lastSnr;  
    uint32_t lastDecodeMs;  
};

bool FlarmDecoder_process(const uint8_t* data, size_t len, int rssi, float snr);
bool FlarmDecoder_decodeCandidate(const uint8_t* data, size_t len, int rssi, float snr, TrafficCandidate& outCandidate);
uint32_t FlarmDecoder_decodeCount();
uint32_t FlarmDecoder_rejectCount();
bool FlarmDecoder_getDiag(FlarmDecodeDiag& outDiag);
void FlarmDecoder_resetStats();
