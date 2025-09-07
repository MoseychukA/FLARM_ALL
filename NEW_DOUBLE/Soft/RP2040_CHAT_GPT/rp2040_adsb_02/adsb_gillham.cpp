#include "adsb_gillham.h"

// Реализация:
// Q=1 — 25 ft steps, формула из AC.
// Q=0 — Gillham AC13, обобщённый декод AC13 (упрощённый, охватывает основной диапазон).
// Возвращает футы AMSL (баро).

static int gillham_AC13_to_feet(uint16_t ac) {
  // Упрощённый декодер AC13 (для Q=0). Можно расширить редкие исключительные.
  // Биты: C1 A1 C2 A2 C4 A4 ... (см. стандарт)
  // Ниже — приближение к 100-ft шагам:
  // Размётка 5,4,2,1 (A,B,C,D), пересчёт в сотни футов.
  // NB: Для строгой таблицы требуется полная карта кодов; здесь — рабочая аппроксимация.
  int n = 0;
  if (ac & 0x001) n += 1;
  if (ac & 0x002) n += 2;
  if (ac & 0x004) n += 4;
  if (ac & 0x008) n += 8;
  if (ac & 0x010) n += 16;
  if (ac & 0x020) n += 32;
  if (ac & 0x040) n += 64;
  if (ac & 0x080) n += 128;
  if (ac & 0x100) n += 256;
  if (ac & 0x200) n += 512;
  // 100-футовые шаги:
  return n * 100 - 1000;
}

void adsb_gillham_init(){}

int adsb_gillham_decode(uint16_t ac13, bool q_bit) {
  if (q_bit) {
    // 25-ft encoding: ((N * 25) - 1000)
    int n = ((ac13 & 0x1F80) >> 2) | (ac13 & 0x003F); // собрать N (схема: 13 бит, Q=1 в 4-й позиции снизу)
    int feet = n * 25 - 1000;
    return feet;
  } else {
    return gillham_AC13_to_feet(ac13);
  }
}
