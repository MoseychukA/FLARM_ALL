/*
    Name:       SoftRD_Decima_23_09_01_01.ino
    Created:	01.09.2023 17:26:28
    Author:     MASTER\Alex
*/

//#include "src/system/OTA.h"
//#include "src/system/Time.h"
//#include "src/driver/GNSS.h"
//#include "src/driver/RF.h"
//#include "src/driver/Battery.h"
//#include "src/protocol/data/NMEA.h"
#include "src/system/SoC.h"
//#include "src/driver/WiFi.h"
//#include "src/ui/Web.h"


#if !defined(SERIAL_FLUSH)
#define SERIAL_FLUSH() Serial.flush()
#endif

#define DEBUG 0
#define DEBUG_TIMING 0

#define isTimeToDisplay() (millis() - LEDTimeMarker     > 1000)
#define isTimeToExport()  (millis() - ExportTimeMarker  > 1000)






void setup()
{


}

void loop()
{


}
