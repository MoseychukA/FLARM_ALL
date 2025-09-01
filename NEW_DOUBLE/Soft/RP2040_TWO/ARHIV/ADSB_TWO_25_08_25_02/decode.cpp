#include "decode.h"
#include "transponder_packet.h"

uint16_t calculate_crc(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

bool try_fix_one_bit(uint8_t* data, size_t len, uint16_t expected_crc) {
    for (size_t byte_idx = 0; byte_idx < len; byte_idx++) {
        for (int bit = 0; bit < 8; bit++) {
            data[byte_idx] ^= (1 << bit);
            if (calculate_crc(data, len) == expected_crc) {
                return true;
            }
            data[byte_idx] ^= (1 << bit);
        }
    }
    return false;
}

uint16_t get_expected_crc(const uint8_t* data, size_t len) {
    if (len >= 2) {
        return (data[len - 2] << 8) | data[len - 1];
    }
    return 0xFFFF;
}

//bool parse_raw_packet(const RawPacket& raw, ADSBPacket& outPkt) 
//{
//    // Пример: извлечь ICAO из первых 6 байт
//    outPkt.icao = 0;
//    if (raw.buffer_len_bits >= 48) {
//        for (int i = 0; i < 6; i++) {
//            uint8_t byte = (raw.buffer[i / 4] >> ((i % 4) * 8)) & 0xFF;
//            outPkt.icao = (outPkt.icao << 8) | byte;
//        }
//    }
//    // Заглушки для других полей
//    strcpy(outPkt.flight, "AB1234");
//    outPkt.squawk = 7500;
//    outPkt.altitude = 12000;
//    outPkt.speed = 600;
//    outPkt.track = 90;
//    outPkt.vert_rate = 0;
//    outPkt.lat_msg = 55.7558;
//    outPkt.lon_msg = 37.6173;
//    outPkt.seen_time = millis();
//    return true;
//}

void print_aircraft(const ADSBPacket& pkt) {
    Serial.printf("ICAO: %06lX; Flight: %s; Lat: %.6f; Lon: %.6f; Alt: %d m; Speed: %d km/h\r\n ",
        pkt.icao, pkt.flight, pkt.lat_msg, pkt.lon_msg, pkt.altitude, pkt.speed);
}


void decode_cpr(const uint8_t* data, double& lat, double& lon) 
{
    // Заглушка
    lat = 55.7558; // Москва
    lon = 37.6173;
}
