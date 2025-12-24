/*
* OutputI2SParallel.cpp - Parallel I2S driver code for ESPixelStick
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
*/

#include "ESPixelStick.h"

#include "output/OutputI2SParallel.hpp"
#include "output/OutputCommon.hpp"

#ifdef ARDUINO_ARCH_ESP32
#include <driver/gpio.h>
#include <driver/i2s.h>
#include <soc/gpio_sig_map.h>
#include <soc/i2s_reg.h>
#endif

#ifdef SUPPORT_OutputType_I2S_Parallel

//----------------------------------------------------------------------------
c_OutputI2SParallel::c_OutputI2SParallel (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                                          gpio_num_t outputGpio,
                                          uart_port_t uart,
                                          c_OutputMgr::e_OutputType outputType) :
    c_OutputWS2811(OutputChannelId, outputGpio, uart, outputType)
{
    DataPins.fill(GPIO_NUM_NC);
#ifdef DEFAULT_RMT_0_GPIO
    DataPins[0] = DEFAULT_RMT_0_GPIO;
#endif
#ifdef DEFAULT_RMT_1_GPIO
    DataPins[1] = DEFAULT_RMT_1_GPIO;
#endif
#ifdef DEFAULT_RMT_2_GPIO
    DataPins[2] = DEFAULT_RMT_2_GPIO;
#endif
#ifdef DEFAULT_RMT_3_GPIO
    DataPins[3] = DEFAULT_RMT_3_GPIO;
#endif
#ifdef DEFAULT_RMT_4_GPIO
    DataPins[4] = DEFAULT_RMT_4_GPIO;
#endif
#ifdef DEFAULT_RMT_5_GPIO
    DataPins[5] = DEFAULT_RMT_5_GPIO;
#endif
#ifdef DEFAULT_RMT_6_GPIO
    DataPins[6] = DEFAULT_RMT_6_GPIO;
#endif
#ifdef DEFAULT_RMT_7_GPIO
    DataPins[7] = DEFAULT_RMT_7_GPIO;
#endif

    if (GPIO_NUM_NC == DataPins[0])
    {
        for (uint8_t index = 0; index < ParallelBitCount; ++index)
        {
            DataPins[index] = gpio_num_t(int(outputGpio) + index);
        }
    }
    UpdateLaneConfiguration();
} // c_OutputI2SParallel

//----------------------------------------------------------------------------
c_OutputI2SParallel::~c_OutputI2SParallel()
{
} // ~c_OutputI2SParallel

//----------------------------------------------------------------------------
// Use the current config to set up the output port
void c_OutputI2SParallel::Begin()
{
    c_OutputWS2811::Begin();
    UpdateLaneConfiguration();
    ConfigureI2S();
    ConfigureParallelPins();
    HasBeenInitialized = true;
} // begin

//----------------------------------------------------------------------------
bool c_OutputI2SParallel::SetConfig(ArduinoJson::JsonObject & jsonConfig)
{
    bool response = c_OutputWS2811::SetConfig(jsonConfig);
    InterFrameGapMicros = InterFrameGapInMicroSec;
    PixelsPerString = GetPixelCount();
    UpdateLaneConfiguration();

    uint32_t totalPixels = PixelsPerString * ActiveLaneCount;
    SetPixelCount(totalPixels);
    SetOutputBufferSize(GetNumOutputBufferBytesNeeded());

    JsonWrite(jsonConfig, CN_pixel_count, PixelsPerString);
    return response;
} // SetConfig

//----------------------------------------------------------------------------
void c_OutputI2SParallel::GetConfig(ArduinoJson::JsonObject & jsonConfig)
{
    c_OutputWS2811::GetConfig(jsonConfig);
    JsonWrite(jsonConfig, CN_pixel_count, PixelsPerString);
} // GetConfig

//----------------------------------------------------------------------------
uint32_t c_OutputI2SParallel::Poll()
{
#ifdef ARDUINO_ARCH_ESP32
    if (Paused || !canRefresh() || (0 == PixelsPerString))
    {
        return ActualFrameDurationMicroSec;
    }

    StartNewFrame();
    BuildFrameBuffer();
    if (FrameBuffer.empty())
    {
        return ActualFrameDurationMicroSec;
    }

    size_t bytes_written = 0;
    IsSendingData = true;
    i2s_write(I2S_NUM_0, FrameBuffer.data(), FrameBuffer.size() * sizeof(uint16_t), &bytes_written, portMAX_DELAY);
    IsSendingData = false;

    delayMicroseconds(InterFrameGapMicros);
    ActualFrameDurationMicroSec = (FrameBuffer.size() * MicroSecondsInASecond) / I2SClockHz + InterFrameGapMicros;
#endif
    return ActualFrameDurationMicroSec;
} // Poll

//----------------------------------------------------------------------------
void c_OutputI2SParallel::GetStatus (ArduinoJson::JsonObject & jsonStatus)
{
    c_OutputWS2811::GetStatus(jsonStatus);
    JsonWrite(jsonStatus, CN_count, PixelsPerString);
    JsonWrite(jsonStatus, CN_status, IsSendingData ? F("Transmitting") : F("Idle"));
} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputI2SParallel::GetNumOutputBufferBytesNeeded ()
{
    return PixelsPerString * uint32_t(PIXEL_DEFAULT_INTENSITY_BYTES_PER_PIXEL) * ActiveLaneCount;
}

//----------------------------------------------------------------------------
uint32_t c_OutputI2SParallel::GetNumOutputBufferChannelsServiced ()
{
    return GetNumOutputBufferBytesNeeded();
}

//----------------------------------------------------------------------------
void c_OutputI2SParallel::SetOutputBufferSize (uint32_t NumChannelsAvailable)
{
    if (0 == ActiveLaneCount)
    {
        PixelsPerString = 0;
        SetPixelCount(0);
        c_OutputWS2811::SetOutputBufferSize(0);
        return;
    }

    PixelsPerString = NumChannelsAvailable / (ActiveLaneCount * PIXEL_DEFAULT_INTENSITY_BYTES_PER_PIXEL);
    SetPixelCount(PixelsPerString * ActiveLaneCount);
    c_OutputWS2811::SetOutputBufferSize(PixelsPerString * PIXEL_DEFAULT_INTENSITY_BYTES_PER_PIXEL * ActiveLaneCount);
}

//----------------------------------------------------------------------------
void c_OutputI2SParallel::PauseOutput(bool State)
{
    c_OutputWS2811::PauseOutput(State);
    Paused = State;
}

#ifdef ARDUINO_ARCH_ESP32
//----------------------------------------------------------------------------
void c_OutputI2SParallel::UpdateLaneConfiguration()
{
    ActivePins.fill(GPIO_NUM_NC);
    ActiveLaneCount = 0;
    for (uint8_t index = 0; index < ParallelBitCount; ++index)
    {
        gpio_num_t pin = DataPins[index];
        if (GPIO_NUM_NC == pin)
        {
            continue;
        }
        ActivePins[ActiveLaneCount++] = pin;
    }
}

//----------------------------------------------------------------------------
uint32_t c_OutputI2SParallel::GetLaneStrideBytes() const
{
    return PixelsPerString * PIXEL_DEFAULT_INTENSITY_BYTES_PER_PIXEL;
}

//----------------------------------------------------------------------------
uint32_t c_OutputI2SParallel::GetFrameSampleCount() const
{
    const uint32_t bitsPerPixel = PIXEL_DEFAULT_INTENSITY_BYTES_PER_PIXEL * 8;
    const uint32_t totalBits = bitsPerPixel * PixelsPerString;
    return totalBits * SamplesPerBit;
}

//----------------------------------------------------------------------------
void c_OutputI2SParallel::ConfigureParallelPins()
{
    for (uint8_t index = 0; index < ActiveLaneCount; ++index)
    {
        gpio_num_t pin = ActivePins[index];
        if (GPIO_NUM_NC == pin)
        {
            continue;
        }

        pinMode(pin, OUTPUT);
        gpio_matrix_out(pin, I2S0O_DATA_OUT0_IDX + index, false, false);
    }
}

//----------------------------------------------------------------------------
void c_OutputI2SParallel::ConfigureI2S()
{
    i2s_driver_uninstall(I2S_NUM_0);
    i2s_config_t i2s_config = {};
    i2s_config.mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_config.sample_rate = I2SClockHz;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
#if defined(I2S_COMM_FORMAT_STAND_I2S)
    i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
#else
    i2s_config.communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB;
#endif
    i2s_config.dma_buf_count = 8;
    i2s_config.dma_buf_len = 64;
    i2s_config.use_apll = false;
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2s_config.tx_desc_auto_clear = true;
    i2s_config.fixed_mclk = 0;
#if defined(I2S_MCLK_MULTIPLE_DEFAULT)
    i2s_config.mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT;
#endif
#if defined(I2S_BITS_PER_CHAN_DEFAULT)
    i2s_config.bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT;
#endif

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, nullptr);

    i2s_pin_config_t pin_config = {};
    pin_config.bck_io_num = I2S_PIN_NO_CHANGE;
    pin_config.ws_io_num = I2S_PIN_NO_CHANGE;
    pin_config.data_out_num = I2S_PIN_NO_CHANGE;
    pin_config.data_in_num = I2S_PIN_NO_CHANGE;
    i2s_set_pin(I2S_NUM_0, &pin_config);

    REG_SET_BIT(I2S0_CONF_REG, I2S_TX_RIGHT_FIRST);
    REG_SET_BIT(I2S0_CONF2_REG, I2S_LCD_EN);
    if (ActiveLaneCount > 0)
    {
        REG_SET_FIELD(I2S0_SAMPLE_RATE_CONF_REG, I2S_TX_BITS_MOD, ActiveLaneCount);
    }
    REG_SET_FIELD(I2S0_CONF_CHAN_REG, I2S_TX_CHAN_MOD, 1);
    REG_SET_FIELD(I2S0_FIFO_CONF_REG, I2S_TX_FIFO_MOD, 1);
}

//----------------------------------------------------------------------------
void c_OutputI2SParallel::BuildFrameBuffer()
{
    FrameBuffer.assign(GetFrameSampleCount(), 0);
    if (FrameBuffer.empty() || (0 == ActiveLaneCount))
    {
        return;
    }

    const uint32_t laneStride = GetLaneStrideBytes();
    uint32_t sampleIndex = 0;

    for (uint32_t pixelIndex = 0; pixelIndex < PixelsPerString; ++pixelIndex)
    {
        for (uint32_t colorByte = 0; colorByte < PIXEL_DEFAULT_INTENSITY_BYTES_PER_PIXEL; ++colorByte)
        {
            for (int8_t bit = 7; bit >= 0; --bit)
            {
                for (uint8_t sample = 0; sample < SamplesPerBit; ++sample)
                {
                    uint16_t sampleMask = 0;
                    for (uint8_t lane = 0; lane < ActiveLaneCount; ++lane)
                    {
                        uint32_t laneOffset = (laneStride * lane) + (pixelIndex * PIXEL_DEFAULT_INTENSITY_BYTES_PER_PIXEL) + colorByte;
                        if (laneOffset >= OutputBufferSize)
                        {
                            continue;
                        }

                        uint8_t value = pOutputBuffer[laneOffset];
                        bool bitIsOne = (value & (1 << bit)) != 0;
                        uint8_t highSamples = bitIsOne ? HighSamplesForOne : HighSamplesForZero;
                        if (sample < highSamples)
                        {
                            sampleMask |= (1 << lane);
                        }
                    }

                    FrameBuffer[sampleIndex++] = sampleMask;
                }
            }
        }
    }
}
#endif // def ARDUINO_ARCH_ESP32

#endif // def SUPPORT_OutputType_I2S_Parallel
