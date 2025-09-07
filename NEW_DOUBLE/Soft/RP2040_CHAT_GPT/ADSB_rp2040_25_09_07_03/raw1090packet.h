#pragma once
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

class Raw1090Packet {
   public:
    static const uint16_t kMaxPacketLenWords32 = 4;
    static const uint16_t kSquitterPacketLenBits = 56;
    static const uint16_t kSquitterPacketNumWords32 = 2;
    static const uint16_t kExtendedSquitterPacketLenBits = 112;
    static const uint16_t kExtendedSquitterPacketNumWords32 = 4;

    Raw1090Packet(char *rx_string, int16_t source_in = -1, int16_t sigs_dbm_in = INT16_MIN,
                  int16_t sigq_db_in = INT16_MIN, uint64_t mlat_48mhz_64bit_counts = 0) {
        memset(buffer, 0, sizeof(buffer));
        buffer_len_bits = 0;
        source = source_in;
        sigs_dbm = sigs_dbm_in;
        sigq_db = sigq_db_in;
        mlat_48mhz_64bit_counts = mlat_48mhz_64bit_counts;
        (void)rx_string;  // пока не используем строковый парсер
    }

    Raw1090Packet(uint32_t rx_buffer[kMaxPacketLenWords32], uint16_t rx_buffer_len_words32,
                  int16_t source_in = -1, int16_t sigs_dbm_in = INT16_MIN,
                  int16_t sigq_db_in = INT16_MIN, uint64_t mlat_48mhz_64bit_counts = 0) {
        for (uint16_t i = 0; i < rx_buffer_len_words32 && i < kMaxPacketLenWords32; i++) {
            buffer[i] = rx_buffer[i];
        }
        buffer_len_bits = rx_buffer_len_words32 * 32;
        source = source_in;
        sigs_dbm = sigs_dbm_in;
        sigq_db = sigq_db_in;
        mlat_48mhz_64bit_counts = mlat_48mhz_64bit_counts;
    }

    Raw1090Packet() {
        for (uint16_t i = 0; i < kMaxPacketLenWords32; i++) {
            buffer[i] = 0;
        }
    }

    uint64_t GetTimestampMs() { return mlat_48mhz_64bit_counts / 48000; }

    uint16_t PrintBuffer(char *buf, uint16_t buf_len_bytes) const {
        uint16_t written = 0;
        for (uint16_t i = 0; i < buffer_len_bits / 32 && written + 9 < buf_len_bytes; i++) {
            written += snprintf(buf + written, buf_len_bytes - written, "%08X", buffer[i]);
        }
        buf[written] = '\0';
        return written;
    }

    uint32_t buffer[kMaxPacketLenWords32] = {0};
    uint16_t buffer_len_bits = 0;
    int8_t source = -1;
    int16_t sigs_dbm = INT16_MIN;
    int16_t sigq_db = INT16_MIN;
    uint64_t mlat_48mhz_64bit_counts = 0;
};
