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
    *   -  ()    ,    .
    * @param[in] reference_position    .  "valid" .
    * @param[in] message    .
    * @param[out] decoded_position  .
    * @return True,     (recovered_position ), false   .
    */
    static bool DecodeAirborneLocalCPR(const DecodedPosition &reference_position, const CPRMessage &message,
                                       DecodedPosition &decoded_position);

    /**
   *     ,    .     
   *   ,   received_timestamp_ms.
   * @param[in] even_message    .
   * @param[in] odd_message    .
   * @param[out] decoded_position  .
   * @return True,     (recovered_position ), false   .
   */
    static bool DecodeAirborneGlobalCPR(const CPRMessage &even_message, const CPRMessage &odd_message,
                                        DecodedPosition &decoded_position);
};
