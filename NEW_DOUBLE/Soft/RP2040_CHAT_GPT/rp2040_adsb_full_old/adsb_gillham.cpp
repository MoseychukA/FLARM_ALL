#include "adsb_gillham.h"

// Таблица на 8192 кодов (Q=0)
static int16_t gillham_table[8192];

// Преобразование бита в «Gillham value»
static int gray2bin(int g) {
    int b = 0;
    for (; g; g >>= 1) b ^= g;
    return b;
}

// Реальное преобразование AC13 → высота
static int decodeGillhamCode(uint16_t ac13) {
    // Извлекаем отдельные биты
    int A1 = (ac13 >> 0) & 1;
    int A2 = (ac13 >> 1) & 1;
    int A4 = (ac13 >> 2) & 1;
    int B1 = (ac13 >> 3) & 1;
    int B2 = (ac13 >> 4) & 1;
    int B4 = (ac13 >> 5) & 1;
    int C1 = (ac13 >> 6) & 1;
    int C2 = (ac13 >> 7) & 1;
    int C4 = (ac13 >> 8) & 1;
    int D1 = (ac13 >> 9) & 1;
    int D2 = (ac13 >> 10) & 1;
    int D4 = (ac13 >> 11) & 1;

    // Составляем «группы» Gray code
    int units = A1 | (A2 << 1) | (A4 << 2);
    int tens  = B1 | (B2 << 1) | (B4 << 2);
    int hundreds = C1 | (C2 << 1) | (C4 << 2);
    int fiveHundreds = D1 | (D2 << 1) | (D4 << 2);

    // Конвертируем Gray → bin
    int u = gray2bin(units);
    int t = gray2bin(tens);
    int h = gray2bin(hundreds);
    int f = gray2bin(fiveHundreds);

    // Высота
    int altitude = -1000 + (f * 500 + h * 100 + t * 10 + u) * 100;

    // Проверка диапазона
    if (altitude < -1000 || altitude > 126750) {
        return GILLHAM_INVALID;
    }

    return altitude;
}

void gillhamInit() {
    for (uint16_t code = 0; code < 8192; code++) {
        gillham_table[code] = decodeGillhamCode(code);
    }
}

int gillham2alt(uint16_t ac13, bool qbit) {
    if (qbit) {
        // Q=1: шаг 25 ft
        int n = ac13 & 0x0FFF;
        return n * 25 - 1000;
    } else {
        // Q=0: используем таблицу
        if (ac13 < 8192) {
            return gillham_table[ac13];
        }
        return GILLHAM_INVALID;
    }
}
