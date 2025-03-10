#pragma once
/*
* GPIO_Defs_ESP32_8CHWROVER.hpp - Output Management class
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


#define DEFAULT_RMT_0_GPIO GPIO_NUM_27
#define DEFAULT_RMT_1_GPIO GPIO_NUM_26
#define DEFAULT_RMT_2_GPIO GPIO_NUM_25
#define DEFAULT_RMT_3_GPIO GPIO_NUM_33
#define DEFAULT_RMT_4_GPIO GPIO_NUM_25
#define DEFAULT_RMT_5_GPIO GPIO_NUM_14
#define DEFAULT_RMT_6_GPIO GPIO_NUM_12
#define DEFAULT_RMT_7_GPIO GPIO_NUM_13

#define DEFAULT_I2C_SDA         GPIO_NUM_21
#define DEFAULT_I2C_SCL         GPIO_NUM_22

// File Manager
#define SUPPORT_SD
#define SD_CARD_MISO_PIN        GPIO_NUM_19
#define SD_CARD_MOSI_PIN        GPIO_NUM_23
#define SD_CARD_CLK_PIN         GPIO_NUM_18
#define SD_CARD_CS_PIN          GPIO_NUM_5

#define BUTTON_GPIO1 GPIO_NUM_34



// Output Types
// Not Finished - #define SUPPORT_OutputType_TLS3001
#define SUPPORT_OutputType_WS2811           // UART / RMT
