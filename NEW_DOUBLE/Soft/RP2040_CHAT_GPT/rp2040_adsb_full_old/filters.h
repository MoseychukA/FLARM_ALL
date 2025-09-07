#pragma once
#include "adsb_types.h"
void filtersInit();
bool filtersAllow(const DecodedADSB &d);
void filtersSetIcao(uint32_t icao); // 0=all
void filtersSetMinAlt(int ft);
void filtersSetMaxAlt(int ft);