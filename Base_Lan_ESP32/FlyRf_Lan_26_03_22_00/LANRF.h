#ifndef LANRF_H
#define LANRF_H

#include <Ethernet2.h>
#include <EthernetUdp2.h>

// Настройки SPI для W5500
#define W5500_CS    19
#define W5500_SCLK  12
#define W5500_MISO  13
#define W5500_MOSI  11
#define W5500_RESET 20
#define W5500_INT   16

#define LAN_PACKET_BUFSIZE 256
#define LAN_UDP_PORT 10110
const IPAddress dstIp(192, 168, 1, 100); // IP получателя вашего UDP

extern EthernetUDP Lan_Udp;
extern char LANpacketBuffer[LAN_PACKET_BUFSIZE];

void LAN_setup();
void LAN_loop();
size_t Raw_Receive_UDP_LAN(uint8_t *buf, size_t maxlen);
void Raw_Transmit_UDP_LAN(const uint8_t *buf, size_t len, IPAddress dst_ip, uint16_t dst_port);
void LAN_fini();

#endif // LANRF_H