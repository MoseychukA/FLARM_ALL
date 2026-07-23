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
static IPAddress g_fallbackIp(192,168,75,247);
static IPAddress g_fallbackGateway(192,168,75,1);
static IPAddress g_fallbackSubnet(255,255,255,0);
static IPAddress g_fallbackDns(8,8,8,8);
static portMUX_TYPE g_lanCountersMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t g_udpTxPackets = 0;
static volatile uint32_t g_udpRxPackets = 0;
static volatile uint32_t g_tcpTxPackets = 0;
static volatile uint32_t g_tcpRxPackets = 0;
static volatile uint32_t g_lastUdpActivityMs = 0;
static volatile uint32_t g_lastDhcpAttemptMs = 0;
static volatile bool g_dhcpLeaseActive = false;
static volatile bool g_lanConfigBusy = false;
static volatile bool g_linkUp = false;
static volatile uint32_t g_lastLinkCheckMs = 0;
static volatile bool g_udpSocketReady = false;
static volatile uint16_t g_udpSocketPort = 0;


static uint16_t configuredUdpPort()
{
    const uint16_t port = (settings != nullptr) ? settings->udp_port : 0U;
    return (port != 0U) ? port : (uint16_t)NMEA_UDP_PORT;
}

static const uint32_t LAN_DHCP_START_DELAY_MS = 15000UL;
static const uint32_t LAN_DHCP_RETRY_INTERVAL_MS = 60000UL;
static const unsigned long LAN_DHCP_TIMEOUT_MS = 1200UL;
static const unsigned long LAN_DHCP_RESPONSE_TIMEOUT_MS = 250UL;
static volatile uint32_t g_lastDhcpMaintainMs = 0;

static bool ipArrayHasAnyValue(const uint8_t ip[4])
{
    return ip && (ip[0] || ip[1] || ip[2] || ip[3]);
}

static bool isUnicastIpv4(const uint8_t ip[4])
{
    if (!ipArrayHasAnyValue(ip)) return false;
    if (ip[0] == 0 || ip[0] == 255) return false;
    if (ip[3] == 0 || ip[3] == 255) return false;
    return true;
}

static bool isValidSubnetMask(const uint8_t ip[4])
{
    if (!ipArrayHasAnyValue(ip)) return false;
    uint32_t mask = ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | ((uint32_t)ip[2] << 8) | (uint32_t)ip[3];
    if (mask == 0 || mask == 0xFFFFFFFFUL) return false;
    bool seenZero = false;
    for (int i = 31; i >= 0; --i)
    {
        const bool bit = (mask >> i) & 1U;
        if (!bit) seenZero = true;
        else if (seenZero) return false;
    }
    return true;
}

static IPAddress ipFromSettingsChecked(const uint8_t ip[4], const IPAddress& fallback, bool subnet = false)
{
    if (subnet)
    {
        if (!isValidSubnetMask(ip)) return fallback;
    }
    else
    {
        if (!isUnicastIpv4(ip)) return fallback;
    }
    return IPAddress(ip[0], ip[1], ip[2], ip[3]);
}

static bool isValidIpAddress(const IPAddress& ip)
{
    return !((uint32_t)ip == 0UL) && ip[0] != 0 && ip[0] != 255 && ip[3] != 0 && ip[3] != 255;
}

static void refreshLocalIpFromEthernet();

static bool hasEffectiveDhcpLease()
{
    refreshLocalIpFromEthernet();
    return g_dhcpLeaseActive && g_linkUp && isValidIpAddress(g_localIp) && (g_localIp != g_fallbackIp);
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

static IPAddress currentSubnetMask()
{
    const IPAddress liveMask = Ethernet.subnetMask();
    const uint8_t maskBytes[4] = { liveMask[0], liveMask[1], liveMask[2], liveMask[3] };
    if (isValidSubnetMask(maskBytes))
    {
        return liveMask;
    }
    return g_fallbackSubnet;
}

static void refreshLocalIpFromEthernet()
{
    const IPAddress liveIp = Ethernet.localIP();
    if (isValidIpAddress(liveIp))
    {
        g_localIp = liveIp;
    }
}

static void restartLanSockets();

static void handleLinkStateChange(bool linkNow)
{
    if (linkNow == g_linkUp)
    {
        return;
    }

    g_linkUp = linkNow;
    if (!g_linkUp)
    {
        g_dhcpLeaseActive = false;
        g_lanConfigBusy = false;
        g_localIp = IPAddress(0,0,0,0);
        g_udp.stop();
        if (g_tcpClient)
        {
            g_tcpClient.stop();
        }
        g_udpSocketReady = false;
    g_udpSocketPort = 0;
        return;
    }

    g_lastDhcpAttemptMs = 0;
    restartLanSockets();
}

static void loadFallbackNetworkFromSettings()
{
    const IPAddress defaultIp(192,168,75,247);
    const IPAddress defaultGateway(192,168,75,1);
    const IPAddress defaultSubnet(255,255,255,0);
    const IPAddress defaultDns(8,8,8,8);

    if (settings == nullptr)
    {
        g_fallbackIp = defaultIp;
        g_fallbackGateway = defaultGateway;
        g_fallbackSubnet = defaultSubnet;
        g_fallbackDns = defaultDns;
        return;
    }

    g_fallbackIp      = ipFromSettingsChecked(settings->g_localIP, defaultIp, false);
    g_fallbackGateway = ipFromSettingsChecked(settings->g_gatewayIP, defaultGateway, false);
    g_fallbackSubnet  = ipFromSettingsChecked(settings->g_subnetMask, defaultSubnet, true);
    g_fallbackDns     = ipFromSettingsChecked(settings->g_dns_server, defaultDns, false);
}

static void applyStaticLanConfig()
{
    g_lanConfigBusy = true;
    Ethernet.begin(g_mac, g_fallbackIp, g_fallbackDns, g_fallbackGateway, g_fallbackSubnet);
    delay(20);
    refreshLocalIpFromEthernet();
    if (!isValidIpAddress(g_localIp))
    {
        g_localIp = g_fallbackIp;
    }
    g_dhcpLeaseActive = false;
    g_lanConfigBusy = false;
    g_lastDhcpMaintainMs = 0;
}

static void tryDhcpLease()
{
    if (!g_lanReady || g_lanConfigBusy || !g_linkUp) return;

    g_lanConfigBusy = true;
    g_lastDhcpAttemptMs = millis();
    g_dhcpLeaseActive = false;

    restartLanSockets();
    yield();
    g_localIp = IPAddress(0,0,0,0);

    const int dhcpResult = Ethernet.begin(g_mac);
    delay(10);
    yield();

    refreshLocalIpFromEthernet();
    const bool dhcpOk = (dhcpResult == 1) && isValidIpAddress(g_localIp);
    if (dhcpOk)
    {
        const IPAddress liveMask = Ethernet.subnetMask();
        const uint8_t maskBytes[4] = { liveMask[0], liveMask[1], liveMask[2], liveMask[3] };
        if (isValidSubnetMask(maskBytes))
        {
            g_fallbackSubnet = liveMask;
        }
        g_dhcpLeaseActive = true;
    }
    else
    {
        applyStaticLanConfig();
    }

    restartLanSockets();
    g_lanConfigBusy = false;
}

static void restartLanSockets()
{
    g_udp.stop();
    if (g_tcpClient)
    {
        g_tcpClient.stop();
    }
    const uint16_t udpPort = configuredUdpPort();
    g_udpSocketReady = (g_udp.begin(udpPort) == 1);
    g_udpSocketPort = g_udpSocketReady ? udpPort : 0U;
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
    g_udpSocketReady = false;
    g_udpSocketPort = 0;
    g_localIp = IPAddress(0,0,0,0);
    g_linkUp = readW5500LinkState();
    g_lastLinkCheckMs = millis();
    g_lastDhcpAttemptMs = 0;
    g_lastDhcpMaintainMs = 0;
    g_dhcpLeaseActive = false;
    g_lanConfigBusy = false;

    portENTER_CRITICAL(&g_lanCountersMux);
    g_udpTxPackets = 0;
    g_udpRxPackets = 0;
    g_tcpTxPackets = 0;
    g_tcpRxPackets = 0;
    g_lastUdpActivityMs = 0;
    portEXIT_CRITICAL(&g_lanCountersMux);

    applyStaticLanConfig();

    // Не открываем UDP-сокет вслепую на этапе setup, пока линк не подтвержден.
    // Это уменьшает нагрузку во время старта и исключает повторные открытия порта
    // на нестабильном питании/линке. Сокет будет открыт позже из LAN_loop().
    if (g_linkUp)
    {
        restartLanSockets();
    }
    else
    {
        g_udpSocketReady = false;
        g_udpSocketPort = 0U;
    }

    g_lanReady = true;
}

void LAN_loop()
{
    if (!g_lanReady) return;

    const uint32_t nowMs = millis();
    if ((uint32_t)(nowMs - g_lastLinkCheckMs) >= 1000UL)
    {
        g_lastLinkCheckMs = nowMs;
        handleLinkStateChange(readW5500LinkState());
        refreshLocalIpFromEthernet();
        const uint16_t udpPortNow = configuredUdpPort();
        if (g_linkUp && (!g_udpSocketReady || g_udpSocketPort != udpPortNow))
        {
            restartLanSockets();
        }
    }

    if (g_linkUp && !g_lanConfigBusy)
    {
        if (g_dhcpLeaseActive)
        {
            if ((uint32_t)(nowMs - g_lastDhcpMaintainMs) >= 5000UL)
            {
                g_lastDhcpMaintainMs = nowMs;
                (void)Ethernet.maintain();
                refreshLocalIpFromEthernet();
            }
        }
        else if ((g_lastDhcpAttemptMs == 0 && nowMs >= LAN_DHCP_START_DELAY_MS) || ((uint32_t)(nowMs - g_lastDhcpAttemptMs) >= LAN_DHCP_RETRY_INTERVAL_MS))
        {
            tryDhcpLease();
            refreshLocalIpFromEthernet();
        }
    }

    if (g_lanConfigBusy) return;

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
    g_udpSocketReady = false;
    g_udpSocketPort = 0;
    g_localIp = IPAddress(0,0,0,0);
    g_linkUp = false;
}

static IPAddress currentBroadcastIp()
{
    const IPAddress ip = g_localIp;
    const IPAddress mask = currentSubnetMask();
    const uint8_t maskBytes[4] = { mask[0], mask[1], mask[2], mask[3] };
    if (isValidIpAddress(ip) && isValidSubnetMask(maskBytes))
    {
        return IPAddress((uint8_t)(ip[0] | (uint8_t)~mask[0]),
                         (uint8_t)(ip[1] | (uint8_t)~mask[1]),
                         (uint8_t)(ip[2] | (uint8_t)~mask[2]),
                         (uint8_t)(ip[3] | (uint8_t)~mask[3]));
    }
    return IPAddress(255,255,255,255);
}

void LAN_sendUDP(const uint8_t* data, size_t len)
{
    if (!g_lanReady || !g_udpSocketReady || !data || !len) return;

    const IPAddress dst = currentBroadcastIp();

    bool sent = false;
    const uint16_t udpPort = configuredUdpPort();

    if (g_udp.beginPacket(dst, udpPort) == 1)
    {
        if (g_udp.write(data, len) == len)
        {
            sent = (g_udp.endPacket() == 1);
        }
        else
        {
            (void)g_udp.endPacket();
        }
    }

    if (!sent && dst != IPAddress(255,255,255,255))
    {
        if (g_udp.beginPacket(IPAddress(255,255,255,255), udpPort) == 1)
        {
            if (g_udp.write(data, len) == len)
            {
                sent = (g_udp.endPacket() == 1);
            }
            else
            {
                (void)g_udp.endPacket();
            }
        }
    }

    if (sent)
    {
        portENTER_CRITICAL(&g_lanCountersMux);
        ++g_udpTxPackets;
        g_lastUdpActivityMs = millis();
        portEXIT_CRITICAL(&g_lanCountersMux);
    }
    else
    {
        restartLanSockets();
    }
}

bool LAN_ready() { return g_lanReady; }
IPAddress LAN_localIP()
{
    refreshLocalIpFromEthernet();
    return g_localIp;
}
String LAN_localIPStr()
{
    refreshLocalIpFromEthernet();
    return g_localIp.toString();
}
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
    if (!g_lanReady) return false;

    const bool udpEnabled = (settings != nullptr) && ((settings->nmea_out & NMEA_OUT_UDP) != 0);
    if (!udpEnabled) return false;

    refreshLocalIpFromEthernet();
    return g_linkUp && g_udpSocketReady && isValidIpAddress(g_localIp);
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
    return LAN_linkUp() ? F("LAN On") : F("LAN Off");
}

bool LAN_dhcpLeaseActive()
{
    return hasEffectiveDhcpLease();
}


String LAN_addressModeStr()
{
    if (!g_lanReady) return F("Off");
    if (hasEffectiveDhcpLease()) return F("DHCP");
    if (g_linkUp)
    {
        const uint32_t nowMs = millis();
        if ((g_lastDhcpAttemptMs == 0 && nowMs < LAN_DHCP_START_DELAY_MS) ||
            (g_lastDhcpAttemptMs != 0 && (uint32_t)(nowMs - g_lastDhcpAttemptMs) < LAN_DHCP_RETRY_INTERVAL_MS))
        {
            return F("DHCP...");
        }
    }
    return F("Static");
}
