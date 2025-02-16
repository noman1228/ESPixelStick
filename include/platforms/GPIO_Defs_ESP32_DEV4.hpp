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
#ifndef MY_GPIO_CONFIG_HPP
#define MY_GPIO_CONFIG_HPP

#include "driver/gpio.h" // Required for ESP32 GPIO functions

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
#define BUTON_GPIO1 gpio_num_t::GPIO_NUM_35
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
#define SUPPORT_OutputType_Servo_PCA9685    // I2C (default pins)
void resetAllGPIOs8()
{
// RMT GPIOs
#ifdef DEFAULT_RMT_0_GPIO
    gpio_reset_pin(DEFAULT_RMT_0_GPIO);
#endif
#ifdef DEFAULT_RMT_1_GPIO
    gpio_reset_pin(DEFAULT_RMT_1_GPIO);
#endif
#ifdef DEFAULT_RMT_2_GPIO
    gpio_reset_pin(DEFAULT_RMT_2_GPIO);
#endif
#ifdef DEFAULT_RMT_3_GPIO
    gpio_reset_pin(DEFAULT_RMT_3_GPIO);
#endif
#ifdef DEFAULT_RMT_4_GPIO
    gpio_reset_pin(DEFAULT_RMT_4_GPIO);
#endif
#ifdef DEFAULT_RMT_5_GPIO
    gpio_reset_pin(DEFAULT_RMT_5_GPIO);
#endif
#ifdef DEFAULT_RMT_6_GPIO
    gpio_reset_pin(DEFAULT_RMT_6_GPIO);
#endif
#ifdef DEFAULT_RMT_7_GPIO
    gpio_reset_pin(DEFAULT_RMT_7_GPIO);
#endif
#ifdef DEFAULT_UART_0_GPIO
    gpio_reset_pin(DEFAULT_UART_0_GPIO);
#endif
#ifdef DEFAULT_UART_1_GPIO
    gpio_reset_pin(DEFAULT_UART_1_GPIO);
#endif
#ifdef DEFAULT_UART_2_GPIO
    gpio_reset_pin(DEFAULT_UART_2_GPIO);
#endif
#ifdef DEFAULT_UART_3_GPIO
    gpio_reset_pin(DEFAULT_UART_3_GPIO);
#endif
// SPI GPIOs
#ifdef DEFAULT_SPI_DATA_GPIO
    gpio_reset_pin(DEFAULT_SPI_DATA_GPIO);
#endif
#ifdef DEFAULT_SPI_CLOCK_GPIO
    gpio_reset_pin(DEFAULT_SPI_CLOCK_GPIO);
#endif
#ifdef DEFAULT_SPI_CS_GPIO
    gpio_reset_pin(DEFAULT_SPI_CS_GPIO);
#endif

// I2C GPIOs
#ifdef DEFAULT_I2C_SDA
    gpio_reset_pin(DEFAULT_I2C_SDA);
#endif
#ifdef DEFAULT_I2C_SCL
    gpio_reset_pin(DEFAULT_I2C_SCL);
#endif

// SD Card GPIOs
#ifdef SD_CARD_MISO_PIN
    gpio_reset_pin(SD_CARD_MISO_PIN);
#endif
#ifdef SD_CARD_MOSI_PIN
    gpio_reset_pin(SD_CARD_MOSI_PIN);
#endif
#ifdef SD_CARD_CLK_PIN
    gpio_reset_pin(SD_CARD_CLK_PIN);
#endif
#ifdef SD_CARD_CS_PIN
    gpio_reset_pin(SD_CARD_CS_PIN);
#endif

// Button GPIO
#ifdef BUTON_GPIO1
    gpio_reset_pin(BUTON_GPIO1);
#endif
}

// Automatically reset GPIOs when this header is included
struct AutoGPIOReset
{
    AutoGPIOReset()
    {
        resetAllGPIOs8();
    }
};

static AutoGPIOReset autoReset;

#endif // MY_GPIO_CONFIG_HPP