#include "cli.h"
#include <Arduino.h>
#include "profiles.h"
#include "outputs.h"
#include "filters.h"
#include "storage.h"

static String buf;

static void help(){
  Serial.println(F("CLI:"));
  Serial.println(F("  SHOW CFG"));
  Serial.println(F("  SET PROFILE=Normal|Urban|HighEMI|Remote"));
  Serial.println(F("  SET OUTPUT=RAW|JSON|CSV|NMEA"));
  Serial.println(F("  SET THRESH=<int> (перенос в профиль Remote/Urban и т.д.)"));
  Serial.println(F("  FILTER ICAO=<hex6>|0"));
  Serial.println(F("  FILTER MINALT=<ft>"));
  Serial.println(F("  FILTER MAXALT=<ft>"));
  Serial.println(F("  SAVE / LOAD"));
}

void cliInit(){ help(); }

void cliTask(){
  while (Serial.available()){
    char c = (char)Serial.read();
    if (c=='\r') continue;
    if (c=='\n'){
      String s=buf; buf="";
      s.trim(); if (s.length()==0) return;
      s.toUpperCase();

      if (s=="HELP") { help(); continue; }
      if (s=="SHOW CFG"){
        Serial.printf("PROFILE=%d OUTPUT=%d ICAO=%06X MINALT=%d MAXALT=%d\n",
          profilesGet(), outputsGet(), storageGetIcao(), storageGetMinAlt(), storageGetMaxAlt());
        continue;
      }
      if (s.startsWith("SET PROFILE=")){
        String v = s.substring(12);
        if (v=="NORMAL") profilesApply(PROF_NORMAL);
        else if (v=="URBAN") profilesApply(PROF_URBAN);
        else if (v=="HIGHEMI") profilesApply(PROF_HIGHEMI);
        else if (v=="REMOTE") profilesApply(PROF_REMOTE);
        Serial.println("OK"); continue;
      }
      if (s.startsWith("SET OUTPUT=")){
        String v = s.substring(11);
        if (v=="RAW") outputsSet(OUT_RAW);
        else if (v=="JSON") outputsSet(OUT_JSON);
        else if (v=="CSV") outputsSet(OUT_CSV);
        else if (v=="NMEA") outputsSet(OUT_NMEA);
        Serial.println("OK"); continue;
      }
      if (s.startsWith("FILTER ICAO=")){
        String v = s.substring(12);
        uint32_t icao = strtoul(v.c_str(),nullptr,16);
        filtersSetIcao(icao);
        Serial.println("OK"); continue;
      }
      if (s.startsWith("FILTER MINALT=")){
        int ft = s.substring(14).toInt(); filtersSetMinAlt(ft); Serial.println("OK"); continue;
      }
      if (s.startsWith("FILTER MAXALT=")){
        int ft = s.substring(14).toInt(); filtersSetMaxAlt(ft); Serial.println("OK"); continue;
      }
      if (s=="SAVE"){ storageSave(); Serial.println("OK"); continue; }
      if (s=="LOAD"){ storageLoad(); Serial.println("OK"); continue; }

      // Быстрый хелп
      Serial.println("ERR. Type HELP");
      continue;
    } else {
      buf += c;
      if (buf.length()>200) buf.remove(0,100);
    }
  }
}
