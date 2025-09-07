#ifndef ADSB_TX_H
#define ADSB_TX_H

#include "adsb_packet.h"

void uartInit();
void sendToESP32(const ToDUMP1090 &pkt);

#endif
