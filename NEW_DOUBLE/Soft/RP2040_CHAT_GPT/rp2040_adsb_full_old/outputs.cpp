#include "outputs.h"
#include "config.h"
#include "storage.h"
#include <Arduino.h>

static OutFmt g_fmt = (OutFmt)DEFAULT_OUTPUT_FMT;

void outputsInit(){ g_fmt = (OutFmt)storageGetOutputFmt(); }
void outputsSet(OutFmt f){ g_fmt=f; storageSetOutputFmt((int)f); }
OutFmt outputsGet(){ return g_fmt; }

static void print_json(const DecodedADSB &d){
  Serial.printf(
    "{\"icao\":\"%06X\",\"flight\":\"%s\",\"lat\":%.6f,\"lon\":%.6f,"
    "\"alt\":%d,\"gs\":%.1f,\"trk\":%.1f,\"vr\":%d,\"sq\":%d,\"df\":%d,\"tc\":%d,\"t\":%u}\n",
    d.addr, d.flight, d.latitude, d.longitude, d.altitude,
    d.speed, d.course, d.vert_rate, d.Squawk, d.df, d.tc, d.timestamp
  );
}
static void print_csv(const DecodedADSB &d){
  Serial.printf("%06X,%s,%.6f,%.6f,%d,%.1f,%.1f,%d,%d,%d,%d,%u\n",
    d.addr, d.flight, d.latitude, d.longitude, d.altitude,
    d.speed, d.course, d.vert_rate, d.Squawk, d.df, d.tc, d.timestamp);
}
static void print_nmea(const DecodedADSB &d){
  // Псевдо-NMEA (для совместимости со сторонними тулзами)
  Serial.printf("$ADSB,%06X,%s,%.6f,%.6f,%d,%.1f,%.1f,%d,%d*00\n",
    d.addr, d.flight, d.latitude, d.longitude, d.altitude,
    d.speed, d.course, d.vert_rate, d.Squawk);
}
static void print_raw(const DecodedADSB &d){
  Serial.printf("ICAO=%06X FLT=%s LAT=%.6f LON=%.6f ALT=%d GS=%.1f TRK=%.1f VR=%d SQ=%d DF=%d TC=%d T=%u\n",
    d.addr, d.flight, d.latitude, d.longitude, d.altitude, d.speed, d.course, d.vert_rate,
    d.Squawk, d.df, d.tc, d.timestamp);
}
void outputsPrint(const DecodedADSB &d){
  switch(g_fmt){
    case OUT_JSON: print_json(d); break;
    case OUT_CSV:  print_csv(d);  break;
    case OUT_NMEA: print_nmea(d); break;
    default:       print_raw(d);  break;
  }
}
