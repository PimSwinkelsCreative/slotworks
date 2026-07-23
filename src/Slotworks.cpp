#include "Slotworks.h"
#include "pinout.h"
#include "userInterface.h"
#include <Wire.h>

void setupSlotworks()
{
    setupDebugLeds();

    // start the I2C:
    Wire.setPins(I2C_SDA, I2C_SCL);
    Wire.begin();

    // start the 7 segment display:
    setup7Segment();
}