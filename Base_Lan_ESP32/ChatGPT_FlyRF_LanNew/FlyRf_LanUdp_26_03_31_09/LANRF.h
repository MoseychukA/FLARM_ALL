#pragma once
#include <Arduino.h>

void LAN_setup();
void LAN_loop();
void LAN_fini();
void LAN_sendUDP(const uint8_t* data, size_t len);
bool LAN_ready();
IPAddress LAN_localIP();
String LAN_localIPStr();
String LAN_macAddressStr();
String LAN_tcpAddressStr();
void LAN_getPacketCounters(uint32_t& txPackets, uint32_t& rxPackets, uint32_t& udpTxPackets, uint32_t& udpRxPackets, uint32_t& tcpTxPackets, uint32_t& tcpRxPackets);
bool LAN_udpWorking();
String LAN_udpStatusStr();
bool LAN_linkUp();
String LAN_linkStatusStr();
bool LAN_dhcpLeaseActive();
