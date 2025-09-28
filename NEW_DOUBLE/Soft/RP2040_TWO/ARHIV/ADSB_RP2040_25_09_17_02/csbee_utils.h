#ifndef CSBEE_UTILS_H_
#define CSBEE_UTILS_H_

#include "aircraft_dictionary.h"
#include "macros.h"
#include "stdio.h"

const uint16_t kCSBeeMessageStrMaxLen = 200;
const uint16_t kCRCMaxNumChars = 4;  // 16 bits = 4 hex characters.
const uint16_t kEOLNumChars = 2;

#pragma pack(push,1)
struct ToDUMP1090
{
    uint32_t  addr;           // ICAO address
    char      squawk[5];      // Squawk
    char      flight[16];     // Flight number	
    int       altitude;       // Altitude
    int       speed;          // Velocity
    int       track;          // Angle of flight
    int       vert_rate;      // Vertical rate.
    float     lat_msg;
    float     lon_msg;      // Coordinated obtained from CPR encoded data
    int       seen_time;      // Time at which the last packet was received
    char      endOfPacket[3]; // 0xFF 0xFF 0xFF
};
#pragma pack(pop)



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

    //int16_t num_chars =  // Вывести все, кроме CRC, в строковый буфер.
    //    snprintf(message_buf, kCSBeeMessageStrMaxLen - kCRCMaxNumChars - 1,
    //        "%06X"                                           // ICAO, e.g. 3C65AC
    //        "%04o"                                           // SQUAWK, e.g. 7232
    //        "%s"                                             // CALL, e.g. N61ZP
    //        "%d"                                             // ALT_BARO, e.g. 5000
    //        "%.0f"                                           // VELH, e.g. 464
    //        "%.0f"                                           // TRACK, e.g. 35
    //        "%d"
    //        "%.5f"                                           // LAT, e.g. 57.57634
    //        "%.5f"                                           // LON, e.g. 17.59554
    //        "%d"
    //        "%X"
    //        "%X"
    //        "%X\r\n"
    //        ,
    //        aircraft.icao_address,                            // ICAO
    //        aircraft.squawk,                                  // SQUAWK
    //        aircraft.callsign,                                // CALL
    //        aircraft.baro_altitude_ft,                        // ALT_BARO
    //        aircraft.velocity_kts,                            // VELH
    //        aircraft.direction_deg,                           // TRACK
    //        aircraft.vertical_rate_fpm,                        // VELV
    //        aircraft.latitude_deg,                            // LAT
    //        aircraft.longitude_deg,                           // LON
    //        (int)aircraft.last_track_update_timestamp_ms/1000,
    //        0xFF, 0xFF, 0xFF
    //    );

  
       char squawk_buf[5];
      int16_t squawk_chars =  // Вывести все, кроме CRC, в строковый буфер.
        snprintf(squawk_buf, kCSBeeMessageStrMaxLen - kCRCMaxNumChars - 1,
              "%04o", aircraft.squawk                                   // SQUAWK, e.g. 7232
                   );


    struct ToDUMP1090 sendBuf;
    memset(&sendBuf, 0, sizeof(sendBuf)); // Очистить массив
    sendBuf.addr = aircraft.icao_address;                                   // ICAO address
    memcpy(sendBuf.squawk, squawk_buf, sizeof(squawk_buf));      // Flight number SQUAWK
    memcpy(sendBuf.flight, aircraft.callsign, sizeof(aircraft.callsign)); // номер рейса
    sendBuf.altitude = aircraft.baro_altitude_ft;                              // Altitude метры
    sendBuf.speed = aircraft.velocity_kts;                                    // Скорость км/час
    sendBuf.track = aircraft.direction_deg;                                 // курс в градусах
    sendBuf.vert_rate = aircraft.vertical_rate_fpm;                         // скорость подъема/снижения
    sendBuf.lat_msg = (float)aircraft.latitude_deg;
    sendBuf.lon_msg = (float)aircraft.longitude_deg;
    sendBuf.seen_time = (int)(aircraft.last_track_update_timestamp_ms / 1000);                  // Время получения последнего пакета
    memcpy(sendBuf.endOfPacket, "\xFF\xFF\xFF", 3);

    int16_t num_chars = snprintf(
        message_buf,
        kCSBeeMessageStrMaxLen - kCRCMaxNumChars - 1,
        "%08X%s%s%d%d%d%d%.6f%.6f%d%02X%02X%02X\r\n",
        sendBuf.addr,              // 1  ICAO address  (hex)
        sendBuf.squawk,            // 2  Squawk code (строка, длина 4+1)
        sendBuf.flight,            // 3  Flight number (строка, длина 15+1)
        sendBuf.altitude,          // 4  Высота
        sendBuf.speed,             // 5  Скорость
        sendBuf.track,             // 6  Курс ("angle")
        sendBuf.vert_rate,         // 7  Vertical rate
        sendBuf.lat_msg,           // 8  Latitude, float (можно задать нужную точность)
        sendBuf.lon_msg,           // 9  Longitude
        sendBuf.seen_time,         // 10 Время последнего пакета
        (unsigned char)sendBuf.endOfPacket[0], // 11 0xFF
        (unsigned char)sendBuf.endOfPacket[1], // 12 0xFF
        (unsigned char)sendBuf.endOfPacket[2]  // 13 0xFF
    );
   if (num_chars < 0) return num_chars;  // Проверяем, был ли остановлен вызов snprintf.

    return num_chars;
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