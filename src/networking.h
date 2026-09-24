#pragma once

#include <Arduino.h>

#include <WiFi.h>
#include <Ethernet.h>

enum NetworkMode {
    NETWORK_OFF = 0,
    NETWORK_WIFI_STA = 1,
    NETWORK_WIFI_AP = 2,
    NETWORK_WIFI_AP_STA = 3,
    NETWORK_ETHERNET = 4
};

bool setNetworkMode(NetworkMode mode, const char* ssid = nullptr, const char* password = nullptr, uint8_t channel = 1, bool hidden = false, uint8_t maxConnections = 4);
bool setNetworkStaticIp(const IPAddress& ip,
                        const IPAddress& gateway = IPAddress(0, 0, 0, 0),
                        const IPAddress& subnet = IPAddress(255, 255, 255, 0),
                        const IPAddress& dnsPrimary = IPAddress(0, 0, 0, 0),
                        const IPAddress& dnsSecondary = IPAddress(0, 0, 0, 0));
bool restartNetworkWithStaticIp(const char* ssid = nullptr,
                                const char* password = nullptr,
                                uint8_t channel = 1,
                                bool hidden = false,
                                uint8_t maxConnections = 4,
                                uint32_t timeoutMs = 20000);
void clearNetworkStaticIp();
void stopNetwork();
void updateNetwork();
bool networkConnected();
String getNetworkIpAddress();
String getNetworkMacAddress();
int8_t getNetworkSignalStrength();

int scanWirelessNetworks(uint32_t timeoutMs = 10000, bool async = false, bool passive = false, uint32_t maxMsPerChannel = 200);
int scanWiFiNetworks(uint32_t timeoutMs = 10000, bool passive = false, uint32_t maxMsPerChannel = 200);
int printAvailableNetworks(Stream& output = Serial);
String getScannedNetworkSsid(int index);
int8_t getScannedNetworkRssi(int index);
int getScannedNetworkChannel(int index);
int getScannedNetworkCount();
void clearScannedNetworks();

bool startNetworkStation(const char* ssid, const char* password = nullptr, uint32_t timeoutMs = 20000);
bool startNetworkAccessPoint(const char* ssid, const char* password = nullptr, uint8_t channel = 1, bool hidden = false, uint8_t maxConnections = 4);
bool startNetworkEthernet(const char* hostname = nullptr, uint32_t timeoutMs = 20000);
