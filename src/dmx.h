#pragma once
#include <Arduino.h>
#include <esp_dmx.h>

typedef void (*callBack)();


void setupDMX(callBack messageReceivedFunction, uint8_t uartPort=1);

void updateDMXInput();

byte getDMXValue(uint16_t channel);

void clearDMXData();