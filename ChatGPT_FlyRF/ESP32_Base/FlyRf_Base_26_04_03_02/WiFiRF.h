
#pragma once
#include <Arduino.h>
#include <WiFi.h>

#ifndef FLYRF_WIFI_AP_SSID_PREFIX
#define FLYRF_WIFI_AP_SSID_PREFIX "FlyRf_Base"
#endif
#ifndef FLYRF_WIFI_AP_PASSWORD
#define FLYRF_WIFI_AP_PASSWORD "12345678"
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
