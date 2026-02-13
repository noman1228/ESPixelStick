#pragma once
/*
* GPIO_Defs_ESP32_JMT.hpp - Output Management class
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

#include "output/OutputMgr.hpp"

//Output Manager
#define DEFAULT_RMT_0_GPIO     gpio_num_t::GPIO_NUM_4
#define DEFAULT_RMT_1_GPIO     gpio_num_t::GPIO_NUM_13

#define DEFAULT_RMT_2_GPIO      gpio_num_t::GPIO_NUM_26
#define DEFAULT_RMT_3_GPIO      gpio_num_t::GPIO_NUM_33

// Output Manager Port Definitions
const OM_OutputPortDefinition_t OM_OutputPortDefinitions[] =
{
	{OM_PortId_t(0), OM_PortType_t::OM_SERIAL, {DEFAULT_RMT_0_GPIO}},
	{OM_PortId_t(1), OM_PortType_t::OM_SERIAL, {DEFAULT_RMT_1_GPIO}},
	{OM_PortId_t(2), OM_PortType_t::OM_SERIAL, {DEFAULT_RMT_2_GPIO}},
	{OM_PortId_t(3), OM_PortType_t::OM_SERIAL, {DEFAULT_RMT_3_GPIO}},
};


// File Manager
#define SUPPORT_SD
#define SD_CARD_MISO_PIN        gpio_num_t::GPIO_NUM_19
#define SD_CARD_MOSI_PIN        gpio_num_t::GPIO_NUM_23
#define SD_CARD_CLK_PIN         gpio_num_t::GPIO_NUM_18
#define SD_CARD_CS_PIN          gpio_num_t::GPIO_NUM_5

// Output Types
// Not Finished - #define SUPPORT_OutputProtocol_TLS3001
// #define SUPPORT_OutputProtocol_APA102         // SPI
#define SUPPORT_OutputProtocol_DMX            // UART
#define SUPPORT_OutputProtocol_GECE           // UART
#define SUPPORT_OutputProtocol_GS8208         // UART / RMT
#define SUPPORT_OutputProtocol_Renard         // UART
#define SUPPORT_OutputProtocol_Serial         // UART
#define SUPPORT_OutputProtocol_TM1814         // UART / RMT
#define SUPPORT_OutputProtocol_UCS1903        // UART / RMT
#define SUPPORT_OutputProtocol_UCS8903        // UART / RMT
// #define SUPPORT_OutputProtocol_WS2801         // SPI
#define SUPPORT_OutputProtocol_WS2811         // UART / RMT
// #define SUPPORT_OutputProtocol_Relay          // GPIO
// #define SUPPORT_OutputProtocol_Servo_PCA9685  // I2C (default pins)
#define SUPPORT_OutputProtocol_FireGod        // UART / RMT