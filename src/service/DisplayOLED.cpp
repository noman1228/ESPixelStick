/*
* DisplayOLED.cpp
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
*/

#ifdef SUPPORT_OLED

#include "service/DisplayOLED.h"

#include "input/InputMgr.hpp"
#include "network/NetworkMgr.hpp"
#include "service/FPPDiscovery.h"

#include <U8g2lib.h>
#include <Wire.h>

#ifndef ESP_SCL_PIN
#define ESP_SCL_PIN SCL
#endif

#ifndef ESP_SDA_PIN
#define ESP_SDA_PIN SDA
#endif

// SSD1306 I2C addresses (0x3C is the most common; 0x3D is alternative)
static const uint8_t OLED_I2C_ADDR = 0x3C;

static U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C gDisplay(U8G2_R0, U8X8_PIN_NONE, ESP_SCL_PIN, ESP_SDA_PIN);

static const unsigned long RefreshIntervalMs = 1000;
static const unsigned long PageSwitchMs = 7000;
static const unsigned long ToastMs = 3500;
static const unsigned long ControlActiveHoldMs = 3000;

c_OLED::c_OLED()
{
}

void c_OLED::Begin()
{
    if (initialized)
    {
        return;
    }

    // Initialise the bus and set a short timeout so that a missing or
    // misbehaving display cannot stall setup() / the main loop.
    Wire.begin(ESP_SDA_PIN, ESP_SCL_PIN);
    Wire.setClock(400000);       // 400 kHz - faster frame transfers (~14 ms vs ~55 ms at 100 kHz)
    Wire.setTimeOut(50);         // 50 ms per transaction; prevents indefinite hangs

    // Quick probe: only proceed if the display ACKs its I2C address
    Wire.beginTransmission(OLED_I2C_ADDR);
    if (Wire.endTransmission() != 0)
    {
        // No display found - stay uninitialised, Poll() will be a no-op
        return;
    }

    if (!gDisplay.begin())
    {
        return;
    }

    initialized = true;
    lastRefreshMs = millis();
    lastPageSwitchMs = millis();

    gDisplay.clearBuffer();
    gDisplay.setFont(u8g2_font_6x10_tf);
    gDisplay.drawStr(20, 14, "ESPixelStick");
    gDisplay.drawStr(8, 28, "OLED Ready");
    gDisplay.sendBuffer();
}

void c_OLED::ShowToast(const String & message, const String & title)
{
    if (!initialized)
    {
        return;
    }

    toastMessage = message;
    toastTitle = title;
    toastStartMs = millis();
    toastActive = true;

    gDisplay.clearBuffer();
    gDisplay.setFont(u8g2_font_6x10_tf);

    if (!toastTitle.isEmpty())
    {
        gDisplay.drawStr(0, 12, toastTitle.c_str());
        gDisplay.drawHLine(0, 14, 128);
        gDisplay.drawStr(0, 30, toastMessage.c_str());
    }
    else
    {
        gDisplay.drawStr(0, 18, toastMessage.c_str());
    }

    gDisplay.sendBuffer();
}

void c_OLED::UpdateStatus(bool force)
{
    if (!initialized)
    {
        return;
    }

    unsigned long nowMs = millis();
    if (!force && ((nowMs - lastRefreshMs) < RefreshIntervalMs))
    {
        return;
    }
    lastRefreshMs = nowMs;

    JsonDocument statusDoc;
    JsonObject root = statusDoc.to<JsonObject>();
    JsonObject system = root[F("system")].to<JsonObject>();
    NetworkMgr.GetStatus(system);
    InputMgr.GetStatus(root);

    JsonObject network = system[F("network")];
    JsonObject wifiStatus = network[F("wifi")];
    JsonObject ethStatus = network[F("eth")];

    const bool wifiConnected = wifiStatus[F("connected")] | false;
    const bool ethConnected = ethStatus[F("connected")] | false;

    String wifiIp = wifiStatus[F("ip")].is<const char*>() ? wifiStatus[F("ip")].as<String>() : String("0.0.0.0");
    String ethIp = ethStatus[F("ip")].is<const char*>() ? ethStatus[F("ip")].as<String>() : String("0.0.0.0");
    String ethGateway = ethStatus[F("gateway")].is<const char*>() ? ethStatus[F("gateway")].as<String>() : String("-");
    int wifiRssi = wifiStatus[F("rssi")] | 0;

    JsonArray inputStatus = root[F("input")].as<JsonArray>();
    JsonObject primaryInput = inputStatus.size() > 0 ? inputStatus[0].as<JsonObject>() : JsonObject();
    JsonObject secondaryInput = inputStatus.size() > 1 ? inputStatus[1].as<JsonObject>() : JsonObject();

    uint32_t e131PacketCount = primaryInput[F("e131")][F("num_packets")] | 0;
    uint32_t ddpPacketCount = primaryInput[F("ddp")][F("packetsreceived")] | 0;
    uint32_t artnetPacketCount = primaryInput[F("Artnet")][F("num_packets")] | 0;

    if (e131PacketCount > lastE131PacketCount)
    {
        lastControlRxMs = nowMs;
        lastControlSource = F("E1.31");
    }
    if (ddpPacketCount > lastDdpPacketCount)
    {
        lastControlRxMs = nowMs;
        lastControlSource = F("DDP");
    }
    if (artnetPacketCount > lastArtnetPacketCount)
    {
        lastControlRxMs = nowMs;
        lastControlSource = F("Artnet");
    }

    lastE131PacketCount = e131PacketCount;
    lastDdpPacketCount = ddpPacketCount;
    lastArtnetPacketCount = artnetPacketCount;

    const bool receivingNetworkControl =
        (lastControlRxMs != 0) && ((nowMs - lastControlRxMs) <= ControlActiveHoldMs);

    String activeEffect = secondaryInput[F("effects")][F("currenteffect")].is<const char*>()
        ? secondaryInput[F("effects")][F("currenteffect")].as<String>()
        : String();
    const bool effectsRunning = !activeEffect.isEmpty();

    JsonObject playerStatus = secondaryInput[F("Player")];
    const bool playbackActive = playerStatus[F("active")] | false;
    String playbackName = F("Playing");
    String playbackTiming = emptyString;
    if (playbackActive)
    {
        if (playerStatus[F("FPPDiscovery")].is<JsonObject>())
        {
            JsonObject p = playerStatus[F("FPPDiscovery")];
            playbackName = p[F("current_sequence")].is<const char*>() ? p[F("current_sequence")].as<String>() : F("Remote");
            String elapsed = p[F("time_elapsed")].is<const char*>() ? p[F("time_elapsed")].as<String>() : String("--:--");
            String remaining = p[F("time_remaining")].is<const char*>() ? p[F("time_remaining")].as<String>() : String("--:--");
            playbackTiming = elapsed + F("/") + remaining;
        }
        else if (playerStatus[F("File")].is<JsonObject>())
        {
            JsonObject p = playerStatus[F("File")];
            playbackName = p[F("current_sequence")].is<const char*>() ? p[F("current_sequence")].as<String>() : F("File");
            String elapsed = p[F("time_elapsed")].is<const char*>() ? p[F("time_elapsed")].as<String>() : String("--:--");
            String remaining = p[F("time_remaining")].is<const char*>() ? p[F("time_remaining")].as<String>() : String("--:--");
            playbackTiming = elapsed + F("/") + remaining;
        }
        else if (playerStatus[F("PlayList")].is<JsonObject>())
        {
            JsonObject p = playerStatus[F("PlayList")][F("File")];
            playbackName = p[F("current_sequence")].is<const char*>() ? p[F("current_sequence")].as<String>() : F("Playlist");
            String elapsed = p[F("time_elapsed")].is<const char*>() ? p[F("time_elapsed")].as<String>() : String("--:--");
            String remaining = p[F("time_remaining")].is<const char*>() ? p[F("time_remaining")].as<String>() : String("--:--");
            playbackTiming = elapsed + F("/") + remaining;
        }
        else if (playerStatus[F("Effect")].is<JsonObject>())
        {
            JsonObject p = playerStatus[F("Effect")];
            playbackName = p[F("currenteffect")].is<const char*>() ? p[F("currenteffect")].as<String>() : F("Effect");
            playbackTiming = p[F("TimeRemaining")].is<const char*>() ? p[F("TimeRemaining")].as<String>() : String();
        }
    }

    enum class e_Page : uint8_t
    {
        WiFi,
        Ethernet,
        Control,
        Effects,
        Playback,
        None
    };

    e_Page pages[5] = {e_Page::None, e_Page::None, e_Page::None, e_Page::None, e_Page::None};
    uint8_t pageCount = 0;
    if (wifiConnected) { pages[pageCount++] = e_Page::WiFi; }
    if (ethConnected) { pages[pageCount++] = e_Page::Ethernet; }
    if (receivingNetworkControl) { pages[pageCount++] = e_Page::Control; }
    else if (effectsRunning) { pages[pageCount++] = e_Page::Effects; }
    if (playbackActive) { pages[pageCount++] = e_Page::Playback; }
    if (0 == pageCount) { pages[pageCount++] = e_Page::WiFi; }

    if (currentPageIndex >= pageCount)
    {
        currentPageIndex = 0;
    }
    else if ((nowMs - lastPageSwitchMs) >= PageSwitchMs)
    {
        currentPageIndex = (currentPageIndex + 1) % pageCount;
        lastPageSwitchMs = nowMs;
    }

    gDisplay.clearBuffer();
    gDisplay.setFont(u8g2_font_6x10_tf);

    switch (pages[currentPageIndex])
    {
        case e_Page::WiFi:
        {
            gDisplay.drawStr(0, 10, "WiFi");
            gDisplay.drawHLine(0, 12, 128);
            gDisplay.drawStr(0, 22, wifiConnected ? wifiIp.c_str() : "Disconnected");
            String line3 = String("RSSI ") + String(wifiRssi);
            gDisplay.drawStr(0, 32, line3.c_str());
            break;
        }
        case e_Page::Ethernet:
        {
            gDisplay.drawStr(0, 10, "Ethernet");
            gDisplay.drawHLine(0, 12, 128);
            gDisplay.drawStr(0, 22, ethConnected ? ethIp.c_str() : "Disconnected");
            String line3 = String("GW ") + ethGateway;
            gDisplay.drawStr(0, 32, line3.c_str());
            break;
        }
        case e_Page::Control:
        {
            gDisplay.drawStr(0, 10, "Network Ctrl");
            gDisplay.drawHLine(0, 12, 128);
            gDisplay.drawStr(0, 22, lastControlSource.c_str());
            uint32_t packetCount = 0;
            if (lastControlSource == F("DDP"))
            {
                packetCount = ddpPacketCount;
            }
            else if (lastControlSource == F("Artnet"))
            {
                packetCount = artnetPacketCount;
            }
            else
            {
                packetCount = e131PacketCount;
            }
            String line3 = String("Pkts ") + String(packetCount);
            gDisplay.drawStr(0, 32, line3.c_str());
            break;
        }
        case e_Page::Effects:
        {
            gDisplay.drawStr(0, 10, "Effects");
            gDisplay.drawHLine(0, 12, 128);
            gDisplay.drawStr(0, 22, activeEffect.c_str());
            gDisplay.drawStr(0, 32, "Local engine");
            break;
        }
        case e_Page::Playback:
        {
            gDisplay.drawStr(0, 10, "Playback");
            gDisplay.drawHLine(0, 12, 128);
            gDisplay.drawStr(0, 22, playbackName.c_str());
            gDisplay.drawStr(0, 32, playbackTiming.c_str());
            break;
        }
        case e_Page::None:
        default:
        {
            gDisplay.drawStr(0, 10, "Network");
            gDisplay.drawHLine(0, 12, 128);
            gDisplay.drawStr(0, 22, "No activity");
            gDisplay.drawStr(0, 32, "Waiting...");
            break;
        }
    }

    gDisplay.sendBuffer();
}

void c_OLED::Poll()
{
    if (!initialized)
    {
        return;
    }

    if (toastActive)
    {
        if ((millis() - toastStartMs) >= ToastMs)
        {
            toastActive = false;
            UpdateStatus(true);
        }
        return;
    }

    UpdateStatus(false);
}

c_OLED OLED;

#endif // def SUPPORT_OLED
