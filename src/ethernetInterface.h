#pragma once

#include <Arduino.h>
#include <ETH.h>
#include <SPI.h>
#include <EthernetUdp.h>

/// Initialize the W5500 Ethernet interface.
/// Returns true when the hardware is present and the interface is configured.
bool setupEthernet();

/// Returns true if the Ethernet interface has been configured successfully.
bool ethernetIsReady();

/// Print Ethernet diagnostic information to Serial.
void printEthernetDiagnostics();

// Networking helpers
// Start UDP listeners for protocols. Returns true if the UDP port was opened.
bool beginOsc(uint16_t port = 8000);
bool beginArtnet(uint16_t port = 6454);
bool beginSACN(uint16_t port = 5568);

// Poll network sockets and dispatch any received packets. Call from loop().
void pollNetwork();

// Callback registration. Handlers receive a pointer to the packet and its length.
using RawUdpHandler = void(*)(const uint8_t *packet, size_t length);
void onOscRaw(RawUdpHandler h);
void onArtnetRaw(RawUdpHandler h);
void onSACNRaw(RawUdpHandler h);
