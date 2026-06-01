/*
  ADS-B receiver for STM32F103CBT6 + R820T2 + AD8310.

  RF path:
    Antenna 1090 MHz -> R820T2 -> AD8310 envelope/log detector -> PA0.

  Display:
    TFT_eSPI ILI9341 on SPI2, XPT2046 touch on PB12.
*/

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include "CONFIG.h"

#if defined(STM32F1xx) || defined(STM32F103xB) || defined(ARDUINO_ARCH_STM32)
#include "stm32f1xx.h"
#endif

#ifndef RCC_CFGR_ADCPRE_DIV6
#define RCC_CFGR_ADCPRE_DIV6 RCC_CFGR_ADCPRE_1
#endif

#ifndef DWT_CTRL_CYCCNTENA_Msk
#define DWT_CTRL_CYCCNTENA_Msk (1UL << 0)
#endif

extern TFT_eSPI tft;

#define ADSB_INPUT_PIN PA0
#define SOUNDER_PIN    PB8
#define ADSB_FREQ_HZ   1090000000UL

static const uint8_t ADSB_LONG_BITS = 112;
static const uint8_t ADSB_SHORT_BITS = 56;
static const uint32_t MODE_S_POLY = 0xFFF409UL;

struct AdsbFrame {
  uint8_t bytes[14];
  uint8_t bitCount;
  uint8_t df;
  uint32_t icao;
  bool crcOk;
  uint8_t typeCode;
  uint32_t seenMs;
  int16_t signal;
};

static AdsbFrame lastFrame;
static uint32_t totalPreambles = 0;
static uint32_t totalFrames = 0;
static uint32_t goodFrames = 0;
static uint32_t crcErrors = 0;
static uint32_t lastUiMs = 0;
static uint32_t lastStatsMs = 0;
static uint32_t lastTouchMs = 0;
static uint32_t halfUsCycles = 36;
static uint16_t adcMin = 4095;
static uint16_t adcMax = 0;
static uint16_t adcNoise = 0;
static uint16_t adcThreshold = 1500;
static bool haveLastFrame = false;

void R820T2_init();
void R820T2_set_frequency(uint32_t freq);
void R820T2_set_bandwidth(uint8_t bw);
int r82xx_set_gain(int set_manual_gain, int gain);

static inline uint32_t cpuCycles()
{
#if defined(DWT)
  return DWT->CYCCNT;
#else
  return micros() * (F_CPU / 1000000UL);
#endif
}

static inline void waitCycles(uint32_t target)
{
  while ((int32_t)(cpuCycles() - target) < 0) {
    __asm volatile("nop");
  }
}

static void setupCycleCounter()
{
#if defined(CoreDebug) && defined(DWT)
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#endif
  uint32_t cycles = F_CPU / 2000000UL;
  halfUsCycles = cycles > 0 ? cycles : 1;
}

static void setupFastAdc()
{
  pinMode(ADSB_INPUT_PIN, INPUT_ANALOG);

#if defined(ADC1) && defined(RCC_APB2ENR_ADC1EN)
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
  RCC->CFGR &= ~RCC_CFGR_ADCPRE;
  RCC->CFGR |= RCC_CFGR_ADCPRE_DIV6;

  ADC1->CR1 = 0;
  ADC1->CR2 = 0;
  ADC1->SMPR2 &= ~0x7UL;
  ADC1->SQR1 = 0;
  ADC1->SQR3 = 0; // PA0 = ADC channel 0.

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

static inline uint16_t readAdsbAdc()
{
#if defined(ADC1)
  return ADC1->DR & 0x0FFF;
#else
  return analogRead(ADSB_INPUT_PIN);
#endif
}

static uint16_t sampleAtHalfSlot(uint32_t startCycle, uint16_t halfSlot)
{
  waitCycles(startCycle + (uint32_t)halfSlot * halfUsCycles);
  return readAdsbAdc();
}

static void updateNoiseEstimator()
{
  static uint32_t sum = 0;
  static uint16_t count = 0;

  uint16_t v = readAdsbAdc();
  if (v < adcMin) adcMin = v;
  if (v > adcMax) adcMax = v;
  sum += v;
  count++;

  if (count >= 256) {
    adcNoise = sum / count;
    uint16_t span = adcMax > adcMin ? adcMax - adcMin : 0;
    uint16_t margin = span / 3;
    if (margin < 35) margin = 35;
    uint32_t nextThreshold = (uint32_t)adcNoise + margin;
    adcThreshold = nextThreshold > 4000 ? 4000 : nextThreshold;
    sum = 0;
    count = 0;

    if (millis() - lastStatsMs > 1000) {
      adcMin = v;
      adcMax = v;
      lastStatsMs = millis();
    }
  }
}

static bool isHigh(uint16_t v)
{
  return v > adcThreshold;
}

static bool checkPreamble(uint32_t startCycle)
{
  uint16_t s0 = sampleAtHalfSlot(startCycle, 0);
  uint16_t s1 = sampleAtHalfSlot(startCycle, 1);
  uint16_t s2 = sampleAtHalfSlot(startCycle, 2);
  uint16_t s3 = sampleAtHalfSlot(startCycle, 3);
  uint16_t s4 = sampleAtHalfSlot(startCycle, 4);
  uint16_t s5 = sampleAtHalfSlot(startCycle, 5);
  uint16_t s6 = sampleAtHalfSlot(startCycle, 6);
  uint16_t s7 = sampleAtHalfSlot(startCycle, 7);
  uint16_t s8 = sampleAtHalfSlot(startCycle, 8);
  uint16_t s9 = sampleAtHalfSlot(startCycle, 9);

  return isHigh(s0) && !isHigh(s1) &&
         isHigh(s2) && !isHigh(s3) &&
         !isHigh(s4) && !isHigh(s5) && !isHigh(s6) &&
         isHigh(s7) && !isHigh(s8) && isHigh(s9);
}

static uint32_t crc24ModeS(const uint8_t *bits, uint8_t bitCount)
{
  uint32_t crc = 0;

  for (uint8_t i = 0; i < bitCount - 24; i++) {
    uint8_t bit = bits[i] & 1;
    uint8_t top = (crc >> 23) & 1;
    crc = ((crc << 1) & 0xFFFFFFUL) | bit;
    if (top) crc ^= MODE_S_POLY;
  }

  return crc & 0xFFFFFFUL;
}

static uint32_t parityFromBits(const uint8_t *bits, uint8_t bitCount)
{
  uint32_t parity = 0;
  for (uint8_t i = bitCount - 24; i < bitCount; i++) {
    parity = (parity << 1) | (bits[i] & 1);
  }
  return parity;
}

static void bitsToBytes(const uint8_t *bits, uint8_t bitCount, uint8_t *bytes)
{
  memset(bytes, 0, 14);
  for (uint8_t i = 0; i < bitCount; i++) {
    if (bits[i]) bytes[i >> 3] |= 0x80 >> (i & 7);
  }
}

static uint8_t adsbBitsForDf(uint8_t df)
{
  switch (df) {
    case 0:
    case 4:
    case 5:
    case 11:
      return ADSB_SHORT_BITS;
    default:
      return ADSB_LONG_BITS;
  }
}

static uint32_t getIcao(const uint8_t *bytes, uint8_t df)
{
  if (df == 17 || df == 18) {
    return ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | bytes[3];
  }
  return 0;
}

static bool decodeFrame(uint32_t startCycle, AdsbFrame *frame)
{
  uint8_t bits[ADSB_LONG_BITS];
  int32_t signalSum = 0;

  memset(bits, 0, sizeof(bits));

  for (uint8_t i = 0; i < ADSB_LONG_BITS; i++) {
    uint16_t early = sampleAtHalfSlot(startCycle, 16 + i * 2);
    uint16_t late  = sampleAtHalfSlot(startCycle, 17 + i * 2);
    bits[i] = early > late;
    signalSum += abs((int16_t)early - (int16_t)late);
  }

  uint8_t df = 0;
  for (uint8_t i = 0; i < 5; i++) df = (df << 1) | bits[i];

  uint8_t bitCount = adsbBitsForDf(df);
  bitsToBytes(bits, bitCount, frame->bytes);

  uint32_t crc = crc24ModeS(bits, bitCount);
  uint32_t parity = parityFromBits(bits, bitCount);

  frame->bitCount = bitCount;
  frame->df = df;
  frame->icao = getIcao(frame->bytes, df);
  frame->crcOk = (crc == parity);
  frame->typeCode = (bitCount == ADSB_LONG_BITS && (df == 17 || df == 18)) ? (frame->bytes[4] >> 3) : 0;
  frame->seenMs = millis();
  frame->signal = signalSum / ADSB_LONG_BITS;

  return true;
}

static void printFrame(const AdsbFrame &frame)
{
  Serial.print(frame.crcOk ? "ADS-B OK " : "ADS-B CRC ");
  Serial.print("DF=");
  Serial.print(frame.df);
  Serial.print(" bits=");
  Serial.print(frame.bitCount);
  Serial.print(" ICAO=");
  if (frame.icao < 0x100000UL) Serial.print('0');
  if (frame.icao < 0x010000UL) Serial.print('0');
  if (frame.icao < 0x001000UL) Serial.print('0');
  if (frame.icao < 0x000100UL) Serial.print('0');
  if (frame.icao < 0x000010UL) Serial.print('0');
  Serial.print(frame.icao, HEX);
  Serial.print(" TC=");
  Serial.print(frame.typeCode);
  Serial.print(" RSSI=");
  Serial.print(frame.signal);
  Serial.print(" MSG=");

  uint8_t n = frame.bitCount / 8;
  for (uint8_t i = 0; i < n; i++) {
    if (frame.bytes[i] < 16) Serial.print('0');
    Serial.print(frame.bytes[i], HEX);
  }
  Serial.println();
}

static void soundValidFrame()
{
  digitalWrite(SOUNDER_PIN, HIGH);
  delay(10);
  digitalWrite(SOUNDER_PIN, LOW);
}

static void drawStaticUi(const String &ver)
{
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(4, 4);
  tft.print("ADS-B SDR STM32F103");

  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(4, 24);
  tft.print(ver);

  tft.drawFastHLine(0, 38, 320, TFT_DARKGREY);
  tft.setTextSize(2);
}

static void printHex24(uint32_t v)
{
  if (v < 0x100000UL) tft.print('0');
  if (v < 0x010000UL) tft.print('0');
  if (v < 0x001000UL) tft.print('0');
  if (v < 0x000100UL) tft.print('0');
  if (v < 0x000010UL) tft.print('0');
  tft.print(v, HEX);
}

static void updateDisplay()
{
  if (millis() - lastUiMs < 250) return;
  lastUiMs = millis();

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.fillRect(0, 44, 320, 196, TFT_BLACK);

  tft.setCursor(4, 46);
  tft.print("Freq: 1090.000 MHz");

  tft.setCursor(4, 70);
  tft.print("ADC:");
  tft.print(readAdsbAdc());
  tft.print(" TH:");
  tft.print(adcThreshold);

  tft.setCursor(4, 94);
  tft.print("Pre:");
  tft.print(totalPreambles);
  tft.print(" Frm:");
  tft.print(totalFrames);

  tft.setCursor(4, 118);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.print("OK:");
  tft.print(goodFrames);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.print(" CRC:");
  tft.print(crcErrors);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(4, 146);
  if (haveLastFrame) {
    tft.print("DF ");
    tft.print(lastFrame.df);
    tft.print(" ICAO ");
    printHex24(lastFrame.icao);

    tft.setCursor(4, 170);
    tft.print("TC ");
    tft.print(lastFrame.typeCode);
    tft.print(" RSSI ");
    tft.print(lastFrame.signal);

    tft.setCursor(4, 194);
    uint8_t n = lastFrame.bitCount / 8;
    if (n > 8) n = 8;
    for (uint8_t i = 0; i < n; i++) {
      if (lastFrame.bytes[i] < 16) tft.print('0');
      tft.print(lastFrame.bytes[i], HEX);
    }
  } else {
    tft.print("Waiting for Mode-S frames");
  }

#ifdef TOUCH_CS
  uint16_t tx, ty;
  if (tft.getTouch(&tx, &ty) && millis() - lastTouchMs > 300) {
    lastTouchMs = millis();
    tft.fillRect(180, 218, 140, 18, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(184, 222);
    tft.print("Touch ");
    tft.print(tx);
    tft.print(',');
    tft.print(ty);
  }
#endif
}

static void setupRadio()
{
  Wire.begin();
  Wire.beginTransmission(0x1A);
  uint8_t error = Wire.endTransmission();

  if (error == 0) {
    Serial.println("R820T2 I2C device found at address 0x1A");
  } else {
    Serial.print("R820T2 I2C error: ");
    Serial.println(error);
  }

  R820T2_init();
  R820T2_set_frequency(ADSB_FREQ_HZ);
  R820T2_set_bandwidth(0);
  r82xx_set_gain(1, 420);
}

static String sketchName()
{
  String verSoft = __FILE__;
  int valSrt = verSoft.lastIndexOf('\\');
  if (valSrt < 0) valSrt = verSoft.lastIndexOf('/');
  verSoft.remove(0, valSrt + 1);
  valSrt = verSoft.lastIndexOf('.');
  if (valSrt > 0) verSoft.remove(valSrt);
  return verSoft;
}

void setup()
{
  Serial.begin(Serial_SPEED);
  while (!Serial && millis() < 1000) {}
  Serial.flush();
  delay(1000);

  Serial.println("Start system");
  Serial.println();

  String verSoft = sketchName();
  Serial.println(verSoft);

  pinMode(SOUNDER_PIN, OUTPUT);
  digitalWrite(SOUNDER_PIN, LOW);

  drawStaticUi(verSoft);
  setupCycleCounter();
  setupFastAdc();
  setupRadio();

  Serial.println("AD8310 input: PA0");
  Serial.println("TFT: ILI9341 SPI2, touch XPT2046 CS PB12");
  Serial.println("Start system END");
}

void loop()
{
  updateNoiseEstimator();

  static bool prevHigh = false;
  bool nowHigh = isHigh(readAdsbAdc());

  if (nowHigh && !prevHigh) {
    uint32_t startCycle = cpuCycles();

    if (checkPreamble(startCycle)) {
      totalPreambles++;

      AdsbFrame frame;
      if (decodeFrame(startCycle, &frame)) {
        totalFrames++;
        if (frame.crcOk) {
          goodFrames++;
          lastFrame = frame;
          haveLastFrame = true;
          printFrame(frame);
          soundValidFrame();
        } else {
          crcErrors++;
          if ((crcErrors & 0x0F) == 1) printFrame(frame);
        }
      }
    }
  }

  prevHigh = nowHigh;
  updateDisplay();
  yield();
}
