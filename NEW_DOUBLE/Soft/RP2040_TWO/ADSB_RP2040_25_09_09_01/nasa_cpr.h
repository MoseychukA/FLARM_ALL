#pragma once
#include "stdint.h"

class NASACPRDecoder 
{
   public:
    struct DecodedPosition 
    {
        float lat_deg;
        float lon_deg;
        // Also provide position result in Alternative Weighted Binary format for use in additional fixed point
        // operations.
        uint32_t lat_awb;
        uint32_t lon_awb;
    };

    struct CPRMessage 
    {
        bool odd;
        uint32_t lat_cpr;
        uint32_t lon_cpr;
        uint32_t received_timestamp_ms = 0;
    };
    /**
    * Декодирует местоположение сердечно-лёгочной реанимации (СЛР) для позиции в воздухе, используя существующую опорную позицию.
    * @param[in] reference_position Опорная позиция для декодирования. Свойство "valid" игнорируется.
    * @param[in] message Сообщение СЛР для декодирования.
    * @param[out] decoded_position Декодированное местоположение.
    * @return True, если декодирование прошло успешно (recovered_position действительно), false в противном случае.
    */
    static bool DecodeAirborneLocalCPR(const DecodedPosition &reference_position, const CPRMessage &message,
                                       DecodedPosition &decoded_position);

    /**
   * Декодирует местоположение СЛР в воздухе, используя два сообщения СЛР. Декодированное местоположение возвращается на основе
   * самого последнего сообщения, отсортированного по received_timestamp_ms.
   * @param[in] even_message Последнее чётное сообщение СЛР.
   * @param[in] odd_message Последнее нечётное сообщение СЛР.
   * @param[out] decoded_position Декодированное местоположение.
   * @return True, если декодирование прошло успешно (recovered_position допустимо), false в противном случае.
   */
    static bool DecodeAirborneGlobalCPR(const CPRMessage &even_message, const CPRMessage &odd_message,
                                        DecodedPosition &decoded_position);
};
