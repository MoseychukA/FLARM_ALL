#pragma once
#include "adsb_types.h"
typedef void (*frame_cb_t)(int ch, const DecodedADSB& f);
void pipeline_init(frame_cb_t cb);
void pipeline_poll_once();