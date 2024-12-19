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
    
    // Display loading screen initially
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr); // Set font for the loading screen
    u8g2.drawStr(0, 12, "Loading...");  // Draw loading message
    u8g2.sendBuffer();  // Send the buffer to the display immediately

    logcon(String(F("OLED Loaded")));
}

void c_OLED::Update()
{
    if (now() > updateTimer_OLED) // Check if OLED needs an update only once every 30 seconds
    {
        updateTimer_OLED = now() + 30;

        String currIP = NetworkMgr.GetlocalIP().toString();
        String currHost;
        NetworkMgr.GetHostname(currHost);
        int rssi = WiFi.RSSI();

        // Convert RSSI to signal strength (0-4 bars)
        int signalStrength = getSignalStrength(rssi);

        // Get the current Wi-Fi mode (AP or STA)
        String wifiMode = (WiFi.getMode() == WIFI_STA) ? "STA" : "AP";

        // Only update if there is a change in IP, Hostname, or Signal Strength
        if (currIP != dispIP || currHost != dispHostName || signalStrength != dispRSSI)
        {
            // Clear the buffer to prepare for a fresh update
            u8g2.clearBuffer();

            // Draw WiFi signal bars and mode
            u8g2.setFont(u8g2_font_5x8_tr); // Set smaller font for WiFi mode
            u8g2.drawStr(95, 8, wifiMode.c_str()); // Draw "AP" or "STA" above the bars

            // Draw a box around the WiFi mode
            int modeWidth = u8g2.getStrWidth(wifiMode.c_str());
            u8g2.drawFrame(93, 0, modeWidth +5, 10); // Adjust position and size as needed

            for (int i = 0; i < signalStrength; i++)
            {
                int barHeight = (i + 1) * 4;
                u8g2.drawBox(105 + i * 6, 16 - barHeight, 4, barHeight); // Draw each bar
            }

            // Set font for text and draw IP address if changed
            u8g2.setFont(u8g2_font_ncenB08_tr); 
            u8g2.drawStr(0, 18, ("IP: " + currIP).c_str());

            // Draw Hostname if changed
            u8g2.drawStr(0, 30, ("HOST: " + currHost).c_str());

            // Update stored values to avoid unnecessary redraw in the next cycle
            dispIP = currIP;
            dispHostName = currHost;
            dispRSSI = signalStrength;

            logcon(String(F("OLED UPDATED"))); // Log the update
        }
        
        // Send the buffer to the display
        u8g2.sendBuffer();
    }
}

c_OLED OLED;

#endif // SUPPORT_OLED
