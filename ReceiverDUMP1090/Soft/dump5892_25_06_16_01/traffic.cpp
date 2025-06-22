/*
 * traffic.cpp
 */

#include "dump5892.h"
#include "ApproxMath.h"
#include "EEPROMRF.h"

 // Простая хеш-таблица для более быстрого поиска идентификаторов в container[]:
 // ноль означает отсутствие, в противном случае *base-1* индекс в container[].
static uint8_t acindex[256] = {0};

// Информация о самом дальнем самолете, который может быть заменен на более близкий
static struct {
    float dist;
    uint32_t addr;
    uint8_t index1;
} farthest = {0, 0, 0};
// Информация о ближайшем самолете
static struct {
    float dist;
    uint32_t addr;
    uint8_t index1;
} closest = {9999.9, 0, 0};

int find_closest_traffic()
{
    if (closest.addr)
        return closest.index1;
    return 0;    // не найдено
}

int find_traffic_by_addr(uint32_t addr)
{
    int a = (addr & 0x0000FF);
    int i = acindex[a];
    while (i != 0) {
        if (container[i-1].addr == addr)
            return i;
        i = container[i-1].next;
    }
    return 0;    // не найдено
}

static int find_empty()
{
    int i = find_traffic_by_addr(0);
    if (i == 0)
        num_tracked = MAX_TRACKING_OBJECTS;   // пустой слот не найден
    return i;
}

// трафик ссылки, который будет записан в контейнер[i-1]
static void insert_traffic_by_index(int i, uint32_t addr)
{
    int k = i-1;
    int a = (addr & 0x0000FF);
    int j = acindex[a];
    acindex[a] = i;
    if (addr == 0) {                 // создание пустого слота
        if (container[k].addr == farthest.addr) {
            farthest.dist = 0;
            farthest.addr = 0;
            farthest.index1 = 0;
        }
        if (container[k].addr == closest.addr) {
            closest.dist = 9999.9;
            closest.addr = 0;
            closest.index1 = 0;
        }
        if (num_tracked > 0)
            --num_tracked;
if(settings->debug>1)
Serial.printf("deleted ID %06X at index0 %d\n", addr, k);
    }
    if (container[k].addr == 0) {   // заполнение пустого слота
        if (num_tracked < MAX_TRACKING_OBJECTS)
            ++num_tracked;
if(settings->debug>1)
Serial.printf("inserted ID %06X at index0 %d\n", addr, k);
    }
    container[k] = EmptyFO;    // все нули перед записью новых данных
    // подразумевает container[i].timestamp = 0; // пока не получим отчет о местоположении
    container[k].addr = addr;
    container[k].next = j;
}

// отсоединить трафик, который должен быть удален из контейнера[i-1]
static void delink_traffic_by_index(int i)
{
    int a = ((container[i-1].addr) & 0x0000FF);
    int j = acindex[a];
    if (j == i) 
    {                          // в начале списка
        acindex[a] = container[i-1].next;   // ноль, если нет следующего
        return;
    }
    while (j != 0) 
    {
        int k = j;
        j = container[j-1].next;
        if (j == i) 
        {
            container[k-1].next = container[i-1].next;
            return;
        }
    }
    // если не найдено (не должно произойти), то ничего не делается
}

// найти существующую запись или создать новую
static int add_traffic_by_addr(uint32_t addr, float distance)
{
    // найти, если уже в контейнере[]
    int j = find_traffic_by_addr(addr);
    if (j != 0) {
    //if(settings->debug>1)
    //Serial.println("add_traffic_by_addr(): already in table");
        return j;
    }

    // иначе заменить пустой объект, если таковой имеется
    j = find_empty();
    if (j != 0) {
        delink_traffic_by_index(j);
        insert_traffic_by_index(j, addr);
if(settings->debug>1)
Serial.println("add_traffic_by_addr(): replaced empty entry in table");
        return (j);
    }

    // иначе заменить самый дальний (неотслеживаемый) объект, если найден
    // (избегает линейного поиска)
    if (distance < farthest.dist) {
        j = farthest.index1;
        farthest.dist = 0;       // будет медленно меняться в traffic_update()
        farthest.addr = 0;
        farthest.index1 = 0;
        delink_traffic_by_index(j);
        insert_traffic_by_index(j, addr);
        return (j);
    }

    /* в противном случае слот не найден, игнорировать новый объект */
    return 0;
}

// заполните определенные поля из каждого типа сообщения
// все, что не заполнено, остается как все нули

void update_traffic_identity()
{
    // не создавать новую запись для сообщения об идентификации,
    // ждать, пока не придет сообщение о позиции
    int i = find_traffic_by_addr(fo.addr);
    if (i == 0)
        return;
    ufo_t *fop = &container[i-1];
    int aircraft_type = fo.aircraft_type;
    ++msg_by_aircraft_type[aircraft_type];
    if (fop->aircraft_type == 0)
        ++new_by_aircraft_type[aircraft_type];
    fop->aircraft_type = aircraft_type;
    memcpy(fop->callsign, fo.callsign, 8);
}

void update_traffic_position()
{
    // найти в таблице или попробовать создать новую запись
    int i = add_traffic_by_addr(fo.addr, fo.distance);
    if (i == 0)
        return;
    ufo_t *fop = &container[i-1];
    if (fop->latitude == 0 && fop->altitude != 0) {
if(settings->debug>1)
Serial.printf("ADS-B overwriting Mode S altitude for ID %06X\n", fo.addr);
    }
    fop->latitude  = fo.latitude;
    fop->longitude = fo.longitude;
    fop->alt_type  = fo.alt_type;
    fop->altitude  = fo.altitude;
    fop->distance  = fo.distance;
    fop->bearing   = fo.bearing;
    if (settings->ac_type != 0) 
    {
        // фильтрация по типу_самолета, ждем, пока не получим сообщение об идентификации
        // - до тех пор fop->тип_самолета равен 0
        fop->positiontime = 0;      // сигналы не отображаются, отфильтрованы
        if (settings->ac_type == 254) 
        {
            // показывать только средние и тяжелые
            if (fop->aircraft_type != 0 && (fop->aircraft_type >= 10 || fop->aircraft_type <= 13))
                fop->positiontime = timenow;
        }
        else if (settings->ac_type == 255) 
        {
            // исключить средний и тяжелый
            if (fop->aircraft_type != 0 && (fop->aircraft_type < 10 || fop->aircraft_type > 13))
                fop->positiontime = timenow;
        }
        else if (fop->aircraft_type == settings->ac_type) 
        {
            fop->positiontime = timenow;
        }
    }
    else 
    {
        fop->positiontime = timenow;
    }
}

void update_traffic_velocity()
{
    // не создавать новую запись, пока не поступит позиция
    int i = find_traffic_by_addr(fo.addr);
    if (i == 0)
        return;
    ufo_t *fop = &container[i-1];
    fop->ewv = fo.ewv;
    fop->nsv = fo.nsv;
    fop->groundspeed = fo.groundspeed;
    fop->track_is_valid = fo.track_is_valid;
    fop->track = fo.track;
    fop->airspeed_type = fo.airspeed_type;
    fop->airspeed = fo.airspeed;
    fop->heading_is_valid = fo.heading_is_valid;
    fop->heading = fo.heading;
    fop->vert_rate = fo.vert_rate;
    fop->alt_diff = fo.alt_diff;
    fop->velocitytime = timenow;
}

// Ответы о высоте в режиме DF4 S - только высота и идентификатор ICAO
void update_mode_s_traffic()
{
    // найти в таблице или попробовать создать новую запись
    int i = add_traffic_by_addr(fo.addr, fo.distance);
    if (i == 0)
        return;
    ufo_t *fop = &container[i-1];
    if (fop->latitude == 0)
    {       // не перезаписывать более полные данные, если они доступны из ADS-B
        //if (fop->altitude == 0) {
        if (fop->altitude != fo.altitude) 
        {
            if(settings->debug>1)
            Serial.printf("Mode S altitude %d for ID %06X\n", fo.altitude, fo.addr);
        }
        fop->altitude = fo.altitude;
        fop->positiontime = timenow;
    }
}

void traffic_update(int i)
{
    ufo_t *fop = &container[i];
    if (fop->addr == 0)
        return;

    // когда должен истечь срок действия объектов трафика (пока есть место)?
    uint32_t exptime = ENTRY_EXPIRATION_TIME;
    if (num_tracked < MAX_TRACKING_OBJECTS)
        exptime <<= 5;
    if (timenow > fop->positiontime + exptime) 
    {
        i++;
        delink_traffic_by_index(i);
        //fop->addr = 0;
        insert_traffic_by_index(i,0);   // ссылка на пустой список (0-адрес)
        return;
    }

    if (fop->positiontime == 0)   // известен только идентификатор, или позиция отфильтрована
        return;

    // отслеживайте, какой (не отслеживаемый) самолет находится дальше всего
    if (fop->distance > farthest.dist && fop->addr != settings->follow) 
    {
        farthest.dist = fop->distance;
        farthest.addr = fop->addr;
        farthest.index1 = i+1;
    }
    else if (fop->addr == farthest.addr) 
    {
        if (fop->distance < farthest.dist)
            farthest.dist = fop->distance;  // может быть, на самом деле уже не самый дальний
    }
    // отслеживайте, какой самолет находится ближе всего
    if (fop->distance > 0 && fop->distance < closest.dist) 
    {
        closest.dist = fop->distance;
        closest.addr = fop->addr;
        closest.index1 = i+1;
    }
    else if (fop->addr == closest.addr) 
    {
        if (fop->distance > closest.dist)
            closest.dist = fop->distance;   // может быть уже не самым близким
    }

#if defined(TESTING)
    if (fop->updatetime < fop->velocitytime) 
    {   // may lag by up to 1 second
        float fgroundspeed = approxHypotenuse( (float)fop->nsv, (float)fop->ewv );
        if ((float)fop->groundspeed > 1.05 * fgroundspeed)
            ++upd_by_gs_incorrect[1];
        else if ((float)fop->groundspeed < 0.95 * fgroundspeed)
            ++upd_by_gs_incorrect[1];
        else
            ++upd_by_gs_incorrect[0];
        float ftrack = atan2_approx((float)fop->nsv, (float)fop->ewv);
        if (ftrack < 0)
            ftrack += 360;
        if (ftrack > 270 && fop->track < 90)
            ftrack -= 360;
        else if (ftrack < 90 && fop->track > 270)
            ftrack += 360;
        if (fop->groundspeed>0 && fabs(ftrack-fop->track) > 3)
            ++upd_by_trk_incorrect[1];
        else
            ++upd_by_trk_incorrect[0];
    }
#endif

#if defined(TESTING)
    if (fop->updatetime < fop->positiontime) {   // может отставать до 1 секунды
        float x, y;
        y = (111300.0 * 0.00053996) * (fop->latitude - reflat); /* nm */
        x = (111300.0 * 0.00053996) * (fop->longitude - reflon) * CosLat(reflat);
        float fdistance = approxHypotenuse(x, y);
        if (fop->distance > 1.02 * fdistance)
            ++upd_by_dist_incorrect[1];
        else if (fop->distance < 0.98 * fdistance)
            ++upd_by_dist_incorrect[1];
        else
            ++upd_by_dist_incorrect[0];
        int16_t fbearing = (int16_t) atan2_approx(y, x);    /* градусы от реф. до цели */
        if (fbearing < 0)
            fbearing += 360;
        if (abs(fop->bearing - fbearing) > 2)
            ++upd_by_brg_incorrect[1];
        else
            ++upd_by_brg_incorrect[0];
    }
#endif

    fop->updatetime = timenow;
}

void traffic_setup()
{
    // связать все пустые слоты с acindex[0]
    // чтобы find_traffic_by_addr(0) нашел их
    acindex[0] = 1;   // указывает на контейнер[0]
    for (int i=0; i<MAX_TRACKING_OBJECTS-1; i++)
        container[i].next = i+2;
    container[MAX_TRACKING_OBJECTS-1].next = 0;

    //num_tracked = 0;
    //farthest.dist = 0;
    //farthest.addr = 0;
    //farthest.index1 = 0;
    //closest.dist = 9999.9;
    //closest.addr = 0;
    //closest.index1 = 0;
}

void traffic_loop()
{
    // периодически обновлять некоторые вещи (для всех самолетов, по одному за раз)
    static unsigned int tick = 0;
    static uint32_t nexttime = 0;
    if (millis() < nexttime)
        return;
    nexttime = millis() + (3000/MAX_TRACKING_OBJECTS);   // каждый раз каждые 3 секунды
    tick++;
    // выбрать, какую запись обновить на этот раз в цикле
    // предполагает, что MAX_TRACKING_OBJECTS — это степень 2
    int i = (tick & (MAX_TRACKING_OBJECTS-1));
    if (i >= MAX_TRACKING_OBJECTS)
        i = 0;                       // просто для безопасности

    traffic_update(i);

    ++ticks_by_numtracked[num_tracked];
}
