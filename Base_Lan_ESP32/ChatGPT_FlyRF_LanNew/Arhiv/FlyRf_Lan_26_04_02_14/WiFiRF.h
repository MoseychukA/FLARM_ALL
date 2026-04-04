
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#ifndef FLYRF_WIFI_AP_SSID_PREFIX
#define FLYRF_WIFI_AP_SSID_PREFIX "FlyRf_LanUdp"
#endif
#ifndef FLYRF_WIFI_AP_PASSWORD
#define FLYRF_WIFI_AP_PASSWORD "12345678"
#endif
#ifndef UDP_PACKET_BUFSIZE
#define UDP_PACKET_BUFSIZE 256
#endif

void WiFi_setup(void);
void WiFi_loop(void);
void WiFi_fini(void);

const char* WiFi_ssid(void);
const char* WiFi_password(void);
IPAddress WiFi_apIP(void);
String WiFi_macAddressStr(void);
bool WiFi_ready(void);

extern String host_name;
extern WiFiUDP Uni_Udp;
extern char UDPpacketBuffer[UDP_PACKET_BUFSIZE];
