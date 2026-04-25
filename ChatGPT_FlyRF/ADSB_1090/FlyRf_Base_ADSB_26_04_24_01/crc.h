#ifndef CRC_HH_
#define CRC_HH_

#include <cstdint>
/**
*  CRC24 .  ,  CRC    ,  buffer_len_bytes 
*    ,   CRC.
* @param[in] buffer      CRC.
* @param[in] buffer_len_bytes     CRC (  ).
* @param[in] initial_value   CRC.
* @retval 24- CRC.
*/
uint32_t crc24(const uint8_t *buffer, uint16_t buffer_len_bytes, uint32_t initial_value = 0x0);

/**
*   CRC24 .  0,   CRC   CRC,  
*  3  .   ,        .
* @param[in] buffer      CRC.
* @param[in] buffer_len_bytes     CRC  CRC   (  + CRC).
* @param[in] initial_value   CRC.
* @retval  CRC24.
*/
uint32_t crc24_syndrome(const uint8_t *buffer, uint16_t buffer_len_bytes, uint32_t initial_value = 0x0);

/**
*       CRC24.
* @param[in]  CRC24 .
* @param[in] message_len_bits    .
* @retval     -1,    .  -2,    .
*/
int16_t crc24_find_single_bit_error(uint32_t syndrome, uint16_t message_len_bits);

/**
*          .
*/
void flip_bit(uint8_t *message, uint16_t index);

/**
*            .
*/void flip_bit(uint32_t *message, uint16_t index);

/**
*  CRC32 .    crc_tables.hh,    IEEE 802.3
*  (  zlib  ).  ,    CRC    0xFFFFFFFF 
*   IEEE 802.3,        XOR  0xFFFFFFFF.
* @param[in] buffer      CRC.
* @param[in] buffer_len_bytes     CRC.
* @param[in] initial_value   CRC.
* @retval 32- CRC.
*/
uint32_t crc32_ieee_802_3(const uint8_t *buffer, uint32_t buffer_len_bytes, uint32_t initial_value = 0xFFFFFFFF);

#endif /* CRC_HH_ */