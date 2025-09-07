#pragma once
#include "adsb_correlator.h"

enum ProfileId { PROF_NORMAL=0, PROF_URBAN=1, PROF_HIGHEMI=2, PROF_REMOTE=3 };
void profilesInit();
void profilesApply(ProfileId id);
ProfileId profilesGet();