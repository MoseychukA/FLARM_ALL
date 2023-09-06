/*
    Name:       SoftRD_Decima_23_09_01_01.ino
    Created:	01.09.2023 17:26:28
    Author:     MASTER\Alex
*/



#include "SoftRF.h"
#include "src/platform/ESP32.h"  // Платформа 
#include "src/driver/Battery.h"  // Драйвер контроллера питания

//#include "src/system/OTA.h"
//#include "src/system/Time.h"
//#include "src/driver/LED.h"
//#include "src/driver/GNSS.h"
//#include "src/driver/RF.h"
//#include "src/driver/Sound.h"
//#include "src/driver/EEPROM.h"
//#include "src/driver/Battery.h"
//#include "src/protocol/data/MAVLink.h"
//#include "src/protocol/data/GDL90.h"
//#include "src/protocol/data/NMEA.h"
//#include "src/protocol/data/D1090.h"
#include "src/system/SoC.h"
//#include "src/driver/WiFi.h"
//#include "src/ui/Web.h"
//#include "src/driver/Baro.h"
//#include "src/TTNHelper.h"
//#include "src/TrafficHelper.h"
//#include "src/system/Recorder.h"

#if !defined(SERIAL_FLUSH)
#define SERIAL_FLUSH() Serial.flush()
#endif

#define DEBUG 0
#define DEBUG_TIMING 0

#define isTimeToDisplay() (millis() - LEDTimeMarker     > 1000)
#define isTimeToExport()  (millis() - ExportTimeMarker  > 1000)

hardware_info_t hw_info = {
  .model = DEFAULT_SOFTRF_MODEL,
  .revision = 0,
  .soc = SOC_NONE,
  //.rf = RF_IC_NONE,
  //.gnss = GNSS_MODULE_NONE,
  //.baro = BARO_MODULE_NONE,
  //.display = DISPLAY_NONE,
  .storage = STORAGE_NONE,
  //.rtc = RTC_NONE,
  .imu = IMU_NONE,
  .mag = MAG_NONE,
  .pmu = PMU_NONE,
};

unsigned long LEDTimeMarker = 0;
unsigned long ExportTimeMarker = 0;



void setup()
{
   // rst_info* resetInfo;
    hw_info.soc = SoC_setup();  // Должна быть самой первой процедурой в порядке выполнения
    //resetInfo = (rst_info*)SoC->getResetInfoPtr();


}

void loop()
{


}
