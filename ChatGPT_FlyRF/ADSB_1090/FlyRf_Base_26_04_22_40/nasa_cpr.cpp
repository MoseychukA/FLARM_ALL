#include "nasa_cpr.h"

extern "C" {
#include "cpr.h"
}
#include "awb_utils.h"

bool NASACPRDecoder::DecodeAirborneLocalCPR(const DecodedPosition &reference_position,
                                            const CPRMessage &message,
                                            DecodedPosition &decoded_position) {
    struct message cpr_msg;
    cpr_msg.format = message.odd ? 1 : 0;
    cpr_msg.yz = message.lat_cpr;
    cpr_msg.xz = message.lon_cpr;

    struct recovered_position result = local_dec(cpr_msg.format,
                                                 reference_position.lat_awb,
                                                 reference_position.lon_awb,
                                                 cpr_msg);
    if (!result.valid) {
        return false;
    }

    decoded_position.lat_awb = result.lat_awb;
    decoded_position.lon_awb = result.lon_awb;
    decoded_position.lat_deg = awb2lat(result.lat_awb);
    decoded_position.lon_deg = awb2lon(result.lon_awb);
    return true;
}

bool NASACPRDecoder::DecodeAirborneGlobalCPR(const CPRMessage &even_message,
                                             const CPRMessage &odd_message,
                                             DecodedPosition &decoded_position) {
    struct message msg0;
    msg0.format = 0;
    msg0.yz = even_message.lat_cpr;
    msg0.xz = even_message.lon_cpr;

    struct message msg1;
    msg1.format = 1;
    msg1.yz = odd_message.lat_cpr;
    msg1.xz = odd_message.lon_cpr;

    int most_recent = (odd_message.received_timestamp_ms > even_message.received_timestamp_ms) ? 1 : 0;
    struct recovered_position result = global_dec(most_recent, msg0, msg1);
    if (!result.valid) {
        return false;
    }

    decoded_position.lat_awb = result.lat_awb;
    decoded_position.lon_awb = result.lon_awb;
    decoded_position.lat_deg = awb2lat(result.lat_awb);
    decoded_position.lon_deg = awb2lon(result.lon_awb);
    return true;
}
