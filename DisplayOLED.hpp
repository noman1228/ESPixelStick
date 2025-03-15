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

#include "ESPixelStick.h"
#include <U8g2lib.h>
#include <ArduinoJson.h>
#include "network/NetworkMgr.hpp"
#include <Preferences.h>
#include "ConstNames.hpp"
#include "service/FPPDiscovery.h"
class c_OLED {
public:
    c_OLED();
    void Begin();
    void Poll();
    void Update(bool forceUpdate = false);
    void Flip();
    void GetConfig(JsonObject& json);
    bool SetConfig(JsonObject& json);
    void initNow();
    void GetDriverName(String& name) const {
        name = "OLED";
    }
private:
    void UpdateRunningStatus();
    void UpdateErrorFlashing();
    void UpdateNetworkInfo(bool forceUpdate);
    int getSignalStrength(int rssi);
    String getUpdateReason(String currIP, String dispIP, String currHost, String dispHostName, int signalStrength, int dispRSSI);

    enum class DisplayPage {
        NETWORK_INFO,
        RUNNING_STATUS
    };

    DisplayPage currentPage;
    unsigned long lastPageSwitchTime;
    const unsigned long pageSwitchInterval;

    bool error_global;
    String dispIP;
    String dispHostName;
    int dispRSSI;

    uint8_t flipState; // Stores the flip state (0 or 1)
};

extern c_OLED OLED;


