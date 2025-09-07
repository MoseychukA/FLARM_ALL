#pragma once
#include <stdint.h>
#include <stddef.h>

// Проверка CRC Mode-S. Возвращает true если ОК.
// Поддерживает 56/112 бит.
// msgBits содержит биты (младший бит каждого msg[i] — первый по времени в этом байте).
bool modes_crc_ok(const uint8_t* msgBytes, int nBits);

// Попытка коррекции одной ошибки бита (flip 1 bit) — если удачно, msgBytes модифицируется.
// Возвращает true, если удачно откорректировано.
bool modes_crc_try_fix_1bit(uint8_t* msgBytes, int nBits);

// Полный расчёт CRC, возвращает 24-битный синдром.
uint32_t modes_crc_syndrome(const uint8_t* msgBytes, int nBits);
