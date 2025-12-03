
#include "TrafficHelper.h"
#include "EEPROMRF.h"
#include "RF.h"
#include "GNSS.h"
#include "WebRF.h"
#include "Legacy.h"
#include "CoreCommandBuffer.h"
#include "NMEA.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern SemaphoreHandle_t containerMutex;// Мьютекс для защиты базы

unsigned long UpdateTrafficTimeMarker = 0;

ufo_t fo, Container[MAX_TRACKING_OBJECTS], EmptyFO, fo_msg, Container_msg[MAX_TRACKING_OBJECTS];
traffic_by_dist_t traffic_by_dist[MAX_TRACKING_OBJECTS];

static int8_t (*Alarm_Level)(ufo_t *, ufo_t *);

/*
 * No any alarms issued by the firmware.
 * Rely upon high-level callsign management software.
 */
static int8_t Alarm_None(ufo_t *this_aircraft, ufo_t *fop)
{
  return ALARM_LEVEL_NONE;
}

/*
 * Simple, distance based alarm level assignment.
 */
static int8_t Alarm_Distance(ufo_t *this_aircraft, ufo_t *fop)
{
  int distance = (int) fop->distance;
  int8_t rval = ALARM_LEVEL_NONE;
  int alt_diff = (int) (fop->altitude - this_aircraft->altitude);

  if (abs(alt_diff) < VERTICAL_SEPARATION) { /* no warnings if too high or too low */
    if (distance < ALARM_ZONE_URGENT) {
      rval = ALARM_LEVEL_URGENT;
    } else if (distance < ALARM_ZONE_IMPORTANT) {
      rval = ALARM_LEVEL_IMPORTANT;
    } else if (distance < ALARM_ZONE_LOW) {
      rval = ALARM_LEVEL_LOW;
    }
  }

  return rval;
}

/*
 * EXPERIMENTAL
 *
 * Linear, CoG and GS based collision prediction.
 */
static int8_t Alarm_Vector(ufo_t *this_aircraft, ufo_t *fop)
{
  int8_t rval = ALARM_LEVEL_NONE;
  int alt_diff = (int) (fop->altitude - this_aircraft->altitude);

  if (abs(alt_diff) < VERTICAL_SEPARATION) { /* no warnings if too high or too low */

    /* Subtract 2D velocity vector of traffic from 2D velocity vector of this aircraft */ 
    float V_rel_x = this_aircraft->speed * cosf(radians(90.0 - this_aircraft->course)) -
                    fop->speed * cosf(radians(90.0 - fop->course)) ;
    float V_rel_y = this_aircraft->speed * sinf(radians(90.0 - this_aircraft->course)) -
                    fop->speed * sinf(radians(90.0 - fop->course)) ;

    float V_rel_magnitude = sqrtf(V_rel_x * V_rel_x + V_rel_y * V_rel_y) * _GPS_MPS_PER_KNOT;
    float V_rel_direction = atan2f(V_rel_y, V_rel_x) * 180.0 / PI;  /* -180 ... 180 */

    /* convert from math angle into course relative to north */
    V_rel_direction = (V_rel_direction <= 90.0 ? 90.0 - V_rel_direction :
                                                450.0 - V_rel_direction);

    /* +- 10 degrees tolerance for collision course */
    if (V_rel_magnitude > 0.1 && fabs(V_rel_direction - fop->bearing) < 10.0) {

      /* time is seconds prior to impact */
      float t = fop->distance / V_rel_magnitude;

      /* time limit values are compliant with FLARM data port specs */
      if (t < 9.0) {
        rval = ALARM_LEVEL_URGENT;
      } else if (t < 13.0) {
        rval = ALARM_LEVEL_IMPORTANT;
      } else if (t < 19.0) {
        rval = ALARM_LEVEL_LOW;
      }    
    }
  }
  return rval;
}

/*
 * "Legacy" method is based on short history of 2D velocity vectors (NS/EW)
 */
static int8_t Alarm_Legacy(ufo_t *this_aircraft, ufo_t *fop)
{

  int8_t rval = ALARM_LEVEL_NONE;

  /* TBD */

  return rval;
}

void Traffic_Update(ufo_t *fop)
{

        fop->distance = gnss.distanceBetween(ThisAircraft.latitude,
            ThisAircraft.longitude,
            fop->latitude,
            fop->longitude);

        fop->bearing = gnss.courseTo(ThisAircraft.latitude,
            ThisAircraft.longitude,
            fop->latitude,
            fop->longitude);
 
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


void ParseData()
{
    fo = EmptyFO;
    size_t rx_size = RF_Payload_Size(settings->rf_protocol);
    rx_size = rx_size > sizeof(fo.raw) ? sizeof(fo.raw) : rx_size;

#if DEBUG
    Hex2Bin(TxDataTemplate, RxBuffer);
#endif

    memset(fo.raw, 0, sizeof(fo.raw));
    memcpy(fo.raw, RxBuffer, rx_size);

    if (settings->nmea_p) {
        StdOut.print(F("$PSRFI,"));
        StdOut.print((unsigned long)now()); StdOut.print(F(","));
        StdOut.print(Bin2Hex(fo.raw, rx_size)); StdOut.print(F(","));
        StdOut.println(RF_last_rssi);
    }

    if (memcmp(RxBuffer, TxBuffer, rx_size) == 0)
    {
        if (settings->nmea_p)
        {
            StdOut.println(F("$PSRFE,RF loopback is detected on Rx"));
        }
        return;
    }

    if (protocol_decode && (*protocol_decode)((void*)RxBuffer, &ThisAircraft, &fo))
    {
        if (gnss.time.isValid())
        {
            fo.hour_msg = (int)gnss.time.hour();
            fo.min_msg = (int)gnss.time.minute();
        }
        else
        {
            fo.hour_msg = 10;
            fo.min_msg = 20;
        }


        fo.rssi_LoRa = RF_last_rssi;
        fo.signal_source = 0;

        if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            Traffic_Update(&fo);
            Traffic_Add(&fo);
            xSemaphoreGive(containerMutex);
        }
    }


 /*   size_t rx_size = RF_Payload_Size(settings->rf_protocol);
    rx_size = rx_size > sizeof(fo.raw) ? sizeof(fo.raw) : rx_size;

#if DEBUG
    Hex2Bin(TxDataTemplate, RxBuffer);
#endif

    memset(fo.raw, 0, sizeof(fo.raw));
    memcpy(fo.raw, RxBuffer, rx_size);

    if (settings->nmea_p) {
      StdOut.print(F("$PSRFI,"));
      StdOut.print((unsigned long) now()); StdOut.print(F(","));
      StdOut.print(Bin2Hex(fo.raw, rx_size)); StdOut.print(F(","));
      StdOut.println(RF_last_rssi);
    }

    if (memcmp(RxBuffer, TxBuffer, rx_size) == 0) {
      if (settings->nmea_p) {
        StdOut.println(F("$PSRFE,RF loopback is detected on Rx"));
      }
      return;
    }

    if (protocol_decode && (*protocol_decode)((void *) RxBuffer, &ThisAircraft, &fo)) {
      fo.rssi_LoRa = RF_last_rssi;
      Traffic_Update(&fo);
      Traffic_Add(&fo);
    }*/
}

void Traffic_setup()
{
  switch (settings->alarm)
  {
  case TRAFFIC_ALARM_NONE:
    Alarm_Level = &Alarm_None;
    break;
  case TRAFFIC_ALARM_VECTOR:
    Alarm_Level = &Alarm_Vector;
    break;
  case TRAFFIC_ALARM_LEGACY:
    Alarm_Level = &Alarm_Legacy;
    break;
  case TRAFFIC_ALARM_DISTANCE:
  default:
    Alarm_Level = &Alarm_Distance;
    break;
  }
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
