#ifndef SHARED_H
#define SHARED_H

#include "decode.h"
#include "transponder_packet.h"

extern volatile bool raw_packet_available; // флаг наличия пакета
extern Raw1090Packet shared_raw_packet;        // глобальный буфер пакета

#endif
