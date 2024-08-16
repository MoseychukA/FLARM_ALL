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

boolean gnss_set_sucess = false ;
TinyGPSPlus gnss;  // Create an Instance of the TinyGPS++ object called gnss

uint8_t GNSSbuf[350]; // at least 3 lines of 80 characters each
                      // and 40+30*N bytes for "UBX-MON-VER" payload

int GNSS_cnt           = 0;
uint16_t FW_Build_Year = 2000 + ((__DATE__[ 9]) - '0') * 10 + ((__DATE__[10]) - '0');

const char *GNSS_name[] = {
  [GNSS_MODULE_NONE]    = "NONE",
  [GNSS_MODULE_NMEA]    = "NMEA",
  [GNSS_MODULE_U6]      = "GPS",
  [GNSS_MODULE_U7]      = "U7",
  [GNSS_MODULE_U8]      = "U8",
  [GNSS_MODULE_U9]      = "U9",
  [GNSS_MODULE_U10]     = "U10",
  [GNSS_MODULE_U11]     = "U11",
  [GNSS_MODULE_MAV]     = "MAV",
  [GNSS_MODULE_SONY]    = "SONY",
  [GNSS_MODULE_AT65]    = "AT65",
  [GNSS_MODULE_MT33]    = "MT33",
  [GNSS_MODULE_GOKE]    = "GOKE",
  [GNSS_MODULE_UC65]    = "UC65"
};

#if defined(ENABLE_GNSS_STATS)
/*
 * Sony: GGA -  24 , RMC -  38
 * L76K: GGA -  70+, RMC - 135+
 * Goke: GGA - 185+, RMC - 265+
 * Neo6: GGA - 138 , RMC -  67
 * MT33: GGA -  48 , RMC - 175
 * UC65: GGA - TBD , RMC - TBD
 */

gnss_stat_t gnss_stats;
#endif /* ENABLE_GNSS_STATS */

bool nmea_handshake(const char *req, const char *resp, bool skipline)
{
  bool rval = false;

  if (resp == NULL || strlen(resp) == 0) 
  {
    return rval;
  }

  // clean any leftovers
  Serial_GNSS_In.flush();

  while (Serial_GNSS_In.available() > 0) { Serial_GNSS_In.read(); }

  unsigned long start_time = millis();
  unsigned long timeout_ms = (req == NULL ? 3000 : 2000) ;

  while ((millis() - start_time) < timeout_ms) 
  {

    while (Serial_GNSS_In.read() != '\n' && (millis() - start_time) < timeout_ms) { yield(); }

    delay(5);

    /* wait for pause after NMEA burst */
    if (req && Serial_GNSS_In.available() > 0) {
      continue;
    } else {
      /* send request */
      if (req) {
       // Serial_GNSS_Out.write((uint8_t *) req, strlen(req));
        GNSS_FLUSH();
      }

      /* skip first line when expected response contains 2 of them */
      if (skipline) 
      {
        start_time = millis();
        while (Serial_GNSS_In.read() != '\n' && (millis() - start_time) < timeout_ms) { yield(); }
      }

      int i=0;
      char c;

      /* take response into buffer */
      while ((millis() - start_time) < timeout_ms) 
      {

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
 /* CFG-CFG */
const uint8_t factoryUBX[] PROGMEM = { 0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0xFF,
                                       0xFB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0xFF, 0xFF, 0x00, 0x00, 0x17, 0x2B, 0x7E } ;


//#if defined(USE_GNSS_PSM)
//static bool gnss_psm_active = false;
//
///* Max Performance Mode (default) */
//const uint8_t RXM_MAXP[] PROGMEM = {0xB5, 0x62, 0x06, 0x11, 0x02, 0x00, 0x08, 0x00, 0x21, 0x91};
//
///* Power Save Mode */
//const uint8_t RXM_PSM[] PROGMEM  = {0xB5, 0x62, 0x06, 0x11, 0x02, 0x00, 0x08, 0x01, 0x22, 0x92};
//#endif /* USE_GNSS_PSM */

uint8_t makeUBXCFG(uint8_t cl, uint8_t id, uint8_t msglen, const uint8_t *msg)
{
  if (msglen > (sizeof(GNSSbuf) - 8) ) 
  {
    msglen = sizeof(GNSSbuf) - 8;
  }

  // Construct the UBX packet
  GNSSbuf[0] = 0xB5;   // header
  GNSSbuf[1] = 0x62;   // header
  GNSSbuf[2] = cl;  // class
  GNSSbuf[3] = id;     // id
  GNSSbuf[4] = msglen; // length
  GNSSbuf[5] = 0x00;

  GNSSbuf[6+msglen] = 0x00; // CK_A
  GNSSbuf[7+msglen] = 0x00; // CK_B

  for (int i = 2; i < 6; i++) 
  {
    GNSSbuf[6+msglen] += GNSSbuf[i];
    GNSSbuf[7+msglen] += GNSSbuf[6+msglen];
  }

  for (int i = 0; i < msglen; i++) 
  {
    GNSSbuf[6+i] = pgm_read_byte(&msg[i]);
    GNSSbuf[6+msglen] += GNSSbuf[6+i];
    GNSSbuf[7+msglen] += GNSSbuf[6+msglen];
  }
  return (msglen + 8);
}

// Send a byte array of UBX protocol to the GPS
static void sendUBX(const uint8_t *MSG, uint8_t len) 
{
 /* for (int i = 0; i < len; i++) {
    Serial_GNSS_Out.write( MSG[i]);
    GNSS_DEBUG_PRINT(MSG[i], HEX);
  }*/
//  Serial_GNSS_Out.println();
}

// Рассчитать ожидаемый пакет UBX ACK и проанализировать ответ UBX от GPS
static boolean getUBX_ACK(uint8_t cl, uint8_t id) 
{
  //uint8_t b;
  //uint8_t ackByteID = 0;
  //uint8_t ackPacket[2] = {cl, id};
  //unsigned long startTime = millis();
  ////GNSS_DEBUG_PRINT(F(" * Reading ACK response: "));

  //// Construct the expected ACK packet
  //makeUBXCFG(0x05, 0x01, 2, ackPacket);

  //while (1) 
  //{

  //  // Test for success
  //  if (ackByteID > 9)
  //  {
  //    // All packets in order!
  //    //GNSS_DEBUG_PRINTLN(F(" (SUCCESS!)"));
  //    return true;
  //  }

  //  // Timeout if no valid response in 2 seconds
  //  if (millis() - startTime > 2000) 
  //  {
  //    //GNSS_DEBUG_PRINTLN(F(" (FAILED!)"));
  //    return false;
  //  }

  //  // Make sure data is available to read
  //  if (Serial_GNSS_In.available()) 
  //  {
  //    b = Serial_GNSS_In.read();

  //    // Check that bytes arrive in sequence as per expected ACK packet
  //    if (b == GNSSbuf[ackByteID]) 
  //    {
  //      ackByteID++;
  //      //GNSS_DEBUG_PRINT(b, HEX);
  //    }
  //    else 
  //    {
  //      ackByteID = 0;  // Reset and look again, invalid order
  //    }
  //  }
  //  yield();
  //}
}

static void setup_UBX()
{
//  uint8_t msglen;
//
//  // Set the navigation mode (Airborne, < 2g)
//  msglen = makeUBXCFG(0x06, 0x24, sizeof(setNav5), setNav5);
//  sendUBX(GNSSbuf, msglen);
//  gnss_set_sucess = getUBX_ACK(0x06, 0x24);
//
//
//  msglen = makeUBXCFG(0x06, 0x01, sizeof(setGLL), setGLL);
//  sendUBX(GNSSbuf, msglen);
//  gnss_set_sucess = getUBX_ACK(0x06, 0x01);
//
//
//  msglen = makeUBXCFG(0x06, 0x01, sizeof(setGSV), setGSV);
//  sendUBX(GNSSbuf, msglen);
//  gnss_set_sucess = getUBX_ACK(0x06, 0x01);
//
//  msglen = makeUBXCFG(0x06, 0x01, sizeof(setVTG), setVTG);
//  sendUBX(GNSSbuf, msglen);
//  gnss_set_sucess = getUBX_ACK(0x06, 0x01);
//
//
//
//#if defined(NMEA_TCP_SERVICE)
//  if (settings->nmea_out != NMEA_TCP)
//#endif /* NMEA_TCP_SERVICE */
//  {
//    
//
//    msglen = makeUBXCFG(0x06, 0x01, sizeof(setGSA), setGSA);
//    sendUBX(GNSSbuf, msglen);
//    gnss_set_sucess = getUBX_ACK(0x06, 0x01);
//
//  }
}

/* ------ BEGIN -----------  https://github.com/Black-Thunder/FPV-Tracker */

enum ubloxState{ WAIT_SYNC1, WAIT_SYNC2, GET_CLASS, GET_ID, GET_LL, GET_LH, GET_DATA, GET_CKA, GET_CKB };

ubloxState ubloxProcessDataState = WAIT_SYNC1;

unsigned short ubloxExpectedDataLength;
unsigned short ubloxClass, ubloxId;
unsigned char  ubloxCKA, ubloxCKB;

// обработка последовательных данных
// данные хранятся внутри #GNSSbuf, размер данных внутри #GNSS_cnt
// предупреждение: если #GNSSbuf слишком короткий, данные будут усечены.
static int ubloxProcessData(unsigned char data) 
{
	int parsed = 0;

	switch (ubloxProcessDataState) 
    {
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
 
  rval = GNSS_MODULE_U6;
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

byte GNSS_setup() 
{

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
//#if !defined(EXCLUDE_GNSS_MTK)
//  gnss_id = gnss_id == GNSS_MODULE_NMEA ?
//            (gnss_chip = &mtk_ops,    gnss_chip->probe()) : gnss_id;
//#endif /* EXCLUDE_GNSS_MTK */
//#if !defined(EXCLUDE_GNSS_GOKE)
//  gnss_id = gnss_id == GNSS_MODULE_NMEA ?
//            (gnss_chip = &goke_ops,   gnss_chip->probe()) : gnss_id;
//#endif /* EXCLUDE_GNSS_GOKE */
//#if !defined(EXCLUDE_GNSS_AT65)
//  gnss_id = gnss_id == GNSS_MODULE_NMEA ?
//            (gnss_chip = &at65_ops,   gnss_chip->probe()) : gnss_id;
//#endif /* EXCLUDE_GNSS_AT65 */
//#if !defined(EXCLUDE_GNSS_UC65)
//  gnss_id = gnss_id == GNSS_MODULE_NMEA ?
//            (gnss_chip = &uc65_ops,   gnss_chip->probe()) : gnss_id;
//#endif /* EXCLUDE_GNSS_UC65 */

  gnss_chip = gnss_id == GNSS_MODULE_NMEA ? &generic_nmea_ops : gnss_chip;

  if (gnss_chip) gnss_chip->setup();

//  if (SOC_GPIO_PIN_GNSS_PPS != SOC_UNUSED_PIN) 
//  {
//    pinMode(SOC_GPIO_PIN_GNSS_PPS, INPUT);
//#if !defined(NOT_AN_INTERRUPT)
//    attachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_GNSS_PPS),
//                    SoC->GNSS_PPS_handler, RISING);
//#else
//    int interrupt_num = digitalPinToInterrupt(SOC_GPIO_PIN_GNSS_PPS);
//    if (interrupt_num != NOT_AN_INTERRUPT) {
//      attachInterrupt(interrupt_num, SoC->GNSS_PPS_handler, RISING);
//    }
//#endif
//  }

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

  if (gnss_chip) gnss_chip->loop();

 // GNSS_fix_cache = true;

}

void GNSS_fini()
{
 /* if (SOC_GPIO_PIN_GNSS_PPS != SOC_UNUSED_PIN) 
  {
#if !defined(NOT_AN_INTERRUPT)
    detachInterrupt(digitalPinToInterrupt(SOC_GPIO_PIN_GNSS_PPS));
#else
    int interrupt_num = digitalPinToInterrupt(SOC_GPIO_PIN_GNSS_PPS);
    if (interrupt_num != NOT_AN_INTERRUPT) {
      detachInterrupt(interrupt_num);
    }
#endif

  }

  if (gnss_chip) gnss_chip->fini();*/
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

  while (Serial_GNSS_In.available() > 0)
      if (gnss.encode(Serial_GNSS_In.read()))
          displayInfo();

  if (millis() > 5000 && gnss.charsProcessed() < 10)
  {
      Serial.println(F("No GPS detected: check wiring."));
      while (true);
  }




  /*
 * Проверьте SW/HW UART, USB и BT на наличие данных
* ВНИМАНИЕ! Используйте только один источник входного сигнала одновременно.
   */
//  while (true) 
//  {
////#if !defined(USE_NMEA_CFG)
////    if (Serial_GNSS_In.available() > 0) {
////      c = Serial_GNSS_In.read();
////    } else if (Serial.available() > 0) {
////      c = Serial.read();
////    } else if (SoC->Bluetooth_ops && SoC->Bluetooth_ops->available() > 0) {
////      c = SoC->Bluetooth_ops->read();
////
////      /*
////       * Don't forget to disable echo:
////       *
////       * stty raw -echo -F /dev/rfcomm0
////       *
////       * GNSS input becomes garbled otherwise
////       */
////
////      // Serial.write((char) c);
////      /* Ignore Bluetooth input for a while */
////      // break;
////#else
////    /*
////     * Give priority to control channels over default GNSS input source on
////     * 'Dongle', 'Retro', 'Uni', 'Mini', 'Badge', 'Academy' and 'Lego' Editions
////     */
////
////    /* Bluetooth input is first */
////    if (SoC->Bluetooth_ops && SoC->Bluetooth_ops->available() > 0) {
////      c = SoC->Bluetooth_ops->read();
////
////      C_NMEA_Source = NMEA_BLUETOOTH;
////
////    /* USB input is second */
////    } else if (SoC->USB_ops && SoC->USB_ops->available() > 0) {
////      c = SoC->USB_ops->read();
////
////      C_NMEA_Source = NMEA_USB;
////
////#if defined(ARDUINO_NUCLEO_L073RZ)
////      /* This makes possible to configure S76x's built-in SONY GNSS from aside */
////      if (hw_info.model == SOFTRF_MODEL_DONGLE) {
////        Serial_GNSS_Out.write(c);
////      }
////#endif
////
////    /* Serial input is third */
////    } else 
////        
////    if (SerialOutput.available() > 0) 
////    {
////      c = SerialOutput.read();
////
////      C_NMEA_Source = NMEA_UART;
////
////#if 0
////      /* This makes possible to configure HTCC-AB02S built-in GOKE GNSS from aside */
////      if (hw_info.model == SOFTRF_MODEL_MINI) {
////        Serial_GNSS_Out.write(c);
////      }
////#endif
////
////    /* Built-in GNSS input */
////    } else 
//        
//    if (Serial_GNSS_In.available() > 0) 
//    {
//    c = Serial_GNSS_In.read();
//    //#endif /* USE_NMEA_CFG */
//    }
//    else 
//    {
//    /* return back if no input data */
//    break;
//    }
//
//    if (c == -1) {
//    /* retry */
//    continue;
//    }
//
//    if (isPrintable(c) || c == '\r' || c == '\n') {
//      GNSSbuf[GNSS_cnt] = c;
//    } else {
//      /* ignore */
//      continue;
//    }
//
//    isValidSentence = gnss.encode(GNSSbuf[GNSS_cnt]);
//    if (GNSSbuf[GNSS_cnt] == '\r' && isValidSentence) 
//    {
//      for (ndx = GNSS_cnt - 4; ndx >= 0; ndx--) 
//      { // skip CS and *
//        if (settings->nmea_g && (GNSSbuf[ndx] == '$') && (GNSSbuf[ndx+1] == 'G')) 
//        {
//
//          size_t write_size = GNSS_cnt - ndx + 1;
//
//#if 0
//          if (!strncmp((char *) &GNSSbuf[ndx+3], "GGA,", strlen("GGA,"))) 
//          {
//            GGA_Stop_Time_Marker = millis();
//
//            Serial.print("GGA Start: ");
//            Serial.print(GGA_Start_Time_Marker);
//            Serial.print(" Stop: ");
//            Serial.print(GGA_Stop_Time_Marker);
//            Serial.print(" gnss.time.age: ");
//            Serial.println(gnss.time.age());
//
//          }
//#endif
//
//          /*
//           * Work around issue with "always 0.0,M" GGA geoid separation value
//           * given by some Chinese GNSS chipsets
//           */
//#if defined(USE_NMEALIB)
//          if (hw_info.model == SOFTRF_MODEL_PRIME_MK2 &&
//              !strncmp((char *) &GNSSbuf[ndx+3], "GGA,", strlen("GGA,")) &&
//              gnss.separation.meters() == 0.0) {
//            NMEA_GGA();
//          }
//          else
//#endif
//          {
//            NMEA_Out(settings->nmea_out, &GNSSbuf[ndx], write_size, true);
//          }
//
//          break;
//        }
//      }
//
//#if defined(USE_NMEA_CFG)
//      NMEA_Process_SRF_SKV_Sentences();
//#endif /* USE_NMEA_CFG */
//    }
//
//
//    if (GNSSbuf[GNSS_cnt] == '\n' || GNSS_cnt == sizeof(GNSSbuf)-1) 
//    {
//      GNSS_cnt = 0;
//    }
//    else 
//    {
//      GNSS_cnt++;
//      yield();
//    }
//  }
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
