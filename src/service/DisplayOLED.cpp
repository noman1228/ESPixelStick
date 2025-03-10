#include "service/DisplayOLED.hpp"

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);

void OLEDTask(void *pvParameters)
{
    while (1)
    {
        OLED.Update();
        vTaskDelay(pdMS_TO_TICKS(1000)); // Update more frequently for upload status
    }
}

c_OLED::c_OLED() : currentPage(DisplayPage::NETWORK_INFO), lastPageSwitchTime(0), flipState(0), beginFileUpload(false), lastUploadUpdate(0), uploadFilename(""), uploadProgress(0) {}

void c_OLED::Begin()
{
    u8g2.begin();
    error_global = false;

#ifdef BUTTON_GPIO1
    pinMode(BUTTON_GPIO1, INPUT);
#endif

    u8g2.setDisplayRotation(flipState ? U8G2_R2 : U8G2_R0);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.drawStr(0, 12, "Initialization...");
    u8g2.sendBuffer();

    xTaskCreatePinnedToCore(
        OLEDTask,
        "OLED Update",
        4096,
        NULL,
        1,
        &oledTaskHandle,
        1);
}

void c_OLED::status(int progress, const String &filename)
{
    beginFileUpload = true;
    lastUploadUpdate = millis();
    uploadFilename = filename;
    uploadProgress = progress;
}

void c_OLED::UpdateUploadStatus()
{
    if (beginFileUpload)
    {
        if (millis() - lastUploadUpdate >= uploadTimeout)
        {
            beginFileUpload = false; // Timeout reached, hide upload status
            return;
        }

        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);

        String progressText = "Expand: " + String(uploadProgress) + "%";
        int progressBarWidth = (uploadProgress * 128) / 100; // Scale progress bar

        u8g2.drawStr(0, 12, uploadFilename.c_str());
        u8g2.drawStr(0, 24, progressText.c_str());
        u8g2.drawBox(0, 28, progressBarWidth, 4); // Draw progress bar

        u8g2.sendBuffer();
    }
}

void c_OLED::UpdateRunningStatus()
{
    static unsigned long lastUpdateTime = 0;
    const unsigned long updateInterval = 10000;

    if (millis() - lastUpdateTime >= updateInterval || error_global) {
        if (FPPDiscovery.PlayingAfile()) {
            JsonDocument doc;
            JsonObject jsonStatus = doc.to<JsonObject>();
            FPPDiscovery.GetStatus(jsonStatus);

            if (jsonStatus["FPPDiscovery"].is<JsonObject>()) {
                JsonObject fpp = jsonStatus["FPPDiscovery"];

                String statusLine1 = fpp[String(CN_current_sequence)].as<String>();
                String statusLine2 = fpp[String(CN_time_elapsed)].as<String>() + " / " + fpp[String(CN_time_remaining)].as<String>();


                u8g2.clearBuffer();
                u8g2.setFont(u8g2_font_ncenB08_tr);

                auto drawCentered = [](int y, const String &text)
                {
                    int width = u8g2.getStrWidth(text.c_str());
                    u8g2.drawStr((128 - width) / 2, y, text.c_str());
                };

                drawCentered(2, statusLine1);
                drawCentered(20, statusLine2);


                u8g2.sendBuffer();
            }
        }
        lastUpdateTime = millis();
    }
}


void c_OLED::Update(bool forceUpdate)
{
    if (beginFileUpload)
    {
        UpdateUploadStatus();
    }
    else if (currentPage == DisplayPage::NETWORK_INFO)
    {
        UpdateNetworkInfo(forceUpdate);
    }
    else
    {
        UpdateRunningStatus();
    }
}

void c_OLED::Poll()
{
    static unsigned long lastDebounceTime = 0;
    const unsigned long debounceDelay = 1000;

#ifdef BUTTON_GPIO1
    if (digitalRead(BUTTON_GPIO1) == LOW && millis() - lastDebounceTime > debounceDelay)
    {
        Flip();
        Update(true);
        lastDebounceTime = millis();
    }
#endif

    if (!beginFileUpload && millis() - lastPageSwitchTime >= pageSwitchInterval)
    {
        currentPage = (FPPDiscovery.PlayingAfile() && currentPage == DisplayPage::NETWORK_INFO) ? DisplayPage::RUNNING_STATUS : DisplayPage::NETWORK_INFO;
        lastPageSwitchTime = millis();
        Update(true);
    }
}
int c_OLED::getSignalStrength(int rssi)
{
    if (rssi > -50)
        return 4;
    if (rssi > -60)
        return 3;
    if (rssi > -70)
        return 2;
    if (rssi > -80)
        return 1;
    return 0;
}
void c_OLED::UpdateNetworkInfo(bool forceUpdate)
{
    String currIP = NetworkMgr.GetlocalIP().toString();
    String currHost;
    NetworkMgr.GetHostname(currHost);
    int signalStrength = getSignalStrength(WiFi.RSSI());

    if (forceUpdate || currIP != dispIP || currHost != dispHostName || signalStrength != dispRSSI)
    {
        u8g2.clearBuffer();

        for (int i = 0; i < signalStrength; i++)
        {
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

void c_OLED::Flip()
{
    flipState = !flipState;
    u8g2.setDisplayRotation(flipState ? U8G2_R2 : U8G2_R0);
}

void c_OLED::GetConfig(JsonObject &json)
{
    JsonObject oledConfig = json[(char *)CN_oled].to<JsonObject>();
    JsonWrite(oledConfig, CN_flipState, flipState);
}

bool c_OLED::SetConfig(JsonObject &json)
{
    bool ConfigChanged = false;

    JsonObject oledConfig = json[(char *)CN_oled];
    if (!oledConfig)
    {
        logcon(F("No OLED settings found."));
        return false;
    }

    uint32_t t = flipState;
    ConfigChanged |= setFromJSON(t, oledConfig, CN_flipState);
    flipState = t;

    u8g2.setDisplayRotation(flipState ? U8G2_R2 : U8G2_R0);

    return ConfigChanged;
}
c_OLED OLED;