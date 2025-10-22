#include "dump1090_adapter.h"
#include <Arduino.h>
#include <string.h>

ToDUMP1090 ConvertToDump1090(const Aircraft1090 &ac)
{
    ToDUMP1090 out{};
    out.addr = ac.icao_address;
    out.squawk = ac.squawk;

    strncpy(out.flight, ac.callsign, sizeof(out.flight) - 1);

    // высота: GNSS приоритетнее, иначе baro
    if (ac.gnss_altitude_ft > 0) out.altitude = ac.gnss_altitude_ft;
    else out.altitude = ac.baro_altitude_ft;

    out.speed = ac.velocity_kts;
    out.track = (int32_t)roundf(ac.direction_deg);
    out.vert_rate = ac.vertical_rate_fpm;
    out.lat = ac.latitude_deg;
    out.lon = ac.longitude_deg;
    out.seen_time = millis();

    out.endOfPacket[0] = 0xFF;
    out.endOfPacket[1] = 0xFF;
    out.endOfPacket[2] = 0xFF;

    return out;
}
