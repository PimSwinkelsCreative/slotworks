#include "userInterface.h"
#include "asciifont_7Segment.h"
#include <ht16k33.h>

// 7 segment display:
HT16K33 HT;

// buttons
bool downButtonState = false;
bool upButtonState = false;
bool enterButtonState = false;

uint32_t start, stop;

void setup7Segment()
{
    HT.begin(0x00); // only the I2C address needs to be given as input, the address is added in the class
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

void readButtons()
{
    uint16_t keysBitmap[3];
    if (HT.keyINTflag()) {
        HT.readKeyRaw(keysBitmap);
    } else {
        keysBitmap[0] = 0;
    }

    downButtonState = keysBitmap[0] & 0b1;
    upButtonState = keysBitmap[0] & 0b10;
    enterButtonState = keysBitmap[0] & 0b100;
}

bool downButtonPressed()
{
    return downButtonState;
}

bool upButtonPressed()
{
    return upButtonState;
}

bool enterButtonPressed()
{
    return enterButtonState;
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