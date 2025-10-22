

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

#include <SoftwareSerial.h>


EspSoftwareSerial::UART Serial_GNSS_In;

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
#define GNSS_FLUSH()        Serial_GNSS_Out.flush()
#endif

unsigned long GNSSTimeSyncMarker = 0;
volatile unsigned long PPS_TimeMarker = 0;

const gnss_chip_ops_t *gnss_chip = NULL;
extern const gnss_chip_ops_t goke_ops; /* forward declaration */

boolean gnss_set_sucess = false ;
TinyGPSPlus gnss;  // Create an Instance of the TinyGPS++ object called gnss

uint8_t GNSSbuf[250]; // at least 3 lines of 80 characters each
                      // and 40+30*N bytes for "UBX-MON-VER" payload

int GNSS_cnt           = 0;
uint16_t FW_Build_Year = 2000 + ((__DATE__[ 9]) - '0') * 10 + ((__DATE__[10]) - '0');

const char *GNSS_name[] = {
  [GNSS_MODULE_NONE]    = "NONE",
  [GNSS_MODULE_NMEA]    = "NMEA",
  [GNSS_MODULE_U6]      = "U6",
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
 /* CFG-CFG */
const uint8_t factoryUBX[] PROGMEM = { 0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0xFF,
                                       0xFB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0xFF, 0xFF, 0x00, 0x00, 0x17, 0x2B, 0x7E } ;
#if defined(USE_U10_EXT)
static byte _ulox_version_cache = GNSS_MODULE_NMEA;
const uint8_t CFG_SIGNAL[] PROGMEM = { 0xB5, 0x62, 0x06, 0x8A, 0x4A,
                                       0x00, 0x01, 0x01, 0x00, 0x00,
                      /* GPS  L1C/A */ 0x01, 0x00, 0x31, 0x10, 0x01,
                      /* SBAS L1C/A */ 0x05, 0x00, 0x31, 0x10, 0x01,
                      /* Galileo E1 */ 0x07, 0x00, 0x31, 0x10, 0x01,
                      /* BeiDou B1I */ 0x0D, 0x00, 0x31, 0x10, 0x00,
                      /* BeiDou B1C */ 0x0F, 0x00, 0x31, 0x10, 0x01,
                      /* QZSS L1C/A */ 0x12, 0x00, 0x31, 0x10, 0x01,
                      /* QZSS   L1S */ 0x14, 0x00, 0x31, 0x10, 0x01,
                      /* GLONASS L1 */ 0x18, 0x00, 0x31, 0x10, 0x01,
                      /* GPS     EN */ 0x1F, 0x00, 0x31, 0x10, 0x01,
                      /* SBAS    EN */ 0x20, 0x00, 0x31, 0x10, 0x01,
                      /* Galileo EN */ 0x21, 0x00, 0x31, 0x10, 0x01,
                      /* BeiDou  EN */ 0x22, 0x00, 0x31, 0x10, 0x01,
                      /* QZSS    EN */ 0x24, 0x00, 0x31, 0x10, 0x01,
                      /* GLONASS EN */ 0x25, 0x00, 0x31, 0x10, 0x01,
                                       0xA9, 0xC3 } ;
#endif /* USE_U10_EXT */

uint8_t makeUBXCFG(uint8_t cl, uint8_t id, uint8_t msglen, const uint8_t *msg)
{
  if (msglen > (sizeof(GNSSbuf) - 8) ) {
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

  for (int i = 2; i < 6; i++) {
    GNSSbuf[6+msglen] += GNSSbuf[i];
    GNSSbuf[7+msglen] += GNSSbuf[6+msglen];
  }

  for (int i = 0; i < msglen; i++) {
    GNSSbuf[6+i] = pgm_read_byte(&msg[i]);
    GNSSbuf[6+msglen] += GNSSbuf[6+i];
    GNSSbuf[7+msglen] += GNSSbuf[6+msglen];
  }
  return (msglen + 8);
}

// Send a byte array of UBX protocol to the GPS
static void sendUBX(const uint8_t *MSG, uint8_t len) 
{
  //for (int i = 0; i < len; i++) {
  //  Serial_GNSS_Out.write( MSG[i]);
  //  GNSS_DEBUG_PRINT(MSG[i], HEX);
  //}
//  Serial_GNSS_Out.println();
}

// Calculate expected UBX ACK packet and parse UBX response from GPS
static boolean getUBX_ACK(uint8_t cl, uint8_t id) 
{
  uint8_t b;
  uint8_t ackByteID = 0;
  uint8_t ackPacket[2] = {cl, id};
  unsigned long startTime = millis();
  GNSS_DEBUG_PRINT(F(" * Reading ACK response: "));

  // Construct the expected ACK packet
  makeUBXCFG(0x05, 0x01, 2, ackPacket);

  while (1) {

    // Test for success
    if (ackByteID > 9) {
      // All packets in order!
      GNSS_DEBUG_PRINTLN(F(" (SUCCESS!)"));
      return true;
    }

    // Timeout if no valid response in 2 seconds
    if (millis() - startTime > 2000) {
      GNSS_DEBUG_PRINTLN(F(" (FAILED!)"));
      return false;
    }

    // Make sure data is available to read
    if (Serial_GNSS_In.available()) {
      b = Serial_GNSS_In.read();

      // Check that bytes arrive in sequence as per expected ACK packet
      if (b == GNSSbuf[ackByteID]) {
        ackByteID++;
        GNSS_DEBUG_PRINT(b, HEX);
      }
      else {
        ackByteID = 0;  // Reset and look again, invalid order
      }
    }
    yield();
  }
}

static void setup_UBX()
{
  uint8_t msglen;


  GNSS_DEBUG_PRINTLN(F("Airborne <2g navigation mode: "));

  // Set the navigation mode (Airborne, < 2g)
  msglen = makeUBXCFG(0x06, 0x24, sizeof(setNav5), setNav5);
  //!!sendUBX(GNSSbuf, msglen);
  gnss_set_sucess = getUBX_ACK(0x06, 0x24);

  if (!gnss_set_sucess) {
    GNSS_DEBUG_PRINTLN(F("WARNING: Unable to set airborne <2g navigation mode."));
  }

  GNSS_DEBUG_PRINTLN(F("Switching off NMEA GLL: "));

  msglen = makeUBXCFG(0x06, 0x01, sizeof(setGLL), setGLL);
  //!!sendUBX(GNSSbuf, msglen);
  gnss_set_sucess = getUBX_ACK(0x06, 0x01);

  if (!gnss_set_sucess) {
    GNSS_DEBUG_PRINTLN(F("WARNING: Unable to turn off NMEA GLL."));
  }

  GNSS_DEBUG_PRINTLN(F("Switching off NMEA GSV: "));

  msglen = makeUBXCFG(0x06, 0x01, sizeof(setGSV), setGSV);
  //!!sendUBX(GNSSbuf, msglen);
  gnss_set_sucess = getUBX_ACK(0x06, 0x01);

  if (!gnss_set_sucess) {
    GNSS_DEBUG_PRINTLN(F("WARNING: Unable to turn off NMEA GSV."));
  }

  GNSS_DEBUG_PRINTLN(F("Switching off NMEA VTG: "));

  msglen = makeUBXCFG(0x06, 0x01, sizeof(setVTG), setVTG);
  //!!sendUBX(GNSSbuf, msglen);
  gnss_set_sucess = getUBX_ACK(0x06, 0x01);

  if (!gnss_set_sucess) {
    GNSS_DEBUG_PRINTLN(F("WARNING: Unable to turn off NMEA VTG."));
  }

#if defined(NMEA_TCP_SERVICE)
  if (settings->nmea_out != NMEA_TCP)
#endif /* NMEA_TCP_SERVICE */
  {
    GNSS_DEBUG_PRINTLN(F("Switching off NMEA GSA: "));

    msglen = makeUBXCFG(0x06, 0x01, sizeof(setGSA), setGSA);
    //!!sendUBX(GNSSbuf, msglen);
    gnss_set_sucess = getUBX_ACK(0x06, 0x01);

    if (!gnss_set_sucess) {
      GNSS_DEBUG_PRINTLN(F("WARNING: Unable to turn off NMEA GSA."));
    }
  }

#if defined(USE_U10_EXT)
  /* Disable BeiDou B1I. Enable BeiDou B1C and GLONASS L1OF */
  if (_ulox_version_cache == GNSS_MODULE_U10) 
  {
    for (int i = 0; i < sizeof(CFG_SIGNAL); i++) 
    {
      Serial_GNSS_Out.write(pgm_read_byte(&CFG_SIGNAL[i]));
    }

    delay(600);
  }
#endif /* USE_U10_EXT */
}

/* ------ BEGIN -----------  https://github.com/Black-Thunder/FPV-Tracker */

enum ubloxState{ WAIT_SYNC1, WAIT_SYNC2, GET_CLASS, GET_ID, GET_LL, GET_LH, GET_DATA, GET_CKA, GET_CKB };

ubloxState ubloxProcessDataState = WAIT_SYNC1;

unsigned short ubloxExpectedDataLength;
unsigned short ubloxClass, ubloxId;
unsigned char  ubloxCKA, ubloxCKB;

// process serial data
// data is stored inside #GNSSbuf, data size inside #GNSS_cnt
// warning : if #GNSSbuf is too short, data is truncated.
static int ubloxProcessData(unsigned char data) {
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

static byte ublox_version() {
  byte rval = GNSS_MODULE_NMEA;
  unsigned long startTime = millis();

  uint8_t msglen = makeUBXCFG(0x0A, 0x04, 0, NULL); // MON-VER
  //!!sendUBX(GNSSbuf, msglen);

  // Get the message back from the GPS
  GNSS_DEBUG_PRINT(F(" * Reading response: "));

  while ((millis() - startTime) < 2000 ) 
  {

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

            // UBX-MON-VER data description
            // uBlox 6  - page 166 : https://www.u-blox.com/sites/default/files/products/documents/u-blox6_ReceiverDescrProtSpec_%28GPS.G6-SW-10018%29_Public.pdf
            // uBlox 7  - page 153 : https://www.u-blox.com/sites/default/files/products/documents/u-blox7-V14_ReceiverDescriptionProtocolSpec_%28GPS.G7-SW-12001%29_Public.pdf
            // uBlox M8 - page 300 : https://www.u-blox.com/sites/default/files/products/documents/u-blox8-M8_ReceiverDescrProtSpec_%28UBX-13003221%29_Public.pdf

            Serial.print(F("INFO: GNSS module HW version: "));
            Serial.println((char *) &GNSSbuf[30]);

            Serial.print(F("INFO: GNSS module FW version: "));
            Serial.println((char *) &GNSSbuf[0]);

#ifdef DO_GNSS_DEBUG
            for(unsigned i = 30 + 10; i < GNSS_cnt; i+=30) {
              Serial.print(F("INFO: GNSS module extension: "));
              Serial.println((char *) &GNSSbuf[i]);
            }
#endif

            if (GNSSbuf[33] == '4')
              rval = GNSS_MODULE_U6;
            else if (GNSSbuf[33] == '7')
              rval = GNSS_MODULE_U7;
            else if (GNSSbuf[33] == '8')
              rval = GNSS_MODULE_U8;
            else if (GNSSbuf[32] == '1' && GNSSbuf[33] == '9')
              rval = GNSS_MODULE_U9;
            else if (GNSSbuf[33] == 'A')
              rval = GNSS_MODULE_U10;

#if defined(USE_U10_EXT)
            _ulox_version_cache = rval;
#endif /* USE_U10_EXT */
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
  return(hw_info.model == FLYRF_MODEL_STANDALONE && hw_info.revision == 0 ?
         GNSS_MODULE_NMEA : (gnss_id_t) ublox_version());
}

static bool ublox_setup()
{
#if 1
  // Set the navigation mode (Airborne, 1G)
  // Turning off some GPS NMEA sentences on the uBlox modules
  setup_UBX();
#endif

  return true;
}

static void ublox_loop()
{

}

static void ublox_fini()
{
  // Controlled Software reset
  for (int i = 0; i < sizeof(CFG_RST); i++) {
    Serial_GNSS_Out.write(pgm_read_byte(&CFG_RST[i]));
  }

  delay(hw_info.gnss == GNSS_MODULE_U8 ? 1000 : 600);

  // power off until wakeup call
  for (int i = 0; i < sizeof(RXM_PMREQ_OFF); i++) {
    Serial_GNSS_Out.write(pgm_read_byte(&RXM_PMREQ_OFF[i]));
  }
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
  // reset GPS to factory settings
  for (int i = 0; i < sizeof(factoryUBX); i++) {
    Serial_GNSS_Out.write(pgm_read_byte(&factoryUBX[i]));
  }

  delay(600);

  // Cold Start (Forced Watchdog)
  for (int i = 0; i < sizeof(CFG_RST_COLD); i++) {
    Serial_GNSS_Out.write(pgm_read_byte(&CFG_RST_COLD[i]));
  }

  delay(1000);
}
#endif /* EXCLUDE_GNSS_UBLOX */

#if !defined(EXCLUDE_GNSS_AT65)
static gnss_id_t at65_probe()
{
  /* Firmware version request */
  return nmea_handshake("$PCAS06,0*1B\r\n", "$GPTXT,01,01,02", false) ?
                        GNSS_MODULE_AT65 : GNSS_MODULE_NMEA;
}

static bool at65_setup()
{
#if !defined(EXCLUDE_LOG_GNSS_VERSION)
  Serial_GNSS_Out.write("$PCAS06,0*1B\r\n");

  int i=0;
  char c;
  unsigned long start_time = millis();

  /* take response into buffer */
  while ((millis() - start_time) < 2000) {

    c = Serial_GNSS_In.read();

    if (isPrintable(c) || c == '\r' || c == '\n') {
      if (i >= sizeof(GNSSbuf) - 1) break;
      GNSSbuf[i++] = c;
    } else {
      /* ignore */
      continue;
    }

    if (c == '\n') break;
  }

  GNSSbuf[i] = 0;

  size_t len = strlen((char *) &GNSSbuf[0]);

  if (len > 19) {
    for (int i=19; i < len; i++) {
      if (GNSSbuf[i] == '*') {
        GNSSbuf[i] = 0;
      }
    }
    Serial.print(F("INFO: GNSS module FW version: "));
    Serial.println((char *) &GNSSbuf[19]);
  }

  delay(250);
#endif

  return true;
}

static void at65_loop()
{

}

static void at65_fini()
{

}

const gnss_chip_ops_t at65_ops = {
  at65_probe,
  at65_setup,
  at65_loop,
  at65_fini,
  70 /* GGA */, 135 /* RMC */
};
#endif /* EXCLUDE_GNSS_AT65 */

#if !defined(EXCLUDE_GNSS_UC65)
static gnss_id_t uc65_probe()
{
  /* Firmware version request */
  return nmea_handshake("$PDTINFO\r\n", "$PDTINFO,FB2S UM600", false) ?
                        GNSS_MODULE_UC65 : GNSS_MODULE_NMEA;
}

static bool uc65_setup()
{
#if !defined(EXCLUDE_LOG_GNSS_VERSION)
  while (Serial_GNSS_In.available() > 0) { Serial_GNSS_In.read(); }

  Serial_GNSS_Out.write("$PDTINFO\r\n");

  int i=0;
  char c;
  unsigned long start_time = millis();

  /* take response into buffer */
  while ((millis() - start_time) < 2000) {

    c = Serial_GNSS_In.read();

    if (isPrintable(c) || c == '\r' || c == '\n') {
      if (i >= sizeof(GNSSbuf) - 1) break;
      GNSSbuf[i++] = c;
    } else {
      /* ignore */
      continue;
    }

    if (c == '\n') break;
  }

  GNSSbuf[i] = 0;

  size_t len = strlen((char *) &GNSSbuf[0]);

  if (len > 14) {
    for (int i=14; i < len; i++) {
      if (GNSSbuf[i] == '*') {
        GNSSbuf[i] = 0;
      }
    }
    Serial.print(F("INFO: GNSS module FW version: "));
    Serial.println((char *) &GNSSbuf[14]);
  }

  delay(250);
#endif

  return true;
}

static void uc65_loop()
{

}

static void uc65_fini()
{

}

const gnss_chip_ops_t uc65_ops = {
  uc65_probe,
  uc65_setup,
  uc65_loop,
  uc65_fini,
  0 /* GGA */, 0 /* RMC */
};
#endif /* EXCLUDE_GNSS_UC65 */

static bool GNSS_fix_cache = false;

bool isValidGNSSFix()
{
  
   return GNSS_fix_cache;
 
}

byte GNSS_setup() 
{

  gnss_id_t gnss_id = GNSS_MODULE_NONE;

  Serial_GNSS_In.begin(SERIAL_IN_BR, EspSoftwareSerial::SWSERIAL_8N1, SOC_GPIO_PIN_GNSS_RX, SOC_GPIO_PIN_GNSS_TX, false, 256);
  Serial_GNSS_In.enableIntTx(false);

 
  gnss_id = gnss_id == GNSS_MODULE_NONE ?
            (gnss_chip = &generic_nmea_ops, gnss_chip->probe()) : gnss_id;

  if (gnss_id == GNSS_MODULE_NONE) 
  {
        return (byte) gnss_id;
  }

#if !defined(EXCLUDE_GNSS_UBLOX)
  gnss_id = gnss_id == GNSS_MODULE_NMEA ?
            (gnss_chip = &ublox_ops,  gnss_chip->probe()) : gnss_id;
#endif /* EXCLUDE_GNSS_UBLOX */
#if !defined(EXCLUDE_GNSS_MTK)
  gnss_id = gnss_id == GNSS_MODULE_NMEA ?
            (gnss_chip = &mtk_ops,    gnss_chip->probe()) : gnss_id;
#endif /* EXCLUDE_GNSS_MTK */
#if !defined(EXCLUDE_GNSS_GOKE)
  gnss_id = gnss_id == GNSS_MODULE_NMEA ?
            (gnss_chip = &goke_ops,   gnss_chip->probe()) : gnss_id;
#endif /* EXCLUDE_GNSS_GOKE */
#if !defined(EXCLUDE_GNSS_AT65)
  gnss_id = gnss_id == GNSS_MODULE_NMEA ?
            (gnss_chip = &at65_ops,   gnss_chip->probe()) : gnss_id;
#endif /* EXCLUDE_GNSS_AT65 */
#if !defined(EXCLUDE_GNSS_UC65)
  gnss_id = gnss_id == GNSS_MODULE_NMEA ?
            (gnss_chip = &uc65_ops,   gnss_chip->probe()) : gnss_id;
#endif /* EXCLUDE_GNSS_UC65 */

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
  while (true) {
#if !defined(USE_NMEA_CFG)
    if (Serial_GNSS_In.available() > 0) {
      c = Serial_GNSS_In.read();
    } else if (Serial.available() > 0) {
      c = Serial.read();
    } else if (SoC->Bluetooth_ops && SoC->Bluetooth_ops->available() > 0) {
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
    if (SoC->Bluetooth_ops && SoC->Bluetooth_ops->available() > 0) {
      c = SoC->Bluetooth_ops->read();

      C_NMEA_Source = NMEA_BLUETOOTH;

    /* USB input is second */
    }
    else if (Serial_GNSS_In.available() > 0) {
      c = Serial_GNSS_In.read();
#endif /* USE_NMEA_CFG */
    } else {
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

#if defined(ENABLE_GNSS_STATS)
    if ( (GNSS_cnt >= 5) &&
         (GNSSbuf[GNSS_cnt-5] == '$') &&
         (GNSSbuf[GNSS_cnt-4] == 'G') &&
        ((GNSSbuf[GNSS_cnt-3] == 'P') || (GNSSbuf[GNSS_cnt-3] == 'N'))) {
      if ( (GNSSbuf[GNSS_cnt-2] == 'G') &&
           (GNSSbuf[GNSS_cnt-1] == 'G') &&
           (GNSSbuf[GNSS_cnt-0] == 'A')) {
        gnss_stats.gga_time_ms = millis();
        gnss_stats.gga_count++;
      } else if ( (GNSSbuf[GNSS_cnt-2] == 'R') &&
                  (GNSSbuf[GNSS_cnt-1] == 'M') &&
                  (GNSSbuf[GNSS_cnt-0] == 'C')) {
        gnss_stats.rmc_time_ms = millis();
        gnss_stats.rmc_count++;
      }
    }
#endif /* ENABLE_GNSS_STATS */

    isValidSentence = gnss.encode(GNSSbuf[GNSS_cnt]);
    if (GNSSbuf[GNSS_cnt] == '\r' && isValidSentence) {
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
          if (hw_info.model == FLYRF_MODEL_STANDALONE && !strncmp((char *) &GNSSbuf[ndx+3], "GGA,", strlen("GGA,")) && gnss.separation.meters() == 0.0) //!
          {
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

#if defined(ENABLE_D1090_INPUT)
    if (GNSSbuf[GNSS_cnt]   == '\n' &&
        GNSS_cnt             >  1   &&
        GNSSbuf[GNSS_cnt-1] == '\r' &&
        GNSSbuf[GNSS_cnt-2] == ';')
    {
      int i=0;

      if (GNSS_cnt > 16 && GNSSbuf[GNSS_cnt-17] == '*') {
        for (i=0; i<14; i++) {
          if (!isxdigit(GNSSbuf[GNSS_cnt-16+i])) break;
        }
        if (i>=14) {
          D1090_Import(&GNSSbuf[GNSS_cnt-17]);
          GNSS_cnt -= 18;
        }
      } else if (GNSS_cnt > 30 && GNSSbuf[GNSS_cnt-31] == '*') {
        for (i=0; i<28; i++) {
          if (!isxdigit(GNSSbuf[GNSS_cnt-30+i])) break;
        }
        if (i>=28) {
          D1090_Import(&GNSSbuf[GNSS_cnt-31]);
          GNSS_cnt -= 32;
        }
      }
    }
#endif /* ENABLE_D1090_INPUT */

    if (GNSSbuf[GNSS_cnt] == '\n' || GNSS_cnt == sizeof(GNSSbuf)-1) {
      GNSS_cnt = 0;
    } else {
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
