#include "networking.h"
#include "pinout.h"

#include <SPI.h>

namespace {
    NetworkMode currentNetworkMode = NETWORK_OFF;
    bool currentNetworkConnected = false;
    int lastScanCount = 0;
    bool staticIpConfigured = false;
    IPAddress configuredIp(0, 0, 0, 0);
    IPAddress configuredGateway(0, 0, 0, 0);
    IPAddress configuredSubnet(255, 255, 255, 0);
    IPAddress configuredDnsPrimary(0, 0, 0, 0);
    IPAddress configuredDnsSecondary(0, 0, 0, 0);

    void warnUnsupportedPlatform()
    {
        Serial.println("Slotworks networking is only intended for ESP32-S3 hardware.");
    }

    bool wifiAvailable()
    {
        //wifi is always available since this is an ESP32-S3 project and that chip has built-in wifi
        return true;
    }

    void resetWifiState()
    {
        WiFi.disconnect(true, true);
        delay(50);
        WiFi.mode(WIFI_OFF);
        delay(50);
    }

    void resetEthernetState()
    {
        Ethernet.init(W5500_SCSN);
        delay(50);
    }
}

int scanWirelessNetworks(uint32_t timeoutMs, bool async, bool passive, uint32_t maxMsPerChannel)
{
    if (!wifiAvailable()) {
        warnUnsupportedPlatform();
        return 0;
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    lastScanCount = WiFi.scanNetworks(async, false, passive, maxMsPerChannel, 0, nullptr, nullptr);
    return lastScanCount;
}

int scanWiFiNetworks(uint32_t timeoutMs, bool passive, uint32_t maxMsPerChannel)
{
    return scanWirelessNetworks(timeoutMs, false, passive, maxMsPerChannel);
}

int printAvailableNetworks(Stream& output)
{
    int foundNetworks = scanWiFiNetworks(2000, false, 200);
    output.print("Found networks: ");
    output.println(foundNetworks);

    if (foundNetworks > 0) {
        for (int i = 0; i < foundNetworks; ++i) {
            output.print(i + 1);
            output.print(": ");
            output.print(getScannedNetworkSsid(i));
            output.print(" (RSSI: ");
            output.print(getScannedNetworkRssi(i));
            output.print(" dBm, CH: ");
            output.print(getScannedNetworkChannel(i));
            output.println(")");
        }
    } else {
        output.println("No networks found.");
    }

    clearScannedNetworks();
    return foundNetworks;
}

String getScannedNetworkSsid(int index)
{
    if (index < 0 || index >= lastScanCount) {
        return String();
    }
    return WiFi.SSID(index);
}

int8_t getScannedNetworkRssi(int index)
{
    if (index < 0 || index >= lastScanCount) {
        return 0;
    }
    return WiFi.RSSI(index);
}

int getScannedNetworkChannel(int index)
{
    if (index < 0 || index >= lastScanCount) {
        return 0;
    }
    return WiFi.channel(index);
}

int getScannedNetworkCount()
{
    if (WiFi.scanComplete() >= 0) {
        lastScanCount = WiFi.scanComplete();
    }
    return lastScanCount;
}

void clearScannedNetworks()
{
    WiFi.scanDelete();
    lastScanCount = 0;
}

bool startNetworkStation(const char* ssid, const char* password, uint32_t timeoutMs)
{
    if (ssid == nullptr || ssid[0] == '\0') {
        return false;
    }

    if (!wifiAvailable()) {
        warnUnsupportedPlatform();
        return false;
    }

    resetWifiState();
    WiFi.mode(WIFI_STA);
    if (staticIpConfigured) {
        WiFi.config(configuredIp, configuredGateway, configuredSubnet, configuredDnsPrimary, configuredDnsSecondary);
    }
    currentNetworkMode = NETWORK_WIFI_STA;
    WiFi.begin(ssid, password ? password : "");

    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (WiFi.status() == WL_CONNECTED) {
            currentNetworkConnected = true;
            return true;
        }
        delay(100);
    }

    currentNetworkConnected = false;
    return false;
}

bool startNetworkAccessPoint(const char* ssid, const char* password, uint8_t channel, bool hidden, uint8_t maxConnections)
{
    if (ssid == nullptr || ssid[0] == '\0') {
        return false;
    }

    if (!wifiAvailable()) {
        warnUnsupportedPlatform();
        return false;
    }

    resetWifiState();
    WiFi.mode(WIFI_AP);
    if (staticIpConfigured) {
        WiFi.softAPConfig(configuredIp, configuredGateway, configuredSubnet);
    }
    currentNetworkMode = NETWORK_WIFI_AP;

    if (password != nullptr && password[0] != '\0') {
        currentNetworkConnected = WiFi.softAP(ssid, password, channel, hidden, maxConnections);
    } else {
        currentNetworkConnected = WiFi.softAP(ssid, nullptr, channel, hidden, maxConnections);
    }
    return currentNetworkConnected;
}

bool startNetworkEthernet(const char* hostname, uint32_t timeoutMs)
{
    (void)hostname;

    resetWifiState();
    resetEthernetState();

    SPI.begin(W5500_SCLK, W5500_MISO, W5500_MOSI, W5500_SCSN);
    Ethernet.init(W5500_SCSN);

    byte mac[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

    uint32_t start = millis();
    if (staticIpConfigured) {
        Ethernet.begin(mac, configuredIp, configuredDnsPrimary, configuredGateway, configuredSubnet);
    } else {
        Ethernet.begin(mac);
    }
    while (millis() - start < timeoutMs) {
        if (Ethernet.linkStatus() == LinkON) {
            currentNetworkConnected = true;
            currentNetworkMode = NETWORK_ETHERNET;
            return true;
        }
        delay(100);
    }

    currentNetworkConnected = false;
    currentNetworkMode = NETWORK_ETHERNET;
    return false;
}

bool setNetworkStaticIp(const IPAddress& ip,
                        const IPAddress& gateway,
                        const IPAddress& subnet,
                        const IPAddress& dnsPrimary,
                        const IPAddress& dnsSecondary)
{
    staticIpConfigured = true;
    configuredIp = ip;
    configuredGateway = gateway;
    configuredSubnet = subnet;
    configuredDnsPrimary = dnsPrimary;
    configuredDnsSecondary = dnsSecondary;

    if (currentNetworkMode == NETWORK_WIFI_STA || currentNetworkMode == NETWORK_WIFI_AP_STA) {
        WiFi.config(ip, gateway, subnet, dnsPrimary, dnsSecondary);
    }

    if (currentNetworkMode == NETWORK_WIFI_AP) {
        WiFi.softAPConfig(ip, gateway, subnet);
    }

    if (currentNetworkMode == NETWORK_ETHERNET) {
        Ethernet.begin((uint8_t*)nullptr, ip, dnsPrimary, gateway, subnet);
    }

    return true;
}

bool restartNetworkWithStaticIp(const char* ssid,
                                const char* password,
                                uint8_t channel,
                                bool hidden,
                                uint8_t maxConnections,
                                uint32_t timeoutMs)
{
    if (currentNetworkMode == NETWORK_OFF) {
        return false;
    }

    stopNetwork();

    switch (currentNetworkMode) {
    case NETWORK_WIFI_STA:
        return startNetworkStation(ssid, password, timeoutMs);
    case NETWORK_WIFI_AP:
        return startNetworkAccessPoint(ssid, password, channel, hidden, maxConnections);
    case NETWORK_WIFI_AP_STA:
        return setNetworkMode(NETWORK_WIFI_AP_STA, ssid, password, channel, hidden, maxConnections);
    case NETWORK_ETHERNET:
        return startNetworkEthernet(nullptr, timeoutMs);
    default:
        return false;
    }
}

void clearNetworkStaticIp()
{
    staticIpConfigured = false;
    configuredIp = IPAddress(0, 0, 0, 0);
    configuredGateway = IPAddress(0, 0, 0, 0);
    configuredSubnet = IPAddress(255, 255, 255, 0);
    configuredDnsPrimary = IPAddress(0, 0, 0, 0);
    configuredDnsSecondary = IPAddress(0, 0, 0, 0);
}

void stopNetwork()
{
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    Ethernet.init(W5500_SCSN);
    currentNetworkMode = NETWORK_OFF;
    currentNetworkConnected = false;
}

void updateNetwork()
{
    if (currentNetworkMode == NETWORK_OFF) {
        return;
    }

    if (currentNetworkMode == NETWORK_ETHERNET) {
        currentNetworkConnected = (Ethernet.linkStatus() == LinkON);
        return;
    }

    if (currentNetworkMode == NETWORK_WIFI_STA || currentNetworkMode == NETWORK_WIFI_AP_STA) {
        currentNetworkConnected = (WiFi.status() == WL_CONNECTED) || (WiFi.softAPgetStationNum() > 0);
    } else if (currentNetworkMode == NETWORK_WIFI_AP) {
        currentNetworkConnected = WiFi.softAPgetStationNum() > 0;
    }
}

bool networkConnected()
{
    updateNetwork();
    return currentNetworkConnected;
}

String getNetworkIpAddress()
{
    if (currentNetworkMode == NETWORK_ETHERNET) {
        return Ethernet.localIP().toString();
    }
    if (WiFi.getMode() == WIFI_AP) {
        return WiFi.softAPIP().toString();
    }
    if (WiFi.getMode() == WIFI_STA || WiFi.getMode() == WIFI_AP_STA) {
        return WiFi.localIP().toString();
    }
    return String("0.0.0.0");
}

String getNetworkMacAddress()
{
    if (currentNetworkMode == NETWORK_ETHERNET) {
        byte mac[6];
        Ethernet.MACAddress(mac);
        char out[18];
        snprintf(out, sizeof(out), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return String(out);
    }
    return WiFi.macAddress();
}

int8_t getNetworkSignalStrength()
{
    if (currentNetworkMode == NETWORK_ETHERNET) {
        return 0;
    }
    if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED) {
        return WiFi.RSSI();
    }
    return -127;
}

bool setNetworkMode(NetworkMode mode, const char* ssid, const char* password, uint8_t channel, bool hidden, uint8_t maxConnections)
{
    switch (mode) {
    case NETWORK_OFF:
        stopNetwork();
        return true;
    case NETWORK_WIFI_STA:
        return startNetworkStation(ssid, password);
    case NETWORK_WIFI_AP:
        return startNetworkAccessPoint(ssid, password, channel, hidden, maxConnections);
    case NETWORK_WIFI_AP_STA:
        if (ssid == nullptr || ssid[0] == '\0') {
            return false;
        }

        if (!startNetworkStation(ssid, password)) {
            WiFi.mode(WIFI_AP_STA);
            currentNetworkMode = NETWORK_WIFI_AP_STA;
            return WiFi.softAP(ssid, password && password[0] != '\0' ? password : nullptr, channel, hidden, maxConnections);
        }

        WiFi.mode(WIFI_AP_STA);
        currentNetworkMode = NETWORK_WIFI_AP_STA;
        return WiFi.softAP(ssid, password && password[0] != '\0' ? password : nullptr, channel, hidden, maxConnections);
    case NETWORK_ETHERNET:
        return startNetworkEthernet();
    default:
        return false;
    }
}
