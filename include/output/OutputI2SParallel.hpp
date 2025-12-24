#pragma once
/*
* OutputI2SParallel.hpp - Parallel I2S placeholder driver
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2025 Shelby Merrick
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
*   This driver provides an ESP32 parallel I2S transport for WS2811-style
*   pixels, driving multiple strings simultaneously from a shared DMA bitstream
*   and exposing the normal configuration and status interfaces used by other
*   pixel outputs.
*
*/

#include <array>
#include <vector>

#include "OutputWS2811.hpp"

#ifdef SUPPORT_OutputType_I2S_Parallel

class c_OutputI2SParallel : public c_OutputWS2811
{
public:
    c_OutputI2SParallel (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                         gpio_num_t outputGpio,
                         uart_port_t uart,
                         c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputI2SParallel ();

    void         Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    bool         SetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Set a new config in the driver
    void         GetConfig (ArduinoJson::JsonObject & jsonConfig); ///< Get the current config used by the driver
    void         GetStatus (ArduinoJson::JsonObject & jsonConfig);
    uint32_t     Poll ();                                          ///< Call from loop(), renders output data
#ifdef ARDUINO_ARCH_ESP32
    bool         RmtPoll () {return false;}
#endif // def ARDUINO_ARCH_ESP32
    void         GetDriverName (String & sDriverName) { sDriverName = CN_I2S_Parallel; }
    uint32_t     GetNumOutputBufferBytesNeeded ();
    uint32_t     GetNumOutputBufferChannelsServiced ();
    void         SetOutputBufferSize (uint32_t NumChannelsAvailable);
    void         PauseOutput(bool State);
    bool         DriverIsSendingIntensityData() {return IsSendingData;}

private:
    static constexpr uint8_t ParallelBitCount = 8;
    static constexpr uint8_t SamplesPerBit = 4;
    static constexpr uint8_t HighSamplesForZero = 1;
    static constexpr uint8_t HighSamplesForOne = 2;
    static constexpr uint32_t I2SClockHz = 3200000; // 4x WS2811 bit clock (1.25us bits)

    void BuildFrameBuffer();
    void ConfigureParallelPins();
    void ConfigureI2S();
    void UpdateLaneConfiguration();
    uint32_t GetLaneStrideBytes() const;
    uint32_t GetFrameSampleCount() const;

    std::array<gpio_num_t, ParallelBitCount> DataPins {};
    std::array<gpio_num_t, ParallelBitCount> ActivePins {};
    uint8_t ActiveLaneCount = 0;
    std::vector<uint16_t> FrameBuffer;
    bool IsSendingData = false;
    uint32_t PixelsPerString = 0;
    uint32_t InterFrameGapMicros = WS2811_PIXEL_IDLE_TIME_US;
}; // c_OutputI2SParallel

#endif // def SUPPORT_OutputType_I2S_Parallel
