/*
* UnzipFiles.cpp - Output Management class
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

#include "UnzipFiles.hpp"
#include "FileMgr.hpp"
#include "network/NetworkMgr.hpp"
#include <TimeLib.h>
#ifdef SUPPORT_OLED
#include "service/DisplayOLED.h"
#endif

// Forward declarations
extern c_NetworkMgr NetworkMgr;
extern c_FileMgr FileMgr;

//
// Callback functions needed by the unzipLIB to access a file system
// The library has built-in code for memory-to-memory transfers, but needs
// these callback functions to allow using other storage media
//
void * _OpenZipFile(const char *FileName, int32_t *size)
{
    return gUnzipFiles.OpenZipFile(FileName, size);
}

void _CloseZipFile(void *p)
{
    gUnzipFiles.CloseZipFile(p);
}

int32_t _ReadZipFile(void *p, uint8_t *buffer, int32_t length)
{
    return gUnzipFiles.ReadZipFile(p, buffer, length);
}

int32_t _SeekZipFile(void *p, int32_t position, int iType)
{
    return gUnzipFiles.SeekZipFile(p, position, iType);
} // _SeekZipFile

//-----------------------------------------------------------------------------
UnzipFiles::UnzipFiles ()
{
    // DEBUG_START;

    // use 75% and align to an even number of KB
    BufferSize = uint32_t(float(ESP.getMaxAllocHeap ()) * 0.05) & 0xfffffc00;
    // DEBUG_V(String("BufferSize: ") + String(BufferSize));

    pOutputBuffer = (uint8_t*)malloc(BufferSize);

    // DEBUG_END;

} // UnzipFiles

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
UnzipFiles::~UnzipFiles ()
{
    // DEBUG_START;

    if(pOutputBuffer)
    {
        // DEBUG_V("Release buffer");
        free(pOutputBuffer);
        pOutputBuffer = nullptr;
    }

    // DEBUG_END;

} // ~UnzipFiles

//-----------------------------------------------------------------------------
void UnzipFiles::Run()
{
    // DEBUG_START;
    // Find all compressed files and store them in a queue, but don't decompress yet
    // Decompression will happen in Poll() after network is connected

    pendingArchives.clear();
    currentArchiveIndex = 0;

    std::vector<String> allFiles;
    FileMgr.GetListOfSdFiles(allFiles);
    for (auto &fn : allFiles)
    {
        FeedWDT();
        // Check if file has a compression extension (.zip, .xlz, etc.)
        if (fn.endsWith(".zip") || fn.endsWith(".xlz"))
        {
            pendingArchives.push_back(fn);
        }
    }

    hasPendingZipFile = !pendingArchives.empty();
    if (hasPendingZipFile)
    {
        currentZipFileName = pendingArchives[currentArchiveIndex];
        logcon(String("Found ") + String(pendingArchives.size()) + " compressed file(s); will decompress after network connection");
    }
    
    // DEBUG_END;
} // Run

//-----------------------------------------------------------------------------
void UnzipFiles::Poll()
{
    // DEBUG_START;
    // Check if we have a pending zip file and network is now connected
    // If so, proceed with decompression
    // Also verify that no file upload is in progress
    
    // Handle delayed reboot after decompression completes
    if(isUnzipComplete)
    {
        if(millis() - unzipCompleteTime > 3000)  // Wait 3 seconds to let GUI display status
        {
            logcon(String(CN_stars) + F("Unzip complete. Requesting reboot"));
            String Reason = F("Requesting reboot after unzipping files");
            RequestReboot(Reason, 1, true);
            isUnzipComplete = false;
        }
        return;
    }
    
    if(!hasPendingZipFile || isUnzipping)
    {
        return;
    }
    
    // Check network connection periodically (not every millisecond)
    if(millis() - networkConnectionCheckTime < 1000)
    {
        return;
    }
    networkConnectionCheckTime = millis();
    
    // Before starting, ensure Ethernet has had a chance to attempt/succeed.
    // We prefer to wait for Ethernet connection if supported, but will fall back
    // after a short timeout so we don't block indefinitely when WiFi is already up.
    if (NetworkMgr.HasEthernetSupport())
    {
        // Initialize the Ethernet gating window once
        if (ethGateStartMs == 0)
        {
            ethGateStartMs = millis();
            waitingForEthGate = true;
            // Defer starting unzip until we've given Ethernet time to power up and attempt
            return;
        }

        // If Ethernet is connected, we can proceed (no need to wait out the timeout)
        if (NetworkMgr.IsEthernetConnectedState())
        {
            waitingForEthGate = false;
        }
        else
        {
            // Still waiting for Ethernet to attempt/succeed within the timeout window
            if ((millis() - ethGateStartMs) < ethGateTimeoutMs)
            {
                return;
            }
            // Timeout elapsed → proceed even if Ethernet didn't connect (WiFi may be up)
            waitingForEthGate = false;
        }
    }

    // Require at least one network connection (WiFi or Ethernet) before proceeding
    if(!NetworkMgr.IsConnected())
    {
        return;
    }
    
    // Make sure no file upload is in progress
    if(FileMgr.IsFileUploadInProgress())
    {
        logcon(F("File upload in progress, delaying decompression"));
        return;
    }
    
    // Network is connected and no upload in progress, proceed with decompression of current archive
    isUnzipping = true;
    currentZipFileName = pendingArchives[currentArchiveIndex];

    logcon(String("Network connected. Starting decompression of (") + String(currentArchiveIndex + 1) + " of " + String(pendingArchives.size()) + "): '" + currentZipFileName + "'");

    #ifdef SUPPORT_OLED
    OLED.ShowToast("DECOMPRESSING", String(currentZipFileName));
    #endif

    // Feed watchdog before starting decompression
    FeedWDT();
    ProcessZipFile(currentZipFileName);
    FileMgr.DeleteSdFile(currentZipFileName);
    FeedWDT();

    #ifdef SUPPORT_OLED
    OLED.ShowToast(" COMPLETED ", String(currentZipFileName));
    #endif

    isUnzipping = false;

    // Advance to next archive or finish
    if (currentArchiveIndex + 1 < pendingArchives.size())
    {
        currentArchiveIndex++;
        currentZipFileName = pendingArchives[currentArchiveIndex];
        hasPendingZipFile = true; // still more files to process
        // short yield between files
        FeedWDT();
    }
    else
    {
        hasPendingZipFile = false;
        isUnzipComplete = true;
        unzipCompleteTime = millis();
        logcon(String("All compressed files processed. Will reboot in 3 seconds"));
    }
    
    // DEBUG_END;
} // Poll

//-----------------------------------------------------------------------------
void UnzipFiles::ProcessZipFile(String & ArchiveFileName)
{
    // DEBUG_START;
    char szComment[256], szName[256];
    unz_file_info fi;

    logcon(String("Unzip file: '") + String(ArchiveFileName) + "'");

    int returnCode = zip.openZIP(ArchiveFileName.c_str(), _OpenZipFile, _CloseZipFile, _ReadZipFile, _SeekZipFile);
    if (returnCode == UNZ_OK)
    {
        bool IsSpecialxLightsZipFile = (-1 != ArchiveFileName.indexOf(".xlz"));
        // logcon(String("Opened zip file: '") + FileName + "'");

        // Display the global comment and all of the FileNames within
        // returnCode = zip.getGlobalComment(szComment, sizeof(szComment));
        // logcon(String("Global comment: '") + String(szComment) + "'");
        logcon(String("Unzipping: ") + ArchiveFileName);
        returnCode = zip.gotoFirstFile();
        while (returnCode == UNZ_OK)
        {
            // Process all files contained in the archive
            returnCode = zip.getFileInfo(&fi, szName, sizeof(szName), NULL, 0, szComment, sizeof(szComment));
            if (returnCode == UNZ_OK)
            {
                String ArchiveSubFileName = String(szName);
                String FinalFileName = String(szName);
                if(IsSpecialxLightsZipFile)
                {
                    // DEBUG_V("Modifying File Name");
                    FinalFileName = ArchiveFileName;
                    FinalFileName.replace(".xlz", ".fseq");
                }
                // DEBUG_V(String("ArchiveSubFileName: ") + ArchiveSubFileName);
                // DEBUG_V(String("     FinalFileName: ") + FinalFileName);

                c_FileMgr::SdInfo info;
                FileMgr.GetSdInfo(info);
                // DEBUG_V(String("Uncompressed Size: ") + String(fi.uncompressed_size));
                // DEBUG_V(String("SD unused Size: ") + String(info.Available));
                if(fi.uncompressed_size + 100 > info.Available)
                {
                    logcon (String(CN_stars) + F("Cannot unzip '") + FinalFileName + F("'. Not enough room on the SD card") + CN_stars);
                    break;
                }
                tmElements_t FileDate = {0};
                DosDateToTmuDate (fi.dosDate, &FileDate);
                setTime(makeTime(FileDate));
                FeedWDT();

                FileMgr.DeleteSdFile(FinalFileName);
                
                // Store total size for progress tracking
                unzipTotalSize = fi.uncompressed_size;
                unzipBytesWritten = 0;

                ProcessCurrentFileInZip(fi, ArchiveSubFileName);

                if(IsSpecialxLightsZipFile)
                {
                    // DEBUG_V(String("Rename '") + ArchiveSubFileName + "' to '" + FinalFileName);
                    FileMgr.RenameSdFile(ArchiveSubFileName, FinalFileName);
                    // only a single file is alowed in an xlz zip file.
                    break;
                }
            }
            returnCode = zip.gotoNextFile();
        } // while more files...
        zip.closeZIP();
        // DEBUG_V("No more files in the zip");
    }
    else
    {
        logcon("Could not open zip file.");
    }

    // DEBUG_END;
} // Run

//-----------------------------------------------------------------------------
void UnzipFiles::ProcessCurrentFileInZip(unz_file_info & fi, String & FileName)
{
    // DEBUG_START;
    // DEBUG_V(String("open Filename: ") + FileName);

    int BytesRead = 0;
    uint32_t TotalBytesWritten = 0;

    do // once
    {
        int ReturnCode = zip.openCurrentFile();
        if(ReturnCode != UNZ_OK)
        {
            // DEBUG_V(String("ReturnCode: ") + String(ReturnCode));
            logcon(FileName + F(" Failed."));
            break;
        }

        c_FileMgr::FileId FileHandle;
        FileMgr.OpenSdFile(FileName, c_FileMgr::FileMode::FileWrite, FileHandle, -1);
        if(FileHandle == c_FileMgr::INVALID_FILE_HANDLE)
        {
            zip.closeCurrentFile();
            logcon(String("Could not open '") + FileName + "' for writing");
            break;
        }

        do
        {
            BytesRead = zip.readCurrentFile(pOutputBuffer, BufferSize);
            // DEBUG_V(String("BytesRead: ") + String(BytesRead));
            if(BytesRead != FileMgr.WriteSdFile(FileHandle, pOutputBuffer, BytesRead))
            {
                logcon(String(F("Failed to write data to '")) + FileName + "'");
                break;
            }
            TotalBytesWritten += BytesRead;
            unzipBytesWritten = TotalBytesWritten;
            
            // Feed watchdog every 64KB written
            if(TotalBytesWritten % 65536 == 0)
            {
                FeedWDT();
            }

        } while (BytesRead > 0);

        // DEBUG_FILE_HANDLE (FileHandle);
        FileMgr.CloseSdFile(FileHandle);
        zip.closeCurrentFile();
        logcon(String(FileName) + F(" - ") + String(TotalBytesWritten) + F(" bytes"));
    } while(false);

    // DEBUG_V(String("Close Filename: ") + FileName);
    // DEBUG_END;
} // ProcessCurrentFileInZip

//-----------------------------------------------------------------------------
void * UnzipFiles::OpenZipFile(const char *FileName, int32_t *size)
{
    // DEBUG_START;
    // DEBUG_V(String("  FileName: '") + String(FileName) + "'");

    c_FileMgr::FileId FileHandle = c_FileMgr::INVALID_FILE_HANDLE;
    FileMgr.OpenSdFile(FileName, c_FileMgr::FileMode::FileRead, FileHandle, -1);
    if(FileHandle == c_FileMgr::INVALID_FILE_HANDLE)
    {
        logcon(String("Could not open file for unzipping: '") + FileName + "'");
    }
    else
    {
        *size = int32_t(FileMgr.GetSdFileSize(FileHandle));
    }

    // DEBUG_V(String("FileHandle: ") + String(FileHandle));
    // DEBUG_V(String("      size: ") + String(*size));

    SeekPosition = 0;

    // DEBUG_END;
    return (void *)FileHandle;

} // OpenZipFile

//-----------------------------------------------------------------------------
void UnzipFiles::CloseZipFile(void *p)
{
    // DEBUG_START;

    // DEBUG_V(String("         p: 0x") + String(uint32_t(p), HEX));

    c_FileMgr::FileId FileHandle = (c_FileMgr::FileId)(((ZIPFILE *)p)->fHandle);
    // DEBUG_V(String("FileHandle: ") + String(FileHandle));

    // DEBUG_FILE_HANDLE (FileHandle);
    FileMgr.CloseSdFile(FileHandle);
    SeekPosition = 0;

    // DEBUG_END;
} // CloseZipFile

//-----------------------------------------------------------------------------
int32_t UnzipFiles::ReadZipFile(void *p, uint8_t *buffer, int32_t length)
{
    // DEBUG_START;

    // DEBUG_V(String("         p: 0x") + String(uint32_t(p), HEX));

    c_FileMgr::FileId FileHandle = (c_FileMgr::FileId)(((ZIPFILE *)p)->fHandle);
    // DEBUG_V(String("FileHandle: ") + String(FileHandle));

    // DEBUG_FILE_HANDLE(FileHandle);
    size_t BytesRead = FileMgr.ReadSdFile(FileHandle, buffer, length, SeekPosition);
    // DEBUG_V(String(" BytesRead: ") + String(BytesRead));

    SeekPosition += BytesRead;

    // DEBUG_END;
    return BytesRead;

} // ReadZipFile

//-----------------------------------------------------------------------------
int32_t UnzipFiles::SeekZipFile(void *p, int32_t position, int iType)
{
    // DEBUG_START;
    SeekPosition = position;
    // DEBUG_V(String("         p: 0x") + String(uint32_t(p), HEX));

    // DEBUG_V(String("FileHandle: ") + String((c_FileMgr::FileId)(((ZIPFILE *)p)->fHandle)));
    // DEBUG_V(String("  Position: ") + String(position));
    // DEBUG_V(String("     iType: ") + String(iType));

    // DEBUG_END;
    return SeekPosition; // FileMgr.SeekSdFile(FileHandle, position, SeekMode(iType));
} // SeekZipFile

void UnzipFiles::DosDateToTmuDate(uint32_t DosDate, tmElements_t *ptm)
{
    // Extract DOS DATE (high 16 bits)
    uint16_t date = uint16_t(DosDate >> 16);

    uint16_t day   =  date        & 0x1F;       // 1–31
    uint16_t month = (date >> 5)  & 0x0F;       // 1–12
    uint16_t year  = (date >> 9)  & 0x7F;       // years since 1980

    // Extract DOS TIME (low 16 bits)
    uint16_t time  = uint16_t(DosDate & 0xFFFF);

    uint16_t hour   = (time >> 11) & 0x1F;      // 0–23
    uint16_t minute = (time >> 5)  & 0x3F;      // 0–59
    uint16_t second = (time & 0x1F) * 2;        // stored in 2-sec increments

    // DOS spec allows month 0 for "invalid" → clamp to 1
    if (month < 1) month = 1;
    if (month > 12) month = 12;

    if (day < 1) day = 1;
    if (day > 31) day = 31;

    if (hour > 23) hour = 23;
    if (minute > 59) minute = 59;
    if (second > 59) second = 59;

    // Convert the fields into TimeLib format
    ptm->Day    = (uInt)day;
    ptm->Month  = (uInt)month;                         // TimeLib expects 1–12
    ptm->Year   = (uInt)CalendarYrToTm(1980 + year);   // Convert to tmElements_t year

    ptm->Hour   = (uInt)hour;
    ptm->Minute = (uInt)minute;
    ptm->Second = (uInt)second;
/*
    // DEBUG_V(String("Year: ") + String(tmYearToCalendar(FileDate.Year)));
    // DEBUG_V(String("Month: ") + String(FileDate.Month));
    // DEBUG_V(String("Day: ") + String(FileDate.Day));
    // DEBUG_V(String("Hour: ") + String(FileDate.Hour));
    // DEBUG_V(String("Minute: ") + String(FileDate.Minute));
    // DEBUG_V(String("Second: ") + String(FileDate.Second));
*/
} // DosDateToTmuDate

UnzipFiles gUnzipFiles;

#endif // def SUPPORT_UNZIP
