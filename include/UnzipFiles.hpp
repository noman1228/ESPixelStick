#pragma once
/*
* UnzipFiles.hpp
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2025 Shelby Merrick
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
*
*/
#ifdef SUPPORT_UNZIP

#include "ESPixelStick.h"
#include <unzipLIB.h>
#include <TimeLib.h>
#include <vector>

class UnzipFiles
{
public:
    UnzipFiles ();
    virtual ~UnzipFiles ();

    void    Run();
    void    Poll(); // Check if we should start unzipping (after network connection)

    void *  OpenZipFile(const char *filename, int32_t *size);
    void    CloseZipFile(void *p);
    int32_t ReadZipFile(void *p, uint8_t *buffer, int32_t length);
    int32_t SeekZipFile(void *p, int32_t position, int iType);
    void    GetDriverName(String & value) {value = "Unzip";}
    void    ProcessZipFile(String & FileName);
    void    ProcessCurrentFileInZip(unz_file_info & fi, String & Name);
    
    bool    IsUnzipping() const { return isUnzipping; }
    bool    HasPendingZipFile() const { return hasPendingZipFile; }
    bool    IsUnzipComplete() const { return isUnzipComplete; }
    String  GetCurrentZipFileName() const { return currentZipFileName; }
    uint32_t GetUnzipProgress() const { return unzipBytesWritten; }
    uint32_t GetUnzipTotalSize() const { return unzipTotalSize; }
    // Multi-archive queue status
    uint32_t GetPendingCount() const { return uint32_t(pendingArchives.size()); }
    uint32_t GetCurrentArchiveIndex1Based() const { return (pendingArchives.empty() ? 0u : (currentArchiveIndex + 1)); }
    // Ethernet gating helpers for UI/status
    bool     IsWaitingForEthernetGate() const { return waitingForEthGate; }
    uint32_t GetEthernetGateRemainingMs() const {
        if (!waitingForEthGate) return 0;
        uint32_t elapsed = millis() - ethGateStartMs;
        return (elapsed >= ethGateTimeoutMs) ? 0u : (ethGateTimeoutMs - elapsed);
    }

private:
    UNZIP       zip; // statically allocate the UNZIP structure (41K)
    uint8_t     *pOutputBuffer = nullptr;
    uint32_t    BufferSize = 0;
    int32_t     SeekPosition = 0;
    void        DosDateToTmuDate (uint32_t DosDate, tmElements_t * ptm);
    
    bool        isUnzipping = false;
    bool        hasPendingZipFile = false;
    bool        isUnzipComplete = false;
    String      currentZipFileName = emptyString;
    uint32_t    networkConnectionCheckTime = 0;
    uint32_t    unzipCompleteTime = 0;
    uint32_t    unzipBytesWritten = 0;
    uint32_t    unzipTotalSize = 0;
    // Support multiple archives (.zip/.xlz) discovered at boot
    std::vector<String> pendingArchives;
    uint32_t    currentArchiveIndex = 0; // 0-based position in pendingArchives
    // Gate decompression until Ethernet has had a chance to attempt connection
    uint32_t    ethGateStartMs = 0;
    uint32_t    ethGateTimeoutMs = 15000; // wait up to 15s for Ethernet attempt/success before proceeding
    bool        waitingForEthGate = false;

protected:

}; // UnzipFiles

extern UnzipFiles gUnzipFiles;

#endif // def SUPPORT_UNZIP
