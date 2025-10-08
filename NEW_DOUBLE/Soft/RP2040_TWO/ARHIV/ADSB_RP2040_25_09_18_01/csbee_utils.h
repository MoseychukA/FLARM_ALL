#ifndef CSBEE_UTILS_H_
#define CSBEE_UTILS_H_

#include "aircraft_dictionary.h"
#include "macros.h"
#include "stdio.h"

const uint16_t kCSBeeMessageStrMaxLen = 200;
const uint16_t kCRCMaxNumChars = 4;  // 16 bits = 4 hex characters.
const uint16_t kEOLNumChars = 2;


// Структура
struct __attribute__((packed)) ToDUMP1090 {
    uint32_t  addr;
    char      squawk[5];
    char      flight[16];
    int32_t   altitude;
    int32_t   speed;
    int32_t   track;
    int32_t   vert_rate;
    float     lat_msg;
    float     lon_msg;
    int32_t   seen_time;
    char      endOfPacket[3];
};

// Глобально объявляем переменную
ToDUMP1090 packet;

uint32_t toBigEndian32(uint32_t val) {
    return ((val & 0xFF) << 24) |
        ((val & 0xFF00) << 8) |
        ((val & 0xFF0000) >> 8) |
        ((val & 0xFF000000) >> 24);
}

float floatToBigEndian(float val) {
    uint32_t temp;
    memcpy(&temp, &val, 4);
    temp = toBigEndian32(temp);
    float res;
    memcpy(&res, &temp, 4);
    return res;
}

void sendToDUMP1090_UART1(const ToDUMP1090& src) {
    ToDUMP1090 out;

    // Преобразование BigEndian как раньше
    out.addr = toBigEndian32(src.addr);
    memcpy(out.squawk, src.squawk, 5);
    memcpy(out.flight, src.flight, 16);
    out.altitude = toBigEndian32(src.altitude);
    out.speed = toBigEndian32(src.speed);
    out.track = toBigEndian32(src.track);
    out.vert_rate = toBigEndian32(src.vert_rate);
    out.lat_msg = floatToBigEndian(src.lat_msg);
    out.lon_msg = floatToBigEndian(src.lon_msg);
    out.seen_time = toBigEndian32(src.seen_time);
    out.endOfPacket[0] = 0xFF;
    out.endOfPacket[1] = 0xFF;
    out.endOfPacket[2] = 0xFF;

    // Отправка структуры как бинарного массива через UART1
    uart_write_blocking(uart1, reinterpret_cast<const uint8_t*>(&out), sizeof(ToDUMP1090));
}

/**
* Выводит объект Aircraft в строковый буфер в формате CSBee. Длина строкового буфера должна быть
* kCSBeeMessageStrMaxLen.
* @param[out] message_buf Массив символов для записи.
* @param[in] aircraft Объект Aircraft, содержимое которого выводится.
* @retval Количество символов, записанных в строковый буфер, или отрицательное значение в случае ошибки.
*/
inline int16_t WriteCSBeeAircraftMessageStr(char message_buf[], const Aircraft1090 &aircraft) 
{
    // #A:ICAO,FLAGS,CALL,SQ,LAT,LON,ALT_BARO,TRACK,VELH,VELV,SIGS,SIGQ,FPS,NICNAC,ALT_GEO,ECAT,CRC\r\n

    // Создать битовое поле SYSINFO.
    // Преобразовать длину и ширину самолёта в максимальное значение.
    uint32_t sysinfo = MAX1(aircraft.length_m, aircraft.width_m) << 22;  // MDIM bitfield.
    // Convert GNSS antenna offset value to CSBee formatted bitfield.
    if (aircraft.gnss_antenna_offset_right_of_roll_axis_m != INT8_MAX) 
    {
        sysinfo |= (((aircraft.gnss_antenna_offset_right_of_roll_axis_m > 0) & 0b1) << 21);         // GAOR bitfield.
        sysinfo |= (((ABS(aircraft.gnss_antenna_offset_right_of_roll_axis_m) >> 1) & 0b11) << 19);  // GAOD bitfield.
        sysinfo |= (0b1 << 18);                                                                     // GAOK bitfield.
    }
    sysinfo |= ((aircraft.system_design_assurance & 0b11) << 16);                 // SDA bitfield.
    sysinfo |= ((aircraft.source_integrity_level & 0b11) << 14);                  // SIL bitfield.
    sysinfo |= ((aircraft.geometric_vertical_accuracy & 0b11) << 12);             // GVA bitfield
    sysinfo |= ((aircraft.navigation_accuracy_category_position & 0b1111) << 8);  // NAC_p bitfield.
    sysinfo |= ((aircraft.navigation_accuracy_category_velocity & 0b111) << 5);   // NAC_v bitfield.
    sysinfo |= ((aircraft.navigation_integrity_category_baro & 0b1) << 4);        // NIC_baro bitfield.
    sysinfo |= ((aircraft.navigation_integrity_category & 0b1111));               // NIC bitfield.

      char squawk_buf[5];
      int16_t squawk_chars =  // Вывести все, кроме CRC, в строковый буфер.
        snprintf(squawk_buf, kCSBeeMessageStrMaxLen - kCRCMaxNumChars - 1,
              "%04o", aircraft.squawk                                   // SQUAWK, e.g. 7232
                   );


    packet.addr = aircraft.icao_address;                                   // ICAO address
    memcpy(packet.squawk, squawk_buf, sizeof(squawk_buf));      // Flight number SQUAWK
    memcpy(packet.flight, aircraft.callsign, sizeof(aircraft.callsign)); // номер рейса
    packet.altitude = aircraft.baro_altitude_ft;                              // Altitude метры
    packet.speed = aircraft.velocity_kts;                                    // Скорость км/час
    packet.track = aircraft.direction_deg;                                 // курс в градусах
    packet.vert_rate = aircraft.vertical_rate_fpm;                         // скорость подъема/снижения
    packet.lat_msg = (float)aircraft.latitude_deg;
    packet.lon_msg = (float)aircraft.longitude_deg;
    packet.seen_time = (int)(aircraft.last_track_update_timestamp_ms / 1000);                  // Время получения последнего пакета
 
    sendToDUMP1090_UART1(packet);
    memset(&packet, 0, sizeof(packet)); // Очистить массив

    return 0;// num_chars;
}


inline int16_t WriteCSBeeStatisticsMessageStr(char message_buf[], const Aircraft1090& aircraft)
{
    // #A:ICAO,FLAGS,CALL,SQ,LAT,LON,ALT_BARO,TRACK,VELH,VELV,SIGS,SIGQ,FPS,NICNAC,ALT_GEO,ECAT,CRC\r\n

    // Создать битовое поле SYSINFO.
    // Преобразовать длину и ширину самолёта в максимальное значение.
    uint32_t sysinfo = MAX1(aircraft.length_m, aircraft.width_m) << 22;  // MDIM bitfield.
    // Convert GNSS antenna offset value to CSBee formatted bitfield.
    if (aircraft.gnss_antenna_offset_right_of_roll_axis_m != INT8_MAX)
    {
        sysinfo |= (((aircraft.gnss_antenna_offset_right_of_roll_axis_m > 0) & 0b1) << 21);         // GAOR bitfield.
        sysinfo |= (((ABS(aircraft.gnss_antenna_offset_right_of_roll_axis_m) >> 1) & 0b11) << 19);  // GAOD bitfield.
        sysinfo |= (0b1 << 18);                                                                     // GAOK bitfield.
    }
    sysinfo |= ((aircraft.system_design_assurance & 0b11) << 16);                 // SDA bitfield.
    sysinfo |= ((aircraft.source_integrity_level & 0b11) << 14);                  // SIL bitfield.
    sysinfo |= ((aircraft.geometric_vertical_accuracy & 0b11) << 12);             // GVA bitfield
    sysinfo |= ((aircraft.navigation_accuracy_category_position & 0b1111) << 8);  // NAC_p bitfield.
    sysinfo |= ((aircraft.navigation_accuracy_category_velocity & 0b111) << 5);   // NAC_v bitfield.
    sysinfo |= ((aircraft.navigation_integrity_category_baro & 0b1) << 4);        // NIC_baro bitfield.
    sysinfo |= ((aircraft.navigation_integrity_category & 0b1111));               // NIC bitfield.

    int16_t num_chars =  // Вывести все, кроме CRC, в строковый буфер.
        snprintf(message_buf, kCSBeeMessageStrMaxLen - kCRCMaxNumChars - 1,
            "%06X,"                                           // ICAO, e.g. 3C65AC
            "%04o,"                                           // SQUAWK, e.g. 7232
            "%s,"                                             // CALL, e.g. N61ZP
            "%d,"                                             // ALT_BARO, e.g. 5000
            "%.0f,"                                           // VELH, e.g. 464
            "%.0f,"                                           // TRACK, e.g. 35
            "%d,"
            "%.5f,"                                           // LAT, e.g. 57.57634
            "%.5f,"                                           // LON, e.g. 17.59554
            "%d\r\n",        
            aircraft.icao_address,                            // ICAO
            aircraft.squawk,                                  // SQUAWK
            aircraft.callsign,                                // CALL
            aircraft.baro_altitude_ft,                        // ALT_BARO
            aircraft.velocity_kts,                            // VELH
            aircraft.direction_deg,                           // TRACK
            aircraft.vertical_rate_fpm,                        // VELV
            aircraft.latitude_deg,                            // LAT
            aircraft.longitude_deg,                           // LON
            (int)aircraft.last_track_update_timestamp_ms / 1000
         );



    if (num_chars < 0) return num_chars;  // Проверяем, был ли остановлен вызов snprintf.

    return num_chars/* + crc_num_chars*/;
}



#endif /* CSBEE_UTILS_HH_ */