/*
  Модуль WiFiRF.h
  Назначение:
  - Публичный интерфейс Wi-Fi/UDP части проекта.

  Что содержит файл:
  - Константы сетевой конфигурации по умолчанию.
  - Объявления функций запуска Wi-Fi, передачи UDP и чтения сетевого состояния.
*/


#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#ifndef FLYRF_WIFI_AP_SSID_PREFIX
#define FLYRF_WIFI_AP_SSID_PREFIX "FlyRf_Base"
#endif
#ifndef FLYRF_WIFI_AP_PASSWORD
#define FLYRF_WIFI_AP_PASSWORD "12345678"
#endif
#ifndef UDP_PACKET_BUFSIZE
#define UDP_PACKET_BUFSIZE 256
#endif


#ifndef FLYRF_DEFAULT_NMEA_UDP_PORT
#define FLYRF_DEFAULT_NMEA_UDP_PORT 10110U
#endif

void WiFi_setup(void);
void WiFi_loop(void);
void WiFi_fini(void);

const char* WiFi_ssid(void);
const char* WiFi_password(void);
IPAddress WiFi_apIP(void);
String WiFi_macAddressStr(void);
bool WiFi_ready(void);

extern String host_name;  // Буфер, текстовая строка или рабочее сообщение.

uint16_t WiFi_defaultNmeaUdpPort(void);
bool WiFi_transmitUDP(uint16_t port, const uint8_t* data, size_t size);
IPAddress WiFi_udpBroadcastIP(void);
