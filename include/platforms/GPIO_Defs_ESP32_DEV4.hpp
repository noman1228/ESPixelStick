#pragma once
/*
* GPIO_Defs_ESP32_DEV4.hpp - Output Management class
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


// RMT GPIOs
#define DEFAULT_RMT_0_GPIO gpio_num_t::GPIO_NUM_27
#define DEFAULT_RMT_1_GPIO gpio_num_t::GPIO_NUM_26
#define DEFAULT_RMT_2_GPIO gpio_num_t::GPIO_NUM_33
#define DEFAULT_RMT_3_GPIO gpio_num_t::GPIO_NUM_32
#define DEFAULT_RMT_4_GPIO gpio_num_t::GPIO_NUM_2
#define DEFAULT_RMT_5_GPIO gpio_num_t::GPIO_NUM_4


// SPI Output
#define SUPPORT_SPI_OUTPUT
#define DEFAULT_SPI_DATA_GPIO gpio_num_t::GPIO_NUM_13
#define DEFAULT_SPI_CLOCK_GPIO gpio_num_t::GPIO_NUM_14
#define DEFAULT_SPI_CS_GPIO gpio_num_t::GPIO_NUM_15
#define DEFAULT_SPI_DEVICE HSPI_HOST

// I2C
#define DEFAULT_I2C_SDA gpio_num_t::GPIO_NUM_21
#define DEFAULT_I2C_SCL gpio_num_t::GPIO_NUM_22

// File Manager
#define SUPPORT_SD
#define SD_CARD_MISO_PIN gpio_num_t::GPIO_NUM_19
#define SD_CARD_MOSI_PIN gpio_num_t::GPIO_NUM_23
#define SD_CARD_CLK_PIN gpio_num_t::GPIO_NUM_18
#define SD_CARD_CS_PIN gpio_num_t::GPIO_NUM_5

// Button
#define BUTTON_GPIO1 gpio_num_t::GPIO_NUM_35


// Output Types
// Not Finished - #define SUPPORT_OutputType_TLS3001
#define SUPPORT_OutputType_APA102           // SPI
#define SUPPORT_OutputType_DMX              // UART
#define SUPPORT_OutputType_GECE             // UART
#define SUPPORT_OutputType_GS8208           // UART / RMT
#define SUPPORT_OutputType_Renard           // UART
#define SUPPORT_OutputType_Serial           // UART
#define SUPPORT_OutputType_TM1814           // UART / RMT
#define SUPPORT_OutputType_UCS1903          // UART / RMT
#define SUPPORT_OutputType_UCS8903          // UART / RMT
#define SUPPORT_OutputType_WS2801           // SPI
#define SUPPORT_OutputType_WS2811           // UART / RMT
#define SUPPORT_OutputType_Relay            // GPIO




