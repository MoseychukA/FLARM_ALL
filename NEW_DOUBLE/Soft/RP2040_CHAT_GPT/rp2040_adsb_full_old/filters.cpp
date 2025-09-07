#include "filters.h"
#include "storage.h"

static uint32_t g_icao=0;
static int g_minAlt=-100000, g_maxAlt=100000;

void filtersInit(){
  g_icao = storageGetIcao();
  g_minAlt = storageGetMinAlt();
  g_maxAlt = storageGetMaxAlt();
}
bool filtersAllow(const DecodedADSB &d){
  if (g_icao && d.addr != g_icao) return false;
  if (d.altitude < g_minAlt) return false;
  if (d.altitude > g_maxAlt) return false;
  return true;
}
void filtersSetIcao(uint32_t icao){ g_icao=icao; storageSetIcao(icao); }
void filtersSetMinAlt(int ft){ g_minAlt=ft; storageSetMinAlt(ft); }
void filtersSetMaxAlt(int ft){ g_maxAlt=ft; storageSetMaxAlt(ft); }
