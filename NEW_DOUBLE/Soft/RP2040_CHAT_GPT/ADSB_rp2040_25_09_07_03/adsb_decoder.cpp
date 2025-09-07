#include "adsb_decoder.h"
#include <string.h>

// CRC-24 функция
static uint32_t crc24(const uint8_t *buffer, uint16_t buffer_len_bytes, uint32_t initial_value = 0) {
    uint32_t crc = initial_value;
    for (uint16_t i = 0; i < buffer_len_bytes; i++) {
        uint8_t byte = buffer[i];
        crc = ((crc << 8) ^ crc24_table[((crc >> 16) ^ byte) & 0xFF]) & 0xFFFFFF;
    }
    return crc;
}

static void WordBufferToByteBuffer(const uint32_t word_buffer[], uint8_t byte_buffer[], uint16_t num_bytes) {
    for (uint16_t i = 0; i < (num_bytes + 3) / 4; i++) {
        uint16_t base = i * 4;
        if (base < num_bytes) byte_buffer[base]     = word_buffer[i] >> 24;
        if (base + 1 < num_bytes) byte_buffer[base+1] = (word_buffer[i] >> 16) & 0xFF;
        if (base + 2 < num_bytes) byte_buffer[base+2] = (word_buffer[i] >> 8) & 0xFF;
        if (base + 3 < num_bytes) byte_buffer[base+3] = word_buffer[i] & 0xFF;
    }
}

bool adsb_check_crc(const Raw1090Packet& pkt) {
    uint16_t packet_len_bytes = pkt.buffer_len_bits / 8;
    if (packet_len_bytes < 7) return false;

    uint8_t raw_buffer[16];
    WordBufferToByteBuffer(pkt.buffer, raw_buffer, packet_len_bytes);

    uint32_t crc_calc = crc24(raw_buffer, packet_len_bytes - 3);
    uint32_t crc_recv = (raw_buffer[packet_len_bytes-3] << 16) |
                        (raw_buffer[packet_len_bytes-2] << 8) |
                        (raw_buffer[packet_len_bytes-1]);

    return crc_calc == crc_recv;
}

DecodedAdsb adsb_decode(const Raw1090Packet& pkt) {
    DecodedAdsb out;
    uint16_t packet_len_bytes = pkt.buffer_len_bits / 8;
    if (packet_len_bytes < 7) return out;

    uint8_t raw_buffer[16];
    WordBufferToByteBuffer(pkt.buffer, raw_buffer, packet_len_bytes);

    out.df  = raw_buffer[0] >> 3;
    out.icao = (raw_buffer[1] << 16) | (raw_buffer[2] << 8) | raw_buffer[3];

    if (out.df == 17 && packet_len_bytes >= 14) {
        out.tc = raw_buffer[4] >> 3;
        if (out.tc >= 1 && out.tc <= 4) {
            for (int i = 0; i < 8; i++) {
                uint8_t c = raw_buffer[5+i];
                out.callsign[i] = (c >= 32 && c <= 126) ? c : '_';
            }
            out.callsign[8] = '\0';
        } else if (out.tc >= 9 && out.tc <= 18) {
            uint32_t raw_alt = ((raw_buffer[5] & 0x07) << 8) | raw_buffer[6];
            out.alt = raw_alt * 25 - 1000;
        }
    }

    return out;
}
