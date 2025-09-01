
#include "storage.h"
#include <LittleFS.h>
static const char* kFile = "/config.txt";
bool storage_init(){ return LittleFS.begin(); }
static void write_kv(File& f, const char* k, const String& v){ f.print(k); f.print('='); f.println(v); }
bool storage_save(const Settings& s){ File f=LittleFS.open(kFile,"w"); if(!f) return false; write_kv(f,"mode",s.mode); write_kv(f,"profile",s.profile); write_kv(f,"log_on",s.log_on?"1":"0"); write_kv(f,"ref_lat",String(s.ref_lat,8)); write_kv(f,"ref_lon",String(s.ref_lon,8)); f.close(); return true; }
bool storage_load(Settings& s){ File f=LittleFS.open(kFile,"r"); if(!f) return false; while(f.available()){ String line=f.readStringUntil('
'); line.trim(); if(!line.length()) continue; int eq=line.indexOf('='); if(eq<0) continue; String k=line.substring(0,eq); String v=line.substring(eq+1); k.toLowerCase(); v.trim(); if(k=="mode") s.mode=v; else if(k=="profile") s.profile=v; else if(k=="log_on") s.log_on=(v=="1"); else if(k=="ref_lat") s.ref_lat=v.toDouble(); else if(k=="ref_lon") s.ref_lon=v.toDouble(); } f.close(); return true; }
