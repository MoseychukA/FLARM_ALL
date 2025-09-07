#pragma once
#include <stdint.h>

int gillham_ac13_to_alt_ft(uint16_t ac13, bool qbit); // Q=0 decode, returns feet (or INT32_MIN if invalid)
