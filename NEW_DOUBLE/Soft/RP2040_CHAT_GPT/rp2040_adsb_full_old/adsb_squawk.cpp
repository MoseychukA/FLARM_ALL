#include "adsb_squawk.h"

/**
 * Декодирование Squawk (Mode A).
 * Вход: 13 бит AC13 кода Mode A.
 * Выход: 4-значный восьмеричный код (0000–7777).
 */
int decodeSquawk(uint16_t code) {
    int A = (code >> 12) & 0x07;
    int B = (code >> 9)  & 0x07;
    int C = (code >> 6)  & 0x07;
    int D = (code >> 3)  & 0x07;

    if (A > 7 || B > 7 || C > 7 || D > 7) return -1;

    return (A << 9) | (B << 6) | (C << 3) | D;
}
