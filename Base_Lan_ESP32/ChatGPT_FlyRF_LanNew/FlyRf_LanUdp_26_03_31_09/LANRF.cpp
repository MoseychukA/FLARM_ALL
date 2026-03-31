#include "LANRF.h"
#include "NMEA.h"
#include "EEPROMRF.h"
#include "Log.h"
#include <SPI.h>
#include <Ethernet2.h>
#include <EthernetUdp2.h>

class EthernetServerCompat : public EthernetServer
{
public:
    explicit EthernetServerCompat(uint16_t port) : EthernetServer(port) {}
    void begin(uint16_t port = 0) override
    {
        (void)port;
        EthernetServer::begin();
    }
};

#define W5500_CS    19
#define W5500_SCLK  12
#define W5500_MISO  13
#define W5500_MOSI  11
#define W5500_INT   16

static EthernetUDP g_udp;
static EthernetClient g_tcpClient;
static bool g_lanReady = false;
static byte g_mac[6] = { 0x02, 0x46, 0x6C, 0x79, 0x52, 0x46 };
static IPAddress g_localIp(0,0,0,0);
static IPAddress g_fallbackIp(192,168,1,247);
static IPAddress g_fallbackGateway(192,168,1,1);
static IPAddress g_fallbackSubnet(255,255,255,0);
static IPAddress g_fallbackDns(192,168,1,1);
static TaskHandle_t g_dhcpTask = nullptr;
static portMUX_TYPE g_lanCountersMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t g_udpTxPackets = 0;
static volatile uint32_t g_udpRxPackets = 0;
static volatile uint32_t g_tcpTxPackets = 0;
static volatile uint32_t g_tcpRxPackets = 0;
static volatile uint32_t g_lastUdpActivityMs = 0;
static volatile uint32_t g_lastDhcpAttemptMs = 0;
static volatile bool g_dhcpTaskRunning = false;
static volatile bool g_dhcpLeaseActive = false;
static volatile bool g_lanConfigBusy = false;
static volatile bool g_linkUp = false;
static volatile uint32_t g_lastLinkCheckMs = 0;

static bool ipArrayIsValid(const uint8_t ip[4])
{
    return ip && (ip[0] || ip[1] || ip[2] || ip[3]);
}

static IPAddress ipFromSettings(const uint8_t ip[4], const IPAddress& fallback)
{
    if (!ipArrayIsValid(ip)) return fallback;
    return IPAddress(ip[0], ip[1], ip[2], ip[3]);
}

static void buildMacFromChipId()
{
    const uint64_t chipid = ESP.getEfuseMac();
    g_mac[0] = 0x02;
    g_mac[1] = (uint8_t)(chipid >> 40);
    g_mac[2] = (uint8_t)(chipid >> 32);
    g_mac[3] = (uint8_t)(chipid >> 24);
    g_mac[4] = (uint8_t)(chipid >> 16);
    g_mac[5] = (uint8_t)(chipid >> 8);
}


static uint8_t w5500ReadCommonReg(uint16_t addr)
{
    SPI.beginTransaction(SPISettings(14000000, MSBFIRST, SPI_MODE0));
    digitalWrite(W5500_CS, LOW);
    SPI.transfer((uint8_t)(addr >> 8));
    SPI.transfer((uint8_t)(addr & 0xFF));
    SPI.transfer((uint8_t)0x00);
    uint8_t value = SPI.transfer((uint8_t)0x00);
    digitalWrite(W5500_CS, HIGH);
    SPI.endTransaction();
    return value;
}

static bool readW5500LinkState()
{
    const uint8_t phycfgr = w5500ReadCommonReg(0x002E);
    return (phycfgr & 0x01) != 0;
}

static void loadFallbackNetworkFromSettings()
{
    g_fallbackIp      = settings ? ipFromSettings(settings->g_localIP,    IPAddress(192,168,1,247)) : IPAddress(192,168,1,247);
    g_fallbackGateway = settings ? ipFromSettings(settings->g_gatewayIP,  IPAddress(192,168,1,1))   : IPAddress(192,168,1,1);
    g_fallbackSubnet  = settings ? ipFromSettings(settings->g_subnetMask, IPAddress(255,255,255,0)) : IPAddress(255,255,255,0);
    g_fallbackDns     = settings ? ipFromSettings(settings->g_dns_server, g_fallbackGateway)         : g_fallbackGateway;
}

static void applyStaticLanConfig()
{
    Ethernet.begin(g_mac, g_fallbackIp, g_fallbackDns, g_fallbackGateway, g_fallbackSubnet);
    delay(20);
    g_localIp = Ethernet.localIP();
}

static void restartLanSockets()
{
    g_udp.stop();
    if (g_tcpClient)
    {
        g_tcpClient.stop();
    }
    g_udp.begin(NMEA_UDP_PORT);
}

static void dhcpTask(void*)
{
    g_lanConfigBusy = true;
    g_dhcpTaskRunning = true;
    g_lastDhcpAttemptMs = millis();

    restartLanSockets();
    const int dhcpOk = Ethernet.begin(g_mac);

    if (dhcpOk == 1)
    {
        g_localIp = Ethernet.localIP();
        g_dhcpLeaseActive = true;
        Serial.print(F("[LAN] DHCP OK: "));
        Serial.println(g_localIp);
    }
    else
    {
        applyStaticLanConfig();
        g_dhcpLeaseActive = false;
        Serial.print(F("[LAN] DHCP timeout, static IP: "));
        Serial.println(g_localIp);
    }

    restartLanSockets();
    g_lanConfigBusy = false;
    g_dhcpTaskRunning = false;
    g_dhcpTask = nullptr;
    vTaskDelete(nullptr);
}

static void startDhcpTaskIfNeeded()
{
    if (!g_lanReady || g_dhcpTaskRunning || g_lanConfigBusy || !g_linkUp) return;
    BaseType_t ok = xTaskCreatePinnedToCore(dhcpTask, "LAN_DHCP", 4096, nullptr, 1, &g_dhcpTask, 1);
    if (ok != pdPASS)
    {
        g_dhcpTask = nullptr;
        g_dhcpTaskRunning = false;
        g_lanConfigBusy = false;
    }
}

void LAN_setup()
{
    buildMacFromChipId();
    loadFallbackNetworkFromSettings();

    pinMode(W5500_INT, INPUT);
    SPI.begin(W5500_SCLK, W5500_MISO, W5500_MOSI, W5500_CS);
    pinMode(W5500_CS, OUTPUT);
    digitalWrite(W5500_CS, HIGH);

    Ethernet.init(W5500_CS);

    g_lanReady = false;
    g_localIp = IPAddress(0,0,0,0);
    g_dhcpTask = nullptr;
    g_dhcpTaskRunning = false;
    g_dhcpLeaseActive = false;
    g_lanConfigBusy = false;
    g_linkUp = readW5500LinkState();
    g_lastLinkCheckMs = millis();
    portENTER_CRITICAL(&g_lanCountersMux);
    g_udpTxPackets = 0;
    g_udpRxPackets = 0;
    g_tcpTxPackets = 0;
    g_tcpRxPackets = 0;
    g_lastUdpActivityMs = 0;
    g_lastDhcpAttemptMs = 0;
    portEXIT_CRITICAL(&g_lanCountersMux);

    applyStaticLanConfig();
    restartLanSockets();
    g_lanReady = true;

    Serial.print(F("[LAN] Static start IP: "));
    Serial.println(g_localIp);
    Serial.print(F("[LAN] Link: "));
    Serial.println(g_linkUp ? F("CONNECTED") : F("DISCONNECTED"));

    startDhcpTaskIfNeeded();
}

void LAN_loop()
{
    if (!g_lanReady) return;

    if ((millis() - g_lastLinkCheckMs) >= 1000UL)
    {
        g_lastLinkCheckMs = millis();
        const bool linkNow = readW5500LinkState();
        if (linkNow != g_linkUp)
        {
            g_linkUp = linkNow;
            Serial.print(F("[LAN] Link "));
            Serial.println(g_linkUp ? F("CONNECTED") : F("DISCONNECTED"));
            if (!g_linkUp)
            {
                g_dhcpLeaseActive = false;
            }
            else if (!g_dhcpTaskRunning)
            {
                startDhcpTaskIfNeeded();
            }
        }
    }

    if (g_lanConfigBusy) return;
    if (!g_linkUp) return;

    if (g_dhcpLeaseActive)
    {
        Ethernet.maintain();
        g_localIp = Ethernet.localIP();
    }
    else if (!g_dhcpTaskRunning && (millis() - g_lastDhcpAttemptMs >= 30000UL))
    {
        startDhcpTaskIfNeeded();
    }


    int udpPacketSize = g_udp.parsePacket();
    if (udpPacketSize > 0)
    {
        while (udpPacketSize > 0)
        {
            (void)g_udp.read();
            --udpPacketSize;
        }
        portENTER_CRITICAL(&g_lanCountersMux);
        ++g_udpRxPackets;
        g_lastUdpActivityMs = millis();
        portEXIT_CRITICAL(&g_lanCountersMux);
    }

}

void LAN_fini()
{
    g_udp.stop();
    if (g_tcpClient) g_tcpClient.stop();
    g_lanReady = false;
    g_localIp = IPAddress(0,0,0,0);
    g_linkUp = false;
}

void LAN_sendUDP(const uint8_t* data, size_t len)
{
    if (!g_lanReady || g_lanConfigBusy || !data || !len) return;
    g_udp.beginPacket(IPAddress(255,255,255,255), NMEA_UDP_PORT);
    g_udp.write(data, len);
    if (g_udp.endPacket())
    {
        portENTER_CRITICAL(&g_lanCountersMux);
        ++g_udpTxPackets;
        g_lastUdpActivityMs = millis();
        portEXIT_CRITICAL(&g_lanCountersMux);
    }
}


bool LAN_ready() { return g_lanReady; }
IPAddress LAN_localIP() { return g_localIp; }
String LAN_localIPStr() { return g_localIp.toString(); }
String LAN_macAddressStr()
{
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", g_mac[0], g_mac[1], g_mac[2], g_mac[3], g_mac[4], g_mac[5]);
    return String(buf);
}
String LAN_tcpAddressStr()
{
    return LAN_localIPStr();
}

void LAN_getPacketCounters(uint32_t& txPackets, uint32_t& rxPackets, uint32_t& udpTxPackets, uint32_t& udpRxPackets, uint32_t& tcpTxPackets, uint32_t& tcpRxPackets)
{
    portENTER_CRITICAL(&g_lanCountersMux);
    udpTxPackets = g_udpTxPackets;
    udpRxPackets = g_udpRxPackets;
    tcpTxPackets = g_tcpTxPackets;
    tcpRxPackets = g_tcpRxPackets;
    portEXIT_CRITICAL(&g_lanCountersMux);

    txPackets = udpTxPackets + tcpTxPackets;
    rxPackets = udpRxPackets + tcpRxPackets;
}

bool LAN_udpWorking()
{
    uint32_t lastUdpActivityMs = 0;
    portENTER_CRITICAL(&g_lanCountersMux);
    lastUdpActivityMs = g_lastUdpActivityMs;
    portEXIT_CRITICAL(&g_lanCountersMux);

    if (!g_lanReady) return false;
    if (lastUdpActivityMs == 0) return false;
    return (millis() - lastUdpActivityMs) <= 5000UL;
}

String LAN_udpStatusStr()
{
    return LAN_udpWorking() ? F("UDP Ok") : F("UDP No");
}

bool LAN_linkUp()
{
    return g_lanReady && g_linkUp;
}

String LAN_linkStatusStr()
{
    if (!g_lanReady) return F("LAN Off");
    return g_linkUp ? F("LAN On") : F("LAN Off");
}

bool LAN_dhcpLeaseActive()
{
    return g_dhcpLeaseActive;
}
