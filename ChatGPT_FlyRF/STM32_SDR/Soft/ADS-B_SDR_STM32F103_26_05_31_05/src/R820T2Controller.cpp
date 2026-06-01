#include "R820T2Controller.h"
#include <Wire.h>

static const uint32_t R820T2_XTAL_HZ = 27000000UL;
static const uint32_t R820T2_IF_HZ = 5000000UL;

static const uint8_t initRegs[32] = {
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x90, 0x80, 0x60, 0x80, 0x40, 0xA8, 0x0F, 0x40, 0x63, 0x75, 0x68,
  0x7C, 0x83, 0x80, 0x00, 0x0F, 0x00, 0xC0, 0x30, 0x48, 0xCC, 0x60,
  0x00, 0x54, 0xAE, 0x0A, 0xC0
};

struct FilterBand {
  uint16_t mhz;
  uint8_t openDrain;
  uint8_t mux;
  uint8_t tracking;
};

static const FilterBand bands[] = {
  {   0, 0x08, 0x02, 0xDF }, {  50, 0x08, 0x02, 0xBE },
  {  55, 0x08, 0x02, 0x8B }, {  60, 0x08, 0x02, 0x7B },
  {  65, 0x08, 0x02, 0x69 }, {  70, 0x08, 0x02, 0x58 },
  {  75, 0x00, 0x02, 0x44 }, {  90, 0x00, 0x02, 0x34 },
  { 110, 0x00, 0x02, 0x24 }, { 140, 0x00, 0x02, 0x14 },
  { 180, 0x00, 0x02, 0x13 }, { 250, 0x00, 0x02, 0x11 },
  { 280, 0x00, 0x02, 0x00 }, { 310, 0x00, 0x41, 0x00 },
  { 588, 0x00, 0x40, 0x00 }, { 650, 0x00, 0x40, 0x00 }
};

bool R820T2Controller::begin()
{
  Wire.beginTransmission(address);
  present = Wire.endTransmission() == 0;

  for (uint8_t i = 5; i < 32; i++) {
    cache[i] = initRegs[i];
    writeReg(i, cache[i]);
    delay(1);
  }

  writeMasked(0x05, 0x10, 0x10); // LNA manual mode.
  writeMasked(0x07, 0x10, 0x10); // Mixer manual mode.
  writeMasked(0x0A, 0x00, 0x0F); // Widest usable channel filter.
  writeMasked(0x0B, 0x00, 0x0F);
  setManualGain(14, 15, 8);
  return present;
}

bool R820T2Controller::writeReg(uint8_t reg, uint8_t value)
{
  cache[reg] = value;
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool R820T2Controller::writeMasked(uint8_t reg, uint8_t value, uint8_t mask)
{
  uint8_t next = (cache[reg] & ~mask) | (value & mask);
  return writeReg(reg, next);
}

void R820T2Controller::setTrackingFilter(uint32_t hz)
{
  uint16_t mhz = hz / 1000000UL;
  const FilterBand *band = &bands[0];
  for (uint8_t i = 0; i < sizeof(bands) / sizeof(bands[0]); i++) {
    if (mhz >= bands[i].mhz) band = &bands[i];
    else break;
  }

  writeMasked(0x17, band->openDrain, 0x08);
  writeMasked(0x1A, band->mux, 0xC3);
  writeReg(0x1B, band->tracking);
}

bool R820T2Controller::setPll(uint32_t loHz)
{
  uint8_t divNum = 0;
  uint32_t vco = loHz;
  while (vco < 1770000000UL && divNum < 5) {
    vco <<= 1;
    divNum++;
  }

  uint8_t divCode = 0;
  switch (divNum) {
    case 0: divCode = 0x00; break;
    case 1: divCode = 0x10; break;
    case 2: divCode = 0x20; break;
    case 3: divCode = 0x30; break;
    case 4: divCode = 0x40; break;
    default: divCode = 0x50; break;
  }
  writeMasked(0x10, divCode, 0x70);

  uint32_t ref = R820T2_XTAL_HZ / 2;
  uint32_t n = vco / ref;
  uint32_t rem = vco - n * ref;
  if (n < 13) n = 13;

  uint8_t ni = (n - 13) / 4;
  uint8_t si = n - 4 * ni - 13;
  writeReg(0x14, (ni << 6) | (si << 4));

  uint16_t sdm = (uint64_t)rem * 65536ULL / ref;
  writeReg(0x16, sdm >> 8);
  writeReg(0x15, sdm & 0xFF);
  writeMasked(0x12, sdm ? 0x00 : 0x08, 0x08);

  delay(10);
  return true;
}

bool R820T2Controller::setFrequency(uint32_t hz)
{
  setTrackingFilter(hz);
  return setPll(hz + R820T2_IF_HZ);
}

void R820T2Controller::setManualGain(uint8_t lna, uint8_t mixer, uint8_t vga)
{
  if (lna > 15) lna = 15;
  if (mixer > 15) mixer = 15;
  if (vga > 15) vga = 15;

  writeMasked(0x05, 0x10 | lna, 0x1F);
  writeMasked(0x07, 0x10 | mixer, 0x1F);
  writeMasked(0x0C, vga, 0x0F);
}
