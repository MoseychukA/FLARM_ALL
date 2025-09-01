#ifndef SHARED_H
#define SHARED_H

#include "Data_Structures.h"
#include <stdlib.h> // дл€ free, если используете

// ќбъ€вление внешних переменных
extern PFBQueue<char*> decode_debug_message_out_queue;
extern volatile bool decode_message_available;

extern volatile bool message_ready;
extern uint8_t message_buffer[];
extern size_t message_len_bytes;



#endif