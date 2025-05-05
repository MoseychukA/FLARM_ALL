
#ifndef WIFIHELPER_H
#define WIFIHELPER_H

#include "SoC.h"

#if defined(ARDUINO) && !defined(EXCLUDE_WIFI)
#include <WiFiUdp.h>
#endif

#define HOSTNAME            FLYRF_IDENT
#ifndef UDP_PACKET_BUFSIZE
#define UDP_PACKET_BUFSIZE  256
#endif
#ifndef WIFI_STA_TIMEOUT
#define WIFI_STA_TIMEOUT    10000
#endif
#define WIFI_DHCP_LEASE_HRS 8

enum
{
    WIFI_PARAM_TX_POWER,
    WIFI_PARAM_DHCP_LEASE_TIME
};

void WiFi_setup(void);
void WiFi_loop(void);
size_t Raw_Receive_UDP(uint8_t*);
void Raw_Transmit_UDP(void);

extern String host_name;
#if defined(ARDUINO) && !defined(EXCLUDE_WIFI)
extern WiFiUDP Uni_Udp;
#endif

extern char UDPpacketBuffer[UDP_PACKET_BUFSIZE];

#endif /* WIFIHELPER_H */
