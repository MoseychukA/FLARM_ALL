#include "transponder_packet.h"
#include <stdio.h>
#include <stdlib.h> // для перехода на русский язык
#include <cstdint>
#include <cstdio>   // for snprintf
#include <cstring>  // for strlen


#define BYTES_PER_WORD_32 4
#define BITS_PER_WORD_32  32
#define BYTES_PER_WORD_24 3
#define BITS_PER_WORD_24  24
#define BITS_PER_WORD_25  25
#define BITS_PER_BYTE     8
#define NIBBLES_PER_BYTE  2
#define BITS_PER_NIBBLE   4

#define MASK_MSBIT_WORD24 (0b1 << (BITS_PER_WORD_24 - 1))
#define MASK_MSBIT_WORD25 (0b1 << BITS_PER_WORD_24)
#define MASK_WORD24       0xFFFFFF

#define CHAR_TO_HEX(c)    ((c >= 'A') ? (c >= 'a') ? (c - 'a' + 10) : (c - 'A' + 10) : (c - '0'))

const uint32_t kExtendedSquitterLastWordIngestionMask = 0xFFFF0000;
const uint32_t kExtendedSquitterLastWordPopCount = 16;
const uint32_t kSquitterLastWordIngestionMask = 0xFFFFFF00;
const uint32_t kSquitterLastWordPopCount = 24;


/** Decoded1090Packet **/

Raw1090Packet::Raw1090Packet(uint32_t rx_buffer[kMaxPacketLenWords32], uint16_t rx_buffer_len_words32,
                             int16_t source_in, int16_t sigs_dbm_in, int16_t sigq_db_in,
                             uint64_t mlat_48mhz_64bit_counts_in)
                            : source(source_in),
                              sigs_dbm(sigs_dbm_in),
                              sigq_db(sigq_db_in),
                              mlat_48mhz_64bit_counts(mlat_48mhz_64bit_counts_in) 
{
    // // Устанавливаем поведение обработки последнего слова на основе длины пакета.
    uint32_t last_word_ingestion_mask, last_word_popcount;
    if (rx_buffer_len_words32 > 2) 
    {
        // 112-битный пакет (расширенный сквиттер)
        last_word_ingestion_mask = kExtendedSquitterLastWordIngestionMask;
        last_word_popcount = kExtendedSquitterLastWordPopCount;
    }
    else 
    {
        // 56-битный пакет (Сквиттер)
        last_word_ingestion_mask = kSquitterLastWordIngestionMask;
        last_word_popcount = kSquitterLastWordPopCount;
    }

    // Упаковываем буфер пакетов.
    for (uint16_t i = 0; i < rx_buffer_len_words32 && i < kMaxPacketLenWords32; i++) 
    {
        if (i == rx_buffer_len_words32 - 1) 
        {
            buffer[i] = rx_buffer[i] & last_word_ingestion_mask;  // Уберите всю ерунду из последнего слова.
            buffer_len_bits += last_word_popcount;
        }
        else 
        {
            buffer[i] = rx_buffer[i];
            buffer_len_bits += BITS_PER_WORD_32;
        }
    }


}

Raw1090Packet::Raw1090Packet(char *rx_string, int16_t source_in, int16_t sigs_dbm_in, int16_t sigq_db_in,
                             uint64_t mlat_48mhz_64bit_counts_in)
    : source(source_in),
      sigs_dbm(sigs_dbm_in),
      sigq_db(sigq_db_in),
      mlat_48mhz_64bit_counts(mlat_48mhz_64bit_counts_in) 
{
    uint16_t rx_num_bytes = strlen(rx_string) / NIBBLES_PER_BYTE;
    for (uint16_t i = 0; i < rx_num_bytes && i < kMaxPacketLenWords32 * BYTES_PER_WORD_32; i++) 
    {
        uint8_t byte = (CHAR_TO_HEX(rx_string[i * NIBBLES_PER_BYTE]) << BITS_PER_NIBBLE) |
                       CHAR_TO_HEX(rx_string[i * NIBBLES_PER_BYTE + 1]);
        uint16_t byte_offset = i % BYTES_PER_WORD_32;  // number of Bytes to shift right from MSB of current word
        if (byte_offset == 0) 
        {
            buffer[i / BYTES_PER_WORD_32] = byte << (3 * BITS_PER_BYTE);  // need to clear out the word
        }
        else 
        {
            buffer[i / BYTES_PER_WORD_32] |= byte << ((3 - byte_offset) * BITS_PER_BYTE);
        }
        buffer_len_bits += BITS_PER_BYTE;
    }
}

//uint16_t Raw1090Packet::PrintBuffer(char *buf, uint16_t buf_len_bytes) const {
//    uint16_t len = 0;
//    switch (buffer_len_bits) 
//    {
//        case kSquitterPacketLenBits:
//            len = snprintf(buf, buf_len_bytes, "%08lX%06lX", buffer[0], buffer[1] >> (2 * kBitsPerNibble));
//            break;
//        case kExtendedSquitterPacketLenBits:
//            len = snprintf(buf, buf_len_bytes, "%08lX%08lX%08lX%04lX", buffer[0], buffer[1], buffer[2],
//                           buffer[3] >> (4 * kBitsPerNibble));
//            break;
//            // Ничего не печатать, если буфер имеет недопустимую длину.
//    }
//    return len;
//}

