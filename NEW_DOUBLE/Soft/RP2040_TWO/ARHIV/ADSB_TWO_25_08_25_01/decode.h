#ifndef DECODE_H
#define DECODE_H

#include <Arduino.h>

const uint16_t kMaxPacketLenWords32 = 13;

struct RawPacket {
    uint32_t buffer[kMaxPacketLenWords32];
    uint16_t buffer_len_bits; // в битах
    int8_t source;
    int16_t sigs_dbm;
    int16_t sigq_db;
};

struct ADSBPacket {
    uint64_t icao;
    char flight[10];
    uint16_t squawk;
    uint16_t altitude;
    uint16_t speed;
    uint16_t track;
    uint16_t vert_rate;
    double lat_msg;
    double lon_msg;
    uint32_t seen_time;
};

// Объявление функций
uint16_t calculate_crc(const uint8_t* data, size_t len);
bool try_fix_one_bit(uint8_t* data, size_t len, uint16_t expected_crc);
uint16_t get_expected_crc(const uint8_t* data, size_t len);
void decode_cpr(const uint8_t* data, double& lat, double& lon);
bool parse_raw_packet(const RawPacket& raw, ADSBPacket& outPkt);
void print_aircraft(const ADSBPacket& pkt);

#endif // DECODE_H

