#include "dmx.h"
#include "pinout.h"

#define DEBUG_DMX
#define DMX_BAUD_RATE 250000
#define DMX_RX_BUFFER_SIZE 2048
#define DMX_TX_BUFFER_SIZE 2048
#define DMX_DISCONNECT_TIMEOUT_MS 5000

uart_port_t dmxRxPort = UART_NUM_1;
uart_port_t dmxTxPort = UART_NUM_2;
int dmxRxPin = DMX_RX;
int dmxTxPin = DMX_TX;
int dmxTxEnablePin = DMX_TX_EN;
bool dmxUseSeparateUarts = true;
bool dmxTxDriverEnabled = false;

byte dmxInData[DMX_PACKET_SIZE];
byte dmxOutData[DMX_PACKET_SIZE];

bool dmxIsConnected = false;
uint32_t lastDmxPacketMs = 0;
uint32_t lastRawUartDumpMs = 0;
uint32_t dmxDetectedFrames = 0;
uint32_t dmxDetectedFramesLastSecond = 0;
uint32_t dmxDetectedFramesLastPrintMs = 0;
uint32_t dmxInvalidBreaks = 0;
uint32_t dmxInvalidBreaksLastSecond = 0;
uint32_t dmxRxBytesCaptured = 0;
uint32_t dmxRxBytesCapturedLastSecond = 0;
uint32_t dmxRxBytesCapturedLastPrintMs = 0;

// A valid DMX break is at least ~92us. A normal UART byte with the worst-case low pattern (0x00, eight data bits
// plus the start bit) is only about 36us of low time, so 40us is a safe detection threshold that rejects
// false break pulses caused by ordinary UART data edges without missing real DMX breaks.
// A valid DMX break is a low pulse of roughly 92us. A normal UART byte can contain at most about 36us of
// low-time for the start bit plus consecutive zeros, so a threshold comfortably above that rejects false
// break detection while still accepting a real DMX break.
#define DMX_BREAK_MIN_US 40

enum DmxRxState {
    DMX_RX_IDLE = 0,
    DMX_RX_WAIT_START_CODE = 1,
    DMX_RX_READING = 2
};

static DmxRxState dmxRxState = DMX_RX_IDLE;
static uint16_t dmxCaptureIndex = 0;
static uint32_t dmxLastByteMs = 0;
static volatile bool dmxBreakInterruptArmed = false;
static volatile bool dmxBreakDetected = false;
static volatile bool dmxBreakLowSeen = false;
static volatile uint32_t dmxBreakStartUs = 0;
static uint32_t dmxBreakRearmMs = 0;
static bool dmxIsrServiceInstalled = false;
static bool dmxBreakPulseInvalid = false;
static TaskHandle_t dmxRxTaskHandle = NULL;
static QueueHandle_t dmxRxQueue = NULL;

callBack dmxMessageReceivedCallback;

static void dmxRxTaskEntry(void *param)
{
    (void)param;
    for (;;) {
        uart_event_t event;
        if (xQueueReceive(dmxRxQueue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (event.type) {
            case UART_BREAK:
                dmxDetectedFrames++;
                dmxRxState = DMX_RX_WAIT_START_CODE;
                dmxCaptureIndex = 0;
                dmxLastByteMs = millis();
                uart_flush_input(dmxRxPort);
                break;

            case UART_FRAME_ERR:
                dmxInvalidBreaks++;
                dmxRxState = DMX_RX_WAIT_START_CODE;
                dmxCaptureIndex = 0;
                dmxLastByteMs = millis();
                uart_flush_input(dmxRxPort);
                break;

            case UART_DATA:
            case UART_PATTERN_DET:
                if (dmxRxState == DMX_RX_IDLE) {
                    dmxRxState = DMX_RX_WAIT_START_CODE;
                    dmxCaptureIndex = 0;
                    dmxLastByteMs = millis();
                }
                updateDMXInput();
                break;

            default:
                break;
        }
    }
}

static void armDMXBreakInterrupt()
{
    if (dmxBreakInterruptArmed) {
        return;
    }

    dmxBreakInterruptArmed = true;
    dmxBreakDetected = false;
    dmxBreakLowSeen = false;
    dmxBreakStartUs = 0;
    dmxBreakPulseInvalid = false;
    dmxBreakRearmMs = millis();
}

static void disarmDMXBreakInterrupt()
{
    dmxBreakInterruptArmed = false;
    dmxBreakDetected = false;
    dmxBreakLowSeen = false;
    dmxBreakStartUs = 0;
    dmxBreakPulseInvalid = false;
}

static void configureUart(uart_port_t uartNum, int txPin, int rxPin)
{
    uart_config_t config = {
        .baud_rate = DMX_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_param_config(uartNum, &config);
    uart_set_pin(uartNum, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_set_mode(uartNum, UART_MODE_UART);
    uart_flush(uartNum);
}

void setupDMX(callBack messageReceivedFunction,
              uart_port_t rxPort,
              uart_port_t txPort,
              int rxPin,
              int txPin,
              int txEnablePin)
{
    clearDMXData();
    dmxIsConnected = false;
    lastDmxPacketMs = 0;
    lastRawUartDumpMs = 0;
    dmxRxState = DMX_RX_IDLE;
    dmxCaptureIndex = 0;
    dmxLastByteMs = 0;

    dmxRxPort = rxPort;
    dmxTxPort = txPort;
    dmxRxPin = rxPin;
    dmxTxPin = txPin;

    if (txEnablePin >= 0) {
        dmxTxEnablePin = txEnablePin;
    }

    dmxUseSeparateUarts = (rxPort != txPort);

    // The board keeps RX and TX on separate physical paths, so the library uses separate UART instances
    // for receive and transmit rather than trying to reuse a single DMX driver instance for both directions.
    // ESP32-S3 UART drivers are stable when each port is installed with a valid RX and TX buffer. A zero-sized
    // TX buffer can leave the driver in a bad state for direct DMX writes, so we install both ports with
    // non-zero buffers and then configure the pins.
    if (!uart_is_driver_installed(dmxRxPort)) {
        uart_driver_install(dmxRxPort, DMX_RX_BUFFER_SIZE, DMX_TX_BUFFER_SIZE, 20, &dmxRxQueue, 0);
    } else {
        // If the driver is already installed, keep using the same event queue for the RX task.
        dmxRxQueue = NULL;
    }
    if (dmxUseSeparateUarts && !uart_is_driver_installed(dmxTxPort)) {
        uart_driver_install(dmxTxPort, DMX_RX_BUFFER_SIZE, DMX_TX_BUFFER_SIZE, 0, NULL, 0);
    }

    if (dmxUseSeparateUarts) {
        configureUart(dmxRxPort, UART_PIN_NO_CHANGE, dmxRxPin);
        uart_flush_input(dmxRxPort);

        configureUart(dmxTxPort, dmxTxPin, UART_PIN_NO_CHANGE);
        uart_flush(dmxTxPort);
    } else {
        configureUart(dmxRxPort, dmxTxPin, dmxRxPin);
        uart_flush_input(dmxRxPort);
        uart_flush(dmxRxPort);
    }

    uart_set_rx_timeout(dmxRxPort, 1);
    uart_enable_pattern_det_baud_intr(dmxRxPort, 0x00, 1, 0, 0, 0);

    pinMode(dmxTxEnablePin, OUTPUT);
    pinMode(dmxRxPin, INPUT);
    enableDMXOutput(false);

    if (dmxRxTaskHandle == NULL) {
        xTaskCreatePinnedToCore(dmxRxTaskEntry, "dmxRxTask", 4096, NULL, 3, &dmxRxTaskHandle, 1);
    }

    dmxMessageReceivedCallback = messageReceivedFunction;
    armDMXBreakInterrupt();
}

void updateDMXInput()
{
    const uint32_t nowMs = millis();

    if (nowMs - dmxDetectedFramesLastPrintMs >= 1000) {
        dmxDetectedFramesLastPrintMs = nowMs;
        dmxDetectedFramesLastSecond = dmxDetectedFrames;
        dmxDetectedFrames = 0;
        dmxInvalidBreaksLastSecond = dmxInvalidBreaks;
        dmxInvalidBreaks = 0;
        dmxRxBytesCapturedLastSecond = dmxRxBytesCaptured;
        dmxRxBytesCaptured = 0;
    }

    if (dmxRxState == DMX_RX_IDLE) {
        return;
    }

    size_t availableBytes = 0;
    if (uart_get_buffered_data_len(dmxRxPort, &availableBytes) != ESP_OK) {
        return;
    }

    while (availableBytes > 0) {
        uint8_t byte = 0;
        const int received = uart_read_bytes(dmxRxPort, &byte, 1, 0);
        if (received != 1) {
            break;
        }

        dmxLastByteMs = millis();
        dmxRxBytesCaptured++;

        if (dmxRxState == DMX_RX_WAIT_START_CODE) {
            if (byte == 0x00) {
                dmxRxState = DMX_RX_READING;
            } else {
                dmxRxState = DMX_RX_IDLE;
                uart_flush_input(dmxRxPort);
                break;
            }
            if (uart_get_buffered_data_len(dmxRxPort, &availableBytes) != ESP_OK) {
                break;
            }
            continue;
        }

        dmxInData[dmxCaptureIndex + 1] = byte;
        dmxCaptureIndex++;

        if (dmxCaptureIndex >= 512) {
            dmxRxState = DMX_RX_IDLE;
            lastDmxPacketMs = millis();
            if (!dmxIsConnected) {
#ifdef DEBUG_DMX
                Serial.println("DMX is connected!");
#endif
                dmxIsConnected = true;
            }
            if (dmxMessageReceivedCallback != nullptr) {
                dmxMessageReceivedCallback();
            }
            break;
        }

        if (uart_get_buffered_data_len(dmxRxPort, &availableBytes) != ESP_OK) {
            break;
        }
    }

    if (dmxRxState == DMX_RX_READING && (millis() - dmxLastByteMs > 20)) {
        dmxRxState = DMX_RX_IDLE;
        lastDmxPacketMs = millis();
        if (!dmxIsConnected) {
#ifdef DEBUG_DMX
            Serial.println("DMX is connected!");
#endif
            dmxIsConnected = true;
        }
        if (dmxMessageReceivedCallback != nullptr) {
            dmxMessageReceivedCallback();
        }
    }

    if (dmxIsConnected && (millis() - lastDmxPacketMs > DMX_DISCONNECT_TIMEOUT_MS)) {
#ifdef DEBUG_DMX
        Serial.println("DMX was disconnected.");
#endif
        dmxIsConnected = false;
    }
}

void dmxSetByte(uint16_t address, uint8_t value)
{
    // DMX channel numbers are 1-based. The start code is stored separately at index 0, so the first data channel
    // is at index 1 in the raw DMX buffer.
    if (address < 1 || address > 512) {
        return;
    }
    dmxOutData[address] = value;
}

void updateDMXOutput(int16_t packetSize)
{
    if (packetSize > DMX_PACKET_SIZE) {
        packetSize = DMX_PACKET_SIZE;
    }
    if (packetSize < 1) {
        packetSize = DMX_PACKET_SIZE;
    }

    if (!uart_is_driver_installed(dmxTxPort)) {
        return;
    }

    // DMX uses a true break condition, not a burst of UART zeros. The wire must be driven low for the
    // break duration, then released high for the mark-after-break (MAB), before the start code and data bytes.
    // The ESP32 UART can generate the break by forcing the TX line low for a sustained interval, then sending
    // the actual DMX payload as standard 8N2 bytes.
    const uint32_t breakDurationUs = 100; // ~92us minimum break; using a slightly larger value keeps it robust.
    const uint32_t mabDurationUs = 12;     // DMX MAB is approximately 12us.

    // Force the UART TX line low to create the DMX break. The UART peripheral needs to be reconnected to the
    // TX pin before actual DMX bytes are written, otherwise the GPIO pin remains in a plain digital-output mode
    // and the UART payload is never driven onto the bus.
    uart_set_line_inverse(dmxTxPort, 0);
    pinMode(dmxTxPin, OUTPUT);
    digitalWrite(dmxTxPin, LOW);
    delayMicroseconds(breakDurationUs);

    // Release high for the mark-after-break before sending the start code.
    digitalWrite(dmxTxPin, HIGH);
    delayMicroseconds(mabDurationUs);

    // Restore the UART function to the TX pin so the payload bytes are actually transmitted by the peripheral.
    uart_set_pin(dmxTxPort, dmxTxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_set_mode(dmxTxPort, UART_MODE_UART);

    // The first byte is always the DMX null start code 0x00, followed by the DMX data slots beginning at
    // channel 1. The internal buffer stores the start code separately at index 0, so we must copy from
    // dmxOutData[1] and not from the zeroed start-code slot.
    uint8_t txPacket[DMX_PACKET_SIZE + 1];
    txPacket[0] = 0x00;
    memcpy(&txPacket[1], &dmxOutData[1], packetSize);

    const int startCodeWritten = uart_write_bytes(dmxTxPort, (const char *)txPacket, packetSize + 1);
    if (startCodeWritten < 0) {
#ifdef DEBUG_DMX
        Serial.println("TX UART write failed.");
#endif
        return;
    }

    // Give the UART time to flush the queued bytes before the next packet is built.
    uart_wait_tx_done(dmxTxPort, 100);
}

byte getDMXValue(uint16_t channel)
{
    if (channel < 1 || channel > 512) {
        Serial.print("ERR: channel out of bounds");
        return 0;
    }
    return dmxInData[channel];
}

void clearDMXData()
{
    for (int i = 0; i < DMX_PACKET_SIZE; i++) {
        dmxInData[i] = 0;
        dmxOutData[i] = 0;
    }
    dmxOutData[0] = 0;
}

void enableDMXOutput(bool enable)
{
    // This pin controls the external DMX transceiver direction. The board hardware keeps the receive path
    // active at all times, while the TX path is only enabled when the application is actively sending.
    if (dmxTxEnablePin < 0) {
        dmxTxDriverEnabled = false;
        return;
    }

    dmxTxDriverEnabled = enable;
    digitalWrite(dmxTxEnablePin, enable ? LOW : HIGH);
}

bool dmxConnected()
{
    return dmxIsConnected;
}

uint32_t dmxGetDetectedFramesPerSecond()
{
    return dmxDetectedFramesLastSecond;
}

uint32_t dmxGetInvalidBreaksPerSecond()
{
    return dmxInvalidBreaksLastSecond;
}

uint32_t dmxGetRxBytesPerSecond()
{
    return dmxRxBytesCapturedLastSecond;
}
