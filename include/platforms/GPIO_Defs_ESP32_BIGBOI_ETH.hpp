#pragma once
/*
* GPIO_Defs_ESP32_LILY_TETH.hpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021 - 2025 Shelby Merrick
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


// Output Manager
#define DEFAULT_RMT_0_GPIO      gpio_num_t::GPIO_NUM_27
#define DEFAULT_RMT_1_GPIO      gpio_num_t::GPIO_NUM_26
#define DEFAULT_RMT_2_GPIO      gpio_num_t::GPIO_NUM_25
#define DEFAULT_RMT_3_GPIO      gpio_num_t::GPIO_NUM_33
#define DEFAULT_RMT_4_GPIO      gpio_num_t::GPIO_NUM_32
#define DEFAULT_RMT_5_GPIO      gpio_num_t::GPIO_NUM_4


// File Manager
#define SUPPORT_SD
#define SD_CARD_MISO_PIN         gpio_num_t::GPIO_NUM_19
#define SD_CARD_MOSI_PIN         gpio_num_t::GPIO_NUM_23
#define SD_CARD_CLK_PIN          gpio_num_t::GPIO_NUM_18
#define SD_CARD_CS_PIN           gpio_num_t::GPIO_NUM_5

// I2C
#define SDA                     gpio_num_t::GPIO_NUM_21
#define SCL                     gpio_num_t::GPIO_NUM_32

// Output Types
#define SUPPORT_OutputType_DMX              // UART
#define SUPPORT_OutputType_GECE             // UART
#define SUPPORT_OutputType_GS8208           // UART / RMT
#define SUPPORT_OutputType_Renard           // UART
#define SUPPORT_OutputType_Serial           // UART
#define SUPPORT_OutputType_TM1814           // UART / RMT
#define SUPPORT_OutputType_UCS1903          // UART / RMT
#define SUPPORT_OutputType_UCS8903          // UART / RMT
#define SUPPORT_OutputType_WS2811           // UART / RMT

// Ethernet Configuration
#define SUPPORT_ETHERNET
#define SUPPORT_W5500

// W5500 Pin Configuration
#define W5500_CS_PIN            gpio_num_t::GPIO_NUM_15  // Use GPIO 12 for W5500 CS
#define W5500_RST_PIN           gpio_num_t::GPIO_NUM_2  // Use GPIO 16 for W5500 RST
#define W5500_INT_PIN           gpio_num_t::GPIO_NUM_4  // Use GPIO 17 for W5500 INT (optional)

// SPI Pins for W5500 (shared with SD card)
#define W5500_MISO_PIN          gpio_num_t::GPIO_NUM_12
#define W5500_MOSI_PIN          gpio_num_t::GPIO_NUM_13
#define W5500_SCK_PIN           gpio_num_t::GPIO_NUM_14
// Built-in Ethernet Configuration (optional, comment out if not used)
/*
#define DEFAULT_ETH_CLK_MODE           eth_clock_mode_t::ETH_CLOCK_GPIO0_IN
#define DEFAULT_ETH_POWER_PIN          gpio_num_t::GPIO_NUM_15
#define DEFAULT_ETH_POWER_PIN_ACTIVE   HIGH
#define DEFAULT_ETH_TYPE               eth_phy_type_t::ETH_PHY_LAN8720
#define DEFAULT_ETH_ADDR               ETH_ADDR_PHY_LAN8720
#define DEFAULT_ETH_TXEN               gpio_num_t::GPIO_NUM_21
#define DEFAULT_ETH_TXD0               gpio_num_t::GPIO_NUM_19
#define DEFAULT_ETH_TXD1               gpio_num_t::GPIO_NUM_22
#define DEFAULT_ETH_CRSDV              gpio_num_t::GPIO_NUM_27
#define DEFAULT_ETH_RXD0               gpio_num_t::GPIO_NUM_25
#define DEFAULT_ETH_RXD1               gpio_num_t::GPIO_NUM_26
#define DEFAULT_ETH_MDC_PIN            gpio_num_t::GPIO_NUM_23
#define DEFAULT_ETH_MDIO_PIN           gpio_num_t::GPIO_NUM_18
*/