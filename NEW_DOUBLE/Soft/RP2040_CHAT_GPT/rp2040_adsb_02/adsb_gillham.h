#pragma once
#include <stdint.h>

void adsb_gillham_init();
int  adsb_gillham_decode(uint16_t ac13, bool q_bit); // высота в футах (или -9999)
