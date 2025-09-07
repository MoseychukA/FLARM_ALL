#pragma once
#include "adsb_structs.h"

void adsb_correlator_init();
bool adsb_correlator_detect(const uint8_t *samples, int len, AdsbPacket &out);