#ifndef ADSB_SQUAWK_H
#define ADSB_SQUAWK_H

#include <Arduino.h>

/**
 * Декодирование Squawk из 13 бит Mode A.
 * @param code — 13 бит (DF5/DF21 Mode A)
 * @return squawk (0000–7777), либо -1 при ошибке
 */
int decodeSquawk(uint16_t code);

#endif // ADSB_SQUAWK_H
