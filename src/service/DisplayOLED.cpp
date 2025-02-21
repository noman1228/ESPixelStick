#ifdef SUPPORT_OLED
#include "service/DisplayOLED.h"
#include "ESPixelStick.h"
#include "Wire.h"
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, GPIO_NUM_22, GPIO_NUM_21);
Preferences preferences;

void c_OLED::UpdateRunningStatus() {
    static unsigned long lastUpdateTime = 0;
    const unsigned long updateInterval = 10000; // 10 seconds

    if (millis() - lastUpdateTime >= updateInterval || error_global) {
        if (FPPDiscovery.PlayingAfile()) {
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

                auto drawCentered = [](int y, const String& text) {
                    int width = u8g2.getStrWidth(text.c_str());
                    u8g2.drawStr((128 - width) / 2, y, text.c_str());
                };

                drawCentered(2, statusLine1);
                drawCentered(16, statusLine2);
                drawCentered(30, statusLine3);

                u8g2.sendBuffer();
            }
        }
        lastUpdateTime = millis();
    }
}

c_OLED::c_OLED() : currentPage(DisplayPage::NETWORK_INFO), lastPageSwitchTime(0), pageSwitchInterval(7500) {}

void c_OLED::Begin() {
    u8g2.begin();
    error_global = false;
    preferences.begin("oled", false);
    pinMode(BUTTON_GPIO1, INPUT_PULLUP);

    u8g2.setDisplayRotation(preferences.getInt("flipState", 0) ? U8G2_R2 : U8G2_R0);
    preferences.end();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.drawStr(0, 12, "Initialization...");
    u8g2.sendBuffer();
}

void c_OLED::UpdateErrorFlashing() {
    static bool errorDisplayed = false;
    static unsigned long lastFlashTime = 0;
    const unsigned long flashInterval = 500;

    if (millis() - lastFlashTime >= flashInterval) {
        lastFlashTime = millis();
        errorDisplayed = !errorDisplayed;

        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        if (errorDisplayed) {
            int errorWidth = u8g2.getStrWidth("ERROR");
            u8g2.drawStr((128 - errorWidth) / 2, 16, "ERROR");
        }
        u8g2.sendBuffer();
    }
}

void c_OLED::Poll() {
    static unsigned long lastDebounceTime = 0;
    const unsigned long debounceDelay = 1000;

    if (digitalRead(BUTTON_GPIO1) == LOW && millis() - lastDebounceTime > debounceDelay) {
        Flip();
        Update(true);
       // DUMPER(0xFFFFFFFFF);
        lastDebounceTime = millis();

    }

    if (millis() - lastPageSwitchTime >= pageSwitchInterval) {
        currentPage = (FPPDiscovery.PlayingAfile() && currentPage == DisplayPage::NETWORK_INFO) ? DisplayPage::RUNNING_STATUS : DisplayPage::NETWORK_INFO;
        lastPageSwitchTime = millis();
        Update(true);
    }
}

void c_OLED::Update(bool forceUpdate) {
    if (currentPage == DisplayPage::NETWORK_INFO) {
        UpdateNetworkInfo(forceUpdate);
    } else {
        UpdateRunningStatus();
    }
}

void c_OLED::UpdateNetworkInfo(bool forceUpdate) {
    String currIP = NetworkMgr.GetlocalIP().toString();
    String currHost;
    NetworkMgr.GetHostname(currHost);
    int signalStrength = getSignalStrength(WiFi.RSSI());

    if (forceUpdate || currIP != dispIP || currHost != dispHostName || signalStrength != dispRSSI) {
        u8g2.clearBuffer();

        u8g2.setFont(u8g2_font_5x8_tr);
        String wifiMode = (WiFi.getMode() == WIFI_STA) ? "STA" : "AP";
        u8g2.drawStr(95, 8, wifiMode.c_str());
        u8g2.drawFrame(93, 0, u8g2.getStrWidth(wifiMode.c_str()) + 5, 10);

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

String c_OLED::getUpdateReason(String currIP, String dispIP, String currHost, String dispHostName, int signalStrength, int dispRSSI) {
    String reason;
    if (currIP != dispIP) reason += "IP ";
    if (currHost != dispHostName) reason += "Host ";
    if (signalStrength != dispRSSI) reason += "RSSI ";
    return reason;
}

c_OLED OLED;
#endif