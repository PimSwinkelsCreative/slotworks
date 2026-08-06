#include "userInterface.h"
#include "asciifont_7Segment.h"
#include <ht16k33.h>

// 7 segment display:
HT16K33 HT;

uint16_t displayBlinkTime = defaultDisplayBlinkTime; // time in milliseconds

// buttons
HT16K33Button downButton(0, true);
HT16K33Button upButton(1, true);
HT16K33Button enterButton(2);
uint16_t buttonScrollSpeedInterval = 100; // milliseconds before auto increase while scrolling
uint16_t buttonScrollHighSpeedTime = 2000; // time after which the auto increase will get even faster

uint64_t lastButtonPress = 0;

// stating
UIMode currentMode = OFF;
void (*UICallbackFunction)(uint16_t);
bool displayBlinkActive = false;
uint64_t lastDisplayBlinkStart = 0;

// variables
uint16_t numberToDisplay = 0;

void setup7Segment()
{
    HT.begin(0x00); // only the I2C address offset needs to be given as input, the address is added in the class
    HT.setBrightness(8); // 50% brightness
    HT.displayOn();
}

void displayInteger(int16_t number, bool leadingZeroes)
{
    // determine the digits:
    uint8_t thousands = (abs(number) % 10000) / 1000;
    uint8_t hundreds = (abs(number) % 1000) / 100;
    uint8_t tens = (abs(number) % 100) / 10;
    uint8_t singles = abs(number) % 10;

    // set the digits:
    if (leadingZeroes) {
        HT.setDisplayRaw(0, sevenSegmentASCII[16 + thousands]);
        HT.setDisplayRaw(2, sevenSegmentASCII[16 + hundreds]);
        HT.setDisplayRaw(4, sevenSegmentASCII[16 + tens]);
    } else {
        HT.setDisplayRaw(0, thousands > 0 ? sevenSegmentASCII[16 + thousands] : sevenSegmentASCII[0]);
        HT.setDisplayRaw(2, (hundreds > 0 || thousands > 0) ? sevenSegmentASCII[16 + hundreds] : sevenSegmentASCII[0]);
        HT.setDisplayRaw(4, (tens > 0 || hundreds > 0 || thousands > 0) ? sevenSegmentASCII[16 + tens] : sevenSegmentASCII[0]);
    }
    HT.setDisplayRaw(6, sevenSegmentASCII[16 + singles]);

    // set the minus sign for negative numbers
    if (number < 0) {
        if (number < -100) {
            HT.setDisplayRaw(0, sevenSegmentASCII[13]);
        } else if (number < -10) {
            HT.setDisplayRaw(2, sevenSegmentASCII[13]);
        } else {
            HT.setDisplayRaw(4, sevenSegmentASCII[13]);
        }
    }
    HT.sendLed();
}

void clearDisplay()
{
    HT.setDisplayRaw(0, sevenSegmentASCII[0]);
    HT.setDisplayRaw(2, sevenSegmentASCII[0]);
    HT.setDisplayRaw(4, sevenSegmentASCII[0]);
    HT.setDisplayRaw(6, sevenSegmentASCII[0]);
    HT.sendLed();
}

void readButtons()
{
    uint16_t keysBitmap[3];
    if (HT.keyINTflag()) {
        HT.readKeyRaw(keysBitmap);
    } else {
        keysBitmap[0] = 0;
        keysBitmap[1] = 0;
        keysBitmap[2] = 0;
    }
    upButton.update(keysBitmap);
    downButton.update(keysBitmap);
    enterButton.update(keysBitmap);
}

// debug leds:
void setupDebugLeds()
{
    pinMode(DEBUG_LED_1, OUTPUT);
    pinMode(DEBUG_LED_2, OUTPUT);
    digitalWrite(DEBUG_LED_1, LOW);
    digitalWrite(DEBUG_LED_2, LOW);
}

void setDebugLed(uint8_t ledNr, bool state)
{
    if (ledNr < 1 || ledNr > 2) {
        return;
    }

    if (ledNr == 1) {
        digitalWrite(DEBUG_LED_1, state);
    } else {
        digitalWrite(DEBUG_LED_2, state);
    }
}

void setUserInterfaceMode(UIMode mode, void (*callback)(uint16_t))
{
    if (mode == DMXADDR && callback == NULL) {
        Serial.println("ERROR! callback function required for this mode! Could not set mode");
        return;
    }
    currentMode = mode;
    UICallbackFunction = callback;
}

void startDisplayBlink(uint16_t duration)
{
    displayBlinkActive = true;
    displayBlinkTime = duration;
    lastDisplayBlinkStart = millis();
}

void setDisplayValue(int16_t value)
{
    numberToDisplay = value;
}

void updateUserInterface()
{
    // poll the buttons and set their flags:
    readButtons();

    // update the user interface:
    switch (currentMode) {
    case OFF:
        clearDisplay();
        break;
    case DMXADDR:
        if (upButton.getPressFlag(true)) {
            if (numberToDisplay < 512) {
                numberToDisplay++;
            }
        }
        if (downButton.getPressFlag(true)) {
            if (numberToDisplay > 0) {
                numberToDisplay--;
            }
        }
        if (enterButton.getPressFlag(true)) {
            // trigger the callback to change the DMX address:
            UICallbackFunction(numberToDisplay);
        }

        if (displayBlinkActive) {
            if (millis() - lastDisplayBlinkStart < displayBlinkTime) {
                clearDisplay();
            } else {
                displayBlinkActive = false;
            }
        }
        if (!displayBlinkActive) {
            displayInteger(numberToDisplay);
        }

        break;
    case IPADDR:

        break;
    case VALUE:
        displayInteger(numberToDisplay);
        break;

    default:
        break;
    }
}

HT16K33Button::HT16K33Button(uint8_t buttonIndex, bool scrollingEnabled)
{
    uint64_t bitmap = 1 << buttonIndex;

    _buttonBitmap[0] = bitmap & 0xFFFF; // 16 LSB of bitmap
    _buttonBitmap[1] = (bitmap & 0xFFFF0000) >> 16; // middle 16 bits of bitmap
    _buttonBitmap[2] = (bitmap & 0xFFFF00000000) >> 32; // 16 MSB of bitmap

    _scrollingEnabled = scrollingEnabled;

    _state = false;
    _prevState = false;
    _pressFlag = false;
    _buttonPressStartMillis = 0;
    _prevScrollUpdateMillis = 0;
}

void HT16K33Button::update(uint16_t keysBitmap[3])
{
    _state = false;
    for (int i = 0; i < 3; i++) {
        if ((_buttonBitmap[i] & keysBitmap[i]) > 0)
            _state = true;
    }

    if (_state && !_prevState) {
        _pressFlag = true;
        _buttonPressStartMillis = millis();
    }

    if (_scrollingEnabled) {
        uint64_t now = millis();
        if (_state && (now - _buttonPressStartMillis >= 3 * buttonScrollSpeedInterval)) {
            if (now - _buttonPressStartMillis >= buttonScrollHighSpeedTime) {
                if (now - _prevScrollUpdateMillis >= buttonScrollSpeedInterval / 5) {
                    _prevScrollUpdateMillis = now;
                    _pressFlag = true;
                }
            } else {
                if (now - _prevScrollUpdateMillis >= buttonScrollSpeedInterval) {
                    _prevScrollUpdateMillis = now;
                    _pressFlag = true;
                }
            }
        }
    }

    _prevState = _state;
}
bool HT16K33Button::getPressFlag(bool clearOnRead)
{
    bool output = _pressFlag;
    if (clearOnRead) {
        clearPressFlag();
    }
    return output;
}

void HT16K33Button::clearPressFlag()
{
    if (_pressFlag) {
        _pressFlag = false;
    }
}
