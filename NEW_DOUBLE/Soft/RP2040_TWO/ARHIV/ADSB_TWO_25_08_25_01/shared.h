#ifndef SHARED_H
#define SHARED_H

#include "decode.h"

// ќбъ€вление глобальных переменных дл€ обмена байтами
extern uint8_t shared_packet_bytes[112];   // буфер байт
extern size_t shared_packet_bytes_len;     // длина байт
extern volatile bool new_packet_ready;     // флаг обновлени€

#endif // SHARED_H
