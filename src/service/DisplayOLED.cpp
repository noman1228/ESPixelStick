/*
 * DisplayOLED.cpp
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
 *
 */

#include "ESPixelStick.h"
#ifdef SUPPORT_OLED

#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "service/DisplayOLED.h"
#include <TimeLib.h>
#include "network/NetworkMgr.hpp"

// Initialize U8g2 display object for I2C (adjust for your specific display model)
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* clock=*/SCL, /* data=*/SDA);
int getSignalStrength(int rssi)
{
    if (rssi > -50)
        return 4; // Excellent signal
    else if (rssi > -60)
        return 3; // Good signal
    else if (rssi > -70)
        return 2; // Fair signal
    else if (rssi > -80)
        return 1; // Weak signal
    return 0;     // No signal
}
void c_OLED::Begin()
{
    // Initialize the display
    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);  // Set the font
    u8g2.drawStr(0, 12, "Loading...  "); // Draw loading message
    u8g2.sendBuffer();
    u8g2.setDisplayRotation(U8G2_R0); // Set rotation

    logcon(String(F("OLED Loaded")));
}

void c_OLED::Update()
{
    if (now() > updateTimer_OLED) // Lets check if OLED needs an update only once every 30 seconds
    {
        updateTimer_OLED = now() + 30;

        String currIP = NetworkMgr.GetlocalIP().toString();
        String currHost;
        NetworkMgr.GetHostname(currHost);
        int rssi = WiFi.RSSI();

        // Convert RSSI to signal strength (0-4 bars)
        int signalStrength = getSignalStrength(rssi);

        // Draw the WiFi signal bars
        u8g2.clearBuffer();

        for (int i = 0; i < signalStrength; i++)
        {
            int barHeight = (i + 1) * 4;
            u8g2.drawBox(105 + i * 6, 16 - barHeight, 4, barHeight);
        }


        if (!dispIP.equals(currIP) || !dispHostName.equals(currHost) || dispRSSI != signalStrength)
        {

            u8g2.setFont(u8g2_font_ncenB08_tr); // Set the font

            u8g2.drawStr(0, 18, ("IP: " + currIP).c_str());
            u8g2.drawStr(0, 30, ("HOST: " + currHost).c_str());


            u8g2.sendBuffer(); // Send the buffer to the display

            dispIP = currIP;
            dispHostName = currHost;
            dispRSSI = signalStrength;
            logcon(String(F("OLED UPDATED")));
        }
    }
}

c_OLED OLED;

#endif // SUPPORT_OLED