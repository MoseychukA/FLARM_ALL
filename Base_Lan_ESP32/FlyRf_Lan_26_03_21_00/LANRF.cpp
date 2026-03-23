#include "LANRF.h"
#include <SPI.h>
//#include <Ethernet.h>
//#include <EthernetUdp.h>         // UDP library from: bjoern@cs.stanford.edu 12/30/2008

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
EthernetUDP Lan_Udp;
char LANpacketBuffer[LAN_PACKET_BUFSIZE];

void LAN_setup() {
  SPI.begin(W5500_SCLK, W5500_MISO, W5500_MOSI, W5500_CS);
  pinMode(W5500_RESET, OUTPUT);
  digitalWrite(W5500_RESET, LOW);
  delay(10);
  digitalWrite(W5500_RESET, HIGH);
  delay(100);
  pinMode(W5500_CS, OUTPUT);         // устанавливает режим работы
  digitalWrite(W5500_CS, HIGH);

  //Ethernet.begin(W5500_CS);
  //Ethernet2.begin(W5500_CS);

  if (Ethernet.begin(mac) == 0) 
  {
    Serial.println(F("Failed to get Ethernet via DHCP, try static..."));
    // Например, IP по умолчанию:
    IPAddress ip(192,168,75,110);
    IPAddress gw(192,168,75,1);
    IPAddress mask(255,255,255,0);
    Ethernet.begin(mac, ip, gw, gw, mask);
    Serial.print(F("Ethernet static IP: "));
    Serial.println(Ethernet.localIP());
  }
  else 
  {
    Serial.println(F("Ethernet via DHCP"));
    Serial.print(F("My IP: "));
    Serial.println(Ethernet.localIP());
  }
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

void LAN_fini() {
  Lan_Udp.stop();
  //Ethernet.stop();
}