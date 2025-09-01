#include "bds.h"

BDS60 parse_bds60(const uint8_t *me){
  BDS60 o; o.valid=false;
  // ME[0..6]
  // Vertical rate in ft/min often in bits 36..46; Implementation varies.
  // Placeholder decode, needs mapping per EHS doc.
  return o;
}

BDS61 parse_bds61(const uint8_t *me){
  BDS61 o; o.valid=false;
  // Baro-GNSS diff mapping per EHS doc.
  return o;
}
