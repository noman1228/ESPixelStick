#pragma once
/*
* OutputRmt.hpp - RMT driver code for ESPixelStick RMT Channel
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
#ifdef ARDUINO_ARCH_ESP32
#include <hal/rmt_ll.h>
#include "OutputPixel.hpp"
#include "OutputSerial.hpp"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    #include <driver/rmt_tx.h>
    #include "driver/rmt_common.h"
    #include "driver/rmt_encoder.h"
    #define rmt_item32_t rmt_symbol_word_t
    typedef enum
    {
        RMT_IDLE_LEVEL_LOW,  /*!< RMT TX idle level: low Level */
        RMT_IDLE_LEVEL_HIGH, /*!< RMT TX idle level: high Level */
        RMT_IDLE_LEVEL_MAX,
    } rmt_idle_level_t;
#else
    #include <driver/rmt.h>
#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)

class c_OutputRmt
{
public:
    enum RmtDataBitIdType_t
    {
        RMT_DATA_BIT_ZERO_ID = 0,   // 0 UART 00
        RMT_DATA_BIT_ONE_ID,        // 1 UART 01
        RMT_DATA_BIT_TWO_ID,        // 2 UART 10
        RMT_DATA_BIT_THREE_ID,      // 3 UART 11
        RMT_INTERFRAME_GAP_ID,      // 4 UART Break / MAB
        RMT_STARTBIT_ID,            // 5
        RMT_STOPBIT_ID,             // 6 UART Stop/start bit
        RMT_END_OF_FRAME,           // 7
        RMT_LIST_END,               // 8 This must be last
        RMT_INVALID_VALUE,
        RMT_NUM_BIT_TYPES = RMT_LIST_END,
        RMT_STOP_START_BIT_ID = RMT_STOPBIT_ID,
    };

    struct ConvertIntensityToRmtDataStreamEntry_t
    {
        rmt_item32_t        Translation;
        RmtDataBitIdType_t  Id;
    };
    typedef ConvertIntensityToRmtDataStreamEntry_t CitrdsArray_t;
    struct LowLevelBitApi
    {
        void * arg = nullptr;
        bool (*func)(void * arg, uint32_t &DataToSend) = nullptr;
        bool UseLowLevelBitAPI = false;
    };

    struct OutputRmtConfig_t
    {
        uint32_t            RmtChannelId           = uint32_t(-1);
        gpio_num_t          DataPin                = gpio_num_t(-1);
        rmt_idle_level_t    idle_level             = rmt_idle_level_t::RMT_IDLE_LEVEL_LOW;
        uint32_t            IntensityDataWidth     = 8;
        bool                SendInterIntensityBits = false;
        bool                SendEndOfFrameBits     = false;
        uint8_t             NumFrameStartBits      = 1;
        uint8_t             NumFrameStopBits       = 1;
        uint8_t             NumIdleBits            = 6;
        enum DataDirection_t
        {
            MSB2LSB = 0,
            LSB2MSB = 1
        };
        DataDirection_t     DataDirection          = DataDirection_t::MSB2LSB;
        const CitrdsArray_t *CitrdsArray           = nullptr;

        c_OutputPixel  *pPixelDataSource           = nullptr;
        LowLevelBitApi BitApi;
		#if defined(SUPPORT_OutputProtocol_FireGod) || defined(SUPPORT_OutputProtocol_DMX) || defined(SUPPORT_OutputProtocol_Serial) || defined(SUPPORT_OutputProtocol_Renard)
        c_OutputSerial *pSerialDataSource = nullptr;
		#endif // defined(SUPPORT_OutputProtocol_FireGod) || defined(SUPPORT_OutputProtocol_DMX) || defined(SUPPORT_OutputProtocol_Serial) || defined(SUPPORT_OutputProtocol_Renard)
    };

    struct isrTxFlags_t
    {
        uint32_t End = 0;
        uint32_t Err = 0;
        uint32_t Thres = 0;
    };

private:
#ifdef CONFIG_IDF_TARGET_ESP32S3
#define MAX_NUM_RMT_CHANNELS 4
#else
#define MAX_NUM_RMT_CHANNELS     8
#endif // def CONFIG_IDF_TARGET_ESP32S3

#define RMT_INT_BIT         uint32_t(1 << uint32_t (OutputRmtConfig.RmtChannelId))
#define InterrupsAreEnabled (0 != (RMT.int_ena.val & RMT_INT_BIT))

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    #define _NUM_RMT_SLOTS 48
#else
    #define _NUM_RMT_SLOTS (sizeof(RMTMEM.chan[0].data32) / sizeof(RMTMEM.chan[0].data32[0]))
#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)


    const uint32_t      NUM_RMT_SLOTS = _NUM_RMT_SLOTS;
    OutputRmtConfig_t   OutputRmtConfig;

    rmt_item32_t        Intensity2Rmt[RmtDataBitIdType_t::RMT_LIST_END];
    bool                OutputIsPaused   = false;

    uint32_t            NumRmtSlotsPerIntensityValue    = 8;
    uint32_t            NumRmtSlotOverruns              = 0;
    const uint32_t      MaxNumRmtSlotsPerInterrupt      = (_NUM_RMT_SLOTS/2);

    #define             NumSendBufferSlots 64
    rmt_item32_t        SendBuffer[NumSendBufferSlots];
    uint32_t            RmtBufferWriteIndex         = 0;
    uint32_t            SendBufferWriteIndex        = 0;
    uint32_t            SendBufferReadIndex         = 0;
    uint32_t            NumUsedEntriesInSendBuffer  = 0;

    uint32_t            TxIntensityDataStartingMask = 0x80;

    inline void IRAM_ATTR ISR_TransferIntensityDataToRMT (uint32_t NumEntriesToTransfer);
    inline void IRAM_ATTR ISR_CreateIntensityData ();
    inline void IRAM_ATTR ISR_WriteToBuffer(uint32_t value);
    inline bool IRAM_ATTR ISR_MoreDataToSend();
    inline bool IRAM_ATTR ISR_GetNextIntensityToSend(uint32_t &DataToSend);
    inline void StartNewDataFrame();
    inline void ResetRmtBlockPointers();

#ifndef HasBeenInitialized
    bool HasBeenInitialized = false;
#endif // ndef HasBeenInitialized

    TaskHandle_t SendIntensityDataTaskHandle = NULL;

public:
    c_OutputRmt ();
    virtual ~c_OutputRmt ();

    void Begin              (OutputRmtConfig_t config, c_OutputCommon * pParent);
    bool StartNewFrame      ();
    bool StartNextFrame     () { return ((nullptr != pParent) & (!OutputIsPaused)) ? pParent->RmtPoll() : false; }
    void GetStatus          (ArduinoJson::JsonObject& jsonStatus);
    void PauseOutput        (bool State);
    void GetDriverName      (String &value)  { value = CN_RMT; }
    void SetBitDuration     (double BitLenNs, rmt_item32_t & OutputBit, uint32_t & OutputNumBits);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define RMT_TX_BITS RMT_LL_EVENT_TX_THRES(OutputRmtConfig.RmtChannelId) | \
                    RMT_LL_EVENT_TX_DONE(OutputRmtConfig.RmtChannelId)  | \
                    RMT_LL_EVENT_TX_ERROR(OutputRmtConfig.RmtChannelId)
#endif // ndef rmt_ll_clear_tx_thres_interrupt

__attribute__((always_inline))
inline void IRAM_ATTR DisableRmtInterrupts()
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    rmt_ll_enable_interrupt(&RMT, RMT_TX_BITS, false);
#else
    rmt_ll_enable_tx_thres_interrupt(&RMT, OutputRmtConfig.RmtChannelId, false);
    rmt_ll_enable_tx_end_interrupt(&RMT, OutputRmtConfig.RmtChannelId, false);
    rmt_ll_enable_tx_err_interrupt(&RMT, OutputRmtConfig.RmtChannelId, false);
#endif //  ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)

    ClearRmtInterrupts();
}

__attribute__((always_inline))
inline void IRAM_ATTR EnableRmtInterrupts()
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    rmt_ll_enable_interrupt(&RMT, RMT_TX_BITS, true);
#else
    rmt_ll_enable_tx_thres_interrupt(&RMT, OutputRmtConfig.RmtChannelId, true);
    rmt_ll_enable_tx_end_interrupt(&RMT, OutputRmtConfig.RmtChannelId, true);
    rmt_ll_enable_tx_err_interrupt(&RMT, OutputRmtConfig.RmtChannelId, true);
#endif // ndef rmt_ll_clear_tx_thres_interrupt
}

__attribute__((always_inline))
inline void IRAM_ATTR ClearRmtInterrupts()
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    rmt_ll_clear_interrupt_status(&RMT, RMT_TX_BITS);
#else
    rmt_ll_clear_tx_thres_interrupt(&RMT, OutputRmtConfig.RmtChannelId);
    rmt_ll_clear_tx_end_interrupt(&RMT, OutputRmtConfig.RmtChannelId);
    rmt_ll_clear_tx_err_interrupt(&RMT, OutputRmtConfig.RmtChannelId);
#endif // ndef rmt_ll_clear_tx_thres_interrupt
}

    bool DriverIsSendingIntensityData() {return 0 != InterrupsAreEnabled;}

#define RMT_ClockRate       80000000.0
#define RMT_Clock_Divisor   2.0
#define RMT_TickLengthNS    float ( (1/ (RMT_ClockRate/RMT_Clock_Divisor)) * float(NanoSecondsInASecond))

    void UpdateBitXlatTable(const CitrdsArray_t * CitrdsArray);
    bool ValidateBitXlatTable(const CitrdsArray_t * CitrdsArray);
    void SetIntensity2Rmt (rmt_item32_t NewValue, RmtDataBitIdType_t ID) { Intensity2Rmt[ID] = NewValue; }

    bool ThereIsDataToSend = false;

    void IRAM_ATTR ISR_Handler (isrTxFlags_t isrFlags);
    c_OutputCommon * pParent = nullptr;

// #define USE_RMT_DEBUG_COUNTERS
#ifdef USE_RMT_DEBUG_COUNTERS
// #define IncludeBufferData
   // debug counters
   uint32_t DataCallbackCounter = 0;
   uint32_t DataTaskcounter = 0;
   uint32_t ISRcounter = 0;
   uint32_t FrameStartCounter = 0;
   uint32_t SendBlockIsrCounter = 0;
   uint32_t RanOutOfData = 0;
   uint32_t UnknownISRcounter = 0;
   uint32_t IntTxEndIsrCounter = 0;
   uint32_t IntTxThrIsrCounter = 0;
   uint32_t RxIsr = 0;
   uint32_t ErrorIsr = 0;
   uint32_t IntensityValuesSent = 0;
   uint32_t IntensityBitsSent = 0;
   uint32_t IntensityValuesSentLastFrame = 0;
   uint32_t IntensityBitsSentLastFrame = 0;
   uint32_t IncompleteFrame = 0;
   uint32_t BitTypeCounters[RmtDataBitIdType_t::RMT_NUM_BIT_TYPES];
   uint32_t RmtEntriesTransfered = 0;
   uint32_t RmtXmtFills = 0;
   uint32_t RmtWhiteDetected = 0;
   uint32_t FailedToSendAllData = 0;

#define RMT_DEBUG_COUNTER(p) p

#else

#define RMT_DEBUG_COUNTER(p)

#endif // def USE_RMT_DEBUG_COUNTERS

};
#endif // def #ifdef ARDUINO_ARCH_ESP32