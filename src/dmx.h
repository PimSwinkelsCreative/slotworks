#pragma once
#include <Arduino.h>
#include <esp_dmx.h>

typedef void (*callBack)();

void setupDMX(callBack messageReceivedFunction, uint16_t dmxAddress = 0, uint8_t uartPort = 1);

void updateDMXInput();

byte getDMXValue(uint16_t channel);

void clearDMXData();

void enableDMXOutput(bool enable = true);

bool dmxConnected();

void setDMXAddress(uint16_t addr);