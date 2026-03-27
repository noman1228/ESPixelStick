#pragma once
/*
* DisplayOLED.h
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
*/

#ifdef SUPPORT_OLED

#include "ESPixelStick.h"

class c_OLED
{
public:
    c_OLED();

    void Begin();
    void Poll();
    void ShowToast(const String & message, const String & title = emptyString);
    void GetDriverName(String & name) const { name = "OLED"; }

private:
    void UpdateStatus(bool force = false);

    bool initialized = false;
    bool toastActive = false;
    uint8_t currentPageIndex = 0;
    unsigned long lastRefreshMs = 0;
    unsigned long lastPageSwitchMs = 0;
    unsigned long toastStartMs = 0;
    unsigned long lastControlRxMs = 0;
    uint32_t lastE131PacketCount = 0;
    uint32_t lastDdpPacketCount = 0;
    uint32_t lastArtnetPacketCount = 0;
    String lastControlSource;
    String toastMessage;
    String toastTitle;
};

extern c_OLED OLED;

#endif // def SUPPORT_OLED
