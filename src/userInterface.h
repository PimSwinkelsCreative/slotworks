#pragma once
#include "pinout.h"
#include <Arduino.h>

const uint16_t defaultDisplayBlinkTime = 200; // time in milliseconds

// user interface modes:
enum UIMode { OFF,
    DMXADDR,
    IPADDR };

void setUserInterfaceMode(UIMode mode, void (*callback)(uint16_t) = NULL);
void updateUserInterface();

// 7 segment display:
#define HT16K33_I2C_ADDR 0x70
void setup7Segment();
void displayInteger(int16_t number, bool leadingZeroes = false);
void startDisplayBlink(uint16_t duration = defaultDisplayBlinkTime);

// debug leds:
void setupDebugLeds();
void setDebugLed(uint8_t ledNr, bool state = true);

void readButtons();
class HT16K33Button {
private:
    uint16_t _buttonBitmap[3];
    bool _state;
    bool _prevState;
    bool _pressFlag;

    // scrolling parameters
    bool _scrollingEnabled;
    uint32_t _buttonPressStartMillis;
    uint32_t _prevScrollUpdateMillis;

public:
    HT16K33Button(uint8_t _buttonIndex, bool scrollingEnabled = false);
    void update(uint16_t keysBitmap[3]);
    bool getPressFlag(bool clearOnRead = false);
    void clearPressFlag();
};
