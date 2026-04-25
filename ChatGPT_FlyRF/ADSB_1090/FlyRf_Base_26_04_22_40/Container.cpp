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




unsigned long UpdateTrafficTimeMarker = 0;           // Структура данных самолета или цели: хранит параметры борта, используемые при обмене и отображении.
ufo_t fo = {};                                       // Структура данных самолета или цели: хранит параметры борта, используемые при обмене и отображении.
ufo_t Container[MAX_TRACKING_OBJECTS] = {};          // Контейнер данных, таблица, база или вспомогательный массив.
ufo_t EmptyFO = {};   
ContainerManager TrafficDB;                          // Контейнер данных, таблица, база или вспомогательный массив.

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `nowSeconds` и обрабатывает now seconds в контексте модуля Container.cpp.
//------------------------------------------------------------------------------
static time_t nowSeconds()
{
    return (time_t)(millis() / 1000UL);
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `deg2radf` и обрабатывает deg2radf в контексте модуля Container.cpp.
//------------------------------------------------------------------------------
static float deg2radf(float v) { return v * 0.01745329251994329577f; }
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `rad2degf` и обрабатывает rad2degf в контексте модуля Container.cpp.
//------------------------------------------------------------------------------
static float rad2degf(float v) { return v * 57.295779513082320876f; }


//------------------------------------------------------------------------------
// Назначение функции: Возвращает blocked адрес, рассчитанное или считанное по текущему состоянию модуля.
//------------------------------------------------------------------------------
static bool isBlockedAddr(uint32_t addr)
{
    if (addr == 0U || settings == nullptr)
    {
        return false;
    }

    for (size_t i = 0; i < 3; ++i)
    {
        if (settings->block_addr[i] != 0U && addr == settings->block_addr[i])
        {
            return true;
        }
    }

    return false;
}

//------------------------------------------------------------------------------
// Назначение функции: Удаляет blocked базу целей Container, освобождает связанные ресурсы и убирает следы объекта из текущего состояния.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `haversineMeters` и обрабатывает haversine meters в контексте модуля Container.cpp.
// Локальные переменные: dLat — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; 
// dLon — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; 
// c — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; 
// sqrtf — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `bearingDeg` и обрабатывает bearing deg в контексте модуля Container.cpp.
// Локальные переменные: phi1 — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; 
// phi2 — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; 
// lam — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; 
// y — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; 
// x — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; 
// brng — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Traffic_Update` и обрабатывает запись о цели в контексте модуля Container.cpp.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Traffic_Add` и обрабатывает запись о цели add в контексте модуля Container.cpp.
// Локальные переменные: oldest — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; 
// ts_min — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; 
// nowTs — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

    int oldest = 0;  //
    time_t ts_min = Container[0].timestamp;  //
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

//------------------------------------------------------------------------------
// Назначение функции: Инициализирует служебную операцию модуля, подготавливает связанные объекты и включает работу соответствующего узла.
//------------------------------------------------------------------------------
void ContainerManager::init()
{
    memset(&fo, 0, sizeof(fo));
    memset(&Container, 0, sizeof(Container));
    memset(&EmptyFO, 0, sizeof(EmptyFO));
    UpdateTrafficTimeMarker = 0;
}

//------------------------------------------------------------------------------
// Назначение функции: Обновляет служебную операцию модуля по новым входным данным и поддерживает актуальное состояние соответствующей подсистемы.
//------------------------------------------------------------------------------
bool ContainerManager::update(const Aircraft& ac)
{
    fo = ac;
    return Traffic_Add(&fo);
}

//------------------------------------------------------------------------------
// Назначение функции: Обновляет candidate по новым входным данным и поддерживает актуальное состояние соответствующей подсистемы.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Очищает expired, сбрасывает связанные флаги и возвращает модуль в исходное состояние для этого участка логики.
// Локальные переменные: nowTs — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Обслуживает запись о цели в основном цикле: проверяет события, обновляет состояние и запускает нужные действия.
// Локальные переменные: nowTs — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Удаляет stale, освобождает связанные ресурсы и убирает следы объекта из текущего состояния.
//------------------------------------------------------------------------------
void ContainerManager::removeStale(uint32_t)
{
    ClearExpired();
}

//------------------------------------------------------------------------------
// Назначение функции: Возвращает list, рассчитанное или считанное по текущему состоянию модуля.
//------------------------------------------------------------------------------
Aircraft* ContainerManager::getList()
{
    return Container;
}

//------------------------------------------------------------------------------
// Назначение функции: Возвращает list, рассчитанное или считанное по текущему состоянию модуля.
//
//------------------------------------------------------------------------------
const Aircraft* ContainerManager::getList() const
{
    return Container;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Traffic_Count` и обрабатывает запись о цели count в контексте модуля Container.cpp.
// Локальные переменные: count — счетчик или индекс: указывает позицию элемента, номер строки, слота или текущую стадию перебора.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Возвращает count, рассчитанное или считанное по текущему состоянию модуля.
//------------------------------------------------------------------------------
int ContainerManager::getCount() const
{
    return Traffic_Count();
}
