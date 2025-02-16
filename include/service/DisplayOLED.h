#pragma once
/*
 * DisplayOLED.h
 *
 * Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
 * Copyright (c) 2018, 2024 Shelby Merrick
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
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WiFi.h> // Include WiFi.h for WiFi functions
#include <TimeLib.h> // Include TimeLib.h for millis()
#include "input/InputFPPRemote.h" // Include the base class header


class c_OLED
{
private:
    String dispIP;
    String reason;
    String dispHostName;
    uint64_t dispRSSI;

public:
    c_OLED();
    virtual ~c_OLED() {}

    void Begin(); // Initialize the OLED display
    void Poll();  // Handle button input and auto-page switching
    void Update(bool forceUpdate = false); // Update the display
    void Flip();  // Flip the display orientation
    void Loading(); // Display loading message
    
    void UpdateErrorFlashing();
    void GetDriverName(String &name) { name = "OLED"; }

private:
enum class DisplayPage {
    NETWORK_INFO,
    RUNNING_STATUS,
    ERROR_FLASHING
};


    // Member variables
    DisplayPage currentPage;
    unsigned long lastPageSwitchTime;
    const unsigned long pageSwitchInterval; // 10 seconds

    // Display-related variables
    String dispIP;
    String dispHostName;
    int dispRSSI;
    bool error_global;
    // Helper functions
    int getSignalStrength(int rssi);
    String getUpdateReason(String currIP, String dispIP, String currHost, String dispHostName, int signalStrength, int dispRSSI);

    // Display update functions
    void UpdateNetworkInfo(bool forceUpdate);
    void UpdateRunningStatus();
};

extern c_OLED OLED;

#endif // DISPLAYOLED_HPP