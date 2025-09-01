#include "shared.h"

PFBQueue<char*> decode_debug_message_out_queue({ .buf_len_num_elements = 10, .buffer = nullptr, .overwrite_when_full = true });
volatile bool decode_message_available = false;

volatile bool message_ready = false;
uint8_t message_buffer[512];
size_t message_len_bytes = 0;