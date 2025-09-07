#include <Arduino.h>
#include "adsb_gillham.h"

static void test_gillham() {
  // Несколько тестов Q=1
  int32_t a1 = gillham_decode_ac13(0x1A00, 1);
  int32_t a2 = gillham_decode_ac13(0x1200, 1);
  Serial.printf("Gillham Q1 tests: %ld %ld\n", (long)a1, (long)a2);
}

void runGillhamTests() {
  test_gillham();
}
