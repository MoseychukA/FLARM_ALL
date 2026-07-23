#pragma once
#include <Arduino.h>

void Remote_setGnssState(bool coordValid, bool timeValid, bool satsValid, uint8_t hour, uint8_t minute, float latitude, float longitude, float altitude, float hdop);
void Remote_setBaseStatus(bool testMode, bool gpsRx, uint8_t satellites, bool coordValid, bool coordIsLocal, float latitude, float longitude, uint32_t loraTxPackets, uint32_t loraRxPackets, uint32_t loraRfHz);
void Remote_setLanStatus(bool show, bool ready, bool linkUp, bool udpWorking, bool dhcp, const uint8_t ip[4], uint16_t udpPort, uint32_t txPackets, uint32_t rxPackets, uint32_t udpTxPackets, uint32_t udpRxPackets);
void Remote_setTrackerMessage(bool active, const char* text, uint8_t hour, uint8_t minute);

bool Remote_baseTestMode();
bool Remote_baseGpsRx();
bool Remote_baseCoordinateValid();
bool Remote_baseCoordinateIsLocal();
float Remote_baseLatitude();
float Remote_baseLongitude();
uint32_t Remote_loraTxPackets();
uint32_t Remote_loraRxPackets();
uint32_t Remote_loraRfHz();
bool Remote_lanShow();
bool Remote_lanStatusReceived();
bool Remote_lanReady();
bool Remote_lanLinkUp();
bool Remote_lanUdpWorking();
bool Remote_lanDhcp();
void Remote_lanIp(uint8_t out[4]);
uint16_t Remote_lanUdpPort();
uint32_t Remote_lanTxPackets();
uint32_t Remote_lanRxPackets();
uint32_t Remote_lanUdpTxPackets();
uint32_t Remote_lanUdpRxPackets();

bool Tracker_hasActiveTextMessage();
const char* Tracker_getActiveTextMessage();
bool Tracker_confirmActiveTextMessage();

bool GNSS_coordinatesValid();
bool GNSS_timeValid();
bool GNSS_satellitesValid();
bool GNSS_altitudeValid();
bool GNSS_waitingForInitialFix();
bool GNSS_waitingForRecovery();
bool GNSS_noDataTimeout();
uint8_t GNSS_hour();
uint8_t GNSS_minute();
uint8_t GNSS_satellites();
float GNSS_latitude();
float GNSS_longitude();
float GNSS_altitudeMeters();
