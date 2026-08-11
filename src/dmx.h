#pragma once
#include <Arduino.h>
#include <esp_dmx.h>

typedef void (*callBack)();

void setupDMX(callBack messageReceivedFunction, uart_port_t uartPort = UART_NUM_1);

void updateDMXInput();

byte getDMXValue(uint16_t channel);

void clearDMXData();

void enableDMXOutput(bool enable = true);

bool dmxConnected();

void updateDMXOutput(int16_t packetSize = DMX_PACKET_SIZE);

void dmxSetByte(uint16_t address, uint8_t value);