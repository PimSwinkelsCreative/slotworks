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