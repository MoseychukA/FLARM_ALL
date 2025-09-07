#include "storage.h"
#include "config.h"
#include <Arduino.h>
#include <LittleFS.h>

static int        s_profile = DEFAULT_PROFILE;
static int        s_outfmt  = DEFAULT_OUTPUT_FMT;
static uint32_t   s_icao    = 0;
static int        s_minAlt  = -100000;
static int        s_maxAlt  =  100000;

void storageInit(){ LittleFS.begin(); }

bool storageLoad(){
  File f = LittleFS.open(FS_FILENAME, "r");
  if (!f) return false;
  while (f.available()){
    String line = f.readStringUntil('\n'); line.trim();
    int eq = line.indexOf('='); if (eq<0) continue;
    String k = line.substring(0,eq); String v = line.substring(eq+1);
    if (k=="PROFILE") s_profile = v.toInt();
    else if (k=="OUTFMT") s_outfmt = v.toInt();
    else if (k=="ICAO") s_icao = strtoul(v.c_str(), nullptr, 16);
    else if (k=="MINALT") s_minAlt = v.toInt();
    else if (k=="MAXALT") s_maxAlt = v.toInt();
  }
  f.close(); return true;
}

bool storageSave(){
  File f = LittleFS.open(FS_FILENAME, "w");
  if (!f) return false;
  f.printf("PROFILE=%d\n", s_profile);
  f.printf("OUTFMT=%d\n", s_outfmt);
  f.printf("ICAO=%06X\n", s_icao);
  f.printf("MINALT=%d\n", s_minAlt);
  f.printf("MAXALT=%d\n", s_maxAlt);
  f.close(); return true;
}

void storageSetProfile(int id){ s_profile=id; storageSave(); }
int  storageGetProfile(){ return s_profile; }
void storageSetOutputFmt(int fmt){ s_outfmt=fmt; storageSave(); }
int  storageGetOutputFmt(){ return s_outfmt; }
void storageSetIcao(uint32_t icao){ s_icao=icao; storageSave(); }
uint32_t storageGetIcao(){ return s_icao; }
void storageSetMinAlt(int ft){ s_minAlt=ft; storageSave(); }
int  storageGetMinAlt(){ return s_minAlt; }
void storageSetMaxAlt(int ft){ s_maxAlt=ft; storageSave(); }
int  storageGetMaxAlt(){ return s_maxAlt; }
