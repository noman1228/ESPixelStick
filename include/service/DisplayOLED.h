#pragma once
/*
 * DisplayOLED.h
 *
 * Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
 * Copyright (c) 2018, 2025 Shelby Merrick
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

 */
#ifdef SUPPORT_OLED
#include "ESPixelStick.h"
#include <U8g2lib.h>
#include "service/FPPDiscovery.h"
#include <ArduinoJson.h>
#include "network/NetworkMgr.hpp"
#include <Preferences.h>
#include "ConstNames.hpp"

class c_OLED {
public:
    c_OLED();
    virtual ~c_OLED() = default;

    void Begin();                          // Initialize the OLED display
    void Poll();                           // Handle button input and auto-page switching
    void Update(bool forceUpdate = false); // Update the display
    void Flip();                           // Flip the display orientation
    void UpdateErrorFlashing();            // Flash error message
    

    enum class DisplayPage { NETWORK_INFO, RUNNING_STATUS, ERROR_FLASHING };

    // Member variables
    DisplayPage currentPage = DisplayPage::NETWORK_INFO;
    unsigned long lastPageSwitchTime = 0;
    const unsigned long pageSwitchInterval = 7500; // 7.5 seconds

    // Display-related variables
    String dispIP, dispHostName;
    int dispRSSI = 0;
    bool error_global = false;

private:
    // Helper functions
    int getSignalStrength(int rssi);
    String getUpdateReason(String currIP, String dispIP, String currHost, String dispHostName, int signalStrength, int dispRSSI);
    // Display update functions
    void UpdateNetworkInfo(bool forceUpdate);
    void UpdateRunningStatus();
    void DUMPER(uint64_t gpio_mask) {
        DEBUG_V("GPIO Configuration Dump:");
    
        for (int gpio = 0; gpio < 40; gpio++) {  // ESP32 
            if (gpio_mask & (1ULL << gpio)) {    // Check if this GPIO is in the mask
                // Get the GPIO mode
                const char* mode = "UNKNOWN";
                uint32_t gpio_reg = GPIO_PIN_MUX_REG[gpio];  // Get the GPIO mux register address
                if (gpio_reg != 0) {  // Check if the GPIO is valid
                    uint32_t func = (*(volatile uint32_t*)gpio_reg) & 0x3FF;  // Get the function (mode)
                    if (func == 0x100) {
                        mode = "INPUT";
                    } else if (func == 0x200) {
                        mode = "OUTPUT";
                    } else if (func == 0x300) {
                        mode = "INPUT/OUTPUT";
                    } else String(mode) = String(func);
                }
                bool pullup = (GPIO_PIN_MUX_REG[gpio] & 0x400) != 0;  // Check pull-up
                bool pulldown = (GPIO_PIN_MUX_REG[gpio] & 0x800) != 0;  // Check pull-down
                String rslt = String("GPIO " + String(gpio) + ": Mode=" + mode + ", PULL-UP=" + (pullup ? "ENABLED" : "DISABLED") + ", PULL-DOWN=" + (pulldown ? "ENABLED" : "DISABLED"));
                DEBUG_V(rslt);
    
            }
        }
    }
};

extern c_OLED OLED;

#endif // DISPLAYOLED_HPP