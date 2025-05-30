#pragma once
/*
* GPIO_Defs_ESP32_D1_MINI.hpp - Output Management class
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

//Output Manager
#define DEFAULT_RMT_0_GPIO  gpio_num_t::GPIO_NUM_5
#define DEFAULT_RMT_1_GPIO  gpio_num_t::GPIO_NUM_17
#define DEFAULT_RMT_2_GPIO  gpio_num_t::GPIO_NUM_2
#define DEFAULT_RMT_3_GPIO  gpio_num_t::GPIO_NUM_4

// #define SUPPORT_SPI_OUTPUT
// I2C GPIOs must be defined but the values are not use.
#define DEFAULT_I2C_SDA     gpio_num_t::GPIO_NUM_1
#define DEFAULT_I2C_SCL     gpio_num_t::GPIO_NUM_1

// File Manager
// GPIOs must be defined but the values are not use.
// #define SUPPORT_SD
#define SD_CARD_MISO_PIN        gpio_num_t::GPIO_NUM_1
#define SD_CARD_MOSI_PIN        gpio_num_t::GPIO_NUM_1
#define SD_CARD_CLK_PIN         gpio_num_t::GPIO_NUM_1
#define SD_CARD_CS_PIN          gpio_num_t::GPIO_NUM_1

#include <ETH.h>

/*
   * ETH_CLOCK_GPIO0_IN   - default: external clock from crystal oscillator
   * ETH_CLOCK_GPIO0_OUT  - 50MHz clock from internal APLL output on GPIO0 - possibly an inverter is needed for LAN8720
   * ETH_CLOCK_GPIO16_OUT - 50MHz clock from internal APLL output on GPIO16 - possibly an inverter is needed for LAN8720
   * ETH_CLOCK_GPIO17_OUT - 50MHz clock from internal APLL inverted output on GPIO17 - tested with LAN8720
*/
#define DEFAULT_ETH_CLK_MODE           eth_clock_mode_t::ETH_CLOCK_GPIO17_OUT

// Pin# of the enable signal for the external crystal oscillator (-1 to disable for internal APLL source)
#define DEFAULT_ETH_POWER_PIN          gpio_num_t::GPIO_NUM_16
#define ETH_POWER_PIN                  DEFAULT_ETH_POWER_PIN
#define DEFAULT_ETH_POWER_PIN_ACTIVE   HIGH

// Type of the Ethernet PHY (LAN8720 or TLK110)
#define DEFAULT_ETH_TYPE               eth_phy_type_t::ETH_PHY_LAN8720

// I2C-address of Ethernet PHY (0 or 1 for LAN8720, 31 for TLK110)
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
#define SUPPORT_OutputType_DMX            // UART
#define SUPPORT_OutputType_GECE           // UART
#define SUPPORT_OutputType_GS8208         // UART / RMT
#define SUPPORT_OutputType_Renard         // UART
#define SUPPORT_OutputType_Serial         // UART
#define SUPPORT_OutputType_TM1814         // UART / RMT
#define SUPPORT_OutputType_UCS1903        // UART / RMT
#define SUPPORT_OutputType_UCS8903        // UART / RMT
#define SUPPORT_OutputType_WS2811         // UART / RMT