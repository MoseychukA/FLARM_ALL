#pragma once
#include <stdint.h>
#include <string.h>
#include <limits.h>

// Функция преобразования буфера слов в байтовый буфер
void WordBufferToByteBuffer(const uint32_t* word_buffer, uint8_t* byte_buffer, uint16_t num_bytes);

// Функция преобразования байтового буфера в буфер слов
void ByteBufferToWordBuffer(const uint8_t* byte_buffer, uint32_t* word_buffer, uint16_t num_bytes);

class Raw1090Packet {
public:
    static const uint16_t kMaxPacketLenWords32 = 4;
    static const uint16_t kSquitterPacketLenBits = 56;
    static const uint16_t kSquitterPacketNumWords32 = 2;
    static const uint16_t kExtendedSquitterPacketLenBits = 112;
    static const uint16_t kExtendedSquitterPacketNumWords32 = 4;

    Raw1090Packet(char* rx_string, int16_t source_in = -1, int16_t sigs_dbm_in = INT16_MIN,
                  int16_t sigq_db_in = INT16_MIN, uint64_t mlat_48mhz_64bit_counts = 0);

    Raw1090Packet(uint32_t rx_buffer[kMaxPacketLenWords32], uint16_t rx_buffer_len_words32,
                  int16_t source_in = -1, int16_t sigs_dbm_in = INT16_MIN, int16_t sigq_db_in = INT16_MIN,
                  uint64_t mlat_48mhz_64bit_counts = 0);

    Raw1090Packet() {
        for (uint16_t i = 0; i < kMaxPacketLenWords32; i++) buffer[i] = 0;
    }

    uint64_t GetTimestampMs() { return mlat_48mhz_64bit_counts / 48000; }

    uint16_t PrintBuffer(char* buf, uint16_t buf_len_bytes) const;

    uint32_t buffer[kMaxPacketLenWords32] = {0};
    uint16_t buffer_len_bits = 0;
    int8_t   source = -1;
    int16_t  sigs_dbm = INT16_MIN;
    int16_t  sigq_db = INT16_MIN;
    uint64_t mlat_48mhz_64bit_counts = 0;
};
