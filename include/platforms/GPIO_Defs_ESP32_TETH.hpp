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

#define SUPPORT_ETHERNET

//Output Manager
#define DEFAULT_RMT_0_GPIO      gpio_num_t::GPIO_NUM_15
#define DEFAULT_RMT_1_GPIO      gpio_num_t::GPIO_NUM_2
#define DEFAULT_RMT_2_GPIO      gpio_num_t::GPIO_NUM_4
#define DEFAULT_RMT_3_GPIO      gpio_num_t::GPIO_NUM_3

// File Manager
#define SUPPORT_SD
#define SD_CARD_MISO_PIN         gpio_num_t::GPIO_NUM_34
#define SD_CARD_MOSI_PIN         gpio_num_t::GPIO_NUM_13
#define SD_CARD_CLK_PIN          gpio_num_t::GPIO_NUM_14
#define SD_CARD_CS_PIN           gpio_num_t::GPIO_NUM_5


#define SDA                     gpio_num_t::GPIO_NUM_33
#define SCL                     gpio_num_t::GPIO_NUM_32

#include <ETH.h>
#define DEFAULT_ETH_CLK_MODE           eth_clock_mode_t::ETH_CLOCK_GPIO0_IN
#define DEFAULT_ETH_POWER_PIN          gpio_num_t::GPIO_NUM_12
#define DEFAULT_ETH_POWER_PIN_ACTIVE   LOW

#define DEFAULT_ETH_TYPE               eth_phy_type_t::ETH_PHY_RTL8201


#define DEFAULT_ETH_ADDR               0
#define DEFAULT_ETH_MDC_PIN            gpio_num_t::GPIO_NUM_23
#define DEFAULT_ETH_MDIO_PIN           gpio_num_t::GPIO_NUM_18
#define ETH_TYPE                        ETH_PHY_RTL8201
#define ETH_ADDR                        0
#define ETH_CLK_MODE                    ETH_CLOCK_GPIO0_IN
#define ETH_RESET_PIN                   -1
#define ETH_MDC_PIN                     23
#define ETH_POWER_PIN                   12
#define ETH_MDIO_PIN                    18
#define SD_MISO_PIN                     34
#define SD_MOSI_PIN                     13
#define SD_SCLK_PIN                     14
#define SD_CS_PIN                       5



// Output Types
#define SUPPORT_OutputType_WS2811         // UART / RMT