

#include "ESPixelStick.h"
#include <Int64String.h>
#include <TimeLib.h>

#include "FileMgr.hpp"
#include "network/NetworkMgr.hpp"
#include "output/OutputMgr.hpp"
#include "UnzipFiles.hpp"
#ifdef SUPPORT_OLED
#include "service/DisplayOLED.h"
#endif
SdFs sd;
const int8_t DISABLE_CS_PIN = -1;

#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(16))
#else
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(16))
#endif

oflag_t XlateFileMode[3] = {O_READ, O_WRITE | O_CREAT, O_WRITE | O_APPEND};
#ifdef SUPPORT_FTP
#include <SimpleFTPServer.h>
FtpServer ftpSrv;

void ftp_callback(FtpOperation ftpOperation, unsigned int freeSpace, unsigned int totalSpace)
{
    FeedWDT();
    switch (ftpOperation)
    {
    case FTP_CONNECT:
    {
        LOG_PORT.println(F("FTP: Connected!"));
        break;
    }

    case FTP_DISCONNECT:
    {
        LOG_PORT.println(F("FTP: Disconnected!"));
        break;
    }

    case FTP_FREE_SPACE_CHANGE:
    {
        FileMgr.BuildFseqList(false);
        LOG_PORT.printf("FTP: Free space change, free %u of %u! Rebuilding FSEQ file list\n", freeSpace, totalSpace);
        break;
    }

    default:
    {
        LOG_PORT.println(F("FTP: unknown callback!"));
        break;
    }
    }
};

void ftp_transferCallback(FtpTransferOperation ftpOperation, const char *name, unsigned int transferredSize)
{
    FeedWDT();
    switch (ftpOperation)
    {
    case FTP_UPLOAD_START:
    {
        LOG_PORT.println(String(F("FTP: Start Uploading '")) + name + "'");
        break;
    }

    case FTP_UPLOAD:
    {

        break;
    }

    case FTP_TRANSFER_STOP:
    {
        LOG_PORT.println(String(F("FTP: Done Uploading '")) + name + "'");
        break;
    }

    case FTP_TRANSFER_ERROR:
    {
        LOG_PORT.println(String(F("FTP: Error Uploading '")) + name + "'");
        break;
    }

    default:
    {
        LOG_PORT.println(F("FTP: Unknown Transfer Callback!"));
        break;
    }
    }
};
#endif

c_FileMgr::c_FileMgr()
{
#ifdef ARDUINO_ARCH_ESP32
    SdAccessSemaphore = xSemaphoreCreateBinary();
    UnLockSd();
#endif // def ARDUINO_ARCH_ESP32
} // c_FileMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
c_FileMgr::~c_FileMgr ()
{
}

void c_FileMgr::Begin()
{

    do
    {
        InitSdFileList();

        if (!LittleFS.begin())
        {
            logcon(String(CN_stars) + F(" Flash file system did not initialize correctly ") + CN_stars);
        }
        else
        {
#ifdef ARDUINO_ARCH_ESP32
            logcon(String(F("Flash file system initialized. Used = ")) + String(LittleFS.usedBytes()) + String(F(" out of ")) + String(LittleFS.totalBytes()));
#else
            logcon(String(F("Flash file system initialized.")));
#endif

            listDir(LittleFS, String("/"), 3);
        }

        SetSpiIoPins();

#ifdef SUPPORT_UNZIP
        if (FoundZipFile)
        {
            FeedWDT();
            UnzipFiles *Unzipper = new (UnzipFiles);
            Unzipper->Run();
            delete Unzipper;
            logcon("Requesting reboot after unzipping files");
            RequestReboot(1, true);
        }
#endif

    } while (false);
}

void c_FileMgr::NetworkStateChanged(bool NewState)
{

#ifdef SUPPORT_FTP
    ftpSrv.end();

    if (NewState && SdCardInstalled && FtpEnabled)
    {

        logcon("Starting FTP server.");
        ftpSrv.begin(FtpUserName.c_str(), FtpPassword.c_str(), WelcomeString.c_str());
        ftpSrv.setCallback(ftp_callback);
        ftpSrv.setTransferCallback(ftp_transferCallback);
    }
#endif
}

void c_FileMgr::Poll()
{

#ifdef SUPPORT_FTP
    if (FtpEnabled)
    {
        FeedWDT();
        ftpSrv.handleFTP();
    }
#endif
}

bool c_FileMgr::SetConfig(JsonObject &json)
{

    bool ConfigChanged = false;
    JsonObject JsonDeviceConfig = json[(char *)CN_device].as<JsonObject>();
    if (JsonDeviceConfig)
    {

        ConfigChanged |= setFromJSON(miso_pin, JsonDeviceConfig, CN_miso_pin);
        ConfigChanged |= setFromJSON(mosi_pin, JsonDeviceConfig, CN_mosi_pin);
        ConfigChanged |= setFromJSON(clk_pin, JsonDeviceConfig, CN_clock_pin);
        ConfigChanged |= setFromJSON(cs_pin, JsonDeviceConfig, CN_cs_pin);
        ConfigChanged |= setFromJSON(MaxSdSpeed, JsonDeviceConfig, CN_sdspeed);

        ConfigChanged |= setFromJSON(FtpUserName, JsonDeviceConfig, CN_user);
        ConfigChanged |= setFromJSON(FtpPassword, JsonDeviceConfig, CN_password);
        ConfigChanged |= setFromJSON(FtpEnabled, JsonDeviceConfig, CN_enabled);
    }
    else
    {
        logcon(F("No File Manager settings found."));
    }

    if (ConfigChanged)
    {
        SetSpiIoPins();
        NetworkStateChanged(NetworkMgr.IsConnected());
    }

    return ConfigChanged;
}

void c_FileMgr::GetConfig(JsonObject &json)
{

    JsonWrite(json, CN_miso_pin, miso_pin);
    JsonWrite(json, CN_mosi_pin, mosi_pin);
    JsonWrite(json, CN_clock_pin, clk_pin);
    JsonWrite(json, CN_cs_pin, cs_pin);
    JsonWrite(json, CN_sdspeed, MaxSdSpeed);

    JsonWrite(json, CN_user, FtpUserName);
    JsonWrite(json, CN_password, FtpPassword);
    JsonWrite(json, CN_enabled, FtpEnabled);
}

void c_FileMgr::GetStatus(JsonObject &json)
{

#ifdef ARDUINO_ARCH_ESP32
    json[F("size")] = LittleFS.totalBytes();
    json[F("used")] = LittleFS.usedBytes();
#endif
}

void dateTime(uint16_t *date, uint16_t *time, uint8_t *ms10)
{

    tmElements_t tm;
    breakTime(now(), tm);

    *date = FS_DATE(tm.Year + 1970, tm.Month, tm.Day);

    *time = FS_TIME(tm.Hour, tm.Minute, tm.Second);

    *ms10 = 0;
}

void c_FileMgr::SetSpiIoPins()
{
    // DEBUG_START;

#ifdef ARDUINO_ARCH_ESP8266
    ESP.wdtDisable();
#endif // def ARDUINO_ARCH_ESP8266

#if defined (SUPPORT_SD) || defined(SUPPORT_SD_MMC)
    if (SdCardInstalled)
    {

        ESP_SD.end();
    }

    FsDateTime::setCallback(dateTime);
#ifdef ARDUINO_ARCH_ESP32

    try
#endif
    {
#ifdef SUPPORT_SD_MMC

        pinMode(SD_CARD_DATA_0, PULLUP);
        pinMode(SD_CARD_DATA_1, PULLUP);
        pinMode(SD_CARD_DATA_2, PULLUP);
        pinMode(SD_CARD_DATA_3, PULLUP);

        if (!ESP_SD.begin())
#else
#ifdef ARDUINO_ARCH_ESP32

        SPI.end();

        ResetGpio(gpio_num_t(cs_pin));
        pinMode(cs_pin, OUTPUT);
#ifdef USE_MISO_PULLUP

        ResetGpio(gpio_num_t(mosi_pin));
        pinMode(miso_pin, INPUT_PULLUP);
#else

        ResetGpio(gpio_num_t(miso_pin));
        pinMode(miso_pin, INPUT);
#endif
        ResetGpio(gpio_num_t(clk_pin));
        ResetGpio(gpio_num_t(miso_pin));
        ResetGpio(gpio_num_t(cs_pin));
        SPI.begin(clk_pin, miso_pin, mosi_pin, cs_pin);
#else
        SPI.end();
        SPI.begin();
#endif

        if (!ESP_SD.begin(SdSpiConfig(cs_pin, SHARED_SPI, SD_SCK_MHZ(16))))
#endif
        {

            logcon(String(F("No SD card installed")));
            SdCardInstalled = false;

            if (nullptr == ESP_SD.card())
            {
                logcon(F("SD 'Card' setup failed."));
            }
            else if (ESP_SD.card()->errorCode())
            {
                logcon(String(F("SD initialization failed - code: ")) + String(ESP_SD.card()->errorCode()));

                printSdErrorText(&Serial, ESP_SD.card()->errorCode());
                LOG_PORT.println("");
            }
            else if (ESP_SD.vol()->fatType() == 0)
            {
                logcon(F("SD Can't find a valid FAT16/FAT32 partition."));
            }
            else
            {
                logcon(F("SD Can't determine error type"));
            }
        }
        else
        {

            SdCardInstalled = true;

            SetSdSpeed();

            DescribeSdCardToUser();
        }

        BuildFseqList(true);
    }
#ifdef ARDUINO_ARCH_ESP32
    catch (const std::exception &e)
    {
        logcon(String(F("ERROR: Could not init the SD Card: ")) + e.what());
        SdCardInstalled = false;
    }
#endif

#else
    SdCardInstalled = false;
#endif // defined (SUPPORT_SD) || defined(SUPPORT_SD_MMC)

#ifdef ARDUINO_ARCH_ESP8266
    ESP.wdtEnable(uint32_t(0));
#endif // def ARDUINO_ARCH_ESP8266
    // DEBUG_END;

} // SetSpiIoPins

//-----------------------------------------------------------------------------
void c_FileMgr::SetSdSpeed ()
{

#if defined(SUPPORT_SD) || defined(SUPPORT_SD_MMC)
    if (SdCardInstalled)
    {

        csd_t csd;
        uint8_t tran_speed = 0;

        CSD MyCsd;

        ESP_SD.card()->readCSD(&csd);
        memcpy(&MyCsd, &csd.csd[0], sizeof(csd.csd));

        tran_speed = MyCsd.Decode_0.tran_speed;
        uint32_t FinalTranSpeedMHz = MaxSdSpeed;

        switch (tran_speed)
        {
        case 0x32:
        {
            FinalTranSpeedMHz = 25;
            break;
        }
        case 0x5A:
        {
            FinalTranSpeedMHz = 50;
            break;
        }
        case 0x0B:
        {
            FinalTranSpeedMHz = 100;
            break;
        }
        case 0x2B:
        {
            FinalTranSpeedMHz = 200;
            break;
        }
        default:
        {
            FinalTranSpeedMHz = 25;
        }
        }

        FinalTranSpeedMHz = min(FinalTranSpeedMHz, MaxSdSpeed);
        SPI.setFrequency(FinalTranSpeedMHz * 1024 * 1024);
        logcon(String("Set SD speed to ") + String(FinalTranSpeedMHz) + "Mhz");

        SdCardSizeMB = 0.000512 * csd.capacity();
    }
#endif
}

void c_FileMgr::ResetSdCard()
{

    byte ResetValue = 0x00;
    digitalWrite(cs_pin, LOW);
    SPI.beginTransaction(SPISettings());

    uint32_t retry = 2000;
    while ((0xff != ResetValue) && (--retry))
    {
        delay(1);
        ResetValue = SPI.transfer(0xff);
    }
    SPI.endTransaction();
    digitalWrite(cs_pin, HIGH);
}

void c_FileMgr::DeleteFlashFile(const String &FileName)
{

    LittleFS.remove(FileName);
    if (!FileName.equals(FSEQFILELIST))
    {
        BuildFseqList(false);
    }
}

void c_FileMgr::listDir(fs::FS &fs, String dirname, uint8_t levels)
{

    do
    {
        logcon(String(F("Listing directory: ")) + dirname);

        File root = fs.open(dirname, CN_r);
        if (!root)
        {
            logcon(String(CN_stars) + F("failed to open directory: ") + dirname + CN_stars);
            break;
        }

        if (!root.isDirectory())
        {
            logcon(String(F("Is not a directory: ")) + dirname);
            break;
        }

        File MyFile = root.openNextFile();

        while (MyFile)
        {
            if (MyFile.isDirectory())
            {
                if (levels)
                {
                    listDir(fs, dirname + "/" + MyFile.name(), levels - 1);
                }
            }
            else
            {
                logcon("'" + String(MyFile.name()) + "': \t'" + String(MyFile.size()) + "'");
            }
            MyFile = root.openNextFile();
        }

    } while (false);
}

bool c_FileMgr::LoadFlashFile(const String &FileName, DeserializationHandler Handler)
{

    bool retval = false;

    do
    {
        String CfgFileMessagePrefix = String(CN_Configuration_File_colon) + "'" + FileName + "' ";

        fs::File file = LittleFS.open(FileName.c_str(), "r");

        if (!file)
        {
            if (!IsBooting)
            {
                logcon(String(CN_stars) + CfgFileMessagePrefix + String(F(" Could not open file for reading ")) + CN_stars);
            }
            break;
        }

        JsonDocument jsonDoc;

        DeserializationError error = deserializeJson(jsonDoc, file);
        file.close();

        if (error)
        {

            logcon(String(CN_stars) + CfgFileMessagePrefix + String(F("Deserialzation Error. Error code = ")) + error.c_str() + CN_stars);

            break;
        }

        logcon(CfgFileMessagePrefix + String(F("loaded.")));

        Handler(jsonDoc);

        retval = true;

    } while (false);

    return retval;
}

bool c_FileMgr::SaveFlashFile(const String &FileName, String &FileData)
{

    bool Response = SaveFlashFile(FileName, FileData.c_str());

    return Response;
}

bool c_FileMgr::SaveFlashFile(const String &FileName, const char *FileData)
{

    bool Response = false;
    String CfgFileMessagePrefix = String(CN_Configuration_File_colon) + "'" + FileName + "' ";

    fs::File file = LittleFS.open(FileName.c_str(), "w");
    if (!file)
    {
        logcon(String(CN_stars) + CfgFileMessagePrefix + String(F("Could not open file for writing..")) + CN_stars);
    }
    else
    {
        file.seek(0, SeekSet);
        file.print(FileData);
        file.close();

        file = LittleFS.open(FileName.c_str(), "r");
        logcon(CfgFileMessagePrefix + String(F("saved ")) + String(file.size()) + F(" bytes."));
        file.close();

        Response = true;
    }

    return Response;
}

bool c_FileMgr::SaveFlashFile(const String &FileName, JsonDocument &FileData)
{

    bool Response = false;

    String CfgFileMessagePrefix = String(CN_Configuration_File_colon) + "'" + FileName + "' ";

    fs::File file = LittleFS.open(FileName.c_str(), "w");

    if (!file)
    {
        logcon(String(CN_stars) + CfgFileMessagePrefix + String(F("Could not open file for writing..")) + CN_stars);
    }
    else
    {

        file.seek(0, SeekSet);

        size_t NumBytesSaved = serializeJson(FileData, file);

        file.close();

        logcon(CfgFileMessagePrefix + String(F("saved ")) + String(NumBytesSaved) + F(" bytes."));

        Response = true;
    }

    return Response;
}

bool c_FileMgr::SaveFlashFile(const String &FileName, uint32_t index, uint8_t *data, uint32_t len, bool final)
{

    bool Response = false;

    String CfgFileMessagePrefix = String(CN_Configuration_File_colon) + "'" + FileName + "' ";

    fs::File file;

    if (0 == index)
    {
        file = LittleFS.open(FileName.c_str(), "w");
    }
    else
    {
        file = LittleFS.open(FileName.c_str(), "a");
    }

    if (!file)
    {
        logcon(String(CN_stars) + CfgFileMessagePrefix + String(F("Could not open file for writing..")) + CN_stars);
    }
    else
    {

        file.seek(int(index), SeekMode::SeekSet);

        size_t NumBytesSaved = file.write(data, size_t(len));

        file.close();

        logcon(CfgFileMessagePrefix + String(F("saved ")) + String(NumBytesSaved) + F(" bytes."));

        Response = true;
    }

    return Response;
}

bool c_FileMgr::ReadFlashFile(const String &FileName, String &FileData)
{

    bool GotFileData = false;
    String CfgFileMessagePrefix = String(CN_Configuration_File_colon) + "'" + FileName + "' ";

    fs::File file = LittleFS.open(FileName.c_str(), CN_r);
    if (file)
    {

        file.seek(0, SeekSet);
        FileData = file.readString();
        file.close();
        GotFileData = true;
    }
    else
    {
        logcon(String(CN_stars) + CN_Configuration_File_colon + "'" + FileName + F("' not found.") + CN_stars);
    }

    return GotFileData;
}

bool c_FileMgr::ReadFlashFile(const String &FileName, JsonDocument &FileData)
{

    bool GotFileData = false;

    do
    {
        String RawFileData;
        if (false == ReadFlashFile(FileName, RawFileData))
        {

            break;
        }

        if (0 == RawFileData.length())
        {

            GotFileData = false;
            break;
        }

        DeserializationError error = deserializeJson(FileData, (const String)RawFileData);

        if (error)
        {
            String CfgFileMessagePrefix = String(CN_Configuration_File_colon) + "'" + FileName + "' ";
            logcon(CN_Heap_colon + String(ESP.getFreeHeap()));
            logcon(CfgFileMessagePrefix + String(F("Deserialzation Error. Error code = ")) + error.c_str());
            logcon(CN_plussigns + RawFileData + CN_minussigns);
            break;
        }

        GotFileData = true;

    } while (false);

    return GotFileData;
}

bool c_FileMgr::ReadFlashFile(const String &FileName, byte *FileData, size_t maxlen)
{

    bool GotFileData = false;

    do
    {

        fs::File file = LittleFS.open(FileName.c_str(), CN_r);
        if (!file)
        {
            logcon(String(CN_stars) + CN_Configuration_File_colon + "'" + FileName + F("' not found.") + CN_stars);
            break;
        }

        if (file.size() >= maxlen)
        {
            logcon(String(CN_stars) + CN_Configuration_File_colon + "'" + FileName + F("' too large for buffer. ") + CN_stars);
            file.close();
            break;
        }

        file.seek(0, SeekSet);
        file.read(FileData, file.size());
        file.close();

        GotFileData = true;

    } while (false);

    return GotFileData;
}

bool c_FileMgr::FlashFileExists(const String &FileName)
{
    return LittleFS.exists(FileName.c_str());
}

void c_FileMgr::InitSdFileList()
{

    int index = 0;
    for (auto &currentFileListEntry : FileList)
    {
        currentFileListEntry.handle = INVALID_FILE_HANDLE;
        currentFileListEntry.entryId = index++;
    }
}

int c_FileMgr::FileListFindSdFileHandle(FileId HandleToFind)
{

    int response = -1;

    if (INVALID_FILE_HANDLE != HandleToFind)
    {
        for (auto &currentFileListEntry : FileList)
        {

            if (currentFileListEntry.handle == HandleToFind)
            {
                response = currentFileListEntry.entryId;
                break;
            }
        }
    }

    return response;
}

c_FileMgr::FileId c_FileMgr::CreateSdFileHandle()
{

    FileId response = INVALID_FILE_HANDLE;
    FileId FileHandle = millis();

    while (-1 != FileListFindSdFileHandle(FileHandle))
    {
        ++FileHandle;
    }

    for (auto &currentFileListEntry : FileList)
    {
        if (currentFileListEntry.handle == INVALID_FILE_HANDLE)
        {
            currentFileListEntry.handle = FileHandle;
            response = FileHandle;

            break;
        }
    }

    if (INVALID_FILE_HANDLE == response)
    {
        logcon(String(CN_stars) + F(" Could not allocate another file handle ") + CN_stars);
    }

    return response;
}

void c_FileMgr::DeleteSdFile(const String &FileName)
{

    LockSd();
    bool FileExists = ESP_SD.exists(FileName);
    UnLockSd();
    if (FileExists)
    {

        LockSd();
        ESP_SD.remove(FileName);
        UnLockSd();
    }
    BuildFseqList(false);
}

void c_FileMgr::DescribeSdCardToUser()
{

    logcon(String(F("SD Card Size: ")) + int64String(SdCardSizeMB) + "MB");
}

void c_FileMgr::GetListOfSdFiles(std::vector<String> &Response)
{

    char entryName[256];
    FsFile Entry;

    Response.clear();
    LockSd();
    do
    {
        if (false == SdCardIsInstalled())
        {

            break;
        }

        FsFile dir;
        ESP_SD.chdir();
        if (!dir.open("/", O_READ))
        {
            logcon("ERROR:GetListOfSdFiles: Could not open root dir");
            break;
        }

        while (Entry.openNext(&dir, O_READ))
        {
            if (!Entry.isFile() || Entry.isHidden())
            {

                continue;
            }

            memset(entryName, 0x0, sizeof(entryName));
            Entry.getName(entryName, sizeof(entryName) - 1);
            String EntryName = String(entryName);
            EntryName = EntryName.substring((('/' == EntryName[0]) ? 1 : 0));

            if ((!EntryName.isEmpty()) &&
                (!EntryName.equals(String(F("System Volume Information")))) &&
                (0 != Entry.size()))
            {

                Response.push_back(EntryName);
            }

            Entry.close();
        }

        dir.close();
    } while (false);
    UnLockSd();
}

void c_FileMgr::printDirectory(FsFile &dir, int numTabs)
{

    char entryName[256];
    FsFile Entry;
    String tabs = emptyString;
    tabs.reserve(2 * (numTabs + 1));
    for (uint8_t i = 0; i < numTabs; i++)
    {
        tabs = tabs + "  ";
    }

    while (Entry.openNext(&dir, O_READ))
    {
        FeedWDT();

        memset(entryName, 0x0, sizeof(entryName));
        Entry.getName(entryName, sizeof(entryName) - 1);

        if (Entry.isDirectory())
        {
            logcon(tabs + F("> ") + entryName);
            printDirectory(dir, numTabs + 1);
            Entry.close();
            continue;
        }

        if (!Entry.isHidden())
        {

            logcon(tabs + entryName + F(" - ") + int64String(Entry.size()));
        }
        Entry.close();
    }
}

void c_FileMgr::SaveSdFile(const String &FileName, String &FileData)
{

    do
    {
        FileId FileHandle = INVALID_FILE_HANDLE;
        if (false == OpenSdFile(FileName, FileMode::FileWrite, FileHandle, -1))
        {
            logcon(String(F("Could not open '")) + FileName + F("' for writting."));
            break;
        }

        int WriteCount = WriteSdFile(FileHandle, (byte *)FileData.c_str(), (uint64_t)FileData.length());
        logcon(String(F("Wrote '")) + FileName + F("' ") + String(WriteCount));

        DEBUG_FILE_HANDLE(FileHandle);
        CloseSdFile(FileHandle);

    } while (false);
}

void c_FileMgr::SaveSdFile(const String &FileName, JsonVariant &FileData)
{

    String Temp;
    serializeJson(FileData, Temp);
    SaveSdFile(FileName, Temp);
}

bool c_FileMgr::OpenSdFile(const String &FileName, FileMode Mode, FileId &FileHandle, int FileListIndex)
{

    bool FileIsOpen = false;

    do
    {
        if (!SdCardInstalled)
        {

            FileHandle = INVALID_FILE_HANDLE;
            break;
        }

        if (FileMode::FileRead == Mode)
        {

            if (false == ESP_SD.exists(FileName))
            {
                logcon(String(F("ERROR: Cannot find '")) + FileName + F("' for reading. File does not exist."));
                break;
            }
        }

        if (-1 == FileListIndex)
        {

            FileHandle = CreateSdFileHandle();

            FileListIndex = FileListFindSdFileHandle(FileHandle);
        }
        else
        {

            FileHandle = FileList[FileListIndex].handle;
        }

        if (-1 != FileListIndex)
        {

            FileList[FileListIndex].Filename = FileName;

            LockSd();
            FileList[FileListIndex].IsOpen = FileList[FileListIndex].fsFile.open(FileList[FileListIndex].Filename.c_str(), XlateFileMode[Mode]);
            UnLockSd();

            FileList[FileListIndex].mode = Mode;

            if (!FileList[FileListIndex].IsOpen)
            {
                logcon(String(F("ERROR: Could not open '")) + FileName + F("'."));

                DEBUG_FILE_HANDLE(FileHandle);
                CloseSdFile(FileHandle);
                break;
            }

            LockSd();
            FileList[FileListIndex].size = FileList[FileListIndex].fsFile.size();
            UnLockSd();

            if (FileMode::FileWrite == Mode)
            {

                LockSd();
                FileList[FileListIndex].fsFile.seek(0);
                UnLockSd();
            }

            FileIsOpen = true;
        }
        else
        {
        }

    } while (false);

    return FileIsOpen;
}

bool c_FileMgr::ReadSdFile(const String &FileName, String &FileData)
{

    bool GotFileData = false;
    FileId FileHandle = INVALID_FILE_HANDLE;

    if (true == OpenSdFile(FileName, FileMode::FileRead, FileHandle, -1))
    {

        int FileListIndex;
        if (-1 != (FileListIndex = FileListFindSdFileHandle(FileHandle)))
        {
            LockSd();
            FileList[FileListIndex].fsFile.seek(0);
            FileData = FileList[FileListIndex].fsFile.readString();
            UnLockSd();
        }

        DEBUG_FILE_HANDLE(FileHandle);
        CloseSdFile(FileHandle);
        GotFileData = (0 != FileData.length());
    }
    else
    {
        DEBUG_FILE_HANDLE(FileHandle);
        CloseSdFile(FileHandle);
        logcon(String(F("SD file: '")) + FileName + String(F("' not found.")));
    }

    return GotFileData;
}

bool c_FileMgr::ReadSdFile(const String &FileName, JsonDocument &FileData)
{

    bool GotFileData = false;
    FileId FileHandle = INVALID_FILE_HANDLE;

    if (true == OpenSdFile(FileName, FileMode::FileRead, FileHandle, -1))
    {

        int FileListIndex;
        if (-1 != (FileListIndex = FileListFindSdFileHandle(FileHandle)))
        {
            LockSd();
            FileList[FileListIndex].fsFile.seek(0);
            String RawFileData = FileList[FileListIndex].fsFile.readString();
            UnLockSd();

            DeserializationError error = deserializeJson(FileData, RawFileData);

            if (error)
            {
                String CfgFileMessagePrefix = String(CN_Configuration_File_colon) + "'" + FileName + "' ";
                logcon(CN_Heap_colon + String(ESP.getFreeHeap()));
                logcon(CfgFileMessagePrefix + String(F("Deserialzation Error. Error code = ")) + error.c_str());
                logcon(CN_plussigns + RawFileData + CN_minussigns);
            }
            else
            {
                GotFileData = true;
            }
        }
        DEBUG_FILE_HANDLE(FileHandle);
        CloseSdFile(FileHandle);
    }
    else
    {
        logcon(String(F("SD file: '")) + FileName + String(F("' not found.")));
    }

    return GotFileData;
}

uint64_t c_FileMgr::ReadSdFile(const FileId &FileHandle, byte *FileData, uint64_t NumBytesToRead, uint64_t StartingPosition)
{

    uint64_t response = 0;

    int FileListIndex;
    if (-1 != (FileListIndex = FileListFindSdFileHandle(FileHandle)))
    {
        uint64_t BytesRemaining = uint64_t(FileList[FileListIndex].size - StartingPosition);
        uint64_t ActualBytesToRead = min(NumBytesToRead, BytesRemaining);

        LockSd();
        FileList[FileListIndex].fsFile.seek(StartingPosition);
        response = FileList[FileListIndex].fsFile.readBytes(FileData, ActualBytesToRead);
        UnLockSd();
    }
    else
    {
        logcon(String(F("ReadSdFile::ERROR::Invalid File Handle: ")) + String(FileHandle));
    }

    return response;
}

void c_FileMgr::CloseSdFile(FileId &FileHandle)
{

    int FileListIndex;
    if (-1 != (FileListIndex = FileListFindSdFileHandle(FileHandle)))
    {

        LockSd();
        FileList[FileListIndex].fsFile.close();
        UnLockSd();

        FileList[FileListIndex].IsOpen = false;
        FileList[FileListIndex].handle = INVALID_FILE_HANDLE;

        if (nullptr != FileList[FileListIndex].buffer.DataBuffer)
        {
            if (FileList[FileListIndex].buffer.DataBuffer != OutputMgr.GetBufferAddress())
            {

                free(FileList[FileListIndex].buffer.DataBuffer);
            }
        }
        FileList[FileListIndex].buffer.DataBuffer = nullptr;
        FileList[FileListIndex].buffer.size = 0;
        FileList[FileListIndex].buffer.offset = 0;
    }
    else
    {
        logcon(String(F("CloseSdFile::ERROR::Invalid File Handle: ")) + String(FileHandle));
    }

    FileHandle = INVALID_FILE_HANDLE;
}

uint64_t c_FileMgr::WriteSdFile(const FileId &FileHandle, byte *FileData, uint64_t NumBytesToWrite)
{

    uint64_t NumBytesWritten = 0;
    do
    {
        int FileListIndex;

        if (-1 == (FileListIndex = FileListFindSdFileHandle(FileHandle)))
        {

            logcon(String(F("WriteSdFile::ERROR::Invalid File Handle: ")) + String(FileHandle));
            break;
        }

        delay(10);
        FeedWDT();
        LockSd();
        NumBytesWritten = FileList[FileListIndex].fsFile.write(FileData, NumBytesToWrite);
        FileList[FileListIndex].fsFile.flush();
        UnLockSd();
        FeedWDT();
        delay(10);

        if (NumBytesWritten != NumBytesToWrite)
        {
            logcon(String(F("ERROR: SD Write failed. Tried writting ")) + String(NumBytesToWrite) + F(" bytes. Actually wrote ") + String(NumBytesWritten) + F(" bytes."));
            NumBytesWritten = 0;
            break;
        }
    } while (false);

    return NumBytesWritten;
}

uint64_t c_FileMgr::WriteSdFileBuf(const FileId &FileHandle, byte *FileData, uint64_t NumBytesInSourceBuffer)
{

    uint64_t NumBytesWrittenToDestBuffer = 0;
    bool ForceWriteToSD = (0 == NumBytesInSourceBuffer);
    do
    {
        int FileListIndex;

        if (-1 == (FileListIndex = FileListFindSdFileHandle(FileHandle)))
        {
            logcon(String(F("WriteSdFileBuf::ERROR::Invalid File Handle: ")) + String(FileHandle));
            break;
        }

        if (nullptr == FileList[FileListIndex].buffer.DataBuffer)
        {
            delay(10);
            FeedWDT();

            LockSd();
            NumBytesWrittenToDestBuffer = FileList[FileListIndex].fsFile.write(FileData, NumBytesInSourceBuffer);
            FileList[FileListIndex].fsFile.flush();
            UnLockSd();

            delay(20);
            FeedWDT();
        }
        else
        {

            uint64_t SpaceRemaining = FileList[FileListIndex].buffer.size - FileList[FileListIndex].buffer.offset;

            if (ForceWriteToSD ||
                (NumBytesInSourceBuffer &&
                 (SpaceRemaining < NumBytesInSourceBuffer)))
            {

                FeedWDT();
                LockSd();
                uint64_t WroteToSdSize = FileList[FileListIndex].fsFile.write(FileList[FileListIndex].buffer.DataBuffer, FileList[FileListIndex].buffer.offset);
                FileList[FileListIndex].fsFile.flush();
                UnLockSd();
                delay(30);
                FeedWDT();

                if (FileList[FileListIndex].buffer.offset != WroteToSdSize)
                {
                    logcon(String("WriteSdFileBuf:ERROR:SD Write Failed. Tried to write: ") +
                           String(FileList[FileListIndex].buffer.offset) +
                           " bytes. Actually wrote: " + String(WroteToSdSize))
                        NumBytesWrittenToDestBuffer = 0;
                    break;
                }

                FileList[FileListIndex].buffer.offset = 0;

                ForceWriteToSD = false;
            }

            if (NumBytesInSourceBuffer)
            {

                memcpy(&(FileList[FileListIndex].buffer.DataBuffer[FileList[FileListIndex].buffer.offset]), FileData, NumBytesInSourceBuffer);

                FileList[FileListIndex].buffer.offset += NumBytesInSourceBuffer;
                NumBytesWrittenToDestBuffer += NumBytesInSourceBuffer;
                NumBytesInSourceBuffer -= NumBytesInSourceBuffer;
            };
        }

    } while (false);

    return NumBytesWrittenToDestBuffer;
}

uint64_t c_FileMgr::WriteSdFile(const FileId &FileHandle, byte *FileData, uint64_t NumBytesToWrite, uint64_t StartingPosition)
{
    uint64_t response = 0;
    int FileListIndex;
    if (-1 != (FileListIndex = FileListFindSdFileHandle(FileHandle)))
    {

        LockSd();
        FileList[FileListIndex].fsFile.seek (StartingPosition);
        UnLockSd();
        response = WriteSdFile (FileHandle, FileData, NumBytesToWrite, true);
    }
    else
    {
        logcon(String(F("WriteSdFile::ERROR::Invalid File Handle: ")) + String(FileHandle));
    }

    return response;
}

uint64_t c_FileMgr::GetSdFileSize(const String &FileName)
{

    uint64_t response = 0;
    FileId Handle = INVALID_FILE_HANDLE;
    if (OpenSdFile(FileName, FileMode::FileRead, Handle, -1))
    {
        response = GetSdFileSize(Handle);
        DEBUG_FILE_HANDLE(Handle);
        CloseSdFile(Handle);
    }
    else
    {
        logcon(String(F("Could not open '")) + FileName + F("' to check size."));
    }

    return response;
}

uint64_t c_FileMgr::GetSdFileSize(const FileId &FileHandle)
{

    uint64_t response = 0;
    int FileListIndex;
    if (-1 != (FileListIndex = FileListFindSdFileHandle(FileHandle)))
    {
        LockSd();
        response = FileList[FileListIndex].fsFile.size();
        UnLockSd();
    }
    else
    {
        logcon(String(F("GetSdFileSize::ERROR::Invalid File Handle: ")) + String(FileHandle));
    }

    return response;
}

void c_FileMgr::RenameSdFile(String &OldName, String &NewName)
{

    LockSd();

    ESP_SD.chdir();

    ESP_SD.remove(NewName);
    if (!ESP_SD.rename(OldName, NewName))
    {
        logcon(String(CN_stars) + F("Could not rename '") + OldName + F("' to '") + NewName + F("'") + CN_stars);
    }
    UnLockSd();
}

void c_FileMgr::BuildFseqList(bool DisplayFileNames)
{

    char entryName[256];

    do
    {
        if (!SdCardIsInstalled())
        {

            BuildDefaultFseqList();
            break;
        }

        FeedWDT();

        LockSd();
        FsFile InputFile;
        ESP_SD.chdir();
        if (!InputFile.open("/", O_READ))
        {
            UnLockSd();
            logcon(F("ERROR: Could not open SD card for Reading FSEQ List."));
            break;
        }
        JsonDocument jsonDoc;
        jsonDoc.to<JsonObject>();

        ESP_SD.chdir();

        JsonWrite(jsonDoc, "totalBytes", SdCardSizeMB * 1024 * 1024);

        uint64_t usedBytes = 0;
        uint32_t numFiles = 0;

        JsonArray jsonDocFileList = jsonDoc["files"].to<JsonArray>();
        uint32_t FileIndex = 0;
        FsFile CurrentEntry;
        while (CurrentEntry.openNext(&InputFile, O_READ))
        {

            FeedWDT();

            if (CurrentEntry.isDirectory() || CurrentEntry.isHidden())
            {

                CurrentEntry.close();
                continue;
            }

            memset(entryName, 0x0, sizeof(entryName));
            CurrentEntry.getName(entryName, sizeof(entryName) - 1);
            String EntryName = String(entryName);

            if ((!EntryName.isEmpty()) &&

                (0 != CurrentEntry.size()))
            {

                if ((-1 != EntryName.indexOf(F(".zip"))) ||
                    (-1 != EntryName.indexOf(F(".ZIP"))) ||
                    (-1 != EntryName.indexOf(F(".xlz"))))
                {
                    FoundZipFile = true;
                }

                usedBytes += CurrentEntry.size();
                ++numFiles;

                if (DisplayFileNames)
                {
                    logcon(String(F("SD File: '")) + EntryName + "'   " + String(CurrentEntry.size()));
                }
                uint16_t Date;
                uint16_t Time;
                CurrentEntry.getCreateDateTime(&Date, &Time);

                tmElements_t tm;
                tm.Year = FS_YEAR(Date) - 1970;
                tm.Month = FS_MONTH(Date);
                tm.Day = FS_DAY(Date);
                tm.Hour = FS_HOUR(Time);
                tm.Minute = FS_MINUTE(Time);
                tm.Second = FS_SECOND(Time);

                jsonDocFileList[FileIndex]["name"] = EntryName;
                jsonDocFileList[FileIndex]["date"] = makeTime(tm);
                jsonDocFileList[FileIndex]["length"] = CurrentEntry.size();
                ++FileIndex;
            }
            else
            {
            }
            CurrentEntry.close();
        }

        InputFile.close();
        UnLockSd();

        JsonWrite(jsonDoc, "usedBytes", usedBytes);
        JsonWrite(jsonDoc, "numFiles", numFiles);
        JsonWrite(jsonDoc, "SdCardPresent", true);

        SaveFlashFile(FSEQFILELIST, jsonDoc);
    } while (false);
}

void c_FileMgr::FindFirstZipFile(String &FileName)
{

    char entryName[256];

    do
    {
        if (!SdCardIsInstalled())
        {

            break;
        }

        FeedWDT();
        LockSd();
        FsFile InputFile;
        ESP_SD.chdir();
        if (!InputFile.open("/", O_READ))
        {
            UnLockSd();
            logcon(F("ERROR: Could not open SD card for Reading FSEQ List."));
            break;
        }

        ESP_SD.chdir();

        FsFile CurrentEntry;
        while (CurrentEntry.openNext(&InputFile, O_READ))
        {

            FeedWDT();

            if (CurrentEntry.isDirectory() || CurrentEntry.isHidden())
            {

                CurrentEntry.close();
                continue;
            }

            memset(entryName, 0x0, sizeof(entryName));
            CurrentEntry.getName(entryName, sizeof(entryName) - 1);
            String EntryName = String(entryName);

            if ((-1 != EntryName.indexOf(F(".zip"))) ||
                (-1 != EntryName.indexOf(F(".ZIP"))) ||
                (-1 != EntryName.indexOf(F(".xlz"))))
            {
                FileName = EntryName;
                CurrentEntry.close();
                InputFile.close();
                break;
            }
            else
            {
            }
            CurrentEntry.close();
        }

        InputFile.close();
        UnLockSd();
    } while (false);
}

bool c_FileMgr::handleFileUpload(
    const String &filename,
    size_t index,
    uint8_t *data,
    size_t len,
    bool final,
    uint32_t totalLen)
{

    bool response = false;

    do
    {
        FeedWDT();
        if ((0 == index))
        {

            handleFileUploadNewFile(filename);
            expectedIndex = 0;
            LOG_PORT.println(".");
        }

        if (index != expectedIndex)
        {
            if (fsUploadFileHandle != INVALID_FILE_HANDLE)
            {
                logcon(String(F("ERROR: Expected index: ")) + String(expectedIndex) + F(" does not match actual index: ") + String(index));

                DEBUG_FILE_HANDLE(fsUploadFileHandle);
                CloseSdFile(fsUploadFileHandle);
                DeleteSdFile(fsUploadFileName);
                delay(100);
                BuildFseqList(false);
                expectedIndex = 0;
                fsUploadFileName = emptyString;
            }
            break;
        }

        expectedIndex = index + len;
        size_t bytesWritten = 0;

        if (len)
        {

            bytesWritten = WriteSdFileBuf(fsUploadFileHandle, data, len);

            LOG_PORT.println(String("\033[Fprogress: ") + String(expectedIndex) + ", heap: " + String(ESP.getFreeHeap()));
            LOG_PORT.flush();
        }

        if (len != bytesWritten)
        {

            DEBUG_FILE_HANDLE(fsUploadFileHandle);
            CloseSdFile(fsUploadFileHandle);
            DeleteSdFile(fsUploadFileName);
            expectedIndex = 0;
            fsUploadFileName = emptyString;
            break;
        }
        response = true;
    } while (false);

    if ((true == final) && (fsUploadFileHandle != INVALID_FILE_HANDLE))
    {

        WriteSdFileBuf(fsUploadFileHandle, data, 0);
        uint32_t uploadTime = (uint32_t)(millis() - fsUploadStartTime) / 1000;
        FeedWDT();
        DEBUG_FILE_HANDLE(fsUploadFileHandle);
        CloseSdFile(fsUploadFileHandle);

        logcon(String(F("Upload File: '")) + fsUploadFileName +
               F("' Done (") + String(uploadTime) +
               F("s). Received: ") + String(expectedIndex) +
               F(" Bytes out of ") + String(totalLen) +
               F(" bytes. FileLen: ") + GetSdFileSize(filename));

        FeedWDT();
        expectedIndex = 0;

        delay(100);
        BuildFseqList(false);

        fsUploadFileName = "";
    }

    return response;
}

void c_FileMgr::handleFileUploadNewFile(const String &filename)
{

    fsUploadStartTime = millis();

    if (0 != fsUploadFileName.length())
    {
        logcon(String(F("Aborting Previous File Upload For: '")) + fsUploadFileName + String(F("'")));
        DEBUG_FILE_HANDLE(fsUploadFileHandle);
        CloseSdFile(fsUploadFileHandle);
        fsUploadFileName = "";
    }

    fsUploadFileName = filename;

    logcon(String(F("Upload File: '")) + fsUploadFileName + String(F("' Started")));

    DeleteSdFile(fsUploadFileName);

    OpenSdFile(fsUploadFileName, FileMode::FileWrite, fsUploadFileHandle, -1);
    int FileListIndex;
    if (-1 == (FileListIndex = FileListFindSdFileHandle(fsUploadFileHandle)))
    {
        logcon(String(F("WriteSdFileBuf::ERROR::Invalid File Handle: ")) + String(fsUploadFileHandle));
    }
    else
    {

        FileList[FileListIndex].buffer.offset = 0;
        FileList[FileListIndex].buffer.size = min(uint32_t(OutputMgr.GetBufferSize() & ~(SD_BLOCK_SIZE - 1)), uint32_t(MAX_SD_BUFFER_SIZE));
        FileList[FileListIndex].buffer.DataBuffer = OutputMgr.GetBufferAddress();
    }
#ifdef SUPPORT_OLED
    OLED.ShowToast(String("Uploading ") + fsUploadFileName);
#endif
}

void c_FileMgr::BuildDefaultFseqList()
{

    JsonDocument jsonDoc;
    jsonDoc.to<JsonObject>();
    JsonWrite(jsonDoc, "SdCardPresent", false);
    JsonWrite(jsonDoc, "totalBytes", 0);
    JsonWrite(jsonDoc, "usedBytes", 0);
    JsonWrite(jsonDoc, "numFiles", 0);
    jsonDoc["files"].to<JsonArray> ();
    SaveFlashFile(FSEQFILELIST, jsonDoc);

    return;
} // BuildDefaultFseqList

bool c_FileMgr::SeekSdFile(const FileId &FileHandle, uint64_t position, SeekMode Mode)
{

    bool response = false;
    int FileListIndex;
    do // once
    {
        if (-1 == (FileListIndex = FileListFindSdFileHandle(FileHandle)))
        {
            logcon(String(F("SeekSdFile::ERROR::Invalid File Handle: ")) + String(FileHandle));
            break;
        }

        LockSd();
        switch(Mode)
        {
            case SeekMode::SeekSet:
            {
                response = FileList[FileListIndex].fsFile.seek (position);
                break;
            }
            case SeekMode::SeekEnd:
            {
                uint64_t EndPosition = FileList[FileListIndex].fsFile.size();
                response = FileList[FileListIndex].fsFile.seek (EndPosition - position);
                break;
            }
            case SeekMode::SeekCur:
            {
                uint64_t CurrentPosition = FileList[FileListIndex].fsFile.position();
                response = FileList[FileListIndex].fsFile.seek (CurrentPosition + position);
                break;
            }
            default:
            {
                logcon("Procedural error. Cannot set seek value");
                break;
            }
        } // end switch mode
        UnLockSd();
    } while(false);

    return response;
}

void c_FileMgr::LockSd()
{
    // DEBUG_START;

#ifdef ARDUINO_ARCH_ESP32
    xSemaphoreTake( SdAccessSemaphore, TickType_t(-1) );
#endif // def ARDUINO_ARCH_ESP32

    // DEBUG_END;
} // LockSd

//-----------------------------------------------------------------------------
void c_FileMgr::UnLockSd()
{
    // DEBUG_START;
#ifdef ARDUINO_ARCH_ESP32
    xSemaphoreGive( SdAccessSemaphore );
#endif // def ARDUINO_ARCH_ESP32

    // DEBUG_END;
} // UnLockSd

void c_FileMgr::AbortSdFileUpload()
{

    do
    {
        if (fsUploadFileHandle == INVALID_FILE_HANDLE)
        {

            break;
        }

        DEBUG_FILE_HANDLE(fsUploadFileHandle);
        CloseSdFile(fsUploadFileHandle);

    } while (false);
}

c_FileMgr FileMgr;
