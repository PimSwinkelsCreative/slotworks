#pragma once
#include "pinout.h"
#include <Arduino.h>

// 7 segment display:
#define HT16K33_I2C_ADDR 0x70

void setup7Segment();
void displayInteger(int16_t number, bool leadingZeroes = false);

// debug leds:
void setupDebugLeds();
void setDebugLed(uint8_t ledNr, bool state = true);
