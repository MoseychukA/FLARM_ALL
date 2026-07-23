/*
  WiFiRF.cpp
  Base WiFi AP and UDP support.
*/

#include "WiFiRF.h"
#include "Log.h"
#include "DeviceInfo.h"
#include "EEPROMRF.h"

String host_name;

namespace
{
    static bool g_wifiReady = false;
    static IPAddress g_apIP(192, 168, 1, 1);
    static IPAddress g_udpBroadcastIP(192, 168, 1, 255);
    static WiFiUDP g_udp;
    static uint32_t g_lastWifiCheckMs = 0;
    static uint8_t g_lastStationCount = 0;

    constexpr uint8_t kApChannel = 1;
    constexpr uint8_t kApMaxClients = 4;
    constexpr uint32_t kWifiCheckIntervalMs = 3000UL;
    const IPAddress kApLocalIP(192, 168, 1, 1);
    const IPAddress kApGateway(192, 168, 1, 1);
    const IPAddress kApSubnet(255, 255, 255, 0);

    void refreshRuntimeInfo()
    {
        g_apIP = WiFi.softAPIP();
        if (g_apIP == IPAddress((uint32_t)0))
        {
            g_apIP = kApLocalIP;
        }
        g_udpBroadcastIP = IPAddress(g_apIP[0], g_apIP[1], g_apIP[2], 255);
        g_lastStationCount = WiFi.softAPgetStationNum();
    }

    bool startAP(bool fullReset)
    {
        WiFi.persistent(false);
        WiFi.setSleep(false);
        WiFi.setTxPower(WIFI_POWER_11dBm);

        if (fullReset)
        {
            g_udp.stop();
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_OFF);
            delay(250);
        }

        WiFi.mode(WIFI_AP);
        delay(150);
        WiFi.setSleep(false);
        WiFi.setTxPower(WIFI_POWER_11dBm);

        const bool cfgOk = WiFi.softAPConfig(kApLocalIP, kApGateway, kApSubnet);
        delay(100);
        const bool apOk = WiFi.softAP(host_name.c_str(), FLYRF_WIFI_AP_PASSWORD);
        delay(400);
        WiFi.setSleep(false);
        WiFi.setTxPower(WIFI_POWER_11dBm);

        refreshRuntimeInfo();
        g_udp.stop();
        g_udp.begin(WiFi_defaultNmeaUdpPort());

        g_wifiReady = cfgOk && apOk && g_apIP != IPAddress((uint32_t)0);
        return g_wifiReady;
    }
}

void WiFi_setup()
{
    host_name = String(FLYRF_WIFI_AP_SSID_PREFIX) + "-" + DeviceInfo_chipIdHex();

    bool ok = startAP(true);
    if (!ok)
    {
        Serial.println(F("[WIFI] AP start retry"));
        delay(500);
        ok = startAP(true);
    }

    Serial.print(F("[WIFI] SSID: "));
    Serial.println(host_name);
    Serial.print(F("[WIFI] PASS: "));
    Serial.println(FLYRF_WIFI_AP_PASSWORD);
    Serial.print(F("[WIFI] AP IP: "));
    Serial.println(g_apIP);
    Serial.print(F("[WIFI] CH: "));
    Serial.println(kApChannel);
    Serial.print(F("[WIFI] Ready: "));
    Serial.println(ok ? F("YES") : F("NO"));
}

void WiFi_loop()
{
    const uint32_t nowMs = millis();
    if ((nowMs - g_lastWifiCheckMs) < kWifiCheckIntervalMs)
    {
        return;
    }
    g_lastWifiCheckMs = nowMs;

    const wifi_mode_t mode = WiFi.getMode();
    const IPAddress liveIp = WiFi.softAPIP();
    const bool apModeOk = mode == WIFI_AP || mode == WIFI_AP_STA;
    const bool ipOk = liveIp != IPAddress((uint32_t)0);

    if (!apModeOk || !ipOk)
    {
        g_wifiReady = false;
        return;
    }

    g_wifiReady = true;
    g_apIP = liveIp;
    g_udpBroadcastIP = IPAddress(g_apIP[0], g_apIP[1], g_apIP[2], 255);
    const uint8_t stationCount = WiFi.softAPgetStationNum();
    if (stationCount != g_lastStationCount)
    {
        g_lastStationCount = stationCount;
        WiFi.setSleep(false);
    }
}

void WiFi_fini()
{
    g_udp.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    g_wifiReady = false;
}

void WiFi_pauseUDP(void)
{
    g_udp.stop();
}

void WiFi_resumeUDP(void)
{
    if (!g_wifiReady)
    {
        return;
    }
    g_udp.stop();
    g_udp.begin(WiFi_defaultNmeaUdpPort());
}

const char* WiFi_ssid(void) { return host_name.c_str(); }
const char* WiFi_password(void) { return FLYRF_WIFI_AP_PASSWORD; }
IPAddress WiFi_apIP(void) { return g_apIP; }
String WiFi_macAddressStr(void) { return WiFi.softAPmacAddress(); }
bool WiFi_ready(void) { return g_wifiReady; }
uint16_t WiFi_defaultNmeaUdpPort(void) { return (uint16_t)FLYRF_DEFAULT_NMEA_UDP_PORT; }
IPAddress WiFi_udpBroadcastIP(void) { return g_udpBroadcastIP; }

bool WiFi_transmitUDP(uint16_t port, const uint8_t* data, size_t size)
{
    if (!g_wifiReady || data == nullptr || size == 0)
    {
        return false;
    }

    const uint16_t targetPort = (port != 0U) ? port : WiFi_defaultNmeaUdpPort();
    if (!g_udp.beginPacket(g_udpBroadcastIP, targetPort))
    {
        return false;
    }
    const size_t written = g_udp.write(data, size);
    const bool ok = g_udp.endPacket() == 1;
    return ok && written == size;
}
