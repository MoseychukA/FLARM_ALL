/*
  Модуль Container.cpp
  Назначение:
  - Центральная база данных сторонних самолетов проекта.

  Основные задачи модуля:
  - Хранить массив Container с ограниченным числом сопровождаемых целей.
  - При поступлении новых данных искать уже существующую запись по адресу и обновлять ее.
  - Если запись новая, помещать ее в свободный слот.
  - Пересчитывать вспомогательные параметры цели: дальность, направление, время обновления.
  - Удалять устаревшие записи, которые давно не обновлялись.
*/

#include "Container.h"
#include "DeviceInfo.h"
#include "EEPROMRF.h"
#include <math.h>
#include <string.h>




unsigned long UpdateTrafficTimeMarker = 0;
ufo_t fo = {};
ufo_t Container[MAX_TRACKING_OBJECTS] = {};
ufo_t EmptyFO = {};
ContainerManager TrafficDB;

static time_t nowSeconds()
{
    return (time_t)(millis() / 1000UL);
}

static float deg2radf(float v) { return v * 0.01745329251994329577f; }
static float rad2degf(float v) { return v * 57.295779513082320876f; }


static bool isBlockedAddr(uint32_t addr)
{
    if (addr == 0U || settings == nullptr)
    {
        return false;
    }

    if (settings->block_addr  != 0U && addr == settings->block_addr)  return true;
    if (settings->block_addr2 != 0U && addr == settings->block_addr2) return true;
    if (settings->block_addr3 != 0U && addr == settings->block_addr3) return true;

    return false;
}

static void removeBlockedFromContainer()
{
    if (settings == nullptr)
    {
        return;
    }

    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
    {
        if (isBlockedAddr(Container[i].addr))
        {
            Container[i] = EmptyFO;
        }
    }
}

static float haversineMeters(float lat1, float lon1, float lat2, float lon2)
{
    const float dLat = deg2radf(lat2 - lat1);
    const float dLon = deg2radf(lon2 - lon1);
    const float a = sinf(dLat * 0.5f) * sinf(dLat * 0.5f) +
                    cosf(deg2radf(lat1)) * cosf(deg2radf(lat2)) *
                    sinf(dLon * 0.5f) * sinf(dLon * 0.5f);
    const float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
    return 6371000.0f * c;
}

static float bearingDeg(float lat1, float lon1, float lat2, float lon2)
{
    const float phi1 = deg2radf(lat1);
    const float phi2 = deg2radf(lat2);
    const float lam = deg2radf(lon2 - lon1);
    const float y = sinf(lam) * cosf(phi2);
    const float x = cosf(phi1) * sinf(phi2) - sinf(phi1) * cosf(phi2) * cosf(lam);
    float brng = rad2degf(atan2f(y, x));
    if (brng < 0.0f) brng += 360.0f;
    return brng;
}

void Traffic_Update(ufo_t *fop)
{
    if (fop == nullptr || fop->addr == 0U)
    {
        return;
    }

    fop->local_latitude = ThisAircraft.local_latitude;
    fop->local_longitude = ThisAircraft.local_longitude;

    if ((fop->latitude != 0.0f && fop->longitude != 0.0f) &&
        (ThisAircraft.local_latitude != 0.0f && ThisAircraft.local_longitude != 0.0f))
    {
        fop->distance = haversineMeters(ThisAircraft.local_latitude, ThisAircraft.local_longitude,
                                    fop->latitude, fop->longitude);
        fop->bearing = bearingDeg(ThisAircraft.local_latitude, ThisAircraft.local_longitude,
                                  fop->latitude, fop->longitude);
    }

    fop->vs = (float)fop->vert_rate;
    fop->rssi = (fop->signal_source == TRAFFIC_SOURCE_ADSB_DUMP1090) ? (int)fop->rssi_rp2040 : (int)fop->rssi_LoRa;
    fop->valid = (fop->addr != 0U);
    if (fop->timestamp == 0)
    {
        fop->timestamp = nowSeconds();
    }
    fop->seen = fop->timestamp;
}

bool Traffic_Add(ufo_t *fop)
{
    if (fop == nullptr)
    {
        return false;
    }

    if (isBlockedAddr(fop->addr))
    {
        return false;
    }

    if (fop->addr == ThisAircraft.addr)
    {
        return false;
    }

    if (fop->addr == 0U)
    {
        return false;
    }

    Traffic_Update(fop);

    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
    {
        if (Container[i].addr == fop->addr)
        {
            Container[i] = *fop;
            Container[i].valid = true;
            return true;
        }
    }

    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
    {
        if (Container[i].addr == 0U)
        {
            Container[i] = *fop;
            Container[i].valid = true;
            return true;
        }
    }

    int oldest = 0;
    time_t ts_min = Container[0].timestamp;
    const time_t nowTs = nowSeconds();
    for (int i = 1; i < MAX_TRACKING_OBJECTS; ++i)
    {
        if (Container[i].timestamp < ts_min)
        {
            oldest = i;
            ts_min = Container[i].timestamp;
        }
        if ((nowTs > Container[i].timestamp) && ((nowTs - Container[i].timestamp) > ENTRY_EXPIRATION_TIME))
        {
            oldest = i;
            break;
        }
    }

    Container[oldest] = *fop;
    Container[oldest].valid = true;
    return true;
}

void ContainerManager::init()
{
    memset(&fo, 0, sizeof(fo));
    memset(&Container, 0, sizeof(Container));
    memset(&EmptyFO, 0, sizeof(EmptyFO));
    UpdateTrafficTimeMarker = 0;
}

bool ContainerManager::update(const Aircraft& ac)
{
    fo = ac;
    return Traffic_Add(&fo);
}

bool ContainerManager::updateFromCandidate(const TrafficCandidate& candidate)
{
    if (!candidate.valid || candidate.address == 0U)
    {
        return false;
    }

    fo = EmptyFO;
    fo.addr = candidate.address & 0xFFFFFFUL;
    fo.altitude = (float)candidate.altitude;
    fo.pressure_altitude = (float)candidate.altitude;
    fo.speed = candidate.speed;
    fo.course = candidate.course;
    fo.latitude = candidate.lat;
    fo.longitude = candidate.lon;
    fo.timestamp = nowSeconds();
    fo.timemsg = fo.timestamp;
    fo.seen = fo.timestamp;
    fo.source = candidate.source;
    fo.signal_source = (uint8_t)candidate.source;
    fo.rssi_rp2040 = (candidate.source == TRAFFIC_SOURCE_ADSB_DUMP1090) ? (int8_t)candidate.rssi : 0;
    fo.rssi_LoRa = (candidate.source == TRAFFIC_SOURCE_FLARM_LORA) ? (int8_t)candidate.rssi : 0;
    fo.rssi = candidate.rssi;
    fo.snr = candidate.snr;
    fo.valid = true;
    Traffic_Update(&fo);
    return Traffic_Add(&fo);
}

void ClearExpired()
{
    const time_t nowTs = nowSeconds();
    removeBlockedFromContainer();
    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
    {
        if (Container[i].addr != 0U && (nowTs - Container[i].timestamp) > ENTRY_EXPIRATION_TIME)
        {
            Container[i] = EmptyFO;
        }
    }
}

void Traffic_loop()
{
    if ((millis() - UpdateTrafficTimeMarker) < (TRAFFIC_VECTOR_UPDATE_INTERVAL * 1000UL))
    {
        return;
    }

    const time_t nowTs = nowSeconds();
    removeBlockedFromContainer();
    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
    {
        if (Container[i].addr != 0U && (nowTs - Container[i].timestamp) <= ENTRY_EXPIRATION_TIME)
        {
            if ((nowTs - Container[i].timestamp) >= TRAFFIC_VECTOR_UPDATE_INTERVAL)
            {
                Traffic_Update(&Container[i]);
            }
        }
        else
        {
            Container[i] = EmptyFO;
        }
    }

    UpdateTrafficTimeMarker = millis();
}

void ContainerManager::removeStale(uint32_t)
{
    ClearExpired();
}

Aircraft* ContainerManager::getList()
{
    return Container;
}

const Aircraft* ContainerManager::getList() const
{
    return Container;
}

int Traffic_Count()
{
    int count = 0;
    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
    {
        if (Container[i].addr != 0U)
        {
            ++count;
        }
    }
    return count;
}

int ContainerManager::getCount() const
{
    return Traffic_Count();
}


//void ParseData()
//{
//    fo = EmptyFO;
//    size_t rx_size = RF_Payload_Size(settings->rf_protocol);
//    rx_size = rx_size > sizeof(fo.raw) ? sizeof(fo.raw) : rx_size;
//
//#if DEBUG
//    Hex2Bin(TxDataTemplate, RxBuffer);
//#endif
//
//    memset(fo.raw, 0, sizeof(fo.raw));
//    memcpy(fo.raw, RxBuffer, rx_size);
//
//    if (settings->nmea_p) {
//        Serial.print(F("$PSRFI,"));
//        Serial.print((unsigned long)now()); Serial.print(F(","));
//        Serial.print(Bin2Hex(fo.raw, rx_size)); Serial.print(F(","));
//        Serial.println(RF_last_rssi);
//    }
//
//    if (memcmp(RxBuffer, TxBuffer, rx_size) == 0)
//    {
//        if (settings->nmea_p)
//        {
//            Serial.println(F("$PSRFE,RF loopback is detected on Rx"));
//        }
//        return;
//    }
//
//    if (protocol_decode && (*protocol_decode)((void*)RxBuffer, &ThisAircraft, &fo))
//    {
//        //if (gnss.time.isValid())
//        //{
//        //    fo.hour_msg = (int)gnss.time.hour();
//        //    fo.min_msg = (int)gnss.time.minute();
//        //}
//        //else
//        //{
//            fo.hour_msg = 10;
//            fo.min_msg = 20;
//        //}
//
//
//            fo.rssi_LoRa = 0;//!! RF_last_rssi;
//        fo.signal_source = 0;
//        Traffic_Update(&fo);
//        Traffic_Add(&fo);
// 
//
//    }
//
//
