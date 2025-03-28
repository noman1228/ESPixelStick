/*
 * DisplayOLED.h
 */

 #ifndef DISPLAY_OLED_H
 #define DISPLAY_OLED_H
 
 #define TOAST_DURATION_MS           1000
 #define TOAST_FLASH_INTERVAL_MS     500
 #define REBOOT_FLASH_INTERVAL_MS    400
 #define NETWORK_UPDATE_INTERVAL_MS  10000
 #define BUTTON_DEBOUNCE_DELAY_MS    1000
 #define OLED_TASK_DELAY_MS          250
 #define VERSION_STRING              "4.x-dev"
 
 #include "ESPixelStick.h"
 #include "ConstNames.hpp"
 #include <ArduinoJson.h>
 #include <U8g2lib.h>
 #include <Preferences.h>
 #include "network/NetworkMgr.hpp"
 #include "FPPDiscovery.h"
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "freertos/semphr.h"
 
 enum class DisplayPage {
     NETWORK_INFO,
     RUNNING_STATUS,
     UPLOAD_STATUS
 };
 
 extern U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2;
 
 class c_OLED {
 public:
     c_OLED();
 
     void Begin();
     void InitNow();
     void End();
 
     void Update(bool forceUpdate = false);
     void UpdateNetworkInfo(bool forceUpdate = false);
     void UpdateUploadStatus(const String& filename, int progress);
     void UpdateRunningStatus();
     void status(int progress, const String& filename);
     void Flip();
     void Poll();
     void drawreboot();
     void ShowToast(const String& message);
     void ShowRebootScreen();
 
     void GetDriverName(String& name) const { name = "OLED"; }
 
     static void IRAM_ATTR buttonISR();
 
     // Public state (maybe refactor into accessors later)
     bool flipState;
     bool isUploading;
     String uploadFilename;
     int uploadProgress;
     unsigned long lastUploadUpdate;
 
 private:
     // Toast
     bool isToastActive = false;
     unsigned long toastStartTime = 0;
     unsigned long lastToastFlash = 0;
     bool toastFlashState = false;
     String toastMessage;
 
     // Pages
     DisplayPage currentPage;
     unsigned long lastPageSwitchTime;
     unsigned long lastNetworkUpdate;
     const unsigned long pageSwitchInterval = 7500;
 
     // State
     bool error_global;
     String dispIP;
     String dispHostName;
     int dispRSSI;
     bool isRebooting = false;
     bool rebootFlashState = false;
     unsigned long lastRebootFlash = 0;
 
     Preferences preferences;
 
     // Internal helpers
     void UpdateUploadStatus();
     int getSignalStrength(int rssi);
 
     void handleRebootDisplay();
     void handleToastDisplay();
     void handleRegularDisplay(bool forceUpdate);
     void drawCenteredToast(const String& message);
 };
 
 // Bitmap for reboot icon
 extern c_OLED OLED;
 static const unsigned char image_clock[] = {
     0xe0, 0x03, 0x18, 0x0c, 0x94, 0x14, 0x82, 0x20,
     0x86, 0x30, 0x81, 0x40, 0x81, 0x40, 0x87, 0x70,
     0x01, 0x41, 0x01, 0x42, 0x06, 0x34, 0x02, 0x20,
     0x94, 0x14, 0x98, 0x0c, 0xe0, 0x03, 0x00, 0x00
 };
 
 #endif // DISPLAY_OLED_H
 