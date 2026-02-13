# ESPixelStick Copilot Instructions (COMPREHENSIVE)

## Project Overview
ESPixelStick is an ESP8266/ESP32 firmware for controlling addressable pixels (WS2811, APA102, etc), DMX512 devices, and relays via various network protocols (E1.31, Artnet, DDP, MQTT, Alexa). The codebase is a hybrid C++/JavaScript project using PlatformIO for embedded builds and Node.js Gulp for web asset processing.

**Target Hardware**: ESP8266 (4MB+ flash), ESP32 variants. Only ~40KB usable RAM on ESP8266—aggressive memory management required.

## Architecture Essentials

### Core Component Topology
**Main execution layers** (see [src/main.cpp](src/main.cpp#L214)):
1. **InputMgr** - Receives data from network protocols, coordinates with OutputMgr
2. **OutputMgr** - Manages GPIO/UART/SPI channels to send data to physical devices
3. **NetworkMgr** - Handles WiFi/Ethernet connectivity and network state callbacks
4. **WebMgr** - REST/WebSocket API for device configuration and status
5. **FileMgr** - LittleFS filesystem operations for config files and SD card management
6. **FPPDiscovery** - xLights/FPP integration, file upload, multi-sync playback
7. **FastTimer** - Nanosecond-precision timing for pixel output bit-banging

**Main Loop** ([src/main.cpp#L530](src/main.cpp#L530)):
```cpp
void loop() {
    FeedWDT();                   // ~2-5sec watchdog per platform
    NetworkMgr.Poll();           // Keep WiFi/Ethernet alive
    OutputMgr.Poll();            // Render frames to GPIO/UART (~25-50ms intervals)
    WebMgr.Process();            // Handle HTTP/WebSocket requests (async)
    FileMgr.Poll();              // SD card operations
    FPPDiscovery.Poll();         // FPP sync packets, file uploads
    SensorDS18B20.Poll();        // Temperature sensor (optional)
    
    // Config loading (non-blocking state machine)
    if (ConfigLoadNeeded) LoadConfig();
    if (ConfigSaveNeeded) SaveConfig();
    
    // Reboot handler (delayed, allows graceful shutdown)
    if (RebootCount != NotRebootingValue) {
        if (--RebootCount == 0) ESP.restart();
    }
}
```

**Data Flow**: 
```
Network Protocol (E1.31/Artnet/MQTT) → InputMgr.ProcessReceivedData()
  → InputMgr.SetBufferTranslation() → OutputMgr pixel buffer
  → OutputMgr.Poll() → UART/SPI/GPIO drivers → Physical pixels/DMX/relays
```

### Configuration System (Critical Pattern)
Configuration is **three-file JSON-based** with **asynchronous loading**:
- **config.json** - Core device settings, network credentials, device name, blanking delays
- **input_config.json** - Protocol selection per channel, universe ranges, MQTT topics, FPP file paths
- **output_config.json** - Output channel types, GPIO pins, pixel counts, refresh rates

**Loading Architecture**:
- Files stored in **LittleFS** (on-device flash filesystem)
- Loaded at boot via `LoadConfig()` → `FileMgr.LoadFlashFile(path, callback)`
- Callback deserializes JSON into config structs (e.g., `deserializeCoreHandler()`)
- Changes from **WebMgr.SetConfig()** (async HTTP context) trigger `ScheduleLoadConfig()`
- Config reloads on **next loop iteration** (not blocking) via `time_t ConfigLoadNeeded` state machine
- **Critical**: File system operations FORBIDDEN in async WebServer context (race conditions on ESP8266)

**Typical Config Flow**:
```cpp
// Web request: PUT /conf/input_config.json
WebMgr::ProcessSetTimeRequest()
  → FileMgr.SaveFlashFile("input_config.json", json)
  → InputMgr.ScheduleLoadConfig()  // Set timer in main loop

// Main loop (next iteration)
if (ConfigLoadNeeded > 0 && now() - ConfigLoadNeeded > 100ms) {
    InputMgr.LoadConfig()
      → FileMgr.LoadFlashFile("input_config.json", &ProcessJsonConfig)
      → ProcessJsonConfig(json) updates running config
}
```

### Input Module Architecture
All inputs inherit from [c_InputCommon](include/input/InputCommon.hpp#L24). **Two channels per device**:
- **Primary** (InputChannelId_0) - Main show input (E1.31, Artnet, MQTT, etc)
- **Secondary** (InputChannelId_1) - Effects or blanking input (redundancy, backup, effects)

**Key Input Types**:
- **InputE131** - E1.31/sACN with universe mapping, configurable start/end universe, per-universe offset
- **InputArtnet** - DMX over Ethernet, polls, multi-universe support
- **InputDDP** - Distributed Display Protocol (push-based pixel data)
- **InputMQTT** - Subscribe to topics, JSON payload parsing
- **InputFPPRemote** - xLights FPP synchronization with **complex FSM-based file playback**:
  - `InputFPPRemotePlayFileFsm` - FSEQ file playback state machine
  - `InputFPPRemotePlayListFsm` - Playlist (multiple files) state machine
  - `InputFPPRemotePlayEffectFsm` - Built-in effect playback state machine
  - Handles seek commands, sync packets, blank-on-stop
- **InputEffectEngine** - Built-in animations (rainbow, chase, pulse), effect transitions
- **InputDisabled** - Placeholder (all inputs can be disabled)

**Input Interface**:
```cpp
// Every input must implement:
virtual void Begin();                              // Initialize per config
virtual bool SetConfig(JsonObject& json);         // Validate & apply
virtual void GetConfig(JsonObject& json);         // Serialize current state
virtual void GetStatus(JsonObject& json);         // Runtime stats (packet counts, errors)
virtual void Process();                           // Called from InputMgr.Process()
virtual void SetBufferInfo(uint32_t size);       // Channel count info
virtual void SetBufferTranslation();              // Map universes to output buffer offsets
virtual void ProcessReceivedData(uint8_t* data, uint32_t len);  // Handle RX data

// Optional overrides:
virtual void NetworkStateChanged(bool isConnected);  // WiFi/Ethernet up/down
virtual void ProcessButtonActions(InputValue_t);    // GPIO input triggering
virtual bool isShutDownRebootNeeded();               // Request reboot if needed
```

**Buffer Translation Pattern** (critical for universe/channel mapping):
```cpp
// Input: E1.31 universe 1, channels 1-512
// Output: Each output has buffer offset (e.g., Output0=0, Output1=512)
SetBufferTranslation() {
    // Map universe 1 channels 1-512 → OutputBuffer[0:512]
    // Map universe 2 channels 1-512 → OutputBuffer[512:1024]
    // etc.
}
```

### Output Module Architecture
All outputs inherit from [c_OutputCommon](include/output/OutputCommon.hpp#L32). **Four categories**:

**1. GPIO/PWM-based** (relays, servo controllers):
- `OutputRelay` - GPIO on/off for relays (frame rate ~20Hz)
- `OutputServoPCA9685` - PCA9685 I2C servo driver (0-255 or 0-1023 PWM)
- Direct GPIO toggling via `canRefresh()` frame timer

**2. UART-based** (most addressable pixels):
- `OutputWS2811`, `OutputUCS1903`, `OutputTM1814`, `OutputGS8208`, etc.
- Base: `OutputUart` → `OutputSerialUart` (bit-bang via UART)
- ESP32 variant: `OutputSerialRmt` (uses RMT peripheral for precision timing)
- **UART Bit-Banging Pattern**: Encode pixel data as UART characters (e.g., `0x0E`=0 bit, `0x0F`=1 bit)
- RMT (Remote Control) on ESP32: Nanosecond-precision pin switching via hardware peripheral

**3. SPI-based** (clocked pixels):
- `OutputAPA102Spi`, `OutputWS2801Spi`, `OutputGrinchSpi`
- DMA-capable on ESP32 for high-speed transfers

**4. Serial protocols**:
- `OutputGECE`, `OutputTLS3001`, `OutputUCS8903`, etc.
- DMX via UART (Renard protocol)

**Output Interface**:
```cpp
virtual void Begin();                                // Init per config
virtual bool SetConfig(JsonObject& json);           // Validate & apply
virtual void GetConfig(JsonObject& json);           // Serialize
virtual void GetStatus(JsonObject& json);           // Stats (frame count, timing)
virtual uint32_t Poll();                            // Render when canRefresh() true
virtual bool RmtPoll() { return false; }            // ESP32 RMT handling
virtual uint32_t GetNumOutputBufferBytesNeeded();   // How many bytes needed
virtual uint32_t GetNumOutputBufferChannelsServiced(); // Channels (3 per RGB pixel)
virtual uint32_t GetFrameTimeMs();                  // Frame duration

// Frame rendering:
uint32_t Poll() {
    if (!canRefresh()) return 0;  // Not time yet
    
    // canRefresh() checks: micros() - FrameStartTime > FrameDurationMicroSec
    RenderPixelsToUart(pOutputBuffer, OutputBufferSize);
    FrameStartTimeInMicroSec = micros();
    return 1;  // Frame sent
}
```

**RMT (Remote Control Peripheral)** - ESP32 exclusive:
- Hardware peripheral for nanosecond-precise GPIO toggle sequences
- Encodes pixel data (bit sequences) with sub-microsecond timing
- Called from `loop()` via `OutputMgr.RmtPoll()` 
- Non-blocking—returns control to main loop immediately
- Critical for WS2811, TM1814 (timing-sensitive protocols)

## Development Workflows

### Build & Flash
**Local compilation**: Use PlatformIO tasks in VS Code (`[ALL]`, `[EFU]`). Define build environment in `platformio_user.ini` (copy from `platformio_user.ini.sample`).

**Web assets (CRITICAL ORDER)**:
1. Run `npm install && gulp` in repo root
2. Outputs **minified + gzipped** files to `data/www/`
3. Then upload filesystem via `[FS-UP]` task

**Available Tasks**:
- `[ALL]` - Full erase/build/upload to device (PlatformIO + filesystem)
- `[EFU]` - Compile and create OTA update file (.efu) in `firmware/EFU/`
- `[FS-UP]` - Upload filesystem only (after gulp runs)
- `[REBOOT]` - Reboot all configured devices
- `[API]` - Send xLights effect names via MQTT
- Python scripts in `.scriptsJT/` automate multi-device operations

**Build Chain**:
```
platformio_user.ini (env config)
  → src/ + include/ (C++ source)
  → html/ (JS/CSS/HTML source)
  → gulp (minify/gzip) → data/www/ (compiled assets)
  → PlatformIO (build + link)
  → LittleFS image (config + web files)
  → firmware/*.bin or *.efu
```

### Memory Management (ESP8266 vs ESP32)

**ESP8266 Constraints**:
- ~40KB usable DRAM (heap)
- Shared instruction + data cache
- **No PSRAM support** (explicitly rejected in build)
- Aggressive static allocation = stack overflow or OOM crashes
- Single-threaded main loop → async libs required for networking

**Optimization Patterns**:
```cpp
// BAD: Large static array
uint8_t buffer[4096];  // Permanently allocates heap

// GOOD: Allocate once in setup, reuse in loop
uint8_t* buffer = nullptr;
void setup() { buffer = new uint8_t[4096]; }
void loop() { /* reuse */ }

// Use ArduinoJson streaming for large documents
JsonDocument doc;
DeserializationError err = deserializeJson(doc, file);
```

**Heap Monitoring**:
```cpp
TestHeap(uint32_t Id);  // Logs current free heap
// Look for patterns in debug output:
// [TestHeap 10] Heap: 30000
// [TestHeap 20] Heap: 28000  <- leaking 2KB
```

**ESP32 Advantages**:
- ~200KB heap (much more breathing room)
- Dual-core (main + task core)
- Built-in RMT peripheral for pixel timing
- Multiple UART/SPI peripherals
- Integrated PSRAM option (rejected by ESPixelStick design)
- Task scheduler for background operations

### Code Patterns & Conventions

**Configuration Methods**: Every manager/driver implements:
```cpp
bool SetConfig(JsonObject& jsonConfig) {
    // Extract from JSON with defaults
    setFromJSON(variable, jsonConfig, CN_fieldname);
    
    // Validate values
    if (variable < MIN || variable > MAX) {
        variable = DEFAULT;
        configValid = false;
    }
    
    // CRITICAL: Reflect validated values back to JSON
    GetConfig(jsonConfig);
    return configValid;
}

void GetConfig(JsonObject& jsonConfig) {
    // Serialize current state (post-validation)
    JsonWrite(jsonObject, CN_fieldname, value);
}
```
Why? Web frontend needs to see what was actually accepted (not what was sent).

**JSON Helpers** (from ConstNames.cpp):
```cpp
setFromJSON(variable, jsonObject, CN_fieldname);  // Extract with type conversion
JsonWrite(jsonObject, CN_fieldname, value);       // Write typed value
CN_* constants = JSON field names (defined in ConstNames.hpp)
```

**Network Callbacks**:
```cpp
// Classes needing network events implement:
void NetworkStateChanged(bool IsConnected) {
    if (IsConnected) {
        // Network up: start UDP listeners, MQTT subscriptions, etc.
    } else {
        // Network down: cleanup, stop async operations
    }
}
// Called by NetworkMgr on WiFi/Ethernet connect/disconnect
```

**String Safety** (buffer overflow prevention):
```cpp
SafeStrncpy(dest, src, sizeof(dest));  // Guaranteed null-termination
// NOT: strcpy(dest, src);  // DANGEROUS - no bounds check
```

**Memory Macros** (for placement allocation):
```cpp
// Allocate large object on stack of buffer in InputMgr
#define InputDriverMemorySize 2476
struct DriverInfo_t {
    alignas(16) byte InputDriver[InputDriverMemorySize];
    bool DriverInUse = false;
};
DriverInfo_t InputChannelDrivers[InputChannelId_End];

// In derived constructor, use placement new:
new(&OutputDriver[i]) OutputWS2811(id, gpio, uart, type);
```

### Network & Async Patterns

**NetworkMgr** ([include/network/NetworkMgr.hpp](include/network/NetworkMgr.hpp)):
- Manages WiFi (c_WiFiDriver) and Ethernet (c_EthernetDriver) simultaneously
- `IsConnected()` = WiFi OR Ethernet connected
- All subscribers notified via `NetworkStateChanged(bool)` callback
- Auto-fallback to AP mode if configured and STA fails after timeout

**WebMgr** ([src/WebMgr.cpp](src/WebMgr.cpp#L84)):
- Built on **ESPAsyncWebServer** (custom fork for reliability)
- Endpoints:
  - `GET /conf/{filename}` - Read config files
  - `PUT /conf/{filename}` - Write config (triggers reload)
  - `GET /admininfo.json` - Device metadata, FPP info
  - `GET /XJ` - Status JSON (WebSocket fallback)
  - `WS /ws` - WebSocket for real-time status updates
- Espalexa integration for Amazon Alexa voice control

**Async Context Rules**:
```cpp
// ALLOWED in WebMgr async handlers:
- JSON parsing
- Data buffering
- Calling ScheduleLoadConfig() / ScheduleSaveConfig()
- Calling RequestReboot()

// FORBIDDEN in async handlers:
- FileMgr operations (race on ESP8266)
- GPIO changes
- SPI/UART writes
// Instead: Schedule work via flags, execute in loop()
```

### Service Modules

**FPPDiscovery** ([src/service/FPPDiscovery.cpp](src/service/FPPDiscovery.cpp#L1)):
- xLights FPP (Falcon Player) integration
- **Dual-port system**:
  - Port 32320 (UDP) - xLights FPP plugin discovery & sync packets
  - Port 80 (HTTP multipart) - File upload for FSEQ sequences
- **File Playback FSMs**:
  - Single file: `InputFPPRemotePlayFileFsm` - handles play/pause/seek
  - Playlist: `InputFPPRemotePlayListFsm` - chains multiple files
  - Effects: `InputFPPRemotePlayEffectFsm` - triggers built-in animations
- **Sync Packets**: Receives frame-sync from FPP master, adjusts playback timing
- **File Upload**: Decompresses ZIP archives, extracts FSEQ files to SD card
- Semaphore protection (`c_FPPSemaphore`) - XSemaphore on ESP32, flag on ESP8266

**FileMgr** ([include/FileMgr.hpp](include/FileMgr.hpp#L47)):
- Abstraction layer for **LittleFS** (config files) and **SD card** (FSEQ sequences)
- Flash file operations: `SaveFlashFile()`, `LoadFlashFile()`, `ReadFlashFile()`
- SD file operations: `SaveSdFile()`, `ReadSdFile()`, `OpenSdFile()`, `BuildFseqList()`
- SPI pins configurable for SD card
- Timestamp callback for file modification times
- JSON document loading with callback: `LoadFlashFile(path, [](JsonDoc& doc) { /* use */ })`

**SensorDS18B20** (optional):
- DS18B20 temperature sensor via 1-Wire protocol
- Polled in main loop, exposed in `/XJ` status JSON

### Web Frontend Integration

**Architecture**:
- Single-Page App ([html/index.html](html/index.html)) using Bootstrap 3 + jQuery
- Dynamic tab panels generated from config objects in JavaScript
- Config files auto-downloaded on page load: `config.json`, `input_config.json`, `output_config.json`, `admininfo.json`
- Status updates via WebSocket or HTTP polling every 4 seconds

**Config Lifecycle**:
1. **Load**: JavaScript calls `GET /conf/input_config.json`, merges into `Input_Config` object
2. **Edit**: User modifies form fields (HTML → JavaScript object properties)
3. **Submit**: `SendAllConfigFilesToServer()` POSTs all three config files simultaneously
4. **Validate**: Server-side `SetConfig()` validates, reflects corrected values back
5. **Reload**: JavaScript re-reads config files to show accepted values

**Status Updates**:
```javascript
// script.js: Real-time status polling
function ProcessReceivedJsonStatusMessage(JsonStat) {
    // Update UI elements from JsonStat.status.*
    // Handles E1.31 counters, Artnet polls, DDP packets, relay states, etc.
}
```

**Dynamic UI Generation**:
```javascript
// Config objects drive form generation
Input_Config: {
    input_config: [{
        type: "E1_31",
        universe: 1,
        universe_limit: 512,
        ...
    }, {...}]
}

// JavaScript creates HTML from template + config
// Allows any new input/output type to auto-generate UI
```

**WebSocket (when available)**:
```javascript
// Efficient real-time status without polling
ws.onmessage = (evt) => ProcessReceivedJsonStatusMessage(JSON.parse(evt.data));
```

### Input Manager Deep Dive

**Two-Channel Design** (primary + secondary):
```cpp
enum e_InputChannelIds {
    InputPrimaryChannelId = 0,    // Main show input
    InputSecondaryChannelId = 1,  // Effects/blank input
    InputChannelId_End,
    EffectsChannel = InputSecondaryChannelId
};

enum e_InputType {
    InputType_E1_31,
    InputType_Effects,
    InputType_MQTT,
    InputType_Alexa,
    InputType_DDP,
    InputType_FPP,
    InputType_Artnet,
    InputType_Disabled,
    ...
};
```

**Instantiation Pattern** (via placement new):
```cpp
void InstantiateNewInputChannel(e_InputChannelIds ChannelId, e_InputType Type) {
    c_InputMgr::DriverInfo_t& Driver = InputChannelDrivers[ChannelId];
    
    // Call destructor if already allocated
    if (Driver.DriverInUse) {
        ((c_InputCommon*)&Driver.InputDriver)->~c_InputCommon();
        Driver.DriverInUse = false;
    }
    
    // Placement new - construct in pre-allocated buffer
    switch(Type) {
        case InputType_E1_31:
            new(&Driver.InputDriver) c_InputE131(...);
            break;
        // ... other types
    }
    Driver.DriverInUse = true;
    Driver.pInputDriver->Begin();
}
```

**Blanking Timer** (blank when input stops):
```cpp
// When input stops for BlankDelay seconds, switch to secondary input
void InputMgr::Process() {
    for (each channel) {
        if (!packet_received) {
            RestartBlankTimer(channel);
        }
        if (BlankTimerHasExpired(channel)) {
            SwitchToSecondaryInput(channel);
        }
    }
}
```

### Output Manager Deep Dive

**Buffer Management**:
```cpp
// Outputs share a common buffer arena managed by OutputMgr
uint8_t* pOutputBuffer = nullptr;
uint32_t OutputBufferSize = 0;

// Each output declares its offset and size
struct OutputChannel_t {
    uint32_t BytesNeeded = 0;
    uint32_t ChannelsServiced = 0;  // pixels * 3 for RGB, etc.
    uint8_t* pOutputBuffer = nullptr;  // base buffer + offset
};
```

**Factory Pattern** (instantiation):
```cpp
// Static buffer for placement new
#define OutputDriver_Size_Max 256
struct DriverInfo_t {
    alignas(16) byte OutputDriver[OutputDriver_Size_Max];
    bool DriverInUse = false;
};
DriverInfo_t OutputChannelDrivers[OutputChannelId_End];

// Macro for type-safe placement new
#define AllocatePort(ClassType, Output, DriverId, GpioId, UartId, OutputType) \
    new(&Output.OutputDriver) ClassType(DriverId, GpioId, UartId, OutputType);
```

**Frame Timing Precision**:
```cpp
// canRefresh() - nanosecond-accurate frame rate limiting
bool canRefresh() {
    uint32_t Now = micros();
    uint32_t Delta = Now - FrameStartTimeInMicroSec;
    if (Now < FrameStartTimeInMicroSec) {  // micros() wrapped
        Delta = Now + (0 - FrameStartTimeInMicroSec);
    }
    return (Delta > FrameDurationInMicroSec);
}

// Called from Poll():
uint32_t Poll() {
    if (!canRefresh()) return 0;
    RenderFrame();
    FrameStartTimeInMicroSec = micros();
    return 1;
}
```

**RMT Integration** (ESP32 only):
```cpp
// RMT = Remote Control peripheral (hardware PWM-like unit)
// Each output may have RMT version (OutputWS2811Rmt, OutputWS2811Uart)

// Separate polling path:
bool OutputMgr::RmtPoll() {
    for (each output) {
        if (output.RmtPoll()) {  // Returns true if RMT is active
            FramesRendered++;
        }
    }
    return FramesRendered > 0;
}
```

**Shared Refresh Rate**:
```cpp
// All outputs render at fastest-required rate
// Slower outputs still render buffer every cycle, but skip transmission
uint32_t OutputMgr::GetRefreshRate() {
    uint32_t MaxRate = 0;
    for (each output) {
        MaxRate = max(MaxRate, output.GetFrameTimeMs());
    }
    return MaxRate;
}
```

## Key Files & References

**Architecture & Setup**:
- [main.cpp](src/main.cpp) - Boot sequence, config loading, main loop orchestration
- [include/ESPixelStick.h](include/ESPixelStick.h) - Global constants, macros, config_t struct
- [platformio.ini](platformio.ini) - Build environments, dependencies

**Managers** (factory/orchestration pattern):
- [include/input/InputMgr.hpp](include/input/InputMgr.hpp), [src/input/InputMgr.cpp](src/input/InputMgr.cpp) - Input protocol factory
- [include/output/OutputMgr.hpp](include/output/OutputMgr.hpp), [src/output/OutputMgr.cpp](src/output/OutputMgr.cpp) - Output driver factory
- [include/WebMgr.hpp](include/WebMgr.hpp), [src/WebMgr.cpp](src/WebMgr.cpp) - REST/WebSocket API
- [include/FileMgr.hpp](include/FileMgr.hpp), [src/FileMgr.cpp](src/FileMgr.cpp) - File system abstraction
- [include/network/NetworkMgr.hpp](include/network/NetworkMgr.hpp), [src/network/NetworkMgr.cpp](src/network/NetworkMgr.cpp) - WiFi/Ethernet coordination

**Input Implementations** (exemplars):
- [src/input/InputE131.cpp](src/input/InputE131.cpp) - sACN/E1.31 protocol
- [src/input/InputArtnet.cpp](src/input/InputArtnet.cpp) - Art-Net DMX protocol with universe mapping
- [src/input/InputMQTT.cpp](src/input/InputMQTT.cpp) - MQTT topic subscription
- [src/input/InputFPPRemote.cpp](src/input/InputFPPRemote.cpp) - FPP remote file playback

**Output Implementations** (exemplars):
- [src/output/OutputWS2811.cpp](src/output/OutputWS2811.cpp) - UART bit-bang WS2811
- [src/output/OutputWS2811Rmt.cpp](src/output/OutputWS2811Rmt.cpp) - ESP32 RMT WS2811 (high-precision)
- [src/output/OutputRelay.cpp](src/output/OutputRelay.cpp) - Simple GPIO relay
- [src/output/OutputServoPCA9685.cpp](src/output/OutputServoPCA9685.cpp) - I2C servo driver

**Service Modules**:
- [src/service/FPPDiscovery.cpp](src/service/FPPDiscovery.cpp) - xLights integration, file upload, multi-sync
- [src/service/FPPRemote*.cpp](src/service/) - File/Playlist/Effect playback FSMs

**Web Frontend**:
- [html/index.html](html/index.html) - UI markup + copy-to-clipboard script
- [html/js/script.js](html/js/script.js) - Config management, status polling, form generation
- [html/css/style.css](html/css/style.css) - Bootstrap overrides

**Constants & Names**:
- [include/ConstNames.hpp](include/ConstNames.hpp) - JSON key constants (CN_universe, CN_port, etc.)
- [src/ConstNames.cpp](src/ConstNames.cpp) - setFromJSON(), JsonWrite() helpers

## Common Tasks

### Add a New Input Protocol

1. **Create header** [include/input/InputMyProtocol.hpp](include/input/):
   ```cpp
   #include "InputCommon.hpp"
   class c_InputMyProtocol : public c_InputCommon {
   public:
       c_InputMyProtocol(c_InputMgr::e_InputChannelIds, uint32_t);
       virtual ~c_InputMyProtocol();
       virtual void Begin() override;
       virtual bool SetConfig(JsonObject&) override;
       virtual void GetConfig(JsonObject&) override;
       virtual void Process() override;
       // ... other virtuals
   };
   ```

2. **Implement** [src/input/InputMyProtocol.cpp](src/input/):
   - Implement all virtual methods
   - Use `setFromJSON()` / `JsonWrite()` for config
   - Call `SetBufferTranslation()` to map channels
   - In `Process()`, check for new packets and call `ProcessReceivedData()`

3. **Register in InputMgr** ([src/input/InputMgr.cpp](src/input/InputMgr.cpp)):
   ```cpp
   // Header: Add to enum
   enum e_InputType { InputType_MyProtocol, ... };
   
   // Instantiation: Add case
   case InputType_MyProtocol:
       new(&Driver.InputDriver) c_InputMyProtocol(ChannelId, bufSize);
       break;
   ```

### Add a New Output Type

1. **Create header** [include/output/OutputMyType.hpp](include/output/):
   - Inherit from `c_OutputCommon` (or subclass like `c_OutputUart`)
   - Implement `Poll()` to render pixels
   - On ESP32, implement `RmtPoll()` if timing-critical

2. **Implement** [src/output/OutputMyType.cpp](src/output/):
   - `Begin()` - Initialize GPIO/UART/SPI
   - `SetConfig()` - Validate parameters
   - `Poll()` - Render when `canRefresh()` true
   - `RmtPoll()` - (ESP32) Use RMT if available

3. **Register in OutputMgr** ([src/output/OutputMgr.cpp](src/output/OutputMgr.cpp)):
   ```cpp
   // Enum: Add to e_OutputType
   // Xlate table: Add entry with name
   // AllocatePort: Add case to instantiate
   ```

### Working with Asynchronous Configuration

**Correct pattern**:
```cpp
// In WebMgr async handler:
void WebMgr::SetConfig(const char* newConfig) {
    FileMgr.SaveFlashFile("input_config.json", newConfig);
    InputMgr.ScheduleLoadConfig();  // Schedule, don't load
    // Return immediately - don't block
}

// In main loop:
void loop() {
    // Check if reload scheduled
    if (NO_CONFIG_NEEDED != ConfigLoadNeeded) {
        if (abs(now() - ConfigLoadNeeded) > LOAD_CONFIG_DELAY) {
            InputMgr.LoadConfig();  // Safe here - not in async context
        }
    }
}
```

### Platform-Specific Considerations

**ESP8266**:
- Use PSRAM sparingly (if at all) - ~40KB total available
- Watchdog ~2 seconds - `FeedWDT()` called in loop
- Single-threaded - use async libraries (ESPAsyncWebServer, ESPAsyncE131)
- No RMT peripheral - use UART bit-banging for pixel output
- Flash wears out - minimize write frequency

**ESP32**:
- Much more RAM (~200KB) - breathe easier
- Watchdog ~5 seconds - `esp_task_wdt_reset()` 
- Multi-core - can use tasks for background work (FPPDiscovery uses semaphores)
- RMT peripheral available - use for timing-critical outputs
- PSRAM optional but explicitly rejected in ESPixelStick (design constraint)

## Debugging Notes

**Reboot Loops** - Check serial output:
```
[   main] Internal Reboot Requested: 'Reason text' Rebooting Now
```
Then search codebase for that reason text to find the `RequestReboot()` call.

**Config Corruption**:
1. Delete `config.json` on device (force factory reset)
2. Device creates default config at next boot
3. Re-configure via web UI

**Memory Leaks**:
- Call `TestHeap(id)` at key points, watch debug output
- Pattern: `[TestHeap 10] Heap: 30000` → `[TestHeap 20] Heap: 25000`
- Search for `new` without corresponding `delete` or placement new without `~destructors`

**Timing Issues** (pixels flickering, dropouts):
- ESP8266: Reduce baud rates, increase `FrameDurationInMicroSec`
- Ensure `FeedWDT()` called regularly in tight loops
- RMT (ESP32): Check RMT ISR is completing - don't block interrupts

**Packet Loss** (E1.31/Artnet):
- Check packet counters in status JSON
- Verify universe ranges don't exceed `BufferSize`
- Monitor `SetBufferTranslation()` - off-by-one errors cause data corruption

**WiFi Issues**:
- Check credentials in web UI
- Verify AP timeout vs STA timeout settings
- Check MAC address printed at boot matches your device

## External Dependencies

- **ArduinoJson** v7+ - JSON serialization/deserialization with streaming support
- **ESPAsyncWebServer** (custom fork) - Non-blocking HTTP + WebSocket
- **ESPAsyncE131** - Async E1.31/sACN receiver
- **Artnet** - Art-Net protocol implementation
- **async-mqtt-client** - Non-blocking MQTT with reconnect
- **SdFat** v2.3+ - SD card access with FATxx support
- **Espalexa** - Amazon Alexa voice control integration
- **Time** - Time library for NTP, time_t functions
- **SimpleFTPServer** - FTP server for file management
- **Int64String** - Convert uint64_t to/from string (for large file sizes)
- PlatformIO ecosystem: board definitions, toolchains, flashing utilities

## Testing & Validation

**Unit Testing Approach**:
- No formal unit tests in codebase (embedded constraints)
- Integration testing via web UI: change config → verify output behavior
- Validate JSON schemas match expected config structure

**Config Validation Strategy**:
1. `SetConfig()` returns `bool` - false = validation failed
2. `GetConfig()` reflects what was actually accepted
3. Frontend re-reads config to show validated values to user
4. Example: User tries universe=999 for E1.31 → validator clamps to 512 → frontend shows 512

**Debugging Protocol Handlers**:
```cpp
// In input/output SetConfig():
// DEBUG_V(String("Config parameter: ") + String(value));
// Enable via #define DEBUG in platformio.ini
// Output appears on Serial at 115200 baud (LOG_PORT)
```

**Timing Validation**:
- Use logic analyzer on data pin (GPIO) to verify output timing
- RMT (ESP32) produces exact-period waveforms
- UART bit-bang (ESP8266) timing varies slightly due to ISR jitter

## Production Deployment Checklist

- [ ] Web assets processed: `npm install && gulp` ✓
- [ ] Filesystem uploaded: `[FS-UP]` task ✓
- [ ] Firmware compiled: `[ALL]` or `[EFU]` task ✓
- [ ] Config backed up (via Admin → Backup button)
- [ ] Output refresh rate appropriate for pixel count
- [ ] Blanking delay configured (0 = disabled)
- [ ] Network credentials set correctly
- [ ] Hostname unique on network
- [ ] FPP integration tested (if using remote file playback)
- [ ] Temperature sensor calibrated (if DS18B20 present)
- [ ] All config files uploaded to device
- [ ] Tested failover (WiFi → AP mode if configured)

