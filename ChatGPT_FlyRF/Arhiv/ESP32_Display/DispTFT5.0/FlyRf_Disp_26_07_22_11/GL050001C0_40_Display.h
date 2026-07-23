#pragma once

#include <Arduino.h>

#define GL050001C0_40_MAX_TARGETS 12

struct GL050001C0_40_Target
{
  uint32_t address;
  char callsign[9];
  uint16_t squawk;
  int16_t altitudeM;
  int16_t relativeAltitudeM;
  uint16_t speedKmh;
  uint16_t courseDeg;
  float latitude;
  float longitude;
  int8_t signalRssi;
  uint8_t signalSource;
  uint16_t distanceM;
  uint16_t bearingDeg;
  int16_t verticalRate;
  uint8_t alarmLevel;
};

struct GL050001C0_40_State
{
  bool baseConnected;
  bool timeValid;
  bool gnssValid;
  bool powerValid;
  bool sosActive;
  bool showPowerVoltage;
  bool showPowerCurrent;
  bool showPowerBattery;
  bool showLoraStatus;
  bool showGpsStatus;
  bool showLanStatus;
  bool lanReady;
  bool lanLinkUp;
  bool lanUdpWorking;
  bool tableVisible;
  bool trackerMessageVisible;
  bool gnssStatusVisible;
  uint8_t hour;
  uint8_t minute;
  uint8_t satellites;
  uint8_t batteryPercent;
  float voltageV;
  float currentMa;
  float latitude;
  float longitude;
  int16_t altitudeM;
  uint16_t speedKmh;
  uint16_t courseDeg;
  uint16_t radarRangeM;
  bool radarRangeManual;
  uint32_t loraTxPackets;
  uint32_t loraRxPackets;
  uint32_t loraRfHz;
  int16_t loraRssiDb;
  uint32_t rs485TxPackets;
  uint32_t rs485RxPackets;
  uint32_t lanTxPackets;
  uint32_t lanRxPackets;
  uint16_t lanUdpPort;
  uint8_t lanIp[4];
  char trackerMessage[161];
  uint8_t targetCount;
  GL050001C0_40_Target targets[GL050001C0_40_MAX_TARGETS];
  uint8_t tableTargetCount;
  GL050001C0_40_Target tableTargets[GL050001C0_40_MAX_TARGETS];
};

bool GL050001C0_40_setup();
void GL050001C0_40_showStartup(const String &version);
void GL050001C0_40_showStatus(uint32_t rxPackets, uint32_t txPackets);
void GL050001C0_40_updateState(const GL050001C0_40_State &state);
bool GL050001C0_40_needsFrameUpdate();
void GL050001C0_40_showPowerOff();
void GL050001C0_40_loop();
