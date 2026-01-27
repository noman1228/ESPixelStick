#pragma once
/*
* OutputMgrPortDefs.hpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2026 Shelby Merrick
* http://www.forkineye.com
*
*  This program is provided free for you to use in any way that you wish,
*  subject to the laws and regulations where you are using it.  Due diligence
*  is strongly suggested before using this code.  Please give credit where due.
*
*  The Author makes no warranty of any kind, express or implied, with regard
*  to this program or the documentation contained in this document.  The
*  Author shall not be liable in any event for incidental or consequential
*  damages in connection with, or arising out of, the furnishing, performance
*  or use of these programs.
*
*/

#include "ESPixelStick.h"

#ifdef ARDUINO_ARCH_ESP8266
typedef enum
{
    GPIO_NUM_0 = 0,
    GPIO_NUM_1,
    GPIO_NUM_2,
    GPIO_NUM_3,
    GPIO_NUM_4,
    GPIO_NUM_5,
    GPIO_NUM_6,
    GPIO_NUM_7,
    GPIO_NUM_8,
    GPIO_NUM_9,
    GPIO_NUM_10,
    GPIO_NUM_11,
    GPIO_NUM_12,
    GPIO_NUM_13,
    GPIO_NUM_14,
    GPIO_NUM_15,
    GPIO_NUM_16,
    GPIO_NUM_17,
    GPIO_NUM_18,
    GPIO_NUM_19,
    GPIO_NUM_20,
    GPIO_NUM_21,
    GPIO_NUM_22,
    GPIO_NUM_23,
    GPIO_NUM_24,
    GPIO_NUM_25,
    GPIO_NUM_26,
    GPIO_NUM_27,
    GPIO_NUM_28,
    GPIO_NUM_29,
    GPIO_NUM_30,
    GPIO_NUM_31,
    GPIO_NUM_32,
    GPIO_NUM_33,
    GPIO_NUM_34,
} gpio_num_t;

typedef enum
{
    UART_NUM_0 = 0,
    UART_NUM_1,
    UART_NUM_2
} uart_port_t;
#endif // def ARDUINO_ARCH_ESP8266

typedef uint8_t OM_PortId_t; // zero based port ID

enum OM_PortType_t
{
    OM_SERIAL,  // WS2811 rtc
    OM_RELAY,   // Relay output
    OM_SPI,     // WS2801 etc
    OM_I2C,     // PCA Relay board etc
    OM_ANY      // used by the disabled protocol
};

struct OM_GPIOS_t
{
    gpio_num_t data; // mosi / SDA
    gpio_num_t clk; // SCL
    gpio_num_t cs;
};

struct OM_OutputPortDefinition_t
{
    OM_PortId_t     PortId;
    OM_PortType_t   PortType;
    OM_GPIOS_t      gpios;
    uint8_t         DeviceId;
};
