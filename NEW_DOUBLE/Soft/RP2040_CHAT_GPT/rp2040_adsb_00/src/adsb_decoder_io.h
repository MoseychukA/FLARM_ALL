#pragma once
#include "adsb_decoder.h"
#include <Arduino.h>

bool packet_queue_init(size_t depth);
bool enqueue_packet_from_core1(const adsb_packet_t& pkt);
bool dequeue_packet_for_core0(adsb_packet_t* out, uint32_t timeout_ms);