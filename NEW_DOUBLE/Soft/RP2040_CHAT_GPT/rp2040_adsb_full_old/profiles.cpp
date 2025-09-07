#include "profiles.h"
#include "storage.h"
#include "config.h"

static ProfileId g_prof = (ProfileId)DEFAULT_PROFILE;

void profilesApply(ProfileId id){
  CorrConfig c{};
  switch(id){
    case PROF_NORMAL:  c={14,2,10, PRE_SAMPLES+MAX_BITS*BIT_SAMPLES}; break;
    case PROF_URBAN:   c={18,3,12, PRE_SAMPLES+MAX_BITS*BIT_SAMPLES}; break;
    case PROF_HIGHEMI: c={22,4,14, PRE_SAMPLES+MAX_BITS*BIT_SAMPLES}; break;
    case PROF_REMOTE:  c={10,2, 8, PRE_SAMPLES+MAX_BITS*BIT_SAMPLES}; break;
  }
  correlator_set_cfg(c);
  g_prof=id;
  storageSetProfile((int)id);
}
void profilesInit(){
  correlator_init();
  profilesApply((ProfileId)storageGetProfile());
}
ProfileId profilesGet(){ return g_prof; }
