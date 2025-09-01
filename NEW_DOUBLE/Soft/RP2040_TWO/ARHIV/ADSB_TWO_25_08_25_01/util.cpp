#include "shared.h"
#include "decode.h"

void finish_processing(const RawPacket& rx_packet, uint64_t icao_value) 
{
    parse_raw_packet(rx_packet, shared_packet);
    decode_cpr((uint8_t*)&rx_packet, shared_packet.lat_msg, shared_packet.lon_msg);
    shared_packet.icao = icao_value;
    new_packet_ready = true;
}