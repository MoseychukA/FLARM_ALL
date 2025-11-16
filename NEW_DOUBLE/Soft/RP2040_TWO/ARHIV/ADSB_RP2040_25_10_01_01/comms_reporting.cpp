#include "adsbee.h"
#include "beast_utils.h"
#include "comms.h"
#include "csbee_utils.h"
#include "hal.h"  // For timestamping.
#include "raw_utils.h"
#include "unit_conversions.h"


//// Структура
//struct __attribute__((packed)) ToDUMP1090 {
//    uint32_t  addr;
//    char      squawk[5];
//    char      flight[16];
//    int32_t   altitude;
//    int32_t   speed;
//    int32_t   track;
//    int32_t   vert_rate;
//    float     lat_msg;
//    float     lon_msg;
//    int32_t   seen_time;
//    char      endOfPacket[3];
//    char      endRN[2];
//};

//// Глобально объявляем переменную
//ToDUMP1090 packet;
//
//uint32_t toBigEndian32(uint32_t val) {
//    return ((val & 0xFF) << 24) |
//        ((val & 0xFF00) << 8) |
//        ((val & 0xFF0000) >> 8) |
//        ((val & 0xFF000000) >> 24);
//}
//
//float floatToBigEndian(float val) {
//    uint32_t temp;
//    memcpy(&temp, &val, 4);
//    temp = toBigEndian32(temp);
//    float res;
//    memcpy(&res, &temp, 4);
//    return res;
//}

//void sendToDUMP1090_UART1(const ToDUMP1090& src)
//{
//    ToDUMP1090 out;
//
//    // Преобразование BigEndian как раньше
//    out.addr = toBigEndian32(src.addr);
//    memcpy(out.squawk, src.squawk, 5);
//    memcpy(out.flight, src.flight, 16);
//    out.altitude = toBigEndian32(src.altitude);
//    out.speed = toBigEndian32(src.speed);
//    out.track = toBigEndian32(src.track);
//    out.vert_rate = toBigEndian32(src.vert_rate);
//    out.lat_msg = floatToBigEndian(src.lat_msg);
//    out.lon_msg = floatToBigEndian(src.lon_msg);
//    out.seen_time = toBigEndian32(src.seen_time);
//    out.endOfPacket[0] = 0xFF;
//    out.endOfPacket[1] = 0xFF;
//    out.endOfPacket[2] = 0xFF;
//    // memcpy(out.endRN, "\r\n", 2);
//
//     // Отправка структуры как бинарного массива через UART1
//    SendBuf(iface, (char*)beast_frame_buf, num_bytes_in_frame);
//   // SendBuf(iface, out, message_len_bytes);
//    // uart_write_blocking(uart1, reinterpret_cast<const uint8_t*>(&out), sizeof(ToDUMP1090));
//}






extern ADSBee adsbee;

bool CommsManager::InitReporting() { return true; }

bool CommsManager::UpdateReporting() 
{
    bool ret = true;
    uint32_t timestamp_ms = millis();

    if (timestamp_ms - last_raw_report_timestamp_ms_ <= kRawReportingIntervalMs) 
    {
        return true;  // Nothing to update.
    }
    // Продолжить обновление и записать временную метку.
    last_raw_report_timestamp_ms_ = timestamp_ms;

    if (timestamp_ms - last_csbee_report_timestamp_ms_ >= kCSBeeReportingIntervalMs) // 1000U
    //if (message_ESP)
    {
        ret = ReportCSBee(SettingsManager::SerialInterface::kCommsUART);
        last_csbee_report_timestamp_ms_ = timestamp_ms;
        message_ESP = false;
    }




    return ret;
}

bool CommsManager::ReportRaw(SettingsManager::SerialInterface iface, const Decoded1090Packet packets_to_report_1090[],
                             uint16_t num_packets_to_report) 
{
    for (uint16_t i = 0; i < num_packets_to_report; i++) 
    {
        char raw_frame_buf[kRaw1090FrameMaxNumChars];
        uint16_t num_bytes_in_frame = BuildRaw1090Frame(packets_to_report_1090[i].GetRaw(), raw_frame_buf);
        SendBuf(iface, (char *)raw_frame_buf, num_bytes_in_frame);
        comms_manager.iface_puts(iface, (char *)"\r\n");  // Send delimiter.
    }
    return true;
}

bool CommsManager::ReportBeast(SettingsManager::SerialInterface iface, const Decoded1090Packet packets_to_report_1090[], uint16_t num_packets_to_report) 
{
    for (uint16_t i = 0; i < num_packets_to_report; i++) 
    {
        uint8_t beast_frame_buf[kBeastFrameMaxLenBytes];
        uint16_t num_bytes_in_frame = Build1090BeastFrame(packets_to_report_1090[i], beast_frame_buf);
        comms_manager.iface_putc(iface, char(0x1a));  // Send beast escape char to denote beginning of frame.
        SendBuf(iface, (char *)beast_frame_buf, num_bytes_in_frame);
    }
    return true;
}

//void CommsManager::sendToDUMP1090_UART1(const ToDUMP1090& src)
//{
//    ToDUMP1090 out;
//
//    // Преобразование BigEndian как раньше
//    out.addr = toBigEndian32(src.addr);
//    memcpy(out.squawk, src.squawk, 5);
//    memcpy(out.flight, src.flight, 16);
//    out.altitude = toBigEndian32(src.altitude);
//    out.speed = toBigEndian32(src.speed);
//    out.track = toBigEndian32(src.track);
//    out.vert_rate = toBigEndian32(src.vert_rate);
//    out.lat_msg = floatToBigEndian(src.lat_msg);
//    out.lon_msg = floatToBigEndian(src.lon_msg);
//    out.seen_time = toBigEndian32(src.seen_time);
//    out.endOfPacket[0] = 0xFF;
//    out.endOfPacket[1] = 0xFF;
//    out.endOfPacket[2] = 0xFF;
//    // memcpy(out.endRN, "\r\n", 2);
//
//     // Отправка структуры как бинарного массива через UART1
//   // SendBuf(iface, reinterpret_cast<const uint8_t*>(&out), sizeof(ToDUMP1090));
//    // SendBuf(iface, out, message_len_bytes);
//      //uart_write_blocking(uart1, reinterpret_cast<const uint8_t*>(&out), sizeof(ToDUMP1090));
//
//    for (int i = 0; i < sizeof(ToDUMP1090); i++)
//    {
//
//        //Serial2.write((ToDUMP1090)out);
//       
//    }
//      
//}
//
//

//void CommsManager::sendToDUMP1090(Stream& s, const ToDUMP1090& src)
//{
//    ToDUMP1090 out;
//
//    // Endian преобразование
//    out.addr = toBigEndian32(src.addr);
//    memcpy(out.squawk, src.squawk, 5);
//    memcpy(out.flight, src.flight, 16);
//    out.altitude = toBigEndian32(src.altitude);
//    out.speed = toBigEndian32(src.speed);
//    out.track = toBigEndian32(src.track);
//    out.vert_rate = toBigEndian32(src.vert_rate);
//    out.lat_msg = floatToBigEndian(src.lat_msg);
//    out.lon_msg = floatToBigEndian(src.lon_msg);
//    out.seen_time = toBigEndian32(src.seen_time);
//    out.endOfPacket[0] = 0xFF;
//    out.endOfPacket[1] = 0xFF;
//    out.endOfPacket[2] = 0xFF;
//
//    s.write((uint8_t*)&out, sizeof(ToDUMP1090));
//}



bool CommsManager::ReportCSBee(SettingsManager::SerialInterface iface)
{
    // Write out a CSBee Aircraft message for each aircraft in the aircraft dictionary.
    for (auto &itr : adsbee.aircraft_dictionary.dict) 
    {
        const Aircraft1090 &aircraft = itr.second;

        char message[kCSBeeMessageStrMaxLen];
        int16_t message_len_bytes = WriteCSBeeAircraftMessageStr(message, aircraft);
        if (message_len_bytes < 0) 
        {
            return false;
        }

       // char squawk_buf[5];
       // int16_t squawk_chars =  // Вывести все, кроме CRC, в строковый буфер.
       //     snprintf(squawk_buf, kCSBeeMessageStrMaxLen - kCRCMaxNumChars - 1,
       //         "%04o", aircraft.squawk                                   // SQUAWK, e.g. 7232
       //     );


       // packet.addr = aircraft.icao_address;                                   // ICAO address
       // memcpy(packet.squawk, squawk_buf, sizeof(squawk_buf));      // Flight number SQUAWK
       // memcpy(packet.flight, aircraft.callsign, sizeof(aircraft.callsign)); // номер рейса
       // packet.altitude = aircraft.baro_altitude_ft;                              // Altitude метры
       // packet.speed = aircraft.velocity_kts;                                    // Скорость км/час
       // packet.track = aircraft.direction_deg;                                 // курс в градусах
       // packet.vert_rate = aircraft.vertical_rate_fpm;                         // скорость подъема/снижения
       // packet.lat_msg = (float)aircraft.latitude_deg;
       // packet.lon_msg = (float)aircraft.longitude_deg;
       // packet.seen_time = (int)(aircraft.last_track_update_timestamp_ms / 1000);                  // Время получения последнего пакета

       // sendToDUMP1090(Serial2,packet);


       //// sendToDUMP1090_UART1(packet);
       // //Serial.printf("sizeof(packet) %d", sizeof(packet));
       // memset(&packet, 0, sizeof(packet)); // Очистить массив


       // SendBuf(iface, message, message_len_bytes);
    }


    // Записать сообщение CSBee Aircraft для каждого самолета в словаре самолетов.
    for (auto& itr : adsbee.aircraft_dictionary.dict)
    {
        const Aircraft1090& aircraft = itr.second;

        char message[kCSBeeMessageStrMaxLen];
        int16_t message_len_bytes = WriteCSBeeStatisticsMessageStr(message, aircraft);
        if (message_len_bytes < 0)
        {
            return false;
        }

       SendBufSerial(iface, message, message_len_bytes);

    }

    return true;
}
