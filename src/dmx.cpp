#include "dmx.h"
#include "driver/uart.h"    //required for temp UART inversion.
#include "pinout.h"

#define DEBUG_DMX

uart_port_t dmxPort = UART_NUM_1; // by default use UART 1

// array to store the DMX data
byte dmxInData[DMX_PACKET_SIZE];
byte dmxOutData[DMX_PACKET_SIZE];

// startAddress:
uint16_t DMXFixtureStartAddress = 0;

// stating:
bool dmxIsConnected = false;

callBack dmxMessageReceivedCallback;

void setupDMX(callBack messageReceivedFunction, uart_port_t uartPort)
{
    // zero the dmx data array:
    clearDMXData();

    dmxPort = uartPort;

    // install the dmx driver:
    dmx_config_t config = DMX_CONFIG_DEFAULT;
    dmx_personality_t personalities[] = {
        { 1, "Default Personality" }
    };
    int personality_count = 1;
    dmx_driver_install(dmxPort, &config, personalities, personality_count);

    /* Set the DMX hardware pins to the pins that we want to use. */
    dmx_set_pin(dmxPort, DMX_TX, DMX_RX, -1);

    // TEMP FIX! RS485 to UART converter is deisgned with inverted output. Idle level should be high, but is low...
    // Will be fixed in next iteration
    uart_set_line_inverse(dmxPort, DMX_RX);
    uart_set_line_inverse(dmxPort, DMX_TX);

    // start the dmx with the output configured as DMX Through
    pinMode(DMX_TX_EN, OUTPUT);
    enableDMXOutput(false);

    dmxMessageReceivedCallback = messageReceivedFunction;
}

void updateDMXInput()
{
    /* We need a place to store information about the DMX packets we receive. We
     *will use a dmx_packet_t to store that packet information.  */
    dmx_packet_t packet;

    uint32_t receiveStartTime = millis();

    if (dmx_receive(dmxPort, &packet, DMX_TIMEOUT_TICK)) {
        // If this code gets called, it means we've received DMX data!
        receiveStartTime = millis();

        // check for dmx errors
        if (!packet.err) {
            /* If this is the first DMX data we've received, lets log it! */
            if (!dmxIsConnected) {
#ifdef DEBUG_DMX
                Serial.println("DMX is connected!");
#endif
                dmxIsConnected = true;
            }
            //   read the packet into the dmx buffer array
            dmx_read(dmxPort, dmxInData, packet.size);

            // trigger the callback to update the led target values:
            dmxMessageReceivedCallback();
        } else {
#ifdef DEBUG_DMX
            Serial.println("A DMX error occurred, err: " + String(packet.err));
#endif
        }
    } else if (dmxIsConnected) {
        // If DMX times out after having been connected, it likely means that
        // the DMX cable was unplugged.
#ifdef DEBUG_DMX
        Serial.println("DMX was disconnected.");
#endif
        dmxIsConnected = false;
    }
}

void dmxSetByte(uint16_t address, uint8_t value)
{
    // check if address is within bounds:
    if (address < 1 || address >= DMX_PACKET_SIZE) {
        return;
    }
    dmxOutData[address] = value;
}

void updateDMXOutput()
{
    dmx_write(dmxPort, dmxOutData, DMX_PACKET_SIZE);
    dmx_send_num(dmxPort, DMX_PACKET_SIZE);
}

byte getDMXValue(uint16_t channel)
{
    if (channel > 511 || channel < 0) {
        Serial.print("ERR: channel out of bounds");
        return 0;
    }
    return dmxInData[channel];
}

void clearDMXData()
{
    for (int i = 0; i < DMX_PACKET_SIZE; i++) {
        dmxInData[i] = 0;
    }
}

void enableDMXOutput(bool enable)
{
    if (enable) {
        digitalWrite(DMX_TX_EN, LOW);
    } else {
        digitalWrite(DMX_TX_EN, HIGH);
    }
}

bool dmxConnected()
{
    return dmxIsConnected;
}
