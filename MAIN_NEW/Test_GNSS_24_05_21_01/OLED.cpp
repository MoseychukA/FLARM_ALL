
#include "SoC.h"

#if defined(USE_OLED)

#include <Wire.h>

#include "OLED.h"

#include "LED.h"
#include "GNSS.h"
#include "BatteryRF.h"
#include "TimeRF.h"

enum
{
  OLED_PAGE_RADIO,
  OLED_PAGE_OTHER,
#if !defined(EXCLUDE_OLED_BARO_PAGE)
  OLED_PAGE_BARO,
#endif /* EXCLUDE_OLED_BARO_PAGE */
#if !defined(EXCLUDE_IMU)
  OLED_PAGE_IMU,
#endif /* EXCLUDE_IMU */
  OLED_PAGE_COUNT
};


U8X8_OLED_I2C_BUS_TYPE u8x8_i2c(U8X8_PIN_NONE);

U8X8 *u8x8                          = NULL;
uint8_t OLED_flip                   = 0;
static bool OLED_display_titles     = false;

static uint32_t prev_acrfts_counter = (uint32_t) -1;
static uint32_t prev_sats_counter   = (uint32_t) -1;
float prev_sats_lat = (uint32_t)-1;
float prev_sats_lon = (uint32_t)-1;
static uint32_t prev_sats_alt = (uint32_t)-1;

static uint32_t prev_uptime_minutes = (uint32_t) -1;
static int32_t  prev_voltage        = (uint32_t) -1;
static int8_t   prev_fix            = (uint8_t)  -1;

#if !defined(EXCLUDE_OLED_BARO_PAGE)
static int32_t  prev_altitude       = (int32_t)   -10000;
static int32_t  prev_temperature    = (int32_t)   -100;
static uint32_t prev_pressure       = (uint32_t)  -1;
static int32_t  prev_cdr            = (int32_t)   -10000; /* climb/descent rate */
#endif /* EXCLUDE_OLED_BARO_PAGE */

#if !defined(EXCLUDE_MAG)
int32_t MAG_heading                 = 0;
static int32_t  prev_heading        = (int32_t) -10000;
#endif /* EXCLUDE_MAG */

#if !defined(EXCLUDE_IMU)
int32_t IMU_g_x10                   = 0;
static int32_t  prev_g_x10          = (int32_t) -10000;
#endif /* EXCLUDE_IMU */

unsigned long OLEDTimeMarker = 0;

const char SoftRF_text1[]  = "FlyRF";
const char SoftRF_text2[]  = "and";
const char SoftRF_text3[]  = "LilyGO";
const char ID_text[]       = "ID";
const char PROTOCOL_text[] = "PROTOCOL";
const char RX_text[]       = "RX";
const char TX_text[]       = "TX";
const char ACFTS_text[]    = "ACFTS";
const char SATS_text[]     = "SATS";
const char FIX_text[]      = "FIX";
const char UPTIME_text[]   = "UPTIME";
const char BAT_text[]      = "BAT";

#if !defined(EXCLUDE_OLED_BARO_PAGE)
const char ALT_text[]      = "ALT M";
const char TEMP_text[]     = "TEMP C";
const char PRES_text[]     = "PRES MB";
const char CDR_text[]      = "CDR FPM";
#endif /* EXCLUDE_OLED_BARO_PAGE */


#if !defined(EXCLUDE_IMU)
const char G_load_text[]   = "G load";
#endif /* EXCLUDE_IMU */

static const uint8_t Dot_Tile[] = { 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00 };

static uint8_t OLED_current_page = OLED_PAGE_RADIO;
static uint8_t page_count        = OLED_PAGE_COUNT;

byte OLED_setup() 
{

  byte rval = DISPLAY_NONE;
  bool oled_probe = false;

#if defined(plat_oled_probe_func)
  oled_probe = plat_oled_probe_func();
#else
  /* SSD1306 I2C OLED probing */
  Wire.begin();
  Wire.beginTransmission(SSD1306_OLED_I2C_ADDR);
  oled_probe = (Wire.endTransmission() == 0);
#endif /* plat_oled_probe_func */
  if (oled_probe)
  {
    u8x8 = &u8x8_i2c;
    rval = (hw_info.model == SOFTRF_MODEL_MINI     ? DISPLAY_OLED_HELTEC :
            hw_info.model == SOFTRF_MODEL_BRACELET ? DISPLAY_OLED_0_49   :
            DISPLAY_OLED_TTGO);
  }

  if (u8x8) {
    u8x8->begin();
    u8x8->setFlipMode(OLED_flip);
    u8x8->setFont(u8x8_font_chroma48medium8_r);

    switch (rval)
    {
    case DISPLAY_OLED_TTGO:
    case DISPLAY_OLED_HELTEC:
    case DISPLAY_OLED_1_3:
    default:
      uint8_t shift_y = (hw_info.model == SOFTRF_MODEL_DONGLE ? 1 : 0);

      u8x8->draw2x2String( 2, 2 - shift_y, SoftRF_text1);

      if (shift_y) {
        u8x8->drawString   ( 6, 3, SoftRF_text2);
        u8x8->draw2x2String( 2, 4, SoftRF_text3);
      }

      u8x8->drawString   ( 3, 6 + shift_y, SOFTRF_FIRMWARE_VERSION);
      break;
    }
  }

  OLEDTimeMarker = millis();

  return rval;
}

static void OLED_radio()
{
  char buf[16];
  uint32_t disp_value;

  if (!OLED_display_titles) {

    u8x8->clear();

    u8x8->drawString(1, 1, ID_text);

    snprintf (buf, sizeof(buf), "%06X", ThisAircraft.addr);
    u8x8->draw2x2String(0, 2, buf);

    u8x8->drawString(8, 1, PROTOCOL_text);

    //u8x8->draw2x2Glyph(14, 2, Protocol_ID[ThisAircraft.protocol][0]);

    u8x8->drawString(1, 5, RX_text);

    u8x8->drawString(9, 5, TX_text);

    OLED_display_titles = true;
  }
}

static void OLED_other()
{
  char buf[16];
  uint32_t disp_value;

  if (!OLED_display_titles) 
  {

    u8x8->clear();

    /*u8x8->drawString( 1, 1, ACFTS_text);

    u8x8->drawString( 7, 1, SATS_text);

    u8x8->drawString(12, 1, FIX_text);

    u8x8->drawString( 1, 5, UPTIME_text);

    u8x8->drawString(12, 5, BAT_text);*/

   /* u8x8->drawTile  (4, 6, 1, (uint8_t *) Dot_Tile);
    u8x8->drawTile  (4, 7, 1, (uint8_t *) Dot_Tile);

    u8x8->drawGlyph (13, 7, '.');*/

    //u8x8->setDrawColor(0);// Black
    //u8x8->drawRBox(0, 3, 100, 13, 0);
    //u8x8->drawRBox(0, 17, 100, 13, 0);
   // u8x8->setDrawColor(1);

    u8x8->setFont(u8x8_font_chroma48medium8_r);

    u8x8->draw2x2String(0, 0, "Sat:");
    u8x8->draw2x2String(0, 2, "Lat:");
    u8x8->draw2x2String(0, 4, "Lon:");
    u8x8->draw2x2String(0, 6, "Alt:");
  
    prev_sats_counter  = (uint32_t) -1;
    prev_sats_lat      = (uint32_t)-1;
    prev_sats_lon      = (uint32_t)-1;
    prev_sats_alt      = (uint32_t)-1;

    prev_fix            = (uint8_t)  -1;
    prev_uptime_minutes = (uint32_t) -1;
    prev_voltage        = (uint32_t) -1;

    OLED_display_titles = true;
  }

 
  uint32_t sats_counter   = gnss.satellites.value();
  float latitude_oled     = gnss.location.lat();
  float longitude_oled    = gnss.location.lng();
  uint32_t altitude_oled  = gnss.altitude.meters();



  uint8_t  fix            = (uint8_t) isValidGNSSFix();
  uint32_t uptime_minutes = UpTime.minutes; 
  int32_t  voltage        = Battery_voltage() > BATTERY_THRESHOLD_INVALID ?
                              (int) (Battery_voltage() * 10.0) : 0;



  if (prev_sats_counter != sats_counter) 
  {
    disp_value = sats_counter > 99 ? 99 : sats_counter;
    itoa(disp_value, buf, 10);

    if (disp_value < 10) 
    {
      strcat_P(buf,PSTR(" "));
    }

    u8x8->draw2x2String(8, 0, buf);
    prev_sats_counter = sats_counter;
  }

  if (prev_sats_lat != latitude_oled)
  {
      dtostrf(latitude_oled,6,4, buf);

    /*  if (altitude_oled < 10)
      {
          strcat_P(buf, PSTR(" "));
      }*/

      u8x8->drawString(8, 3, buf);
      prev_sats_lat = latitude_oled;
  }

  if (prev_sats_lon != longitude_oled)
  {
      dtostrf(longitude_oled,6,4, buf);

    /*  if (altitude_oled < 10)
      {
          strcat_P(buf, PSTR(" "));
      }*/

      u8x8->drawString(8, 5, buf);
      prev_sats_lon = longitude_oled;
  }

  if (prev_sats_alt != altitude_oled)
  {
      itoa(altitude_oled, buf, 10);

      if (altitude_oled < 10)
      {
          strcat_P(buf, PSTR(" "));
      }

      u8x8->draw2x2String(8, 6, buf);
      prev_sats_alt = altitude_oled;
  }



  //if (prev_fix != fix) 
  //{
  //  u8x8->draw2x2Glyph(12, 2, fix > 0 ? '+' : '-');
  //  prev_fix = fix;
  //}

  //if (prev_uptime_minutes != uptime_minutes) 
  //{
  //  disp_value = UpTime.hours; /* 0-23 */
  //  if (disp_value < 10) {
  //    buf[0] = '0';
  //    itoa(disp_value, buf+1, 10);
  //  } else {
  //    itoa(disp_value, buf, 10);
  //  }

  //  u8x8->draw2x2String(0, 6, buf);

  //  disp_value = uptime_minutes;
  //  if (disp_value < 10) {
  //    buf[0] = '0';
  //    itoa(disp_value, buf+1, 10);
  //  } else {
  //    itoa(disp_value, buf, 10);
  //  }

  //  u8x8->draw2x2String(5, 6, buf);

  //  prev_uptime_minutes = uptime_minutes;
  //}

  //if (prev_voltage != voltage) 
  //{
  //  if (voltage) {
  //    disp_value = voltage / 10;
  //    disp_value = disp_value > 9 ? 9 : disp_value;
  //    u8x8->draw2x2Glyph(11, 6, '0' + disp_value);

  //    disp_value = voltage % 10;

  //    u8x8->draw2x2Glyph(14, 6, '0' + disp_value);
  //  } else {
  //    u8x8->draw2x2Glyph(11, 6, 'N');
  //    u8x8->draw2x2Glyph(14, 6, 'A');
  //  }
  //  prev_voltage = voltage;
  //}

  /*
  
  
          u8g2->setDrawColor(0);// Black
          u8g2->drawRBox(0, 3, 100, 13, 0);
          u8g2->drawRBox(0, 17, 100, 13, 0);
          u8g2->setDrawColor(1);
          u8g2->drawStr(0, 15, "Lat: ");
          u8g2->drawStr(0, 30, "Lon: ");
          sprintf(s_lat, "%.5f", latitude);
          sprintf(s_lon, "%.5f", longitude);
          u8g2->drawStr(30, 15, s_lat);
          u8g2->drawStr(30, 30, s_lon);

          uint8_t num_sat = nmea.getNumSatellites();
          Settings.setNumSat(num_sat);
          char s[3];
          sprintf(s, "%d", num_sat);
          Serial.print("No Fix - ");
          Serial.print("Num. satellites: ");
          Serial.println(num_sat);

          u8g2->setDrawColor(0);// Black
          u8g2->drawRBox(62, 32, 20, 14, 0);
          u8g2->setDrawColor(1);
          u8g2->drawStr(0, 45, "Num.sat: ");
          u8g2->drawStr(65, 45, s);

          u8g2->setDrawColor(0);// Black
          u8g2->drawRBox(45, 48, 60, 13, 0);
          u8g2->setDrawColor(1);
          sprintf(s_alt, "%.1f", altitude);
          u8g2->drawStr(0, 60, "Alt m: ");
          u8g2->drawStr(50, 60, s_alt);
          u8g2->sendBuffer();
  
  
  
  
  */






}

#if !defined(EXCLUDE_OLED_BARO_PAGE)
static void OLED_baro()
{
  char buf[16];

  if (!OLED_display_titles) 
  {

    u8x8->clear();

    u8x8->drawString( 2, 1, ALT_text);

    u8x8->drawString( 10, 1, TEMP_text);

    u8x8->drawString( 1, 5, PRES_text);

    u8x8->drawString( 9, 5, CDR_text);

    prev_altitude     = (int32_t)   -10000;
    prev_temperature  = prev_altitude;
    prev_pressure     = (uint32_t)  -1;
    prev_cdr          = prev_altitude;

    OLED_display_titles = true;
  }

  int32_t cdr         = ThisAircraft.vs;        /* feet per minute */

  if (prev_cdr != cdr) 
  {
    int disp_value = constrain(cdr, -999, 999);
    snprintf(buf, sizeof(buf), "%3d", abs(disp_value));
    u8x8->drawGlyph    ( 9, 6, disp_value < 0 ? '_' : ' ');
    u8x8->draw2x2String(10, 6, buf);
    prev_cdr = cdr;
  }
}
#endif /* EXCLUDE_OLED_BARO_PAGE */

#if !defined(EXCLUDE_IMU)
static void OLED_imu()
{
  char buf[16];

  if (!OLED_display_titles) {

    u8x8->clear();

    u8x8->drawString( 5, 1, G_load_text);

    prev_g_x10 = (int32_t) -10000;

    OLED_display_titles = true;
  }

  int32_t disp_value = (IMU_g_x10 > 99) ? 99 : IMU_g_x10;

  if (prev_g_x10 != disp_value) {
    snprintf(buf, sizeof(buf), "%01d.%01d", disp_value / 10, disp_value % 10);
    u8x8->draw2x2String(5, 3, buf);
    prev_g_x10 = disp_value;
  }
}
#endif /* EXCLUDE_IMU */

void OLED_loop()
{
  if (u8x8) 
  {
    if (isTimeToOLED()) 
    {
//#if !defined(EXCLUDE_OLED_049)
//      if (hw_info.display == DISPLAY_OLED_0_49) {
//        OLED_049_func();
//      } else
//#endif /* EXCLUDE_OLED_049 */
        switch (OLED_current_page)
        {
        case OLED_PAGE_OTHER:
          OLED_other();
          break;
//#if defined(ENABLE_OLED_TEXT_PAGE)
//        case OLED_PAGE_TEXT:
//          OLED_text();
//          break;
//#endif /* ENABLE_OLED_TEXT_PAGE */
#if !defined(EXCLUDE_OLED_BARO_PAGE)
        case OLED_PAGE_BARO:
          OLED_baro();
          break;
#endif /* EXCLUDE_OLED_BARO_PAGE */
#if !defined(EXCLUDE_IMU)
        case OLED_PAGE_IMU:
          OLED_imu();
          break;
#endif /* EXCLUDE_IMU */
        case OLED_PAGE_RADIO:
        default:
          OLED_other();
          //OLED_radio();
          break;
        }

      OLEDTimeMarker = millis();
    }
  }
}

void OLED_fini(int reason)
{
  if (u8x8) {
    u8x8->clear();
    switch (hw_info.display)
    {
        case DISPLAY_OLED_TTGO:
        case DISPLAY_OLED_HELTEC:
        case DISPLAY_OLED_1_3:
        default:
        break;
    }
  }
}

void OLED_info1()
{
  if (u8x8) {

    u8x8->clear();

    switch (hw_info.display)
    {
        case DISPLAY_OLED_TTGO:
        case DISPLAY_OLED_HELTEC:
        case DISPLAY_OLED_1_3:
        default:
        break;
    }

    delay(3000);
  }
}

void OLED_info2()
{
  if (u8x8) {

    u8x8->clear();

    switch (hw_info.display)
    {
        case DISPLAY_OLED_TTGO:
        case DISPLAY_OLED_HELTEC:
        case DISPLAY_OLED_1_3:
        default:
        break;
    }

    delay(3000);
  }
}

void OLED_info3(int acfts, char *reg, char *mam, char *cn)
{
  if (u8x8) {

    u8x8->clear();

    switch (hw_info.display)
    {
    case DISPLAY_OLED_TTGO:
    case DISPLAY_OLED_HELTEC:
    case DISPLAY_OLED_1_3:
    default:

      if (acfts == -1) {
        u8x8->draw2x2String( 6, 1, "NO");
        u8x8->draw2x2String( 0, 3, "AIRCRAFT");
        u8x8->draw2x2String( 4, 5, "DATA");
      } else {
        char str1[9], str2[9], str3[9], str4[9];

        memset(str1, 0, sizeof(str1));
        memset(str2, 0, sizeof(str2));
        memset(str3, 0, sizeof(str3));
        memset(str4, 0, sizeof(str4));

        snprintf(str1, 6, "%d", acfts);
        strncpy (str2, reg, 8);
        strncpy (str3, mam, 8);
        strncpy (str4,  cn, 8);

        u8x8->draw2x2String( 4, 0, str1);
        u8x8->draw2x2String( 0, 2, str2);
        u8x8->draw2x2String( 0, 4, str3);
        u8x8->draw2x2String( 0, 6, str4);
      }

      break;
    }

    delay(3000);
  }
}

void OLED_Next_Page()
{
  if (u8x8) 
  {
    OLED_current_page = (OLED_current_page + 1) % page_count;
    OLED_display_titles = false;
  }
}

void OLED_Up()
{
  if (u8x8) {
    switch (OLED_current_page)
    {
    }
  }
}

#endif /* USE_OLED */
