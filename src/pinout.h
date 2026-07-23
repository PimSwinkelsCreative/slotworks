#pragma once
#include <Arduino.h>

// DMX:
#define DMX_TX_EN 3
#define DMX_TX 4
#define DMX_RS 5

// I2C:
#define I2C_INTN 6
#define I2C_SCL 7
#define I2C_SDA 8

// Ethernet
#define W5500_INTN 9
#define W5500_SCSN 10
#define W5500_MOSI 11
#define W5500_SCLK 12
#define W5500_MISO 13
#define W5500_RSTN 14

// Modbus:
#define MODBUS_RX 42
#define MODBUS_TX 45
#define MODBUS_TX_EN 46

// external SPI:
#define SPI_EXT_MOSI 35
#define SPI_EXT_SCLK 36
#define SPI_EXT_MISO 37
#define SPI_EXT_CS 39

// external GPIO:
#define EXT_GPIO_1 1
#define EXT_GPIO_2 2
#define EXT_GPIO_17 17
#define EXT_GPIO_18 18
#define EXT_GPIO_21 21
#define EXT_GPIO_33 33
#define EXT_GPIO_34 34
#define EXT_GPIO_38 38
#define EXT_GPIO_40 40
#define EXT_GPIO_41 41

// debug leds:
#define DEBUG_LED_1 EXT_GPIO_38
#define DEBUG_LED_2 EXT_GPIO_41