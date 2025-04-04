#include "service/DisplayOLED.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include "network/NetworkMgr.hpp"
#include "service/FPPDiscovery.h"

// Display
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);
SemaphoreHandle_t displayMutex = nullptr;
TaskHandle_t oledTaskHandle = nullptr;

#define VERSION_STRING             "4.x-dev"
#define TOAST_DURATION_MS          5000
#define NETWORK_UPDATE_INTERVAL_MS 10000
#define BUTTON_DEBOUNCE_DELAY_MS   1000

c_OLED::c_OLED()
    : state(DisplayState::Init),
      lastStateUpdate(0),
      toastStartTime(0),
      isUploading(false),
      uploadProgress(0),
      dispRSSI(0),
      flipState(false)
{}

void c_OLED::Begin() {
    displayMutex = xSemaphoreCreateMutex();
    if (!displayMutex) return;

    preferences.begin("oled", false);
    flipState = preferences.getBool("flipState", false);
    u8g2.setDisplayRotation(flipState ? U8G2_R2 : U8G2_R0);

    if (!u8g2.begin()) return;

    xTaskCreatePinnedToCore(OLEDTask, "OLEDTask", 4096, this, 1, &oledTaskHandle, 1);

#ifdef BUTTON_GPIO1
    pinMode(BUTTON_GPIO1, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_GPIO1), buttonISR, FALLING);
#endif

    InitNow();
}

void c_OLED::InitNow() {
    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    static const unsigned char image_menu_settings_sliders_two_bits[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x1c,0x00,0x22,0x00,0xe3,0x3f,0x22,0x00,0x1c,0x00,0x00,0x0e,0x00,0x11,0xff,0x31,0x00,0x11,0x00,0x0e,0x00,0x00,0x00,0x00,0x00,0x00};
    static const unsigned char image_menu_settings_sliders_two_1_bits[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0e,0x00,0x11,0xff,0x31,0x00,0x11,0x00,0x0e,0x1c,0x00,0x22,0x00,0xe3,0x3f,0x22,0x00,0x1c,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    
    u8g2.clearBuffer();
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setFont(u8g2_font_t0_12b_tr);
    u8g2.drawStr(29, 28, "ESPixelStick");    
    u8g2.drawXBM(3, 16, 14, 16, image_menu_settings_sliders_two_bits);    
    u8g2.setFont(u8g2_font_profont22_tr);
    u8g2.drawStr(10, 17, "INITIALZE");    
    u8g2.drawXBM(111, 16, 14, 16, image_menu_settings_sliders_two_1_bits);    
    u8g2.drawFrame(0, 0, 128, 31);    
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(2, 13, "4.x");    
    u8g2.drawEllipse(22, 24, 2, 2);    
    u8g2.drawStr(115, 13, "DEV");    
    u8g2.drawEllipse(105, 24, 2, 2);    
    u8g2.sendBuffer();    

    xSemaphoreGive(displayMutex);
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

void c_OLED::OLEDTask(void *param) {
    c_OLED *instance = static_cast<c_OLED *>(param);
    while (true) {
        instance->RefreshDisplay();
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void c_OLED::RefreshDisplay() {
    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    switch (state) {
        case DisplayState::Init:
            RenderInit(); break;
        case DisplayState::Upload:
            RenderUpload(); break;
        case DisplayState::Network:
            RenderNetwork(); break;
        case DisplayState::Running:
            RenderRunning(); break;
        case DisplayState::Toast:
            if (millis() - toastStartTime >= TOAST_DURATION_MS) {
                changeState(DisplayState::Network);
            } else {
                RenderToast();
            }
            break;
        case DisplayState::Reboot:
            RenderReboot(); break;
    }

    xSemaphoreGive(displayMutex);
}

void c_OLED::RenderInit() {
    InitNow();
}

void c_OLED::RenderUpload() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 12, uploadFilename.c_str());

    String progress = "Progress: " + String(uploadProgress) + "%";
    u8g2.drawStr(0, 28, progress.c_str());

    int barWidth = map(uploadProgress, 0, 100, 0, 128);
    u8g2.drawFrame(0, 30, 128, 2);
    u8g2.drawBox(0, 30, barWidth, 2);

    u8g2.sendBuffer();
}

void c_OLED::RenderNetwork() {
    String ip = NetworkMgr.GetlocalIP().toString();
    String host;
    NetworkMgr.GetHostname(host);
    int signal = getSignalStrength(WiFi.RSSI());

    if (ip != dispIP || host != dispHostName || signal != dispRSSI) {
        dispIP = ip;
        dispHostName = host;
        dispRSSI = signal;

        u8g2.clearBuffer();
        for (int i = 0; i < signal; ++i) {
            u8g2.drawBox(105 + i * 6, 16 - (i + 1) * 4, 4, (i + 1) * 4);
        }

        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.drawStr(0, 18, ("IP: " + ip).c_str());
        u8g2.drawStr(0, 30, ("HOST: " + host).c_str());
        u8g2.sendBuffer();
    }
}

void c_OLED::RenderRunning() {
    static JsonDocument doc;
    doc.clear();
    JsonObject status = doc.to<JsonObject>();
    FPPDiscovery.GetStatus(status);

    if (!status["current_sequence"].is<String>()) return;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, status["current_sequence"]);
    
    String timeInfo = status["time_elapsed"].as<String>() + " / " + status["time_remaining"].as<String>();
    u8g2.drawStr(0, 20, timeInfo.c_str());
    
    u8g2.sendBuffer();
}


void c_OLED::RenderToast() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(10, 20, toastMessage.c_str());
    u8g2.sendBuffer();
}

void c_OLED::RenderReboot() {
    static const unsigned char image_download_bits[] = {0xe0,0x03,0x18,0x0c,0x94,0x14,0x82,0x20,0x86,0x30,0x81,0x40,0x81,0x40,0x87,0x70,0x01,0x41,0x01,0x42,0x06,0x34,0x02,0x20,0x94,0x14,0x98,0x0c,0xe0,0x03,0x00,0x00};
    u8g2.clearBuffer();
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setFont(u8g2_font_t0_12b_tr);
    u8g2.drawStr(28, 28, "ESPixelStick");    
    u8g2.setFont(u8g2_font_profont22_tr);
    u8g2.drawStr(28, 17, "REBOOT");    
    u8g2.drawFrame(0, 0, 128, 31);    
    u8g2.drawXBM(110, 8, 15, 16, image_download_bits);    
    u8g2.drawXBM(3, 8, 15, 16, image_download_bits);    
    u8g2.sendBuffer();
}

void c_OLED::changeState(DisplayState newState) {
    state = newState;
    lastStateUpdate = millis();
}

void c_OLED::UpdateUpload(const String &filename, int progress) {
    uploadFilename = filename;
    uploadProgress = progress;
    isUploading = true;
    changeState(DisplayState::Upload);
}

void c_OLED::SetUploadComplete() {
    isUploading = false;
    changeState(DisplayState::Network);
}

void c_OLED::ForceNetworkRefresh() {
    changeState(DisplayState::Network);
}

void c_OLED::ShowToast(const String &message) {
    toastMessage = message;
    toastStartTime = millis();
    changeState(DisplayState::Toast);
}

void c_OLED::ShowRebootScreen() {
    changeState(DisplayState::Reboot);
}

void c_OLED::Flip() {
    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    flipState = !flipState;
    u8g2.setDisplayRotation(flipState ? U8G2_R2 : U8G2_R0);
    preferences.putBool("flipState", flipState);

    // Force immediate redraw of current screen (don't re-lock mutex!)
    switch (state) {
        case DisplayState::Init:      RenderInit(); break;
        case DisplayState::Upload:    RenderUpload(); break;
        case DisplayState::Network:   RenderNetwork(); break;
        case DisplayState::Running:   RenderRunning(); break;
        case DisplayState::Toast:     RenderToast(); break;
        case DisplayState::Reboot:    RenderReboot(); break;
    }

    xSemaphoreGive(displayMutex);
}



void c_OLED::Poll() {
    static unsigned long lastDebounce = 0;
    uint32_t notificationValue;

    if (xTaskNotifyWait(0, ULONG_MAX, &notificationValue, 0) == pdTRUE) {
        if (millis() - lastDebounce > BUTTON_DEBOUNCE_DELAY_MS) {
            Flip();
            lastDebounce = millis();
        }
    }

#ifdef BUTTON_GPIO1
    if (digitalRead(BUTTON_GPIO1) == LOW && (millis() - lastDebounce) > BUTTON_DEBOUNCE_DELAY_MS) {
        Flip();
        lastDebounce = millis();
    }
#endif

    if (!isUploading && millis() - lastStateUpdate > NETWORK_UPDATE_INTERVAL_MS) {
        if (FPPDiscovery.PlayingAfile()) {
            changeState(state == DisplayState::Network ? DisplayState::Running : DisplayState::Network);
        } else {
            changeState(DisplayState::Network);
        }
    }
}

int c_OLED::getSignalStrength(int rssi) {
    if (rssi > -50) return 4;
    if (rssi > -60) return 3;
    if (rssi > -70) return 2;
    if (rssi > -80) return 1;
    return 0;
}

void IRAM_ATTR c_OLED::buttonISR() {
    if (oledTaskHandle) {
        BaseType_t high = pdFALSE;
        xTaskNotifyFromISR(oledTaskHandle, 1, eSetBits, &high);
        if (high) portYIELD_FROM_ISR();
    }
}

c_OLED OLED;
