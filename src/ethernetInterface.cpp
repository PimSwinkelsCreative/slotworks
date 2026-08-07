#include "ethernetInterface.h"
#include "pinout.h"

#include <esp_system.h>
#include <esp_mac.h>

static SPIClass ethernetSPI(HSPI);
static byte ethernetMac[6] = {0};
static const IPAddress ethernetIp(192, 168, 0, 50);
static const IPAddress ethernetDns(192, 168, 0, 1);
static const IPAddress ethernetGateway(192, 168, 0, 1);
static const IPAddress ethernetSubnet(255, 255, 255, 0);
static bool ethernetConfigured = false;

static const char *ethernetLinkStatusName(bool connected)
{
    return connected ? "Connected" : "Disconnected";
}

static void fillEthernetMacFromEsp32()
{
    esp_err_t err = esp_efuse_mac_get_default(ethernetMac);
    (void)err;

    bool allZero = true;
    for (int i = 0; i < 6; ++i) {
        if (ethernetMac[i] != 0) {
            allZero = false;
            break;
        }
    }

    if (allZero) {
        ethernetMac[0] = 0x02;
        ethernetMac[1] = 0x00;
        ethernetMac[2] = 0x00;
        ethernetMac[3] = 0x00;
        ethernetMac[4] = 0x00;
        ethernetMac[5] = 0x01;
        Serial.println("efuse MAC empty — using fallback dummy MAC");
    }
}

void printEthernetDiagnostics()
{
    Serial.println("--- Ethernet diagnostics ---");
    Serial.print("W5500 CS pin: ");
    Serial.println(W5500_SCSN);
    Serial.print("W5500 reset pin: ");
    Serial.println(W5500_RSTN);
    Serial.print("W5500 reset pin state: ");
    Serial.println(digitalRead(W5500_RSTN));
    Serial.print("SPI pins MOSI/MISO/SCLK: ");
    Serial.print(W5500_MOSI);
    Serial.print("/");
    Serial.print(W5500_MISO);
    Serial.print("/");
    Serial.println(W5500_SCLK);

    Serial.print("Configured IP: ");
    Serial.println(ethernetIp);
    Serial.print("Gateway: ");
    Serial.println(ethernetGateway);
    Serial.print("Subnet: ");
    Serial.println(ethernetSubnet);
    Serial.print("DNS: ");
    Serial.println(ethernetDns);

    Serial.print("MAC: ");
    for (int i = 0; i < 6; ++i) {
        if (i) Serial.print(':');
        if (ethernetMac[i] < 0x10) Serial.print('0');
        Serial.print(ethernetMac[i], HEX);
    }
    Serial.println();

    Serial.print("ETH started: ");
    Serial.println(ETH.started() ? "yes" : "no");
    Serial.print("ETH connected: ");
    Serial.println(ETH.connected() ? "yes" : "no");
    Serial.print("ETH link up: ");
    Serial.println(ETH.linkUp() ? "yes" : "no");
    Serial.print("ETH local IP: ");
    Serial.println(ETH.localIP());
    Serial.println("-----------------------------");
}

bool setupEthernet()
{
    fillEthernetMacFromEsp32();

    Serial.println("Resetting W5500...");
    pinMode(W5500_RSTN, OUTPUT);
    digitalWrite(W5500_RSTN, LOW);
    delay(10);
    digitalWrite(W5500_RSTN, HIGH);
    delay(200);

    Serial.println("Starting HSPI for W5500...");
    ethernetSPI.begin(W5500_SCLK, W5500_MISO, W5500_MOSI, W5500_SCSN);

    Serial.println("Initializing ETH driver with custom SPI...");
    bool ok = ETH.begin(ETH_PHY_W5500, 1, W5500_SCSN, W5500_INTN, W5500_RSTN, ethernetSPI, 20);
    if (!ok) {
        Serial.println("ETH.begin failed. Check W5500 wiring and HSPI pin assignment.");
        ethernetConfigured = false;
        return false;
    }

    Serial.println("Applying static IP configuration...");
    if (!ETH.config(ethernetIp, ethernetGateway, ethernetSubnet, ethernetDns)) {
        Serial.println("ETH.config failed. Static IP may not be applied.");
    }

    delay(200);

    bool connected = ETH.connected();
    bool linkUp = ETH.linkUp();
    Serial.print("ETH connected after init: ");
    Serial.println(connected ? "yes" : "no");
    Serial.print("ETH link after init: ");
    Serial.println(linkUp ? "yes" : "no");

    if (!linkUp) {
        Serial.println("Ethernet initialization failed: link is not up.");
        ethernetConfigured = false;
        return false;
    }

    ethernetConfigured = true;
    Serial.println("Ethernet initialized successfully.");
    Serial.print("Local IP: ");
    Serial.println(ETH.localIP());
    return true;
}

bool ethernetIsReady()
{
    if (!ethernetConfigured) {
        return false;
    }

    bool ready = ETH.started() && ETH.connected() && ETH.linkUp();
    Serial.print("ethernetIsReady check: configured=true, ready=");
    Serial.println(ready ? "true" : "false");
    return ready;
}
