
#include "TrafficHelper.h"
#include "EEPROMRF.h"
//#include "RF.h"
//#include "GNSS.h"
#include "WebRF.h"
//#include "Legacy.h"
//#include "CoreCommandBuffer.h"
//#include "NMEA.h"
#include <TimeLib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern SemaphoreHandle_t containerMutex;// Мьютекс для защиты базы

unsigned long UpdateTrafficTimeMarker = 0;

extern ufo_t fo, Container[MAX_TRACKING_OBJECTS], EmptyFO, fo_msg, Container_msg[MAX_TRACKING_OBJECTS];
traffic_by_dist_t traffic_by_dist[MAX_TRACKING_OBJECTS];


void Traffic_Update(ufo_t *fop)
{

   /*     fop->distance = gnss.distanceBetween(ThisAircraft.latitude,
            ThisAircraft.longitude,
            fop->latitude,
            fop->longitude);

        fop->bearing = gnss.courseTo(ThisAircraft.latitude,
            ThisAircraft.longitude,
            fop->latitude,
            fop->longitude);*/
 
}


bool Traffic_Add(ufo_t* fop)
{
    int i, oldest = 0;
    uint32_t ts_min;
    bool result = false;

    if (!fop->addr) return false;

         // 1. Обновление
        for (i = 0; i < MAX_TRACKING_OBJECTS; i++) 
        {
            if (Container[i].addr == fop->addr) 
            {
                Container[i] = *fop;
                result = true;
                goto AddFinish;
            }
        }
        // 2. Пустой слот
        for (i = 0; i < MAX_TRACKING_OBJECTS; i++) 
        {
            if (Container[i].addr == 0) 
            {
                Container[i] = *fop;
                result = true;
                goto AddFinish;
            }
        }
        // 3. Перезапись самого старого/просроченного
        oldest = 0; ts_min = Container[0].timestamp;
        for (i = 1; i < MAX_TRACKING_OBJECTS; i++) 
        {
            if (Container[i].timestamp < ts_min) 
            {
                oldest = i;
                ts_min = Container[i].timestamp;
            }
            if ((now() > Container[i].timestamp) && ((now() - Container[i].timestamp) > ENTRY_EXPIRATION_TIME)) 
            {
                oldest = i;
                break;
            }
        }
        Container[oldest] = *fop;
        result = true;
    AddFinish:

    return result;
}


void Traffic_setup()
{

}

void Traffic_loop()
{
    if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        if (isTimeToUpdateTraffic()) 
        {
            for (int i=0; i < MAX_TRACKING_OBJECTS; i++) 
            {

                if (Container[i].addr &&  (ThisAircraft.timestamp - Container[i].timestamp) <= ENTRY_EXPIRATION_TIME) 
                {
                    if ((ThisAircraft.timestamp - Container[i].timestamp) >= TRAFFIC_VECTOR_UPDATE_INTERVAL) 
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
        xSemaphoreGive(containerMutex);
    }
}

void ClearExpired()
{
    if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        for (int i=0; i < MAX_TRACKING_OBJECTS; i++)
        {
            if (Container[i].addr && (ThisAircraft.timestamp - Container[i].timestamp) > ENTRY_EXPIRATION_TIME) 
            {
                Container[i] = EmptyFO;
            }
        }
        xSemaphoreGive(containerMutex);
    }
}

int Traffic_Count()
{
    int count = 0;
    if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE) 
    {
        for (int i=0; i < MAX_TRACKING_OBJECTS; i++) 
        {
            if (Container[i].addr) 
            {
                count++;
            }
        }
        xSemaphoreGive(containerMutex);
    }
    return count;
}

int traffic_cmp_by_distance(const void *a, const void *b)
{
  traffic_by_dist_t *ta = (traffic_by_dist_t *)a;
  traffic_by_dist_t *tb = (traffic_by_dist_t *)b;

  if (ta->distance >  tb->distance) return  1;
  if (ta->distance == tb->distance) return  0;
#if 0
  if (ta->distance <  tb->distance) return -1;
#else
  else return -1;
#endif
}
