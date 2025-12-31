# Deferred Unzip Processing with GUI Notification

## Overview
This change modifies the ESPixelStick firmware to defer `.xlz` file decompression until **after** a successful network connection (WiFi or Ethernet) is established, and adds a GUI notification to display the decompression status.

## Problem Addressed
Previously, when a `.xlz` file was found on the SD card, it was immediately decompressed during the FileMgr initialization phase, before any network negotiation had occurred. This could cause issues if:
- You wanted to monitor the decompression process via the web GUI
- The device needed network connectivity before certain operations
- You wanted to see clear feedback about what the device is doing during startup

## Solution
The solution uses a deferred polling approach:

### 1. **Deferred Execution** (`UnzipFiles` class)
   - Added new member variables:
     - `isUnzipping`: Flag indicating active decompression
     - `hasPendingZipFile`: Flag indicating a file is waiting for network
     - `currentZipFileName`: Stores the filename being processed
     - `networkConnectionCheckTime`: Throttles network checks to once per second
   
   - Added new method: `Poll()`
     - Checks every second if network is connected
     - Starts decompression when network connection is detected
     - Requests reboot after completion

### 2. **Modified Initialization Flow**
   - `FileMgr::StartSdCard()` now only initializes the UnzipFiles object and calls `Run()` once
   - `Run()` only searches for a `.xlz` file and stores the filename for later
   - The file is NOT processed immediately

### 3. **Main Loop Integration** (`src/main.cpp`)
   - Added `gUnzipFiles.Poll()` call in the main loop
   - Polling happens after network initialization, ensuring network is available
   - Executes every loop iteration (throttled internally in `Poll()`)

### 4. **Status Reporting** (`WebMgr::ProcessXJRequest()`)
   - Added JSON response field: `system.unzip`
   - Contains:
     - `isUnzipping`: Boolean - true if currently decompressing
     - `hasPending`: Boolean - true if waiting for network
     - `fileName`: String - current .xlz filename being processed

### 5. **GUI Notification** (`html/index.html` + `js/script.js`)
   - Added notification area at the top of the page
   - **Network Status Indicator**: Shows WiFi/Ethernet connection state
   - **Unzip Status Indicator**: 
     - **Info Alert (Blue)**: File found, waiting for network connection with message "(Waiting for network...)"
     - **Warning Alert (Orange)**: Currently decompressing with filename displayed
     - Automatically hides when unzipping completes

## Files Modified

### Backend (C++)
1. **include/UnzipFiles.hpp**
   - Added state tracking variables
   - Added `Poll()` method declaration
   - Added query methods: `IsUnzipping()`, `HasPendingZipFile()`, `GetCurrentZipFileName()`

2. **src/UnzipFiles.cpp**
   - Rewrote `Run()` to only find and store the filename
   - Implemented new `Poll()` method with network checking logic
   - Moved actual decompression logic into `Poll()`

3. **src/FileMgr.cpp**
   - Modified `StartSdCard()` to not immediately decompress
   - Added comment explaining deferred processing

4. **src/main.cpp**
   - Added `#include "UnzipFiles.hpp"` (conditionally compiled with `#ifdef SUPPORT_UNZIP`)
   - Added `gUnzipFiles.Poll()` call in main loop after FileMgr.Poll()

5. **src/WebMgr.cpp**
   - Added `#include "UnzipFiles.hpp"`
   - Added unzip status to JSON response in `ProcessXJRequest()`

### Frontend (Web UI)
1. **html/index.html**
   - Added notification area div with:
     - `networkStatusIndicator`: Shows network connection status
     - `unzipStatusIndicator`: Shows decompression status

2. **html/js/script.js**
   - Added unzip status processing in `ProcessReceivedJsonStatusMessage()`
   - Shows/hides notification based on unzip state
   - Displays waiting message when pending, filename when active

## Behavior Timeline

### On Startup with .xlz File:
1. **FileMgr initialization** → Finds .xlz file, stores filename
2. **Network initialization begins** → Device tries WiFi/Ethernet
3. **Main loop polling** → 
   - Network connects → Unzip begins immediately
   - Network not connected → Unzip waits (GUI shows blue alert "Waiting for network...")
4. **Decompression** → GUI shows orange alert with filename
5. **Completion** → Device reboots

## Benefits
✅ GUI shows clear status of what the device is doing  
✅ Ensures network is available before processing files  
✅ User can see progress/status via web interface  
✅ OLED display (if installed) still shows toast notifications  
✅ Backward compatible - falls back to existing behavior if no network  
✅ Throttled polling (1 second interval) - minimal CPU impact  

## Impact
✅ SLOWER DECOMPRESSION BY 13%-20%


## Testing Notes
- The notification will appear **only on the Home tab** when first loading the page
- Status updates every 4 seconds (existing RequestStatusUpdate frequency)
- OLED displays still show "DECOMPRESSING" and "COMPLETED" toasts
- Serial console logs key events: "Found zip file", "Network connected", "Starting decompression"

## TODO
- REALLY need to test each output and input option as well as load impact on opposing core


## Conclusion:
-  Leaps and BOUNDS closer to an open-source, community built, Martin maintained, Production-Grade firmware