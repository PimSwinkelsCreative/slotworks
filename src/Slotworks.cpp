#include "Slotworks.h"
#include "pinout.h"
#include "userInterface.h"
#include <Wire.h>
#include "ethernetInterface.h"

void setupSlotworks()
{
    setupDebugLeds();

    // start the I2C:
    Wire.setPins(I2C_SDA, I2C_SCL);
    Wire.begin();

    // start the 7 segment display:
    setup7Segment();

    // Setup the W5500 Ethernet interface.
    if (!setupEthernet()) {
        Serial.println("Ethernet setup failed. Verify W5500 wiring and SPI pins.");
    }

    // Print diagnostics once after setup.
    printEthernetDiagnostics();
}