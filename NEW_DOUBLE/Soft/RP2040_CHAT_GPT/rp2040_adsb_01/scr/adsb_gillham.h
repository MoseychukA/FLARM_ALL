#pragma once
#include <stdint.h>

// Инициализация (если нужно)
void adsb_gillham_init();

// Декод высоты из AC13 поле DF17/ME TC=9/11/18 и при Q=1/0.
// Возвращает высоту в футах или INT32_MIN при ошибке.
// altCode — 13 бит (без M битов паритета), qBit — 0/1.
int32_t gillham_decode_ac13(uint16_t altCode, int qBit);

// Обратное кодирование (при необходимости)
uint16_t gillham_encode_q1(int32_t feet); // Q=1 (25ft)
