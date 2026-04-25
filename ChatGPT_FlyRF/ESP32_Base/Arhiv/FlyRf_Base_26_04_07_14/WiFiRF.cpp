#include "WiFiRF.h"
#include "Log.h"
#include "DeviceInfo.h"
#include "EEPROMRF.h"

String host_name;
static bool g_wifiReady = false;
static IPAddress g_apIP(192,168,1,1);
static IPAddress g_udpBroadcastIP(192,168,1,255);
static WiFiUDP g_udp;

void WiFi_setup()
{
    host_name = String(FLYRF_WIFI_AP_SSID_PREFIX) + "-" + DeviceInfo_chipIdHex();

    const IPAddress localIP(192,168,1,1);
    const IPAddress gateway(192,168,1,1);
    const IPAddress subnet(255,255,255,0);

    auto startAp = [&](void) -> bool
    {
        WiFi.persistent(false);
        WiFi.setSleep(false);
        WiFi.disconnect(true, true);
        delay(150);

        WiFi.mode(WIFI_OFF);
        delay(150);
        WiFi.mode(WIFI_AP);
        delay(150);

        const bool cfgOk = WiFi.softAPConfig(localIP, gateway, subnet);
        delay(100);
        const bool apOk = WiFi.softAP(host_name.c_str(), FLYRF_WIFI_AP_PASSWORD, 1, 0, 4);
        delay(250);

        g_apIP = WiFi.softAPIP();
        if (g_apIP == IPAddress((uint32_t)0))
        {
            g_apIP = localIP;
        }
        g_udpBroadcastIP = IPAddress(g_apIP[0], g_apIP[1], g_apIP[2], 255);
        g_udp.stop();
        g_udp.begin(WiFi_defaultNmeaUdpPort());
        g_wifiReady = cfgOk && apOk;
        return g_wifiReady;
    };

    bool ok = startAp();
    if (!ok)
    {
        Serial.println(F("[WIFI] AP start retry"));
        delay(300);
        ok = startAp();
    }

    Serial.print(F("[WIFI] SSID: "));
    Serial.println(host_name);
    Serial.print(F("[WIFI] PASS: "));
    Serial.println(FLYRF_WIFI_AP_PASSWORD);
    Serial.print(F("[WIFI] AP IP: "));
    Serial.println(g_apIP);
    Serial.print(F("[WIFI] Ready: "));
    Serial.println(ok ? F("YES") : F("NO"));
}

void WiFi_loop() {}

void WiFi_fini()
{
    g_udp.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    g_wifiReady = false;
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
    if (!g_wifiReady || data == nullptr || size == 0) return false;
    const uint16_t targetPort = (port != 0U) ? port : WiFi_defaultNmeaUdpPort();
    if (!g_udp.beginPacket(g_udpBroadcastIP, targetPort)) return false;
    const size_t written = g_udp.write(data, size);
    const bool ok = g_udp.endPacket() == 1;
    return ok && written == size;
}
