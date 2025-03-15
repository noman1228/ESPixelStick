/*
 * DisplayOLED.h
 *
 * Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
 * Copyright (c) 2018, 2025 Shelby Merrick
 * http://www.forkineye.com
 */

 #ifndef DISPLAY_OLED_H
 #define DISPLAY_OLED_H
 
 #include "ESPixelStick.h"
 #include <U8g2lib.h>
 #include <ArduinoJson.h>
 #include "network/NetworkMgr.hpp"
 #include "ConstNames.hpp"
 #include "FPPDiscovery.h"
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 
 // Enum for OLED display pages
 enum class DisplayPage {
     NETWORK_INFO,
     RUNNING_STATUS,
     UPLOAD_STATUS
 };
 
 class c_OLED {
 public:
     c_OLED();
     void Begin();
     void Poll();
     void Update(bool forceUpdate = false);
     void Flip();
     void GetConfig(JsonObject& json);
     bool SetConfig(JsonObject& json);
     void GetDriverName(String& name) const {
         name = "OLED";
     }
 
     void UpdateRunningStatus();  // Now public for FreeRTOS task
     void status(int progress, const String& filename); // New function for upload status

 private:
     void UpdateErrorFlashing();
     void UpdateNetworkInfo(bool forceUpdate);
     void UpdateUploadStatus(); // New function to handle upload progress page
     int getSignalStrength(int rssi);
     String getUpdateReason(String currIP, String dispIP, String currHost, String dispHostName, int signalStrength, int dispRSSI);
 
     DisplayPage currentPage;
     unsigned long lastPageSwitchTime;
     const unsigned long pageSwitchInterval = 7500;
 
     bool error_global;
     String dispIP;
     String dispHostName;
     int dispRSSI;
     

     uint8_t flipState; // Flip state (0 or 1)
 
     TaskHandle_t oledTaskHandle = NULL; // Task handle for OLED updates
 
     bool beginFileUpload;
     unsigned long lastUploadUpdate;
     String uploadFilename;
     int uploadProgress;
     const unsigned long uploadTimeout = 10000; // 10 seconds timeout
 };
 
 extern c_OLED OLED;
 
 #endif // DISPLAY_OLED_H
 