#ifdef SUPPORT_OLED
#include "service/DisplayOLED.h"
#include <Preferences.h>

// Constants for timing and display
#define TOAST_DURATION_MS          5000
#define NETWORK_UPDATE_INTERVAL_MS 10000
#define BUTTON_DEBOUNCE_DELAY_MS   1000
#define VERSION_STRING             "4.x-dev"
bool isRebooting = false;
// Display mutex and handles
static SemaphoreHandle_t displayMutex = nullptr;
static TaskHandle_t oledTaskHandle = nullptr;
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);

static void OLEDTask(void *)
{
    while (true)
    {
        if (displayMutex != nullptr && xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            OLED.Update();
            xSemaphoreGive(displayMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(OLED_TASK_DELAY_MS));
    }
}

c_OLED::c_OLED()
    : currentPage(DisplayPage::NETWORK_INFO),
      lastPageSwitchTime(0),
      lastNetworkUpdate(0),
      isUploading(false),
      uploadProgress(0),
      lastUploadUpdate(0),
      dispRSSI(0),
      isToastActive(false),
      toastStartTime(0),
      flipState(false),
      preferences() {}

void c_OLED::Begin()
{
    displayMutex = xSemaphoreCreateMutex();
    if (displayMutex == nullptr) return;

    preferences.begin("oled", false);
    flipState = preferences.getBool("flipState", false);

    u8g2.setDisplayRotation(flipState ? U8G2_R2 : U8G2_R0);
    if (!u8g2.begin()) return;

    BaseType_t taskCreated = xTaskCreatePinnedToCore(
        OLEDTask,
        "OLEDTask",
        4096,
        nullptr,
        1,
        &oledTaskHandle,
        1
    );
    if (taskCreated != pdPASS) return;

#ifdef BUTTON_GPIO1
    pinMode(BUTTON_GPIO1, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_GPIO1), buttonISR, FALLING);
#endif

    InitNow();
}
static void DrawCenteredWrappedText(U8G2 &u8g2, const String &text, uint8_t maxWidth, uint8_t xOrigin, uint8_t yOrigin);

void c_OLED::InitNow()
{
    if (displayMutex == nullptr || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    u8g2.clearBuffer();
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    // Layer 2
    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.drawStr(2, 10, "ESPixelStick");
    u8g2.setFont(u8g2_font_t0_22b_tr);
    u8g2.drawStr(-2, 32, "INITIALIZING");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(91, 11, VERSION_STRING);
    u8g2.sendBuffer();

    xSemaphoreGive(displayMutex);
}

void c_OLED::End()
{
    if (oledTaskHandle != nullptr) {
        vTaskDelete(oledTaskHandle);
        oledTaskHandle = nullptr;
    }
    if (displayMutex != nullptr) {
        vSemaphoreDelete(displayMutex);
        displayMutex = nullptr;
    }
    preferences.end();
}

void c_OLED::status(int progress, const String &filename)
{
    if (isRebooting || displayMutex == nullptr || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    isUploading = true;
    lastUploadUpdate = millis();
    uploadFilename = filename;
    uploadProgress = progress;
    xSemaphoreGive(displayMutex);
}

void c_OLED::UpdateUploadStatus()
{
    if (isRebooting || displayMutex == nullptr || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    String progressText = "Expand: " + String(uploadProgress) + "%";
    u8g2.drawStr(0, 12, uploadFilename.c_str());
    u8g2.drawStr(0, 28, progressText.c_str());
    int barWidth = map(uploadProgress, 0, 100, 0, 128);
    u8g2.drawFrame(0, 30, 128, 2);
    u8g2.drawBox(0, 30, barWidth, 2);
    u8g2.sendBuffer();

    xSemaphoreGive(displayMutex);
}

void c_OLED::UpdateUploadStatus(const String &filename, int progress)
{
    if (isRebooting || displayMutex == nullptr || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
#ifdef STATUS_LED1
    digitalWrite(STATUS_LED1, !digitalRead(STATUS_LED2));
#endif
    uploadFilename = filename;
    uploadProgress = progress;
    isUploading = true;
    xSemaphoreGive(displayMutex);
    UpdateUploadStatus();
}

void c_OLED::UpdateRunningStatus()
{
    if (isRebooting || displayMutex == nullptr || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    static JsonDocument doc;
    doc.clear();
    JsonObject jsonStatus = doc.to<JsonObject>();
    FPPDiscovery.GetStatus(jsonStatus);
    if (jsonStatus.isNull() || !jsonStatus["current_sequence"].is<String>()) {
        xSemaphoreGive(displayMutex);
        return;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, jsonStatus["current_sequence"].as<String>().c_str());
    u8g2.drawStr(0, 20, (jsonStatus["time_elapsed"].as<String>() + " / " + jsonStatus["time_remaining"].as<String>()).c_str());
    u8g2.sendBuffer();

    xSemaphoreGive(displayMutex);
}

void c_OLED::UpdateNetworkInfo(bool forceUpdate)
{
    if (isRebooting || displayMutex == nullptr || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    String currIP = NetworkMgr.GetlocalIP().toString();
    String currHost;
    NetworkMgr.GetHostname(currHost);
    int signalStrength = getSignalStrength(WiFi.RSSI());

    if (forceUpdate || currIP != dispIP || currHost != dispHostName || signalStrength != dispRSSI)
    {
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

    xSemaphoreGive(displayMutex);
}

void c_OLED::Update(bool forceUpdate)
{
    if (isRebooting)    {
        unsigned long now = millis();          
            if (displayMutex != nullptr && xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE)
            {
                u8g2.clearBuffer();
                u8g2.setFont(u8g2_font_profont15_tr);
                u8g2.drawStr(22, 12, "ESPixelStick");
                u8g2.setFont(u8g2_font_t0_22b_tr);
                u8g2.drawStr(15, 29, "REBOOTING");
                u8g2.sendBuffer();
                xSemaphoreGive(displayMutex);
            }        
        return;
    }

    if (displayMutex == nullptr || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    unsigned long now = millis();

    if (isToastActive)
    {
        if (now - toastStartTime >= TOAST_DURATION_MS) {
            isToastActive = false;
            u8g2.setDisplayRotation(flipState ? U8G2_R2 : U8G2_R0);
            xSemaphoreGive(displayMutex);
            Update(true);
            return;
        }
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr); // or whatever your favorite is
        DrawCenteredWrappedText(u8g2, toastMessage, 120, 0, 0);

        u8g2.sendBuffer();
        xSemaphoreGive(displayMutex);
        return;
    }

    if (isUploading) {
        xSemaphoreGive(displayMutex);
        UpdateUploadStatus();
        return;
    }

    if (currentPage == DisplayPage::NETWORK_INFO) {
        if (forceUpdate || now - lastNetworkUpdate >= NETWORK_UPDATE_INTERVAL_MS) {
            xSemaphoreGive(displayMutex);
            UpdateNetworkInfo(forceUpdate);
            lastNetworkUpdate = now;
            return;
        }
    } else if (currentPage == DisplayPage::RUNNING_STATUS) {
        xSemaphoreGive(displayMutex);
        UpdateRunningStatus();
        return;
    }

    xSemaphoreGive(displayMutex);
}
void DrawCenteredWrappedText(U8G2 &u8g2, const String &text, uint8_t maxWidth, uint8_t xOrigin, uint8_t yOrigin)
{
    const uint8_t lineHeight = u8g2.getFontAscent() - u8g2.getFontDescent() + 2;
    std::vector<String> lines;

    String currentLine = "";
    String currentWord = "";

    for (uint16_t i = 0; i < text.length(); i++) {
        char c = text.charAt(i);
        if (c == ' ' || c == '\n') {
            if (u8g2.getStrWidth((currentLine + currentWord).c_str()) > maxWidth) {
                lines.push_back(currentLine);
                currentLine = currentWord + " ";
            } else {
                currentLine += currentWord + " ";
            }
            currentWord = "";
            if (c == '\n') {
                lines.push_back(currentLine);
                currentLine = "";
            }
        } else {
            currentWord += c;
        }
    }

    if (!currentWord.isEmpty()) {
        if (u8g2.getStrWidth((currentLine + currentWord).c_str()) > maxWidth) {
            lines.push_back(currentLine);
            currentLine = currentWord;
        } else {
            currentLine += currentWord;
        }
    }
    if (!currentLine.isEmpty()) lines.push_back(currentLine);

    // Calculate total height
    uint8_t totalHeight = lineHeight * lines.size();
    uint8_t startY = yOrigin + ((32 - totalHeight) / 2);

    for (size_t i = 0; i < lines.size(); i++) {
        uint8_t strWidth = u8g2.getStrWidth(lines[i].c_str());
        uint8_t x = xOrigin + ((128 - strWidth) / 2);
        uint8_t y = startY + (i * lineHeight);
        u8g2.drawStr(x, y, lines[i].c_str());
    }
}

void c_OLED::ShowRebootScreen()
{
    if (displayMutex == nullptr || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    isRebooting = true;
    ESP.restart();
    xSemaphoreGive(displayMutex);
}

int c_OLED::getSignalStrength(int rssi)
{
    if (rssi > -50) return 4;
    if (rssi > -60) return 3;
    if (rssi > -70) return 2;
    if (rssi > -80) return 1;
    return 0;
}

void IRAM_ATTR c_OLED::buttonISR()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (oledTaskHandle != nullptr) {
        xTaskNotifyFromISR(oledTaskHandle, 1, eSetBits, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
    }
}

void c_OLED::Flip()
{
    if (isRebooting || displayMutex == nullptr || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    flipState = !flipState;
    u8g2.setDisplayRotation(flipState ? U8G2_R2 : U8G2_R0);
    preferences.putBool("flipState", flipState);
    u8g2.updateDisplay();
    xSemaphoreGive(displayMutex);
}

void c_OLED::ShowToast(const String &message)
{
    if (isRebooting || displayMutex == nullptr || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    toastMessage = message;
    isToastActive = true;
    toastStartTime = millis();
    xSemaphoreGive(displayMutex);
}

void c_OLED::Poll()
{
    if (isRebooting) return;

    static unsigned long lastDebounceTime = 0;
    uint32_t notificationValue = 0;

    if (xTaskNotifyWait(0, ULONG_MAX, &notificationValue, 0) == pdTRUE) {
        if ((millis() - lastDebounceTime) > BUTTON_DEBOUNCE_DELAY_MS) {
            Flip();
            Update(true);
            lastDebounceTime = millis();
        }
    }

#ifdef BUTTON_GPIO1
    if (digitalRead(BUTTON_GPIO1) == LOW && (millis() - lastDebounceTime) > BUTTON_DEBOUNCE_DELAY_MS) {
        Flip();
        Update(true);
        lastDebounceTime = millis();
    }
#endif

    if (!isUploading && millis() - lastPageSwitchTime >= pageSwitchInterval) {
        if (FPPDiscovery.PlayingAfile()) {
            currentPage = (currentPage == DisplayPage::NETWORK_INFO)
                ? DisplayPage::RUNNING_STATUS
                : DisplayPage::NETWORK_INFO;
        } else {
            currentPage = DisplayPage::NETWORK_INFO;
        }

        lastPageSwitchTime = millis();
        Update(true);
    }
}

c_OLED OLED;
#endif
