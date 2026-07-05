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

#if !defined(EXCLUDE_EGM96)
#include <egm96s.h>
#endif /* EXCLUDE_EGM96 */

#if !defined(DO_GNSS_DEBUG)
#define GNSS_DEBUG_PRINT
#define GNSS_DEBUG_PRINTLN
#else
#define GNSS_DEBUG_PRINT    Serial.print
#define GNSS_DEBUG_PRINTLN  Serial.println
#endif

#if !defined(GNSS_FLUSH)
#define GNSS_FLUSH()      
#endif

unsigned long GNSSTimeSyncMarker = 0;
volatile unsigned long PPS_TimeMarker = 0;

const gnss_chip_ops_t *gnss_chip = NULL;
extern const gnss_chip_ops_t goke_ops; /* forward declaration */

boolean gnss_set_sucess = false ;
TinyGPSPlus gnss;  // Create an Instance of the TinyGPS++ object called gnss

uint8_t GNSSbuf[350]; // at least 3 lines of 80 characters each
                      // and 40+30*N bytes for "UBX-MON-VER" payload

int GNSS_cnt           = 0;
uint16_t FW_Build_Year = 2000 + ((__DATE__[ 9]) - '0') * 10 + ((__DATE__[10]) - '0');

const char *GNSS_name[] = {
  [GNSS_MODULE_NONE]    = "NONE",
  [GNSS_MODULE_NMEA]    = "NMEA",
  [GNSS_MODULE_U6]      = "U6",
  [GNSS_MODULE_U7]      = "U7"

};



bool nmea_handshake(const char *req, const char *resp, bool skipline)
{
  bool rval = false;

  if (resp == NULL || strlen(resp) == 0) {
    return rval;
  }

  // clean any leftovers
  Serial_GNSS_In.flush();

  while (Serial_GNSS_In.available() > 0) { Serial_GNSS_In.read(); }

  unsigned long start_time = millis();
  unsigned long timeout_ms = (req == NULL ? 3000 : 2000) ;

  while ((millis() - start_time) < timeout_ms) {

    while (Serial_GNSS_In.read() != '\n' && (millis() - start_time) < timeout_ms) { yield(); }

    delay(5);

    /* wait for pause after NMEA burst */
    if (req && Serial_GNSS_In.available() > 0) {
      continue;
    } else {
      /* send request */
      if (req) {
        Serial_GNSS_Out.write((uint8_t *) req, strlen(req));
        GNSS_FLUSH();
      }

      /* skip first line when expected response contains 2 of them */
      if (skipline) {
        start_time = millis();
        while (Serial_GNSS_In.read() != '\n' && (millis() - start_time) < timeout_ms) { yield(); }
      }

      int i=0;
      char c;

      /* take response into buffer */
      while ((millis() - start_time) < timeout_ms) {

        c = Serial_GNSS_In.read();

        if (isPrintable(c) || c == '\r' || c == '\n') {
          if (i >= sizeof(GNSSbuf)) break;
          GNSSbuf[i++] = c;
        } else {
          /* ignore */
          continue;
        }

        if (c == '\n') break;
      }

      if (!strncmp((char *) &GNSSbuf[0], resp, strlen(resp))) {
        rval = true;
        break;
      }
    }
  }

  return rval;
}

static gnss_id_t generic_nmea_probe()
{
  return nmea_handshake(NULL, "$G", false) ? GNSS_MODULE_NMEA : GNSS_MODULE_NONE;
}

static bool generic_nmea_setup()
{
  return true;
}

static void generic_nmea_loop()
{

}

static void generic_nmea_fini()
{

}

const gnss_chip_ops_t generic_nmea_ops = {
  generic_nmea_probe,
  generic_nmea_setup,
  generic_nmea_loop,
  generic_nmea_fini,
  /* use Ublox timing values for 'generic NMEA' module */
  138 /* GGA */, 67 /* RMC */
};

#if !defined(EXCLUDE_GNSS_UBLOX)
 /* CFG-MSG */
const uint8_t setGLL[] PROGMEM = {0xF0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
const uint8_t setGSV[] PROGMEM = {0xF0, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
const uint8_t setVTG[] PROGMEM = {0xF0, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
const uint8_t setGSA[] PROGMEM = {0xF0, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
 /* CFG-PRT */
uint8_t setBR[] = {0x01, 0x00, 0x00, 0x00, 0xD0, 0x08, 0x00, 0x00, 0x00, 0x96,
                   0x00, 0x00, 0x07, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00};

const uint8_t setNav5[] PROGMEM = {0xFF, 0xFF, 0x07, 0x03, 0x00, 0x00, 0x00, 0x00,
                                   0x10, 0x27, 0x00, 0x00, 0x05, 0x00, 0xFA, 0x00,
                                   0xFA, 0x00, 0x64, 0x00, 0x2C, 0x01, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00};

const uint8_t CFG_RST[12] PROGMEM = { 0xb5, 0x62, 0x06, 0x04, 0x04, 0x00, 0x00,
                                      0x00, 0x01, 0x00, 0x0F, 0x66};

const uint8_t CFG_RST_COLD[12] PROGMEM = { 0xB5, 0x62, 0x06, 0x04, 0x04, 0x00,
                                           0xFF, 0xB9, 0x00, 0x00, 0xC6, 0x8B };

const uint8_t RXM_PMREQ_OFF[16] PROGMEM = {0xb5, 0x62, 0x02, 0x41, 0x08, 0x00,
                                           0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
                                           0x00, 0x00, 0x4d, 0x3b};


/* ------ BEGIN -----------  https://github.com/Black-Thunder/FPV-Tracker */

enum ubloxState{ WAIT_SYNC1, WAIT_SYNC2, GET_CLASS, GET_ID, GET_LL, GET_LH, GET_DATA, GET_CKA, GET_CKB };

ubloxState ubloxProcessDataState = WAIT_SYNC1;

unsigned short ubloxExpectedDataLength;
unsigned short ubloxClass, ubloxId;
unsigned char  ubloxCKA, ubloxCKB;

// process serial data
// data is stored inside #GNSSbuf, data size inside #GNSS_cnt
// warning : if #GNSSbuf is too short, data is truncated.
static int ubloxProcessData(unsigned char data) 
{
	int parsed = 0;

	switch (ubloxProcessDataState) {
	case WAIT_SYNC1:
		if (data == 0xb5) {
			ubloxProcessDataState = WAIT_SYNC2;
		}
		break;

	case WAIT_SYNC2:
		if (data == 0x62) {
			ubloxProcessDataState = GET_CLASS;
		}
		else if (data == 0xb5) {
			// ubloxProcessDataState = GET_SYNC2;
		}
		else {
			ubloxProcessDataState = WAIT_SYNC1;
		}
		break;
	case GET_CLASS:
		ubloxClass = data;
		ubloxCKA = data;
		ubloxCKB = data;
		ubloxProcessDataState = GET_ID;
		break;

	case GET_ID:
		ubloxId = data;
		ubloxCKA += data;
		ubloxCKB += ubloxCKA;
		ubloxProcessDataState = GET_LL;
		break;

	case GET_LL:
		ubloxExpectedDataLength = data;
		ubloxCKA += data;
		ubloxCKB += ubloxCKA;
		ubloxProcessDataState = GET_LH;
		break;

	case GET_LH:
		ubloxExpectedDataLength += data << 8;
		GNSS_cnt = 0;
		ubloxCKA += data;
		ubloxCKB += ubloxCKA;
		ubloxProcessDataState = GET_DATA;
		break;

	case GET_DATA:
		ubloxCKA += data;
		ubloxCKB += ubloxCKA;
		if (GNSS_cnt < sizeof(GNSSbuf)) {
			GNSSbuf[GNSS_cnt++] = data;
		}
		if ((--ubloxExpectedDataLength) == 0) {
			ubloxProcessDataState = GET_CKA;
		}
		break;

	case GET_CKA:
		if (ubloxCKA != data) {
			ubloxProcessDataState = WAIT_SYNC1;
		}
		else {
			ubloxProcessDataState = GET_CKB;
		}
		break;

	case GET_CKB:
		if (ubloxCKB == data) {
			parsed = 1;
		}
		ubloxProcessDataState = WAIT_SYNC1;
		break;

	}

	return parsed;
}

/* ------ END -----------  https://github.com/Black-Thunder/FPV-Tracker */

static byte ublox_version() 
{
  byte rval = GNSS_MODULE_NMEA;
  unsigned long startTime = millis();


  // Get the message back from the GPS
  GNSS_DEBUG_PRINT(F(" * Reading response: "));

  while ((millis() - startTime) < 2000 ) {

    if (Serial_GNSS_In.available()) 
    {
      unsigned char c = Serial_GNSS_In.read();
      int ret = 0;

      GNSS_DEBUG_PRINT(c, HEX);
      ret = ubloxProcessData(c);

      // Upon a successfully parsed sentence, do the version detection
      if (ret) {

        if (ubloxClass == 0x0A) { // MON
          if (ubloxId == 0x04) {  // VER

            Serial.print(F("INFO: GNSS module HW version: "));
            Serial.println((char *) &GNSSbuf[30]);

            Serial.print(F("INFO: GNSS module FW version: "));
            Serial.println((char *) &GNSSbuf[0]);


            if (GNSSbuf[33] == '4')
              rval = GNSS_MODULE_U6;
            else if (GNSSbuf[33] == '7')
              rval = GNSS_MODULE_U7;
            else if (GNSSbuf[33] == '8')
              rval = GNSS_MODULE_U8;
 /*           else if (GNSSbuf[32] == '1' && GNSSbuf[33] == '9')
              rval = GNSS_MODULE_U9;
            else if (GNSSbuf[33] == 'A')
              rval = GNSS_MODULE_U10;*/

            break;
          }
        }
      }
    }
  }

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

static bool ublox_setup()
{

  return true;
}

static void ublox_loop()
{

}

static void ublox_fini()
{
 
}

const gnss_chip_ops_t ublox_ops = {
  ublox_probe,
  ublox_setup,
  ublox_loop,
  ublox_fini,
  138 /* GGA */, 67 /* RMC */
};

static void ublox_factory_reset()
{
  
}
#endif /* EXCLUDE_GNSS_UBLOX */

static bool GNSS_fix_cache = false;

bool isValidGNSSFix()
{
  return GNSS_fix_cache;
}

byte GNSS_setup() {

  gnss_id_t gnss_id = GNSS_MODULE_NONE;

  SoC->swSer_begin(SERIAL_IN_BR);

  gnss_id = gnss_id == GNSS_MODULE_NONE ?
            (gnss_chip = &generic_nmea_ops, gnss_chip->probe()) : gnss_id;

  if (gnss_id == GNSS_MODULE_NONE) {

#if !defined(EXCLUDE_GNSS_UBLOX) && defined(ENABLE_UBLOX_RFS)
    if (hw_info.model == SOFTRF_MODEL_PRIME_MK2 ||
        hw_info.model == SOFTRF_MODEL_PRIME_MK3) {

      byte version = ublox_version();

      if (version == GNSS_MODULE_U6 ||
          version == GNSS_MODULE_U7 ||
          version == GNSS_MODULE_U8) {

        Serial.println(F("WARNING: Misconfigured UBLOX GNSS detected!"));
        Serial.print(F("Reset to factory default state: "));

        ublox_factory_reset();

        gnss_id = generic_nmea_ops.probe();

        if (gnss_id == GNSS_MODULE_NONE) {
          Serial.println(F("FAILURE"));
          return (byte) gnss_id;
        }
        Serial.println(F("SUCCESS"));
      } else {
        return (byte) gnss_id;
      }
    } else
#endif /* EXCLUDE_GNSS_UBLOX && ENABLE_UBLOX_RFS */

        return (byte) gnss_id;
  }

#if !defined(EXCLUDE_GNSS_UBLOX)
  gnss_id = gnss_id == GNSS_MODULE_NMEA ?
            (gnss_chip = &ublox_ops,  gnss_chip->probe()) : gnss_id;
#endif /* EXCLUDE_GNSS_UBLOX */

  gnss_chip = gnss_id == GNSS_MODULE_NMEA ? &generic_nmea_ops : gnss_chip;

  if (gnss_chip) gnss_chip->setup();

 
#if defined(USE_NMEA_CFG)
  C_NMEA_Source = settings->nmea_out;
#endif /* USE_NMEA_CFG */

  return (byte) gnss_id;
}

void GNSS_loop()
{
  PickGNSSFix();

  /*
   * Both GGA and RMC NMEA sentences are required.
   * No fix when any of them is missing or lost.
   * Valid date is critical for legacy protocol (only).
   */
  GNSS_fix_cache = gnss.location.isValid()               &&
                   gnss.altitude.isValid()               &&
                   gnss.date.isValid()                   &&
                  (gnss.location.age() <= NMEA_EXP_TIME) &&
                  (gnss.altitude.age() <= NMEA_EXP_TIME) &&
                  (gnss.date.age()     <= NMEA_EXP_TIME);

  GNSSTimeSync();

  if (gnss_chip) gnss_chip->loop();
}

void GNSS_fini()
{
  if (gnss_chip) gnss_chip->fini();
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
#if 0
    Serial.print("Valid: ");
    Serial.println(gnss.time.isValid());
    Serial.print("isUpdated: ");
    Serial.println(gnss.time.isUpdated());
    Serial.print("age: ");
    Serial.println(gnss.time.age());
#endif
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

  /*
   * Check SW/HW UARTs, USB and BT for data
   * WARNING! Make use only one input source at a time.
   */
  while (true) 
  {
#if !defined(USE_NMEA_CFG)
    if (Serial_GNSS_In.available() > 0) {
      c = Serial_GNSS_In.read();
    } else if (Serial.available() > 0) {
      c = Serial.read();
    }
    else if (SoC->Bluetooth_ops && SoC->Bluetooth_ops->available() > 0) 
    { 
      c = SoC->Bluetooth_ops->read();

      /*
       * Don't forget to disable echo:
       *
       * stty raw -echo -F /dev/rfcomm0
       *
       * GNSS input becomes garbled otherwise
       */

      // Serial.write((char) c);
      /* Ignore Bluetooth input for a while */
      // break;
#else
    /*
     * Give priority to control channels over default GNSS input source on
     * 'Dongle', 'Retro', 'Uni', 'Mini', 'Badge', 'Academy' and 'Lego' Editions
     */

    /* Bluetooth input is first */
    if (SoC->Bluetooth_ops && SoC->Bluetooth_ops->available() > 0) 
    {
      c = SoC->Bluetooth_ops->read();

      C_NMEA_Source = NMEA_BLUETOOTH;

    /* USB input is second */
    }

    else if (Serial_GNSS_In.available() > 0) 
    {
      c = Serial_GNSS_In.read();
#endif /* USE_NMEA_CFG */
    }
    else
    {
      /* return back if no input data */
      break;
    }

    if (c == -1) {
      /* retry */
      continue;
    }

    if (isPrintable(c) || c == '\r' || c == '\n') {
      GNSSbuf[GNSS_cnt] = c;
    } else {
      /* ignore */
      continue;
    }


    isValidSentence = gnss.encode(GNSSbuf[GNSS_cnt]);
    if (GNSSbuf[GNSS_cnt] == '\r' && isValidSentence) 
    {
      for (ndx = GNSS_cnt - 4; ndx >= 0; ndx--) { // skip CS and *
        if (settings->nmea_g && (GNSSbuf[ndx] == '$') && (GNSSbuf[ndx+1] == 'G')) {

          size_t write_size = GNSS_cnt - ndx + 1;

#if 0
          if (!strncmp((char *) &GNSSbuf[ndx+3], "GGA,", strlen("GGA,"))) {
            GGA_Stop_Time_Marker = millis();

            Serial.print("GGA Start: ");
            Serial.print(GGA_Start_Time_Marker);
            Serial.print(" Stop: ");
            Serial.print(GGA_Stop_Time_Marker);
            Serial.print(" gnss.time.age: ");
            Serial.println(gnss.time.age());

          }
#endif

          /*
           * Work around issue with "always 0.0,M" GGA geoid separation value
           * given by some Chinese GNSS chipsets
           */
#if defined(USE_NMEALIB)
          if (hw_info.model == SOFTRF_MODEL_PRIME_MK2 &&
              !strncmp((char *) &GNSSbuf[ndx+3], "GGA,", strlen("GGA,")) &&
              gnss.separation.meters() == 0.0) {
            NMEA_GGA();
          }
          else
#endif
          {
            NMEA_Out(settings->nmea_out, &GNSSbuf[ndx], write_size, true);
          }

          break;
        }
      }

#if defined(USE_NMEA_CFG)
      NMEA_Process_SRF_SKV_Sentences();
#endif /* USE_NMEA_CFG */
    }

    if (GNSSbuf[GNSS_cnt] == '\n' || GNSS_cnt == sizeof(GNSSbuf)-1) 
    {
      GNSS_cnt = 0;
    }
    else 
    {
      GNSS_cnt++;
      yield();
    }
  }
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


