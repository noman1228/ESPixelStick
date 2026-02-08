#pragma once
/*
* GPIO_Defs_ESP32_JMT-ETH.hpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2025 Shelby Merrick
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

#define SUPPORT_ETHERNET
#include <ETH.h>
//Output Manager
#define DEFAULT_RMT_0_GPIO      gpio_num_t::GPIO_NUM_2
#define DEFAULT_RMT_1_GPIO      gpio_num_t::GPIO_NUM_4
#define DEFAULT_RMT_2_GPIO      gpio_num_t::GPIO_NUM_5
#define DEFAULT_RMT_3_GPIO      gpio_num_t::GPIO_NUM_16

#define DEFAULT_RMT_4_GPIO      gpio_num_t::GPIO_NUM_0
#define DEFAULT_RMT_5_GPIO      gpio_num_t::GPIO_NUM_12
#define DEFAULT_RMT_6_GPIO      gpio_num_t::GPIO_NUM_3
#define DEFAULT_RMT_7_GPIO      gpio_num_t::GPIO_NUM_1

// Output Manager Port Definitions
const OM_OutputPortDefinition_t OM_OutputPortDefinitions[] =
{
    {OM_PortId_t(0), OM_PortType_t::OM_SERIAL, {DEFAULT_RMT_0_GPIO}},
    {OM_PortId_t(1), OM_PortType_t::OM_SERIAL, {DEFAULT_RMT_1_GPIO}},
    {OM_PortId_t(2), OM_PortType_t::OM_SERIAL, {DEFAULT_RMT_2_GPIO}},
    {OM_PortId_t(3), OM_PortType_t::OM_SERIAL, {DEFAULT_RMT_3_GPIO}},
    {OM_PortId_t(4), OM_PortType_t::OM_SERIAL, {DEFAULT_RMT_4_GPIO}},
    {OM_PortId_t(5), OM_PortType_t::OM_SERIAL, {DEFAULT_RMT_5_GPIO}},
    {OM_PortId_t(6), OM_PortType_t::OM_SERIAL, {DEFAULT_RMT_6_GPIO}},
    {OM_PortId_t(7), OM_PortType_t::OM_SERIAL, {DEFAULT_RMT_7_GPIO}},
};
// OLED Housekeeping
#ifndef ESP_SCL_PIN 
    #define ESP_SCL_PIN             gpio_num_t::GPIO_NUM_33
#endif
#ifndef ESP_SDA_PIN 
    #define ESP_SDA_PIN             gpio_num_t::GPIO_NUM_32
#endif
// File Manager
#define SUPPORT_SD
#define SD_CARD_MISO_PIN         gpio_num_t::GPIO_NUM_34
#define SD_CARD_MOSI_PIN         gpio_num_t::GPIO_NUM_13
#define SD_CARD_CLK_PIN          gpio_num_t::GPIO_NUM_14
#define SD_CARD_CS_PIN           gpio_num_t::GPIO_NUM_15



#define DEFAULT_ETH_CLK_MODE           eth_clock_mode_t::ETH_CLOCK_GPIO17_OUT
#define DEFAULT_ETH_POWER_PIN          gpio_num_t::GPIO_NUM_20
#define DEFAULT_ETH_POWER_PIN_ACTIVE   HIGH
#define DEFAULT_ETH_TYPE               eth_phy_type_t::ETH_PHY_LAN8720
#define ETH_ADDR_PHY_LAN8720           1
#define DEFAULT_ETH_ADDR               ETH_ADDR_PHY_LAN8720
#define DEFAULT_ETH_TXEN               gpio_num_t::GPIO_NUM_21
#define DEFAULT_ETH_TXD0               gpio_num_t::GPIO_NUM_19
#define DEFAULT_ETH_TXD1               gpio_num_t::GPIO_NUM_22
#define DEFAULT_ETH_CRSDV              gpio_num_t::GPIO_NUM_27
#define DEFAULT_ETH_RXD0               gpio_num_t::GPIO_NUM_25
#define DEFAULT_ETH_RXD1               gpio_num_t::GPIO_NUM_26
#define DEFAULT_ETH_MDC_PIN            gpio_num_t::GPIO_NUM_23
#define DEFAULT_ETH_MDIO_PIN           gpio_num_t::GPIO_NUM_18


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