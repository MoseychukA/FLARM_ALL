/*
 * GNSSHelper.cpp
 * Copyright (C) 2016-2023 Linar Yusupov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if defined(ARDUINO)
#include <Arduino.h>
#endif
#include <TimeLib.h>

#include "GNSS.h"
#include "EEPROMRF.h"
#include "NMEA.h"
#include "SoC.h"
#include "WiFiRF.h"
#include "RF.h"
#include "BatteryRF.h"

#if !defined(EXCLUDE_EGM96)
#include <egm96s.h>
#endif /* EXCLUDE_EGM96 */



#if !defined(GNSS_FLUSH)
#define GNSS_FLUSH()     //   Serial_GNSS_Out.flush()
#endif

unsigned long GNSSTimeSyncMarker = 0;
volatile unsigned long PPS_TimeMarker = 0;

const gnss_chip_ops_t *gnss_chip = NULL;
extern const gnss_chip_ops_t goke_ops; /* forward declaration */

//boolean gnss_set_sucess = false ;
TinyGPSPlus gnss;  // Create an Instance of the TinyGPS++ object called gnss

//int GNSS_cnt           = 0;
uint16_t FW_Build_Year = 2000 + ((__DATE__[ 9]) - '0') * 10 + ((__DATE__[10]) - '0');

const char *GNSS_name[] = {
  [GNSS_MODULE_NONE]    = "NONE",
  [GNSS_MODULE_NMEA]    = "NMEA",
  [GNSS_MODULE_U6]      = "GPS"
};




#if !defined(EXCLUDE_GNSS_UBLOX)

static byte ublox_version() 
{
  byte rval = GNSS_MODULE_U6;
  return rval;
}

static gnss_id_t ublox_probe()
{
  /*
   * ESP8266 NodeMCU and ESP32 DevKit (with NodeMCU adapter)
   * have no any spare GPIO pin to provide GNSS Tx feedback
   */
  return(hw_info.model == SOFTRF_MODEL_STANDALONE && hw_info.revision == 0 ?
         GNSS_MODULE_NMEA : (gnss_id_t) ublox_version());
}



const gnss_chip_ops_t ublox_ops = {
  ublox_probe,
  138,
  67
};


#endif /* EXCLUDE_GNSS_UBLOX */


static bool GNSS_fix_cache = false;

bool isValidGNSSFix()
{
  return GNSS_fix_cache;
}

byte GNSS_setup() 
{

  gnss_id_t gnss_id = GNSS_MODULE_U6;

  SoC->swSer_begin(SERIAL_IN_BR);



#if defined(USE_NMEA_CFG)
  C_NMEA_Source = settings->nmea_out;
#endif /* USE_NMEA_CFG */

  return (byte) gnss_id;
}

void GNSS_loop()
{
  PickGNSSFix();

  /*
    * Требуются оба предложения GGA и RMC NMEA.
    * Невозможно исправить, если какое-либо из них отсутствует или потеряно.
    * Действительная дата имеет решающее значение для устаревшего протокола (только).
   */
  GNSS_fix_cache = gnss.location.isValid()               &&
                   gnss.altitude.isValid()               &&
                   gnss.date.isValid()                   &&
                  (gnss.location.age() <= NMEA_EXP_TIME) &&
                  (gnss.altitude.age() <= NMEA_EXP_TIME) &&
                  (gnss.date.age()     <= NMEA_EXP_TIME);

  GNSSTimeSync();
}

void GNSS_fini()
{

}

/*
 * Sync with GNSS time every 60 seconds
 */
void GNSSTimeSync()
{
  if ((GNSSTimeSyncMarker == 0 || (millis() - GNSSTimeSyncMarker > 60000)) &&
       gnss.time.isValid()                                                 &&
       gnss.time.isUpdated()                                               &&
       gnss.date.year() >= FW_Build_Year                                   &&
      (gnss.time.age() <= 1000) /* 1s */ ) {

    setTime(gnss.time.hour(),
            gnss.time.minute(),
            gnss.time.second(),
            gnss.date.day(),
            gnss.date.month(),
            gnss.date.year());
    GNSSTimeSyncMarker = millis();
  }
}

void PickGNSSFix()
{
  bool isValidSentence = false;
  int ndx;
  int c = -1;

  while (Serial_GNSS_In.available() > 0)
      if (gnss.encode(Serial_GNSS_In.read()))
          displayInfo();


}

#if !defined(EXCLUDE_EGM96)
/*
 *  Algorithm of EGM96 geoid offset approximation was taken from XCSoar
 */

static float AsBearing(float angle)
{
  float retval = angle;

  while (retval < 0)
    retval += 360.0;

  while (retval >= 360.0)
    retval -= 360.0;

  return retval;
}

int LookupSeparation(float lat, float lon)
{
  int ilat, ilon;

  ilat = round((90.0 - lat) / 2.0);
  ilon = round(AsBearing(lon) / 2.0);

  int offset = ilat * 180 + ilon;

  if (offset >= egm96s_dem_len)
    return 0;

  if (offset < 0)
    return 0;

  return (int) pgm_read_byte(&egm96s_dem[offset]) - 127;
}
#endif /* EXCLUDE_EGM96 */


void displayInfo()
{
    /*   Serial.print(F("Location: "));
       if (gnss.location.isValid())
       {
           Serial.print(gnss.location.lat(), 6); 
           Serial.print(F(","));
           Serial.print(gnss.location.lng(), 6);
       }
       else
       {
           Serial.print(F("INVALID"));
       }

       Serial.print(F("  Date/Time: "));
       if (gnss.date.isValid())
       {
           Serial.print(gnss.date.month());
           Serial.print(F("/"));
           Serial.print(gnss.date.day());
           Serial.print(F("/"));
           Serial.print(gnss.date.year());
       }
       else
       {
           Serial.print(F("INVALID"));
       }

       Serial.print(F(" "));
       if (gnss.time.isValid())
       {
           if (gnss.time.hour() < 10) Serial.print(F("0"));
           Serial.print(gnss.time.hour());
           Serial.print(F(":"));
           if (gnss.time.minute() < 10) Serial.print(F("0"));
           Serial.print(gnss.time.minute());
           Serial.print(F(":"));
           if (gnss.time.second() < 10) Serial.print(F("0"));
           Serial.print(gnss.time.second());
           Serial.print(F("."));
           if (gnss.time.centisecond() < 10) Serial.print(F("0"));
           Serial.print(gnss.time.centisecond());
       }
       else
       {
           Serial.print(F("INVALID"));
       }

       Serial.println();*/
}
