#pragma once
#include <Arduino.h>
#include "adsb_structs.h"

void adsb_decoder_init();
bool adsb_decoder_fetch(AdsbPacket *pkt);       // из очереди полностью собранных пакетов
DecodedADSB adsb_decode_packet(const AdsbPacket &pkt);
