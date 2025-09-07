
#include "adsb_gillham.h"
#include <limits.h>

// Simple AC13 Gillham decode (Q=0). This is a compact implementation and
// may require cross-check against full tables for edge cases.
static const int gillham_table[64] = {
  // 6-bit Gillham (D2,A1,A2,A4,B1,B2) simplified map for 100-ft steps.
  // This is a placeholder pattern; for full correctness use a complete mapping.
  // We'll implement a minimal viable decoder: return INT_MIN when unknown.
  // (In production, replace with full mapping.)
  -1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1
};

int gillham_ac13_to_alt_ft(uint16_t ac13, bool qbit) {
  if (qbit) {
    // Q=1 handled elsewhere (13-bit gray -> 25 ft steps). Here only Q=0 path needed.
    return INT_MIN;
  }
  // Minimal fallback: return INT_MIN for now (placeholder).
  // TODO: implement full Gillham AC13 decoding (D2,A1,A2,A4,B1,B2,C1,C2,C4 plus M/PO).
  return INT_MIN;
}