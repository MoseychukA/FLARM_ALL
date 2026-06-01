#pragma once

#include <Arduino.h>

struct AdsbMessage {
  uint8_t raw[14];
  uint8_t bitCount;
  uint8_t df;
  uint8_t typeCode;
  uint32_t icao;
  uint32_t seenMs;
  int16_t signal;
  bool crcOk;
};

struct AdsbStats {
  uint32_t preambles;
  uint32_t frames;
  uint32_t validFrames;
  uint32_t crcErrors;
  uint16_t adcNow;
  uint16_t adcNoise;
  uint16_t threshold;
};

class AdsbReceiver {
public:
  void begin(uint8_t inputPin);
  void poll();
  bool available();
  bool hasMessage() const { return hasLast; }
  const AdsbMessage &lastMessage() const { return last; }
  const AdsbStats &stats() const { return counters; }

private:
  uint8_t pin = PA0;
  AdsbMessage last = {};
  AdsbStats counters = {};
  bool hasLast = false;
  bool newMessage = false;
  bool prevHigh = false;
  uint16_t adcMin = 4095;
  uint16_t adcMax = 0;
  uint32_t noiseSum = 0;
  uint16_t noiseCount = 0;
  uint32_t cyclesPerHalfUs = 36;

  void setupCycleCounter();
  void setupAdc();
  uint16_t readAdc();
  bool levelHigh(uint16_t value) const;
  void updateThreshold();
  bool capture(uint32_t firstEdgeCycle, AdsbMessage &msg);
  bool preambleAt(uint32_t firstEdgeCycle);
  uint16_t sampleHalfSlot(uint32_t startCycle, uint16_t halfSlot);
  uint32_t crc24(const uint8_t *bits, uint8_t bitCount);
};
