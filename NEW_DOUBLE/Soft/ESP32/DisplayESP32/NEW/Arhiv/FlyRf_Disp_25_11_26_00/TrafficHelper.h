
#ifndef TRAFFICHELPER_H
#define TRAFFICHELPER_H

#include "SoC.h"

#define ALARM_ZONE_NONE       65500 /* zone range is 1000m <-> 25500m */
#define ALARM_ZONE_LOW        2000  /* zone range is  700m <->  2000m */
#define ALARM_ZONE_IMPORTANT  700   /* zone range is  400m <->   700m */
#define ALARM_ZONE_URGENT     400   /* zone range is    0m <->   400m */

#define VERTICAL_SEPARATION         300 /* metres */
#define VERTICAL_VISIBILITY_RANGE   500 /* value from Classic FLARM data port specs */
#define VERTICAL_VISIBILITY_MAX    2000 /* limit for PowerFLARM */

#define TRAFFIC_VECTOR_UPDATE_INTERVAL 2 /* seconds */
#define TRAFFIC_UPDATE_INTERVAL_MS (TRAFFIC_VECTOR_UPDATE_INTERVAL * 1000)
#define isTimeToUpdateTraffic() (millis() - UpdateTrafficTimeMarker > \
                                  TRAFFIC_UPDATE_INTERVAL_MS)

typedef struct traffic_by_dist_struct {
  ufo_t *fop;
  float distance;
} traffic_by_dist_t;

enum
{
	TRAFFIC_ALARM_NONE,
	TRAFFIC_ALARM_DISTANCE,
	TRAFFIC_ALARM_VECTOR,
	TRAFFIC_ALARM_LEGACY
};


//void ParseData(void);
void Traffic_setup(void);
void Traffic_loop(void);
void ClearExpired(void);
void Traffic_Update(ufo_t *);
bool Traffic_Add(ufo_t *);
int  Traffic_Count(void);

int  traffic_cmp_by_distance(const void *, const void *);

extern ufo_t fo, fo_1090, Container[MAX_TRACKING_OBJECTS], EmptyFO, fo_msg, Container_msg[MAX_TRACKING_OBJECTS];
extern traffic_by_dist_t traffic_by_dist[MAX_TRACKING_OBJECTS];

#endif /* TRAFFICHELPER_H */
