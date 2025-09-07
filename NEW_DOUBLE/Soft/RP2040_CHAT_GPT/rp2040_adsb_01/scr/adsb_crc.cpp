#include "adsb_crc.h"

// Полином Mode-S (24 бита), используем таблицу/сдвиговый метод.
// Здесь используем «битовый» расчёт без большой таблицы (быстро для 112 бит).

// Возвращает i-й бит (0..nBits-1), где i=0 — старший бит всего сообщения (MSB first по стандарту).
// Но наши байты приходят "LSB first в байте". Преобразуем.
static inline int get_bit_msbf(const uint8_t* b, int i, int nBits) {
  // MSB-first позиция: bit 0 — самый старший бит msg[0]
  int byte = i >> 3;
  int bitInByte = 7 - (i & 7);
  // Но наши данные упакуем MSB-first перед CRC. Предположим, что msgBytes уже MSB-first.
  // Для простоты в сборке бит мы сформируем msgBytes MSB-first. Тогда тут:
  return (b[byte] >> bitInByte) & 1;
}

uint32_t modes_crc_syndrome(const uint8_t* msg, int nBits) {
  // Генератор Mode-S: G(x) = x^24 + x^23 + x^18 + x^17 + x^14 + x^11 + x^10 + x^7 + x^6 + x^3 + x + 1
  const uint32_t poly = 0xFFF409; // обратный полином в стандартной реализации не нужен — используем битовый метод

  // Битовый сдвиговый регистр 24 бита
  uint32_t reg = 0;
  for (int i = 0; i < nBits - 24; i++) {
    int bit = get_bit_msbf(msg, i, nBits) ^ ((reg >> 23) & 1);
    reg = ((reg << 1) & 0xFFFFFF);
    if (bit) {
      // XOR с полиномом, сдвинутым так, чтобы старший бит попадал в 23 позицию
      reg ^= 0x1FFF409; // x^24 убран, остаётся 24-битное представление
    }
  }
  // Остаток должен равняться 24 битам последних из сообщения
  // Для синдрома до конца прогоняем ещё 24 бита
  for (int i = nBits - 24; i < nBits; i++) {
    int bit = get_bit_msbf(msg, i, nBits) ^ ((reg >> 23) & 1);
    reg = ((reg << 1) & 0xFFFFFF);
    if (bit) reg ^= 0x1FFF409;
  }
  return reg & 0xFFFFFF;
}

bool modes_crc_ok(const uint8_t* msg, int nBits) {
  return modes_crc_syndrome(msg, nBits) == 0;
}

static inline void flip_bit_msbf(uint8_t* msg, int i) {
  int byte = i >> 3;
  int bitInByte = 7 - (i & 7);
  msg[byte] ^= (1u << bitInByte);
}

bool modes_crc_try_fix_1bit(uint8_t* msg, int nBits) {
  uint32_t syn = modes_crc_syndrome(msg, nBits);
  if (syn == 0) return true;
  // Попытаемся перевернуть каждый из первых nBits-24 бит (информационные).
  // Это грубо O(nBits) — допустимо.
  for (int i = 0; i < nBits - 24; i++) {
    flip_bit_msbf(msg, i);
    if (modes_crc_syndrome(msg, nBits) == 0) return true;
    flip_bit_msbf(msg, i); // вернуть назад
  }
  return false;
}
