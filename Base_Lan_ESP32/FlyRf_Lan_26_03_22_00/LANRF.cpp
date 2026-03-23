#include "LANRF.h"
#include <SPI.h>
#include "EEPROMRF.h"
#include <Ethernet2.h>
#include <EthernetUdp2.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
EthernetUDP Lan_Udp;
char LANpacketBuffer[LAN_PACKET_BUFSIZE];

void LAN_setup() 
{
    pinMode(W5500_INT, INPUT);         // устанавливает режим работы
    SPI.begin(W5500_SCLK, W5500_MISO, W5500_MOSI, W5500_CS);
    pinMode(W5500_RESET, OUTPUT);
    digitalWrite(W5500_RESET, LOW);
    delay(10);
    digitalWrite(W5500_RESET, HIGH);
    delay(100);
    pinMode(W5500_CS, OUTPUT);         // устанавливает режим работы
    digitalWrite(W5500_CS, HIGH);

    IPAddress localIP(settings->g_localIP[0], settings->g_localIP[1], settings->g_localIP[2], settings->g_localIP[3]);
    IPAddress gatewayIP(settings->g_gatewayIP[0], settings->g_gatewayIP[1], settings->g_gatewayIP[2], settings->g_gatewayIP[3]);
    IPAddress subnetMaskUDP(settings->g_subnetMask[0], settings->g_subnetMask[1], settings->g_subnetMask[2], settings->g_subnetMask[3]);
    IPAddress dnsServer(settings->g_dns_server[0], settings->g_dns_server[1], settings->g_dns_server[2], settings->g_dns_server[3]);
    Ethernet.begin(mac, localIP, dnsServer, gatewayIP, subnetMask);
 
    Lan_Udp.begin(LAN_UDP_PORT);
    Serial.print(F("Ethernet UDP started at port: "));
    Serial.println(LAN_UDP_PORT);
}

void LAN_loop() 
{
  int pktSize = Lan_Udp.parsePacket();
  if (pktSize > 0 && pktSize < LAN_PACKET_BUFSIZE) 
  {
    Lan_Udp.read(LANpacketBuffer, pktSize);
    LANpacketBuffer[pktSize] = '\0';
    Serial.print(F("LAN UDP Received: "));
    Serial.println(LANpacketBuffer);
  }
}

size_t Raw_Receive_UDP_LAN(uint8_t *buf, size_t maxlen) 
{
  int pktSize = Lan_Udp.parsePacket();
  if (pktSize > 0 && pktSize < maxlen) {
    return Lan_Udp.read(buf, pktSize);
  }
  return 0;
}

void Raw_Transmit_UDP_LAN(const uint8_t *buf, size_t len, IPAddress dst_ip, uint16_t dst_port) 
{

  Lan_Udp.beginPacket(dst_ip, dst_port);
  Lan_Udp.write(buf, len);
  Lan_Udp.endPacket();
}

void LAN_fini() 
{
  Lan_Udp.stop();
}