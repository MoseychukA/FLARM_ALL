#pragma once
#include "adsb_types.h"
enum OutFmt { OUT_RAW=0, OUT_JSON=1, OUT_CSV=2, OUT_NMEA=3 };
void outputsInit();
void outputsSet(OutFmt f);
OutFmt outputsGet();
void outputsPrint(const DecodedADSB &d);
