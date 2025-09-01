#ifndef _ADSB_PACKET_HH_
#define _ADSB_PACKET_HH_

#include <cstdint>

//kMaxPacketLenWords32


class Raw1090Packet {
   public:
    static const uint16_t kMaxPacketLenWords32 = 4;
    static const uint16_t kSquitterPacketLenBits = 56;
    static const uint16_t kSquitterPacketNumWords32 = 2;  // 56 bits = 1.75 words, round up to 2.
    static const uint16_t kExtendedSquitterPacketLenBits = 112;
    static const uint16_t kExtendedSquitterPacketNumWords32 = 4;  // 112 bits = 3.5 words, round up to 4.

    Raw1090Packet(char *rx_string, int16_t source_in = -1, int16_t sigs_dbm_in = INT16_MIN,
                  int16_t sigq_db_in = INT16_MIN, uint64_t mlat_48mhz_64bit_counts = 0);

    Raw1090Packet(uint32_t rx_buffer[kMaxPacketLenWords32], uint16_t rx_buffer_len_words32, int16_t source_in = -1,
                  int16_t sigs_dbm_in = INT16_MIN, int16_t sigq_db_in = INT16_MIN,
                  uint64_t mlat_48mhz_64bit_counts = 0);
    /**
     * Default constructor.
     */
    Raw1090Packet() 
    {
        for (uint16_t i = 0; i < kMaxPacketLenWords32; i++) 
        {
            buffer[i] = 0;
        }
    }

    /**
    * Вспомогательная функция, возвращающая временную метку в секундах. Используется для абстрагирования разрешения временной метки MLAT для
    * функций, которым это не нужно.
    * @return Временная метка в секундах.
    */
    uint64_t GetTimestampMs() { return mlat_48mhz_64bit_counts / 48'000; }

    /**
    * Вывести содержимое буфера в строку.
    * @param[in] buf Буфер для печати.
    * @param[in] buf_len_bytes Длина буфера в символах.
    * @return Количество символов, записанных в буфер.
    */
   // uint16_t PrintBuffer(char *buf, uint16_t buf_len_bytes) const;

    uint32_t buffer[kMaxPacketLenWords32] = {0};
    uint16_t buffer_len_bits = 0;
    int8_t source = -1;                    // Источник пакета ADS-B (номер конечного автомата PIO).
    int16_t sigs_dbm = INT16_MIN;          // Уровень сигнала, в дБм.
    int16_t sigq_db = INT16_MIN;           // Качество сигнала (дБ выше уровня шума), в дБ.
    uint64_t mlat_48mhz_64bit_counts = 0;  // Счетчик MLAT высокого разрешения.
};

#endif /* _ADSB_PACKET_HH_ */