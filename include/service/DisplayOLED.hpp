#ifndef DISPLAY_OLED_HPP
#define DISPLAY_OLED_HPP

#include "Arduino.h"
#include "Preferences.h"
#include "U8g2lib.h"
#include "network/NetworkMgr.hpp"
#include "FPPDiscovery.h"

// Display states
enum class DisplayState { NETWORK, PLAYING, ERROR };

class c_OLED {
public:
    c_OLED();
    void Begin(); // Initialize the OLED display
    void Poll(); // Main polling function to update the display
    void Update(bool forceUpdate = false); // Update the display based on the current state
    void Flip(); // Flip the display orientation

    bool error_global; // Global error flag

private:
    DisplayState currentState; // Current display state (NETWORK, PLAYING, ERROR)
    unsigned long lastUpdateTime; // Timestamp of the last update
    unsigned long lastErrorFlashTime; // Timestamp of the last error flash
    bool errorDisplayed; // Flag to track if the error is currently displayed

    void UpdateNetworkInfo(bool forceUpdate); // Update and display network information
    void UpdatePlayingStatus(); // Update and display playing status
    void UpdateErrorFlashing(); // Handle error flashing logic
    void DrawCentered(int y, const String& text); // Draw centered text on the display
    int getSignalStrength(int rssi); // Convert RSSI to a signal strength indicator (0-4)
};

extern c_OLED OLED; // Global instance of the OLED class

#endif // DISPLAY_OLED_HPP
