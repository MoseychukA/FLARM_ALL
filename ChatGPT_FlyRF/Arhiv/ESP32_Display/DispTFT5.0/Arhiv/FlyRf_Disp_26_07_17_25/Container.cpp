#include "Container.h"
#include "DeviceInfo.h"
#include <string.h>

unsigned long UpdateTrafficTimeMarker = 0;
ufo_t fo = {};
ufo_t Container[MAX_TRACKING_OBJECTS] = {};
ufo_t EmptyFO = {};
ContainerManager TrafficDB;

void ContainerManager::init()
{
    memset(Container, 0, sizeof(Container));
    memset(&fo, 0, sizeof(fo));
    memset(&EmptyFO, 0, sizeof(EmptyFO));
}

bool ContainerManager::update(const Aircraft& ac)
{
    if (ac.addr == 0) return false;
    int freeIdx = -1;
    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
    {
        if (Container[i].addr == ac.addr)
        {
            Container[i] = ac;
            Container[i].timestamp = time(nullptr);
            Container[i].lastUpdate = millis();
            return true;
        }
        if (freeIdx < 0 && Container[i].addr == 0) freeIdx = i;
    }
    if (freeIdx >= 0)
    {
        Container[freeIdx] = ac;
        Container[freeIdx].timestamp = time(nullptr);
        Container[freeIdx].lastUpdate = millis();
        return true;
    }
    return false;
}

bool ContainerManager::updateFromCandidate(const TrafficCandidate& candidate)
{
    Aircraft ac = {};
    ac.addr = candidate.address;
    ac.latitude = candidate.lat;
    ac.longitude = candidate.lon;
    ac.altitude = candidate.altitude;
    ac.course = candidate.course;
    ac.speed = candidate.speed;
    ac.source = candidate.source;
    ac.valid = true;
    return update(ac);
}

void ContainerManager::removeStale(uint32_t nowMs)
{
    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
    {
        if (Container[i].addr != 0 && (uint32_t)(nowMs - Container[i].lastUpdate) > CONTAINER_STALE_TIMEOUT_MS)
            Container[i] = EmptyFO;
    }
}
Aircraft* ContainerManager::getList() { return Container; }
const Aircraft* ContainerManager::getList() const { return Container; }
int ContainerManager::getCount() const { return Traffic_Count(); }

bool Traffic_Add(ufo_t *fop)
{
    if (!fop) return false;
    return TrafficDB.update(*fop);
}
void Traffic_Update(ufo_t *fop) { (void)Traffic_Add(fop); }
void Traffic_loop() { TrafficDB.removeStale(millis()); }
void ClearExpired() { TrafficDB.removeStale(millis()); }
int Traffic_Count()
{
    int n = 0;
    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) if (Container[i].addr != 0) ++n;
    return n;
}
