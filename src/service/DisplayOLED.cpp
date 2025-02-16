// In DisplayOLED.cpp
#ifdef SUPPORT_OLED
#include "service/DisplayOLED.h"
#include "service/FPPDiscovery.h"
#include <ArduinoJson.h>
#include "network/NetworkMgr.hpp"
#include <Preferences.h>
#include <WiFi.h> // Ensure WiFi.h is included
#include <TimeLib.h> // Ensure TimeLib.h is included

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, GPIO_NUM_22, GPIO_NUM_21);
Preferences preferences;

void c_OLED::UpdateRunningStatus() {
    JsonDocument doc; // Use JsonDocument instead of DynamicJsonDocument
    JsonObject jsonStatus = doc.to<JsonObject>();

    // Fetch FPP Remote status using the global instance
    FPPDiscovery.GetStatus(jsonStatus);

    // Extract data from JSON
    String statusLine1 = "NOT";
    String statusLine2 = "PLAYING";
    String statusLine3 = "REMOTE FILE";

    // Check if FPPDiscovery data exists and extract it
    if (jsonStatus["FPPDiscovery"].is<JsonObject>()) { // Ensure "FPPDiscovery" exists
        JsonObject fpp = jsonStatus["FPPDiscovery"];
        // Override statusLine1 with "ERROR" if it should be displayed
        String currentSequence = fpp["current_sequence"].as<String>();
        String timein = fpp["time_elapsed"].as<String>();
        String timeleft = fpp["time_remaining"].as<String>();
        String syncAdj = fpp["SyncAdjustmentCount"].as<String>();
        String syncCnt = fpp["SyncCount"].as<String>();
        statusLine1 = currentSequence;
        statusLine2 = timein + " / " + timeleft;
        statusLine3 = "Adj:" + syncAdj + " Cnt: " + syncCnt;
    }
    
    // Display on OLED
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    
    // Center statusLine1
    int statusLine1Width = u8g2.getStrWidth(statusLine1.c_str());
    int x1 = (128 - statusLine1Width) / 2; // Center horizontally on a 128-pixel wide display
    u8g2.drawStr(x1, 10, statusLine1.c_str()); // Y-coordinate is 10 (first line)
    
    // Center statusLine2
    int statusLine2Width = u8g2.getStrWidth(statusLine2.c_str());
    int x2 = (128 - statusLine2Width) / 2; // Center horizontally
    u8g2.drawStr(x2, 20, statusLine2.c_str()); // Y-coordinate is 20 (second line)
    
    // Center statusLine3
    int statusLine3Width = u8g2.getStrWidth(statusLine3.c_str());
    int x3 = (128 - statusLine3Width) / 2; // Center horizontally
    u8g2.drawStr(x3, 30, statusLine3.c_str()); // Y-coordinate is 30 (third line)
    
    u8g2.sendBuffer();
}

c_OLED::c_OLED() :
    currentPage(DisplayPage::NETWORK_INFO),
    lastPageSwitchTime(0),
    pageSwitchInterval(7500) // 7.5 seconds
    
{
}

void c_OLED::Begin()
{
    error_global = false;
    u8g2.begin();
    preferences.begin("oled", false);
    pinMode(25, INPUT_PULLUP);
    digitalWrite(25, HIGH);

    int flipState = preferences.getInt("flipState", 0);
    u8g2.setDisplayRotation(flipState ? U8G2_R2 : U8G2_R0);

    preferences.end();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 12, "Initialization...");
    u8g2.sendBuffer();
}

void c_OLED::UpdateErrorFlashing()
{
    static bool errorDisplayed = false; // Tracks whether "ERROR" is currently displayed
    static unsigned long lastFlashTime = 0; // Tracks the last time the display was updated
    const unsigned long flashInterval = 500; // Flash interval in milliseconds (500ms on, 500ms off)

    // Toggle the error display state every 500ms
    unsigned long currentTime = millis();
    if (currentTime - lastFlashTime >= flashInterval) {
        lastFlashTime = currentTime;
        errorDisplayed = !errorDisplayed; // Toggle the display state
    }

    // Display "ERROR" or blank the screen
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    if (errorDisplayed) {
        int errorWidth = u8g2.getStrWidth("ERROR");
        int x = (128 - errorWidth) / 2; // Center "ERROR" horizontally
        u8g2.drawStr(x, 16, "ERROR"); // Y-coordinate is 16 (middle of the screen)
    }
    u8g2.sendBuffer();
}

void c_OLED::Poll()
{
    static unsigned long lastDebounceTime = 0;
    const unsigned long debounceDelay = 1000;

    // Button handling for display flip
    if (digitalRead(25) == LOW && (millis() - lastDebounceTime > debounceDelay))
    {
        Flip();
        Update(true);
        lastDebounceTime = millis();
    }

    // Auto-page switching
    if (millis() - lastPageSwitchTime >= pageSwitchInterval)
    {
        // Cycle through pages: NETWORK_INFO -> RUNNING_STATUS -> ERROR_FLASHING

        if(error_global){
            currentPage = DisplayPage::ERROR_FLASHING;
            lastPageSwitchTime = millis();
            Update(true);
            return;;
        }
        switch (currentPage) {

            case DisplayPage::NETWORK_INFO:
                currentPage = DisplayPage::RUNNING_STATUS;
                break;
            case DisplayPage::RUNNING_STATUS:
                currentPage = DisplayPage::NETWORK_INFO;
                break;
        }
        lastPageSwitchTime = millis();
        Update(true);
    }

    Update();
}

void c_OLED::Update(bool forceUpdate)
{
    switch (currentPage) {
        case DisplayPage::NETWORK_INFO:
            UpdateNetworkInfo(forceUpdate);
            break;
        case DisplayPage::RUNNING_STATUS:
            UpdateRunningStatus();
            break;
        case DisplayPage::ERROR_FLASHING:
            UpdateErrorFlashing();
            break;
    }
}

void c_OLED::UpdateNetworkInfo(bool forceUpdate)
{
    String currIP = NetworkMgr.GetlocalIP().toString();
    String currHost;
    NetworkMgr.GetHostname(currHost);
    int rssi = WiFi.RSSI();
    int signalStrength = getSignalStrength(rssi);

    String wifiMode = (WiFi.getMode() == WIFI_STA) ? "STA" : "AP";
    String reason = getUpdateReason(currIP, dispIP, currHost, dispHostName, signalStrength, dispRSSI);

    if (forceUpdate || !reason.isEmpty())
    {
        u8g2.clearBuffer();

        // WiFi mode indicator
        u8g2.setFont(u8g2_font_5x8_tr);
        u8g2.drawStr(95, 8, wifiMode.c_str());
        int modeWidth = u8g2.getStrWidth(wifiMode.c_str());
        u8g2.drawFrame(93, 0, modeWidth + 5, 10);

        // Signal strength bars
        for (int i = 0; i < signalStrength; i++) {
            int barHeight = (i + 1) * 4;
            u8g2.drawBox(105 + i * 6, 16 - barHeight, 4, barHeight);
        }

        // Main info
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.drawStr(0, 18, ("IP: " + currIP).c_str());
        u8g2.drawStr(0, 30, ("HOST: " + currHost).c_str());

        dispIP = currIP;
        dispHostName = currHost;
        dispRSSI = signalStrength;
    }
    u8g2.sendBuffer();
}

void c_OLED::Flip()
{
    preferences.begin("oled", false);
    int flipState = preferences.getInt("flipState", 0);
    flipState = !flipState;
    u8g2.setDisplayRotation(flipState ? U8G2_R2 : U8G2_R0);
    preferences.putInt("flipState", flipState);
    preferences.end();
}

int c_OLED::getSignalStrength(int rssi)
{
    if (rssi > -50) return 4;
    if (rssi > -60) return 3;
    if (rssi > -70) return 2;
    if (rssi > -80) return 1;
    return 0;
}

String c_OLED::getUpdateReason(String currIP, String dispIP, String currHost, 
                              String dispHostName, int signalStrength, int dispRSSI)
{
    String reason;
    if (currIP != dispIP) reason += "IP ";
    if (currHost != dispHostName) reason += "Host ";
    if (signalStrength != dispRSSI) reason += "RSSI ";
    return reason;
}

c_OLED OLED;
#endif