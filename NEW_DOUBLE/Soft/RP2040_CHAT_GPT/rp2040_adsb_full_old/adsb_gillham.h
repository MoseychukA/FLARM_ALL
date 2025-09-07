#ifndef ADSB_GILLHAM_H
#define ADSB_GILLHAM_H

#include <Arduino.h>

#define GILLHAM_INVALID -99999

void gillhamInit();
int gillham2alt(uint16_t ac13, bool qbit);

#endif
