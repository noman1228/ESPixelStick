#pragma once

#define SUPPORT_ETHERNET
#include <ETH.h>

// Output Manager
// MAX 8 Serial port on ESP32
const OM_OutputPortDefinition_t OM_OutputPortDefinitions[] =
{
    {OM_PortId_t(0), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_2}},
    {OM_PortId_t(0), OM_PortType_t::OM_RELAY,  {gpio_num_t::GPIO_NUM_2}},
    {OM_PortId_t(1), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_4}},
    {OM_PortId_t(1), OM_PortType_t::OM_RELAY,  {gpio_num_t::GPIO_NUM_4}},
    {OM_PortId_t(2), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_5}},
    {OM_PortId_t(2), OM_PortType_t::OM_RELAY,  {gpio_num_t::GPIO_NUM_5}},
    {OM_PortId_t(3), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_16}},
    {OM_PortId_t(3), OM_PortType_t::OM_RELAY,  {gpio_num_t::GPIO_NUM_16}},
    {OM_PortId_t(4), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_0}},
    {OM_PortId_t(4), OM_PortType_t::OM_RELAY,  {gpio_num_t::GPIO_NUM_0}},
    {OM_PortId_t(5), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_12}},
    {OM_PortId_t(5), OM_PortType_t::OM_RELAY,  {gpio_num_t::GPIO_NUM_12}},
    {OM_PortId_t(6), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_3}},
    {OM_PortId_t(7), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_1}},
    {OM_PortId_t(8), OM_PortType_t::OM_I2C,    {gpio_num_t::GPIO_NUM_33, gpio_num_t::GPIO_NUM_32}},
};

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
#define SUPPORT_OutputProtocol_DMX              // OM_SERIAL
#define SUPPORT_OutputProtocol_GECE             // OM_SERIAL
#define SUPPORT_OutputProtocol_GS8208           // OM_SERIAL
#define SUPPORT_OutputProtocol_Renard           // OM_SERIAL
#define SUPPORT_OutputProtocol_Serial           // OM_SERIAL
#define SUPPORT_OutputProtocol_TM1814           // OM_SERIAL
#define SUPPORT_OutputProtocol_UCS1903          // OM_SERIAL
#define SUPPORT_OutputProtocol_UCS8903          // OM_SERIAL
#define SUPPORT_OutputProtocol_WS2811           // OM_SERIAL
#define SUPPORT_OutputProtocol_Relay            // OM_RELAY
#define SUPPORT_OutputProtocol_Servo_PCA9685    // OM_I2C