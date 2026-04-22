/*
* OutputTLS3001Rmt.cpp - TLS3001 driver code for ESPixelStick RMT Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2026 Shelby Merrick
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
#if defined(SUPPORT_OutputProtocol_TLS3001) && defined (ARDUINO_ARCH_ESP32)

#include "output/OutputTLS3001Rmt.hpp"

#define TLS3001_PIXEL_RMT_TICKS_BIT  uint16_t(TLS3001_PIXEL_NS_BIT / RMT_TickLengthNS)

//----------------------------------------------------------------------------
static bool IRAM_ATTR ISR_GetNextBitToSendBase (void * arg, uint32_t & DataToSend)
{
    return reinterpret_cast<c_OutputTLS3001Rmt*>(arg)->ISR_GetNextBitToSend(DataToSend);
} // ISR_GetNextBitToSend

static const c_OutputRmt::ConvertIntensityToRmtDataStreamEntry_t ConvertIntensityToRmtDataStream[] =
{
    // {{.duration0,.level0,.duration1,.level1},Type},

    {{TLS3001_PIXEL_RMT_TICKS_BIT / 2, 1, TLS3001_PIXEL_RMT_TICKS_BIT / 2, 0}, c_OutputRmt::RmtDataBitIdType_t::RMT_STARTBIT_ID},
    {{TLS3001_PIXEL_RMT_TICKS_BIT / 2, 0, TLS3001_PIXEL_RMT_TICKS_BIT / 2, 1}, c_OutputRmt::RmtDataBitIdType_t::RMT_DATA_BIT_ZERO_ID},
    {{TLS3001_PIXEL_RMT_TICKS_BIT / 2, 1, TLS3001_PIXEL_RMT_TICKS_BIT / 2, 0}, c_OutputRmt::RmtDataBitIdType_t::RMT_DATA_BIT_ONE_ID},
    {{TLS3001_PIXEL_RMT_TICKS_BIT / 2, 0, TLS3001_PIXEL_RMT_TICKS_BIT / 2, 0}, c_OutputRmt::RmtDataBitIdType_t::RMT_INTERFRAME_GAP_ID},
    {{                              0, 0,                               0, 0}, c_OutputRmt::RmtDataBitIdType_t::RMT_STOPBIT_ID},
    {{                              0, 0,                               0, 0}, c_OutputRmt::RmtDataBitIdType_t::RMT_LIST_END},
}; // ConvertIntensityToRmtDataStream

//----------------------------------------------------------------------------
c_OutputTLS3001Rmt::c_OutputTLS3001Rmt(OM_OutputPortDefinition_t & OutputPortDefinition,
                                       c_OutputMgr::e_OutputProtocolType outputType) :
    c_OutputTLS3001 (OutputPortDefinition, outputType)
{
    // DEBUG_START;

    // DEBUG_V (String ("TLS3001_PIXEL_RMT_TICKS_BIT: 0x") + String (TLS3001_PIXEL_RMT_TICKS_BIT, HEX));
    OneBit.val  = ConvertIntensityToRmtDataStream[2].Translation.val;
    ZeroBit.val = ConvertIntensityToRmtDataStream[1].Translation.val;

    fsm_RMT_state_SendSync_imp.SetParent(this);
    fsm_RMT_state_SendReset_imp.SetParent(this);
    fsm_RMT_state_SendDataStart_imp.SetParent(this);
    fsm_RMT_state_SendData_imp.SetParent(this);
    fsm_RMT_state_SendDataIdle_imp.SetParent(this);

    fsm_RMT_state_SendDataIdle_imp.Init ();

    // DEBUG_V("RmtResetOneMsDelay");
    Rmt.SetBitDuration(NanoSecondsInAMilliSecond, RmtResetOneMsDelay, NumResetDelayBits);
    RmtResetOneMsDelay.level0 = 0;
    RmtResetOneMsDelay.level1 = 0;
    // DEBUG_V(String("NumResetDelayBits: ") + String(NumResetDelayBits));
    // DEBUG_V(String("duration0: ") + String(RmtResetOneMsDelay.duration0));
    // DEBUG_V(String("duration1: ") + String(RmtResetOneMsDelay.duration1));

    // DEBUG_END;

} // c_OutputTLS3001Rmt

//----------------------------------------------------------------------------
c_OutputTLS3001Rmt::~c_OutputTLS3001Rmt ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputTLS3001Rmt

//----------------------------------------------------------------------------
/* Use the current config to set up the output port
*/
void c_OutputTLS3001Rmt::Begin ()
{
    // DEBUG_START;

    c_OutputTLS3001::Begin ();
    HasBeenInitialized = true;

    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
bool c_OutputTLS3001Rmt::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputTLS3001::SetConfig (jsonConfig);

    uint32_t ifgNS = 1000; // (InterFrameGapInMicroSec * NanoSecondsInAMicroSecond);
    uint32_t ifgTicks = ifgNS / RMT_TickLengthNS;

    // Default is 100us * 3
    rmt_item32_t BitValue;
    // by default there are 6 rmt_item32_t instances replicated for the start of a frame.
    // 1 instances times 2 time periods per instance = 2
    BitValue.duration0 = ifgTicks / 2;
    BitValue.level0    = 0;
    BitValue.duration1 = ifgTicks / 2;
    BitValue.level1    = 0;

    c_OutputRmt::OutputRmtConfig_t OutputRmtConfig;
    OutputRmtConfig.RmtChannelId        = uint32_t(OutputPortDefinition.PortId);
    OutputRmtConfig.DataPin             = gpio_num_t(OutputPortDefinition.gpios.data);
    OutputRmtConfig.idle_level          = rmt_idle_level_t::RMT_IDLE_LEVEL_LOW;
    OutputRmtConfig.pPixelDataSource    = this;
    OutputRmtConfig.CitrdsArray         = ConvertIntensityToRmtDataStream;
    OutputRmtConfig.NumFrameStartBits   = 0;
    OutputRmtConfig.NumFrameStopBits    = 0;
    OutputRmtConfig.NumIdleBits         = 0;

    OutputRmtConfig.BitApi.UseLowLevelBitAPI = true;
    OutputRmtConfig.BitApi.arg               = this;
    OutputRmtConfig.BitApi.func              = ISR_GetNextBitToSendBase;

    // DEBUG_V();
    SetBitTimes();
    fsm_RMT_state_SendDataIdle_imp.Init ();
    Rmt.Begin(OutputRmtConfig, this);
    Rmt.ValidateBitXlatTable(ConvertIntensityToRmtDataStream);
    Rmt.SetIntensity2Rmt (BitValue, c_OutputRmt::RmtDataBitIdType_t::RMT_INTERFRAME_GAP_ID);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputTLS3001Rmt::SetBitTimes ()
{
    // DEBUG_START;

    double NumChannelsAvailable = double(GetNumOutputBufferChannelsServiced());
    double NumPixels            = NumChannelsAvailable / double(PIXEL_DEFAULT_INTENSITY_BYTES_PER_PIXEL);
    double BaudRateMhz          = double(TLS3001_PIXEL_DATA_RATE) / 1000000.0;
    // calculate the after sync delay based on bit rate and number of pixels on port
    // (Number of connected chips) ÷ (Communication baud rate in MHz) × 30
    double delayUs              = (NumPixels / BaudRateMhz) * 30;
    double delyNS               = NanoSecondsInAMicroSecond * delayUs;

    // DEBUG_V(String("NumChannelsAvailable: ") + String(NumChannelsAvailable));
    // DEBUG_V(String("           NumPixels: ") + String(NumPixels));
    // DEBUG_V(String("         BaudRateMhz: ") + String(BaudRateMhz));
    // DEBUG_V(String("             delayUs: ") + String(delayUs));
    // DEBUG_V(String("              delyNS: ") + String(delyNS));

    // DEBUG_V("RmtSyncIdle");
    Rmt.SetBitDuration(delyNS, RmtSyncIdle, NumSyncIdleBits);
    RmtSyncIdle.level0 = 0;
    RmtSyncIdle.level1 = 0;

    double BitsPerChannel = 13.0; // 1 zero + 12 data
    double SecPerBit = 1.0 / double(TLS3001_PIXEL_DATA_RATE);
    double NsPerBit = SecPerBit * NanoSecondsInASecond;
    double NsPerChannel = NsPerBit * BitsPerChannel;
    double FrameLenNs = NsPerChannel * NumChannelsAvailable;
    double MinFrameLenNs = 25 * NanoSecondsInAMilliSecond; // Min frame len is 25ms
    double DeltaFrameLenNs = RMT_TickLengthNS;

    if(FrameLenNs < MinFrameLenNs)
    {
        DeltaFrameLenNs = MinFrameLenNs - FrameLenNs;
    }
    // DEBUG_V(String(" BitsPerChannel: ") + String(BitsPerChannel));
    // DEBUG_V(String("      SecPerBit: ") + String(SecPerBit));
    // DEBUG_V(String("       NsPerBit: ") + String(NsPerBit));
    // DEBUG_V(String("   NsPerChannel: ") + String(NsPerChannel));
    // DEBUG_V(String("     FrameLenNs: ") + String(FrameLenNs));
    // DEBUG_V(String("  MinFrameLenNs: ") + String(MinFrameLenNs));
    // DEBUG_V(String("DeltaFrameLenNs: ") + String(DeltaFrameLenNs));

    // DEBUG_V("SendDataIfg");
    Rmt.SetBitDuration(int(DeltaFrameLenNs), SendDataIfg, NumIfgBitsPerFrame);
    SendDataIfg.level0 = 0;
    SendDataIfg.level1 = 0;

    // DEBUG_END;
} // SetBitTimes

//----------------------------------------------------------------------------
void c_OutputTLS3001Rmt::SetOutputBufferSize (uint32_t NumChannelsAvailable)
{
    // DEBUG_START;

    c_OutputTLS3001::SetOutputBufferSize (NumChannelsAvailable);
    // Rmt.SetMinFrameDurationInUs (FrameDurationInMicroSec);
    // DEBUG_V(String("    TLS3001_PIXEL_DATA_RATE: ") + String(TLS3001_PIXEL_DATA_RATE));

    SetBitTimes();

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
void c_OutputTLS3001Rmt::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    // // DEBUG_START;
    c_OutputTLS3001::GetStatus (jsonStatus);
    Rmt.GetStatus (jsonStatus);
#ifdef USE_RMT_DEBUG_COUNTERS
    // jsonStatus[F("Can Refresh")] = CanRefresh;
    // jsonStatus[F("Cannot Refresh")] = CannotRefresh;
    jsonStatus[F("FrameDurationInMicroSec")] = FrameDurationInMicroSec;
    jsonStatus[F("FrameStartTimeInMicroSec")] = FrameStartTimeInMicroSec;
    uint32_t now = micros();
    jsonStatus[F("Now")] = now;
    jsonStatus[F("FrameStartDelta")] = now - FrameStartTimeInMicroSec;
#endif // def USE_RMT_DEBUG_COUNTERS

    #ifdef USE_TLS3001RMT_COUNTERS
    JsonObject CounterStatus = jsonStatus.createNestedObject("TLS3001");
    CounterStatus[F("MinFrameLenMs")] = TLS3001RMTCounters.MinFrameLenMs;
    CounterStatus[F("MaxFrameLenMs")] = TLS3001RMTCounters.MaxFrameLenMs;
    CounterStatus[F("MinPollTimeMs")] = TLS3001RMTCounters.MinPollTimeMs;
    CounterStatus[F("MaxPollTimeMs")] = TLS3001RMTCounters.MaxPollTimeMs;
    CounterStatus[F("PollCounter")]   = TLS3001RMTCounters.PollCounter;
    CounterStatus[F("NoGpio")]        = TLS3001RMTCounters.NoGpio;
    CounterStatus[F("FramesStarted")] = TLS3001RMTCounters.FrameStarted;
    CounterStatus[F("FrameErrorNotFinished")] = TLS3001RMTCounters.FrameErrorNotFinished;
    CounterStatus[F("TooSoon")]       = TLS3001RMTCounters.TooSoon;

    #endif // def USE_TLS3001RMT_COUNTERS

    // // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputTLS3001Rmt::Poll ()
{
    // DEBUG_START;

    // DEBUG_END;
    return ActualFrameDurationMicroSec;

} // Poll

//----------------------------------------------------------------------------
bool c_OutputTLS3001Rmt::RmtPoll ()
{
    // DEBUG_START;
    bool Response = false;
    do // Once
    {
        INCREMENT_TLS3001_COUNTER(PollCounter);

        if (gpio_num_t(-1) == OutputPortDefinition.gpios.data)
        {
            INCREMENT_TLS3001_COUNTER(NoGpio);
            break;
        }

        if(!canRefresh())
        {
            INCREMENT_TLS3001_COUNTER(TooSoon);
            // too soon to start another frame
            break;
        }

        // TLS3001RMTCounters.MinPollTimeMs = min((uint32_t(micros()) - FrameStartTimeInMicroSec) / MicroSecondsInAmilliSecond, TLS3001RMTCounters.MinPollTimeMs);
        // TLS3001RMTCounters.MaxPollTimeMs = max((uint32_t(micros()) - FrameStartTimeInMicroSec) / MicroSecondsInAmilliSecond, TLS3001RMTCounters.MaxPollTimeMs);

        if(false == SendingData)
        {
            // DEBUG_V(String("frame started on ") + String(OutputPortDefinition.gpios.data));
            INCREMENT_TLS3001_COUNTER(FrameStarted);

            if(FrameResetIsNeeded())
            {
                fsm_RMT_state_SendReset_imp.Init();
            }
            else
            {
                fsm_RMT_state_SendDataStart_imp.Init();
            }
            Response = Rmt.StartNewFrame ();
        }
        else
        {
            INCREMENT_TLS3001_COUNTER(FrameErrorNotFinished);
            fsm_RMT_state_SendReset_imp.Init();
            Response = Rmt.StartNewFrame();
        }

        #ifdef DEBUG_RMT_XLAT_ISSUES
        Rmt.ValidateBitXlatTable(ConvertIntensityToRmtDataStream);
        #endif // def DEBUG_RMT_XLAT_ISSUES

        // DEBUG_V();

    } while (false);

    // DEBUG_END;
    return Response;

} // Poll

//----------------------------------------------------------------------------
void c_OutputTLS3001Rmt::PauseOutput (bool State)
{
    // DEBUG_START;

    c_OutputTLS3001::PauseOutput(State);
    Rmt.PauseOutput(State);
    if(State)
    {
        fsm_RMT_state_SendDataIdle_imp.Init();
    }

    // DEBUG_END;
} // PauseOutput

//----------------------------------------------------------------------------
bool IRAM_ATTR c_OutputTLS3001Rmt::ISR_GetNextBitToSend (uint32_t &DataToSend)
{
    bool Response = false;
    switch(CurrentFsmState)
    {
        case OutputTLS3001RmtFsmStates_t::TLS3001Sync:
        {
            Response = fsm_RMT_state_SendSync_imp.ISR_GetNextBitToSend(DataToSend);
            break;
        }
        case OutputTLS3001RmtFsmStates_t::TLS3001DataStart:
        {
            Response = fsm_RMT_state_SendDataStart_imp.ISR_GetNextBitToSend(DataToSend);
            break;
        }
        case OutputTLS3001RmtFsmStates_t::TLS3001DataSend:
        {
            Response = fsm_RMT_state_SendData_imp.ISR_GetNextBitToSend(DataToSend);
            break;
        }
        case OutputTLS3001RmtFsmStates_t::TLS3001DataIdle:
        {
            Response = fsm_RMT_state_SendDataIdle_imp.ISR_GetNextBitToSend(DataToSend);
            break;
        }
        default:
        case OutputTLS3001RmtFsmStates_t::TLS3001Reset:
        {
            Response = fsm_RMT_state_SendReset_imp.ISR_GetNextBitToSend(DataToSend);
            break;
        }
    };
    return Response;
} // ISR_GetNextIntensityToSend

//----------------------------------------------------------------------------
// FSM
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void IRAM_ATTR fsm_RMT_state_SendReset::Init ()
{
    // DEBUG_START;

    // Serial.print('b');

    Parent->CurrentFsmState = OutputTLS3001RmtFsmStates_t::TLS3001Reset;
    Parent->SendingData = true;
    Parent->ResetFrameCounter();

    NumberOfOneBitsToSend = 15;
    CommandCodeMask = 0b1000; // reset
    NumberOfIdleBitsToSend = Parent->NumResetDelayBits;

    // DEBUG_END;
} // fsm_RMT_state_SendReset

//----------------------------------------------------------------------------
bool IRAM_ATTR fsm_RMT_state_SendReset::ISR_GetNextBitToSend (uint32_t &DataToSend)
{
    // Serial.print('2');
    if(NumberOfOneBitsToSend)
    {
        --NumberOfOneBitsToSend;
        DataToSend = Parent->OneBit.val;
    }
    else if(CommandCodeMask)
    {
        DataToSend = (CommandCode & CommandCodeMask) ? Parent->OneBit.val : Parent->ZeroBit.val;
        CommandCodeMask = CommandCodeMask >> 1;
    }
    else if(NumberOfIdleBitsToSend)
    {
        --NumberOfIdleBitsToSend;
        DataToSend = Parent->RmtResetOneMsDelay.val;
        if(0 == NumberOfIdleBitsToSend)
        {
            Parent->fsm_RMT_state_SendSync_imp.Init();
        }
    }
    else
    {
        Parent->fsm_RMT_state_SendSync_imp.Init();
        Parent->fsm_RMT_state_SendSync_imp.ISR_GetNextBitToSend(DataToSend);
    }

    return true;
} // fsm_RMT_state_SendReset

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void IRAM_ATTR fsm_RMT_state_SendSync::Init ()
{
    // DEBUG_START;

    // Serial.print('c');

    Parent->CurrentFsmState = OutputTLS3001RmtFsmStates_t::TLS3001Sync;
    Parent->SendingData = true;

    NumberOfOneBitsToSend = 15;
    CommandCodeMask = 0b1000;
    NumberOfZeroBitsToSend = 15;
    NumberOfIdleBitsToSend = Parent->NumSyncIdleBits;

    // DEBUG_END;
} // fsm_RMT_state_SendSync

//----------------------------------------------------------------------------
bool IRAM_ATTR fsm_RMT_state_SendSync::ISR_GetNextBitToSend (uint32_t &DataToSend)
{
    // Serial.print('3');
    if(NumberOfOneBitsToSend)
    {
        --NumberOfOneBitsToSend;
        DataToSend = Parent->OneBit.val;
    }
    else if(CommandCodeMask)
    {
        DataToSend = (CommandCode & CommandCodeMask) ? Parent->OneBit.val : Parent->ZeroBit.val;
        CommandCodeMask = CommandCodeMask >> 1;
    }
    else if(NumberOfZeroBitsToSend)
    {
        --NumberOfZeroBitsToSend;
        DataToSend = Parent->ZeroBit.val;
    }
    else if (NumberOfIdleBitsToSend)
    {
        DataToSend = Parent->RmtSyncIdle.val;
        -- NumberOfIdleBitsToSend;
        if(!NumberOfIdleBitsToSend)
        {
            // no more bits to send. Go to the data state
            Parent->fsm_RMT_state_SendDataStart_imp.Init();
        }
    }

    return true;
} // fsm_RMT_state_SendSync

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void IRAM_ATTR fsm_RMT_state_SendDataStart::Init ()
{
    // DEBUG_START;
    // Serial.print('d');

    Parent->CurrentFsmState = OutputTLS3001RmtFsmStates_t::TLS3001DataStart;
    Parent->SendingData = true;
    Parent->IncrementFrameCounter();

    NumberOfOneBitsToSend = 15;
    CommandCodeMask = 0b1000;

    // DEBUG_END;
} // fsm_RMT_state_SendStart

//----------------------------------------------------------------------------
bool IRAM_ATTR fsm_RMT_state_SendDataStart::ISR_GetNextBitToSend (uint32_t &DataToSend)
{
    // Serial.print('4');
    if(NumberOfOneBitsToSend)
    {
        --NumberOfOneBitsToSend;
        DataToSend = Parent->OneBit.val;
    }
    else if(CommandCodeMask)
    {
        DataToSend = (CommandCode & CommandCodeMask) ? Parent->OneBit.val : Parent->ZeroBit.val;
        CommandCodeMask = CommandCodeMask >> 1;

        if(0 == CommandCodeMask)
        {
            // no more bits to send. Go to the data state and send the first zero bit.
            Parent->fsm_RMT_state_SendData_imp.Init();
        }
    }

    return true;
} // fsm_RMT_state_SendStart

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void IRAM_ATTR fsm_RMT_state_SendData::Init ()
{
    // DEBUG_START;
    // Serial.print('e');

    Parent->CurrentFsmState = OutputTLS3001RmtFsmStates_t::TLS3001DataSend;

    ISR_RefreshData();
    NumPostDataBitsToSend = Parent->NumIfgBitsPerFrame;
    NumPostDataBitsToSend = 0;

    // DEBUG_END;
} // fsm_RMT_state_SendData

//----------------------------------------------------------------------------
bool IRAM_ATTR fsm_RMT_state_SendData::ISR_GetNextBitToSend (uint32_t &DataToSend)
{
    bool Response = true;
    // Serial.print('5');
    // starting zero bit
    if(DataPatternMask)
    {
        DataToSend = (DataPattern & DataPatternMask) ? Parent->OneBit.val : Parent->ZeroBit.val;
        DataPatternMask = DataPatternMask >> 1;

        // is there more data to send?
        if(0 == DataPatternMask)
        {
            ISR_RefreshData();
        }
    }
    else if(NumPostDataBitsToSend)
    {
        --NumPostDataBitsToSend;
        DataToSend = Parent->SendDataIfg.val;
    }
    else
    {
        Parent->fsm_RMT_state_SendDataIdle_imp.Init();
        Response = false;
    }

    return Response;
} // fsm_RMT_state_SendData

//----------------------------------------------------------------------------
bool IRAM_ATTR fsm_RMT_state_SendData::ISR_RefreshData ()
{
    MoreDataAvailable = Parent->ISR_MoreDataToSend();

    if(MoreDataAvailable)
    {
        uint32_t NewData = 0;
        Parent->c_OutputTLS3001::ISR_GetNextIntensityToSend(NewData);
        // NewData = 0xff;

        DataPatternMask = 0b1000000000000;
        // convert 8 bit data to 12 bit data
        DataPattern = (NewData * 4095) / 255;
        // DataPattern = 0b111111111111;
        DataPattern = DataPattern & ~DataPatternMask;
    }
    else
    {
        DataPatternMask = 0;
    }

    return MoreDataAvailable;
} // ISR_RefreshData

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void IRAM_ATTR fsm_RMT_state_SendDataIdle::Init ()
{
    // DEBUG_START;
    // Serial.print('f');

    Parent->CurrentFsmState = OutputTLS3001RmtFsmStates_t::TLS3001DataIdle;
    Parent->SendingData = false;
    // DEBUG_END;
} // fsm_RMT_state_SendData

//----------------------------------------------------------------------------
bool IRAM_ATTR fsm_RMT_state_SendDataIdle::ISR_GetNextBitToSend (uint32_t &DataToSend)
{
    // Serial.print('6');
    return false;
} // fsm_RMT_state_SendDataIdle

#endif // defined(SUPPORT_OutputProtocol_TLS3001) && defined (ARDUINO_ARCH_ESP32)
