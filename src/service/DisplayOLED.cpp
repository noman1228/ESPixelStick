#include "service/DisplayOLED.hpp"
#include <driver/rmt.h>
// Initialize the global instance of the OLED class
c_OLED OLED;
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, GPIO_NUM_22, GPIO_NUM_21);
Preferences preferences;
c_OLED::c_OLED() : currentState(DisplayState::NETWORK), lastUpdateTime(0), lastErrorFlashTime(0), errorDisplayed(false), error_global(false) {}
void c_OLED::Begin() {
    #ifdef BUTTON_GPIO1
        pinMode(BUTTON_GPIO1, INPUT);
    #endif
    u8g2.begin();
    preferences.begin("oled", false);
    u8g2.setDisplayRotation(preferences.getInt("flipState", 0) ? U8G2_R2 : U8G2_R0);
    preferences.end();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB14_tr);
    DrawCentered(12, "Initialization...");
    u8g2.sendBuffer();
}

void c_OLED::Poll() {
    unsigned long currentTime = millis();
    static unsigned long lastDebounceTime = 0;
    const unsigned long debounceDelay = 1000;
    #ifdef BUTTON_GPIO1
    if (digitalRead(BUTTON_GPIO1) == LOW && millis() - lastDebounceTime > debounceDelay) {
        Flip();
        Update(true);
        lastDebounceTime = millis();
    }
    #endif
    // Handle error flashing
    if (error_global) {
        UpdateErrorFlashing();
        return;
    }

    // Update display state based on conditions
    if (currentTime - lastUpdateTime >= (FPPDiscovery.PlayingAfile() ? 5000 : 10000)) {
        if (FPPDiscovery.PlayingAfile()) {
            currentState = (currentState == DisplayState::NETWORK) ? DisplayState::PLAYING : DisplayState::NETWORK;
        } else {
            currentState = DisplayState::NETWORK;
        }
        lastUpdateTime = currentTime;
        Update(true);
    }
}

void c_OLED::Update(bool forceUpdate) {
    switch (currentState) {
        case DisplayState::NETWORK:
            UpdateNetworkInfo(forceUpdate);
            break;
        case DisplayState::PLAYING:
            UpdatePlayingStatus();
            break;
        case DisplayState::ERROR:
            UpdateErrorFlashing();
            break;
    }
}

void c_OLED::UpdateNetworkInfo(bool forceUpdate) {
    static String dispIP, dispHostName;
    static int dispRSSI = -1;

    String currIP = NetworkMgr.GetlocalIP().toString();
    String currHost;
    NetworkMgr.GetHostname(currHost);
    int signalStrength = getSignalStrength(WiFi.RSSI());

    if (forceUpdate || currIP != dispIP || currHost != dispHostName || signalStrength != dispRSSI) {
        u8g2.clearBuffer();
        for (int i = 0; i < signalStrength; i++) {
            u8g2.drawBox(105 + i * 6, 16 - (i + 1) * 4, 4, (i + 1) * 4);
        }

        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.drawStr(0, 18, ("IP: " + currIP).c_str());
        u8g2.drawStr(0, 30, ("HOST: " + currHost).c_str());

        dispIP = currIP;
        dispHostName = currHost;
        dispRSSI = signalStrength;
        u8g2.sendBuffer();
    }
}

void c_OLED::UpdatePlayingStatus() {
    JsonDocument doc;
    JsonObject jsonStatus = doc.to<JsonObject>();
    FPPDiscovery.GetStatus(jsonStatus);

    if (jsonStatus["FPPDiscovery"].is<JsonObject>()) {
        JsonObject fpp = jsonStatus["FPPDiscovery"];
        String statusLine1 = fpp[String(CN_current_sequence)].as<String>();
        String statusLine2 = fpp[String(CN_time_elapsed)].as<String>() + " / " + fpp[String(CN_time_remaining)].as<String>();
        String statusLine3 = "ERRORS: " + fpp["errors"].as<String>() + "   CT: " + fpp["SyncCount"].as<String>();

        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        DrawCentered(10, statusLine1);
        DrawCentered(22, statusLine2);
        DrawCentered(34, statusLine3);
        u8g2.sendBuffer();
    }
}

void c_OLED::UpdateErrorFlashing() {
    if (millis() - lastErrorFlashTime >= 500) {
        lastErrorFlashTime = millis();
        errorDisplayed = !errorDisplayed;

        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        if (errorDisplayed) {
            DrawCentered(16, "ERROR");
        }
        u8g2.sendBuffer();
    }
}

void c_OLED::DrawCentered(int y, const String& text) {
    int width = u8g2.getStrWidth(text.c_str());
    u8g2.drawStr((128 - width) / 2, y, text.c_str());
}

void c_OLED::Flip() {
    preferences.begin("oled", false);
    int flipState = !preferences.getInt("flipState", 0);
    u8g2.setDisplayRotation(flipState ? U8G2_R2 : U8G2_R0);
    preferences.putInt("flipState", flipState);
    preferences.end();
}

int c_OLED::getSignalStrength(int rssi) {
    if (rssi > -50) return 4;
    if (rssi > -60) return 3;
    if (rssi > -70) return 2;
    if (rssi > -80) return 1;
    return 0;
}