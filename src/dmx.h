#pragma once
#include <Arduino.h>
#include "driver/uart.h"

#ifndef DMX_PACKET_SIZE
#define DMX_PACKET_SIZE 513
#endif

typedef void (*callBack)();

// Optional dual-UART DMX configuration. The default keeps the previous single-port behavior.
// Set rxPort and txPort to different UART numbers when you want separate receive and transmit ports.
struct DmxPortConfig {
    uart_port_t rxPort = UART_NUM_1;
    uart_port_t txPort = UART_NUM_2;
    int rxPin = -1;
    int txPin = -1;
    int txEnablePin = -1;
};

void setupDMX(callBack messageReceivedFunction,
              uart_port_t rxPort = UART_NUM_1,
              uart_port_t txPort = UART_NUM_2,
              int rxPin = -1,
              int txPin = -1,
              int txEnablePin = -1);

void updateDMXInput();

byte getDMXValue(uint16_t channel);

void clearDMXData();

void enableDMXOutput(bool enable = true);

bool dmxConnected();
uint32_t dmxGetDetectedFramesPerSecond();
uint32_t dmxGetInvalidBreaksPerSecond();
uint32_t dmxGetRxBytesPerSecond();

void updateDMXOutput(int16_t packetSize = DMX_PACKET_SIZE);

void dmxSetByte(uint16_t address, uint8_t value);