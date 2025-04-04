#ifndef DISPLAYOLED_H
#define DISPLAYOLED_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <Preferences.h>

enum class DisplayState {
    Init,
    Upload,
    Network,
    Running,
    Toast,
    Reboot
};

class c_OLED {
public:
    c_OLED();

    void Begin();
    void End();
    void Poll();
    void Flip();
    void ShowToast(const String &message);
    void ShowRebootScreen();
    void UpdateUpload(const String &filename, int progress);
    void SetUploadComplete();
    void ForceNetworkRefresh();

private:
    void InitNow();
    void RefreshDisplay();
    void RenderInit();
    void RenderUpload();
    void RenderNetwork();
    void RenderRunning();
    void RenderToast();
    void RenderReboot();
    void changeState(DisplayState newState);

    static void OLEDTask(void *);
    static void IRAM_ATTR buttonISR();
    static int getSignalStrength(int rssi);

    DisplayState state;
    unsigned long lastStateUpdate;
    unsigned long toastStartTime;

    bool isUploading;
    int uploadProgress;
    String uploadFilename;

    String dispIP;
    String dispHostName;
    int dispRSSI;

    String toastMessage;
    bool flipState;

    Preferences preferences;
};

extern c_OLED OLED;

#endif // DISPLAYOLED_H
