#include "WiFiRF.h"
#include "Log.h"
#include "DeviceInfo.h"
#include "EEPROMRF.h"

String host_name;
WiFiUDP Uni_Udp;
char UDPpacketBuffer[UDP_PACKET_BUFSIZE] = {0};
static bool g_wifiReady = false;
static IPAddress g_apIP(192,168,1,1);

void WiFi_setup()
{
    host_name = String(FLYRF_WIFI_AP_SSID_PREFIX) + "-" + DeviceInfo_chipIdHex();

    WiFi.persistent(false);
    WiFi.disconnect(true, true);
    delay(100);

    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_AP);
    delay(100);

    IPAddress localIP(192,168,1,1);
    IPAddress gateway(192,168,1,1);
    IPAddress subnet(255,255,255,0);

    WiFi.softAPConfig(localIP, gateway, subnet);
    delay(100);
    const bool apOk = WiFi.softAP(host_name.c_str(), FLYRF_WIFI_AP_PASSWORD, 1, 0, 4);
    delay(150);

    g_apIP = WiFi.softAPIP();
    g_wifiReady = apOk && (g_apIP[0] != 0 || g_apIP[1] != 0 || g_apIP[2] != 0 || g_apIP[3] != 0);

    (void)g_wifiReady;
}

void WiFi_loop() {}

void WiFi_fini()
{
    Uni_Udp.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    g_wifiReady = false;
}

const char* WiFi_ssid(void) { return host_name.c_str(); }
const char* WiFi_password(void) { return FLYRF_WIFI_AP_PASSWORD; }
IPAddress WiFi_apIP(void) { return g_apIP; }
String WiFi_macAddressStr(void) { return WiFi.softAPmacAddress(); }
bool WiFi_ready(void) { return g_wifiReady; }
