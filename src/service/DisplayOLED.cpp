#include "service/DisplayOLED.h"
#include "ESPixelStick.h"
// === GLOBALS ===

static SemaphoreHandle_t displayMutex = nullptr;
static TaskHandle_t oledTaskHandle = nullptr;
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCL, SDA);

// === INTERNAL UTILS ===

bool acquireDisplayLock() {
    return displayMutex && (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE);
}

void releaseDisplayLock() {
    if (displayMutex) {
        xSemaphoreGive(displayMutex);
    } else {
        // Optional: log or assert
         LOG_PORT.println("releaseDisplayLock() called with null mutex!");
    }
}

// === TASK ===

static void OLEDTask(void*) {
    while (true) {
        if (acquireDisplayLock()) {
            OLED.Update();
            releaseDisplayLock();
        }
        vTaskDelay(pdMS_TO_TICKS(OLED_TASK_DELAY_MS));
    }
}

// === CONSTRUCTOR ===

c_OLED::c_OLED()
    : currentPage(DisplayPage::NETWORK_INFO),
      lastPageSwitchTime(0),
      lastNetworkUpdate(0),
      isUploading(false),
      uploadProgress(0),
      error_global(false),
      lastUploadUpdate(0),
      dispRSSI(0),
      preferences() {}

// === BEGIN / END ===

void c_OLED::Begin() {
    displayMutex = xSemaphoreCreateMutex();
    if (!displayMutex) return;

    preferences.begin("oled", false);
    flipState = preferences.getBool("flipState", false);

    u8g2.setDisplayRotation(flipState ? U8G2_R2 : U8G2_R0);
    if (!u8g2.begin()) return;

    if (xTaskCreatePinnedToCore(OLEDTask, "OLEDTask", 4096, nullptr, 10, &oledTaskHandle, 0) != pdPASS)
        return;

#ifdef BUTTON_GPIO1
    pinMode(BUTTON_GPIO1, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_GPIO1), buttonISR, FALLING);
#endif

    InitNow();
}

void c_OLED::End() {
    if (oledTaskHandle) {
        vTaskDelete(oledTaskHandle);
        oledTaskHandle = nullptr;
    }
    if (displayMutex) {
        vSemaphoreDelete(displayMutex);
        displayMutex = nullptr;
    }
    preferences.end();
}

// === INITIAL DISPLAY ===

void c_OLED::InitNow() {
    if (!acquireDisplayLock()) return;

    u8g2.clearBuffer();
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.drawStr(2, 10, "ESPixelStick");
    u8g2.setFont(u8g2_font_t0_22b_tr);
    u8g2.drawStr(-2, 32, "INITIALIZING");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(91, 11, VERSION_STRING);
    u8g2.sendBuffer();
    releaseDisplayLock();
}

// === REBOOT ===

void c_OLED::ShowRebootScreen() {
    if (!acquireDisplayLock()) return;
    isRebooting = true;
    lastRebootFlash = millis();
    rebootFlashState = false;
    releaseDisplayLock();
}

void c_OLED::drawreboot() {
    u8g2.clearBuffer();
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setFont(u8g2_font_t0_22b_tr);
    u8g2.drawStr(3, 28, "REBOOTING");
    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.drawStr(4, 12, "ESPixelStick");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(90, 11, VERSION_STRING);
    u8g2.drawXBM(108, 13, 15, 16, image_clock);
    u8g2.drawFrame(0, 0, 128, 31);
    u8g2.sendBuffer();
}

void c_OLED::handleRebootDisplay() {
    unsigned long now = millis();
    if (now - lastRebootFlash >= REBOOT_FLASH_INTERVAL_MS) {
        rebootFlashState = !rebootFlashState;
        u8g2.setDisplayRotation(rebootFlashState ? U8G2_R2 : U8G2_R0);
        lastRebootFlash = now;

        if (acquireDisplayLock()) {
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_profont15_tr);
            u8g2.drawStr(22, 12, "ESPixelStick");
            u8g2.setFont(u8g2_font_t0_22b_tr);
            u8g2.drawStr(15, 29, "REBOOTING");
            u8g2.sendBuffer();
            releaseDisplayLock();
        }
    }
}

// === TOAST ===

void c_OLED::ShowToast(const String& message) {
    if (!acquireDisplayLock()) return;
    toastMessage = message;
    isToastActive = true;
    toastStartTime = millis();
    lastToastFlash = millis();
    toastFlashState = false;
    releaseDisplayLock();
}

void c_OLED::drawCenteredToast(const String& message) {
    int16_t textWidth = u8g2.getUTF8Width(message.c_str());
    uint8_t fontHeight = u8g2.getMaxCharHeight();

    if (textWidth <= 128) {
        int x = (128 - textWidth) / 2;
        int y = (32 + fontHeight) / 2;
        u8g2.drawStr(x, y, message.c_str());
    } else {
        int splitPos = message.length() / 2;
        while (splitPos > 0 && message.charAt(splitPos) != ' ') --splitPos;
        if (splitPos == 0) splitPos = message.length() / 2;

        String line1 = message.substring(0, splitPos);
        String line2 = message.substring(splitPos + 1);

        int x1 = (128 - u8g2.getUTF8Width(line1.c_str())) / 2;
        int x2 = (128 - u8g2.getUTF8Width(line2.c_str())) / 2;
        int y1 = (32 - fontHeight * 2) / 2 + fontHeight;
        int y2 = y1 + fontHeight;

        u8g2.drawStr(x1, y1, line1.c_str());
        u8g2.drawStr(x2, y2, line2.c_str());
    }
}

void c_OLED::handleToastDisplay() {
    unsigned long now = millis();

    if (now - toastStartTime >= TOAST_DURATION_MS) {
        isToastActive = false;
        toastFlashState = false;
        u8g2.setDisplayRotation(flipState ? U8G2_R2 : U8G2_R0);
        Update(true);
        return;
    }

    if (now - lastToastFlash >= TOAST_FLASH_INTERVAL_MS) {
        toastFlashState = !toastFlashState;
        u8g2.setDisplayRotation(toastFlashState ? U8G2_R2 : U8G2_R0);
        lastToastFlash = now;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    drawCenteredToast(toastMessage);
    u8g2.sendBuffer();
}

// === STATUS ===

void c_OLED::status(int progress, const String& filename) {
    if (isRebooting || !acquireDisplayLock()) return;
    isUploading = true;
    lastUploadUpdate = millis();
    uploadFilename = filename;
    uploadProgress = progress;
    releaseDisplayLock();
}

void c_OLED::UpdateUploadStatus() {
    if (isRebooting || !acquireDisplayLock()) return;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    String progressText = "Expand: " + String(uploadProgress) + "%";
    u8g2.drawStr(0, 12, uploadFilename.c_str());
    u8g2.drawStr(0, 28, progressText.c_str());
    int barWidth = map(uploadProgress, 0, 100, 0, 128);
    u8g2.drawFrame(0, 30, 128, 2);
    u8g2.drawBox(0, 30, barWidth, 2);
    u8g2.sendBuffer();

    releaseDisplayLock();
}

void c_OLED::UpdateUploadStatus(const String& filename, int progress) {
    if (isRebooting || !acquireDisplayLock()) return;

#ifdef STATUS_LED1
    digitalWrite(STATUS_LED1, !digitalRead(STATUS_LED2));
#endif

    uploadFilename = filename;
    uploadProgress = progress;
    isUploading = true;

    releaseDisplayLock();
    UpdateUploadStatus();
}

// === NETWORK / SEQUENCE ===

void c_OLED::UpdateNetworkInfo(bool forceUpdate) {
    if (isRebooting || !acquireDisplayLock()) return;

    String currIP = NetworkMgr.GetlocalIP().toString();
    String currHost;
    NetworkMgr.GetHostname(currHost);
    int signalStrength = getSignalStrength(WiFi.RSSI());

    bool ipNotAvailable = (currIP == "0.0.0.0" || currIP == "");

    if (forceUpdate || currIP != dispIP || currHost != dispHostName || signalStrength != dispRSSI) {
        u8g2.clearBuffer();

        if (ipNotAvailable) {
            u8g2.setFont(u8g2_font_ncenB08_tr);
            int16_t textWidth = u8g2.getUTF8Width("- awaiting connection -");
            int x = (128 - textWidth) / 2;
            u8g2.drawStr(x, 20, "- awaiting connection -");
        } else {
            for (int i = 0; i < signalStrength; i++) {
                u8g2.drawBox(105 + i * 6, 16 - (i + 1) * 4, 4, (i + 1) * 4);
            }

            u8g2.setFont(u8g2_font_ncenB08_tr);
            u8g2.drawStr(0, 18, ("IP: " + currIP).c_str());
            u8g2.drawStr(0, 30, ("HOST: " + currHost).c_str());
        }

        dispIP = currIP;
        dispHostName = currHost;
        dispRSSI = signalStrength;
        u8g2.sendBuffer();
    }

    releaseDisplayLock();
}

void c_OLED::UpdateRunningStatus() {
    if (isRebooting || !acquireDisplayLock()) return;

    static JsonDocument doc;
    doc.clear();
    JsonObject jsonStatus = doc.to<JsonObject>();
    FPPDiscovery.GetStatus(jsonStatus);

    if (jsonStatus.isNull() || !jsonStatus["current_sequence"].is<String>()) {
        releaseDisplayLock();
        return;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, jsonStatus["current_sequence"].as<String>().c_str());
    u8g2.drawStr(0, 20, (jsonStatus["time_elapsed"].as<String>() + " / " + jsonStatus["time_remaining"].as<String>()).c_str());
    u8g2.sendBuffer();

    releaseDisplayLock();
}

// === UPDATE LOOP ===

void c_OLED::Update(bool forceUpdate) {
    if (isRebooting) {
        handleRebootDisplay();
        return;
    }

    if (!acquireDisplayLock()) return;

    if (isToastActive) {
        handleToastDisplay();
        releaseDisplayLock();
        return;
    }

    if (isUploading) {
        releaseDisplayLock();
        UpdateUploadStatus();
        return;
    }

    handleRegularDisplay(forceUpdate);
    releaseDisplayLock();
}

void c_OLED::handleRegularDisplay(bool forceUpdate) {
    unsigned long now = millis();

    if (currentPage == DisplayPage::NETWORK_INFO) {
        if (forceUpdate || now - lastNetworkUpdate >= NETWORK_UPDATE_INTERVAL_MS) {
            releaseDisplayLock();
            UpdateNetworkInfo(forceUpdate);
            lastNetworkUpdate = now;
        }
    } else if (currentPage == DisplayPage::RUNNING_STATUS) {
        releaseDisplayLock();
        UpdateRunningStatus();
    }
}

// === BUTTON ===

void c_OLED::Poll() {
    if (isRebooting) {
        drawreboot();
        ESP.restart();
        return;
    }

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

void c_OLED::Flip() {
    if (isRebooting || !acquireDisplayLock()) return;
    flipState = !flipState;
    u8g2.setDisplayRotation(flipState ? U8G2_R2 : U8G2_R0);
    preferences.putBool("flipState", flipState);
    releaseDisplayLock();
}

void IRAM_ATTR c_OLED::buttonISR() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (oledTaskHandle) {
        xTaskNotifyFromISR(oledTaskHandle, 1, eSetBits, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
    }
}

// === SIGNAL STRENGTH ===

int c_OLED::getSignalStrength(int rssi) {
    if (rssi > -50) return 4;
    if (rssi > -60) return 3;
    if (rssi > -70) return 2;
    if (rssi > -80) return 1;
    return 0;
}
c_OLED OLED;