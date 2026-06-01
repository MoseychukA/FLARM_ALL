#include "AdsbReceiver.h"

#if defined(ARDUINO_ARCH_STM32)
#include "stm32f1xx.h"
#endif

#ifndef DWT_CTRL_CYCCNTENA_Msk
#define DWT_CTRL_CYCCNTENA_Msk (1UL << 0)
#endif

static const uint8_t MODE_S_LONG_BITS = 112;
static const uint8_t MODE_S_SHORT_BITS = 56;
static const uint32_t MODE_S_POLY = 0xFFF409UL;

static inline uint32_t cycleNow()
{
#if defined(DWT)
  return DWT->CYCCNT;
#else
  return micros() * (F_CPU / 1000000UL);
#endif
}

static inline void waitUntilCycle(uint32_t target)
{
  while ((int32_t)(cycleNow() - target) < 0) {
    __asm volatile("nop");
  }
}

void AdsbReceiver::begin(uint8_t inputPin)
{
  pin = inputPin;
  counters.threshold = 1600;
  setupCycleCounter();
  setupAdc();
}

bool AdsbReceiver::available()
{
  bool value = newMessage;
  newMessage = false;
  return value;
}

void AdsbReceiver::setupCycleCounter()
{
#if defined(CoreDebug) && defined(DWT)
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#endif
  uint32_t c = F_CPU / 2000000UL;
  cyclesPerHalfUs = c ? c : 1;
}

void AdsbReceiver::setupAdc()
{
  pinMode(pin, INPUT_ANALOG);

#if defined(ADC1) && defined(RCC_APB2ENR_ADC1EN)
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
  RCC->CFGR &= ~RCC_CFGR_ADCPRE;
  RCC->CFGR |= RCC_CFGR_ADCPRE_1;
  ADC1->CR1 = 0;
  ADC1->CR2 = 0;
  ADC1->SMPR2 &= ~0x7UL;
  ADC1->SQR1 = 0;
  ADC1->SQR3 = 0;
  ADC1->CR2 |= ADC_CR2_ADON;
  delay(2);
  ADC1->CR2 |= ADC_CR2_RSTCAL;
  while (ADC1->CR2 & ADC_CR2_RSTCAL) {}
  ADC1->CR2 |= ADC_CR2_CAL;
  while (ADC1->CR2 & ADC_CR2_CAL) {}
  ADC1->CR2 |= ADC_CR2_CONT;
  ADC1->CR2 |= ADC_CR2_ADON;
  ADC1->CR2 |= ADC_CR2_SWSTART;
#endif
}

uint16_t AdsbReceiver::readAdc()
{
#if defined(ADC1)
  return ADC1->DR & 0x0FFF;
#else
  return analogRead(pin);
#endif
}

bool AdsbReceiver::levelHigh(uint16_t value) const
{
  return value > counters.threshold;
}

void AdsbReceiver::updateThreshold()
{
  uint16_t value = readAdc();
  counters.adcNow = value;
  if (value < adcMin) adcMin = value;
  if (value > adcMax) adcMax = value;

  noiseSum += value;
  noiseCount++;
  if (noiseCount < 256) return;

  counters.adcNoise = noiseSum / noiseCount;
  uint16_t span = adcMax > adcMin ? adcMax - adcMin : 0;
  uint16_t margin = span / 3;
  if (margin < 35) margin = 35;

  uint32_t next = (uint32_t)counters.adcNoise + margin;
  counters.threshold = next > 4000 ? 4000 : next;

  adcMin = value;
  adcMax = value;
  noiseSum = 0;
  noiseCount = 0;
}

uint16_t AdsbReceiver::sampleHalfSlot(uint32_t startCycle, uint16_t halfSlot)
{
  waitUntilCycle(startCycle + (uint32_t)halfSlot * cyclesPerHalfUs);
  return readAdc();
}

bool AdsbReceiver::preambleAt(uint32_t firstEdgeCycle)
{
  bool h0 = levelHigh(sampleHalfSlot(firstEdgeCycle, 0));
  bool h1 = levelHigh(sampleHalfSlot(firstEdgeCycle, 1));
  bool h2 = levelHigh(sampleHalfSlot(firstEdgeCycle, 2));
  bool h3 = levelHigh(sampleHalfSlot(firstEdgeCycle, 3));
  bool h4 = levelHigh(sampleHalfSlot(firstEdgeCycle, 4));
  bool h5 = levelHigh(sampleHalfSlot(firstEdgeCycle, 5));
  bool h6 = levelHigh(sampleHalfSlot(firstEdgeCycle, 6));
  bool h7 = levelHigh(sampleHalfSlot(firstEdgeCycle, 7));
  bool h8 = levelHigh(sampleHalfSlot(firstEdgeCycle, 8));
  bool h9 = levelHigh(sampleHalfSlot(firstEdgeCycle, 9));

  return h0 && !h1 && h2 && !h3 && !h4 && !h5 && !h6 && h7 && !h8 && h9;
}

uint32_t AdsbReceiver::crc24(const uint8_t *bits, uint8_t bitCount)
{
  uint32_t crc = 0;
  for (uint8_t i = 0; i < bitCount - 24; i++) {
    uint8_t top = (crc >> 23) & 1;
    crc = ((crc << 1) & 0xFFFFFFUL) | (bits[i] & 1);
    if (top) crc ^= MODE_S_POLY;
  }
  return crc & 0xFFFFFFUL;
}

static uint8_t bitsForDf(uint8_t df)
{
  if (df == 0 || df == 4 || df == 5 || df == 11) return MODE_S_SHORT_BITS;
  return MODE_S_LONG_BITS;
}

bool AdsbReceiver::capture(uint32_t firstEdgeCycle, AdsbMessage &msg)
{
  uint8_t bits[MODE_S_LONG_BITS];
  int32_t signal = 0;
  memset(bits, 0, sizeof(bits));
  memset(&msg, 0, sizeof(msg));

  for (uint8_t i = 0; i < MODE_S_LONG_BITS; i++) {
    uint16_t early = sampleHalfSlot(firstEdgeCycle, 16 + i * 2);
    uint16_t late = sampleHalfSlot(firstEdgeCycle, 17 + i * 2);
    bits[i] = early > late ? 1 : 0;
    signal += abs((int16_t)early - (int16_t)late);
  }

  for (uint8_t i = 0; i < 5; i++) msg.df = (msg.df << 1) | bits[i];
  msg.bitCount = bitsForDf(msg.df);

  for (uint8_t i = 0; i < msg.bitCount; i++) {
    if (bits[i]) msg.raw[i >> 3] |= 0x80 >> (i & 7);
  }

  uint32_t parity = 0;
  for (uint8_t i = msg.bitCount - 24; i < msg.bitCount; i++) {
    parity = (parity << 1) | bits[i];
  }

  msg.crcOk = crc24(bits, msg.bitCount) == parity;
  msg.seenMs = millis();
  msg.signal = signal / MODE_S_LONG_BITS;

  if (msg.df == 17 || msg.df == 18) {
    msg.icao = ((uint32_t)msg.raw[1] << 16) | ((uint32_t)msg.raw[2] << 8) | msg.raw[3];
    msg.typeCode = msg.raw[4] >> 3;
  }

  return true;
}

void AdsbReceiver::poll()
{
  updateThreshold();

  bool high = levelHigh(counters.adcNow);
  if (high && !prevHigh) {
    uint32_t edge = cycleNow();
    if (preambleAt(edge)) {
      counters.preambles++;

      AdsbMessage msg;
      if (capture(edge, msg)) {
        counters.frames++;
        if (msg.crcOk) counters.validFrames++;
        else counters.crcErrors++;

        last = msg;
        hasLast = true;
        newMessage = true;
      }
    }
  }

  prevHigh = high;
}
