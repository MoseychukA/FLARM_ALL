#include "shared.h"

uint8_t shared_packet_bytes[112] = { 0 };
size_t shared_packet_bytes_len = 0;
volatile bool new_packet_ready = false;