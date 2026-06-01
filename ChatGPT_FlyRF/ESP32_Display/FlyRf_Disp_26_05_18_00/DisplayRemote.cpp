#include "DisplayRemote.h"
#include "RS485Display.h"
#include <string.h>

namespace {
    bool g_coordValid = false;
    bool g_timeValid = false;
    bool g_satsValid = false;
    uint8_t g_hour = 0;
    uint8_t g_minute = 0;
    uint8_t g_satellites = 0;
    bool g_testMode = false;
    bool g_gpsRx = false;
    bool g_coordIsLocal = false;
    float g_latitude = 0.0f;
    float g_longitude = 0.0f;
    float g_altitude = 0.0f;
    uint32_t g_loraTxPackets = 0;
    uint32_t g_loraRxPackets = 0;
    uint32_t g_loraRfHz = 0;
    bool g_lanShow = false;
    bool g_lanStatusReceived = false;
    bool g_lanReady = false;
    bool g_lanLinkUp = false;
    bool g_lanUdpWorking = false;
    bool g_lanDhcp = false;
    uint8_t g_lanIp[4] = {};
    uint16_t g_lanUdpPort = 0;
    uint32_t g_lanTxPackets = 0;
    uint32_t g_lanRxPackets = 0;
    uint32_t g_lanUdpTxPackets = 0;
    uint32_t g_lanUdpRxPackets = 0;
    bool g_trackerActive = false;
    char g_trackerText[BUFFER_SIZE] = {};
}

void Remote_setGnssState(bool coordValid, bool timeValid, bool satsValid, uint8_t hour, uint8_t minute, float latitude, float longitude, float altitude, float)
{
    g_coordValid = coordValid;
    g_timeValid = timeValid;
    g_satsValid = satsValid;
    g_hour = hour;
    g_minute = minute;
    g_latitude = latitude;
    g_longitude = longitude;
    g_altitude = altitude;
}

void Remote_setBaseStatus(bool testMode, bool gpsRx, uint8_t satellites, bool coordValid, bool coordIsLocal, float latitude, float longitude, uint32_t loraTxPackets, uint32_t loraRxPackets, uint32_t loraRfHz)
{
    g_testMode = testMode;
    g_gpsRx = gpsRx;
    g_satellites = satellites;
    g_coordIsLocal = coordIsLocal;
    g_loraTxPackets = loraTxPackets;
    g_loraRxPackets = loraRxPackets;
    g_loraRfHz = loraRfHz;
    if (coordValid) {
        g_coordValid = true;
        g_latitude = latitude;
        g_longitude = longitude;
    }
}

void Remote_setLanStatus(bool show, bool ready, bool linkUp, bool udpWorking, bool dhcp, const uint8_t ip[4], uint16_t udpPort, uint32_t txPackets, uint32_t rxPackets, uint32_t udpTxPackets, uint32_t udpRxPackets)
{
    g_lanStatusReceived = true;
    g_lanShow = show;
    g_lanReady = ready;
    g_lanLinkUp = linkUp;
    g_lanUdpWorking = udpWorking;
    g_lanDhcp = dhcp;
    if (ip != nullptr) {
        memcpy(g_lanIp, ip, sizeof(g_lanIp));
    } else {
        memset(g_lanIp, 0, sizeof(g_lanIp));
    }
    g_lanUdpPort = udpPort;
    g_lanTxPackets = txPackets;
    g_lanRxPackets = rxPackets;
    g_lanUdpTxPackets = udpTxPackets;
    g_lanUdpRxPackets = udpRxPackets;
}

void Remote_setTrackerMessage(bool active, const char* text, uint8_t, uint8_t)
{
    g_trackerActive = (active && text != nullptr && text[0] != '\0');
    if (g_trackerActive) {
        strncpy(g_trackerText, text, sizeof(g_trackerText) - 1U);
        g_trackerText[sizeof(g_trackerText) - 1U] = '\0';
    } else {
        g_trackerText[0] = '\0';
    }
}

bool Remote_baseTestMode() { return g_testMode; }
bool Remote_baseGpsRx() { return g_gpsRx; }
bool Remote_baseCoordinateValid() { return g_coordValid; }
bool Remote_baseCoordinateIsLocal() { return g_coordIsLocal; }
float Remote_baseLatitude() { return g_latitude; }
float Remote_baseLongitude() { return g_longitude; }
uint32_t Remote_loraTxPackets() { return g_loraTxPackets; }
uint32_t Remote_loraRxPackets() { return g_loraRxPackets; }
uint32_t Remote_loraRfHz() { return g_loraRfHz; }
bool Remote_lanShow() { return g_lanShow; }
bool Remote_lanStatusReceived() { return g_lanStatusReceived; }
bool Remote_lanReady() { return g_lanReady; }
bool Remote_lanLinkUp() { return g_lanLinkUp; }
bool Remote_lanUdpWorking() { return g_lanUdpWorking; }
bool Remote_lanDhcp() { return g_lanDhcp; }
void Remote_lanIp(uint8_t out[4]) { if (out) memcpy(out, g_lanIp, sizeof(g_lanIp)); }
uint16_t Remote_lanUdpPort() { return g_lanUdpPort; }
uint32_t Remote_lanTxPackets() { return g_lanTxPackets; }
uint32_t Remote_lanRxPackets() { return g_lanRxPackets; }
uint32_t Remote_lanUdpTxPackets() { return g_lanUdpTxPackets; }
uint32_t Remote_lanUdpRxPackets() { return g_lanUdpRxPackets; }

bool Tracker_hasActiveTextMessage() { return g_trackerActive && g_trackerText[0] != '\0'; }
const char* Tracker_getActiveTextMessage() { return g_trackerText; }
bool Tracker_confirmActiveTextMessage()
{
    if (!Tracker_hasActiveTextMessage()) {
        return false;
    }
    g_trackerActive = false;
    g_trackerText[0] = '\0';

    aux_t aux = {};
    RS485Display_getOutgoingAux(&aux);
    aux.confirm_message_M = true;
    aux.message_received = true;
    RS485Display_setOutgoingAux(&aux);
    return true;
}

bool GNSS_coordinatesValid() { return g_coordValid; }
bool GNSS_timeValid() { return g_timeValid; }
bool GNSS_satellitesValid() { return g_satsValid; }
bool GNSS_altitudeValid() { return g_coordValid; }
bool GNSS_waitingForInitialFix() { return false; }
bool GNSS_waitingForRecovery() { return false; }
bool GNSS_noDataTimeout() { return false; }
uint8_t GNSS_hour() { return g_hour; }
uint8_t GNSS_minute() { return g_minute; }
uint8_t GNSS_satellites() { return g_satellites; }
float GNSS_latitude() { return g_latitude; }
float GNSS_longitude() { return g_longitude; }
float GNSS_altitudeMeters() { return g_altitude; }
