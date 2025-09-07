#pragma once
#include "adsb_structs.h"

void adsb_dma_init();
bool adsb_dma_fetch(AdsbPacket *pkt);