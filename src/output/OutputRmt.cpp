
#include "ESPixelStick.h"
#ifdef ARDUINO_ARCH_ESP32
#include "output/OutputRmt.hpp"

static void IRAM_ATTR rmt_intr_handler(void *param);
static rmt_isr_handle_t RMT_intr_handle = NULL;
static c_OutputRmt *rmt_isr_ThisPtrs[MAX_NUM_RMT_CHANNELS];
static bool InIsr = false;

#ifdef USE_RMT_DEBUG_COUNTERS
static uint32_t RawIsrCounter = 0;
#endif

static TaskHandle_t SendFrameTaskHandle = NULL;
static BaseType_t xHigherPriorityTaskWoken = pdTRUE;
static uint32_t FrameCompletes = 0;
static uint32_t FrameTimeouts = 0;

void RMT_Task(void *arg) {

    while (1) {

        for (c_OutputRmt *pRmt : rmt_isr_ThisPtrs) {

            if (nullptr != pRmt) {

                if (pRmt->StartNextFrame()) {

                    uint32_t NotificationValue = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
                    if (1 == NotificationValue) {

                        ++FrameCompletes;
                    } else {
                        ++FrameTimeouts;
                    }
                }
            }
        }
    }
}

c_OutputRmt::c_OutputRmt() {

    memset((void *)&Intensity2Rmt[0], 0x00, sizeof(Intensity2Rmt));
    memset((void *)&SendBuffer[0], 0x00, sizeof(SendBuffer));

#ifdef USE_RMT_DEBUG_COUNTERS
    memset((void *)&BitTypeCounters[0], 0x00, sizeof(BitTypeCounters));
#endif
}

c_OutputRmt::~c_OutputRmt() {

    if (HasBeenInitialized) {
        logcon(F("Shutting down an RMT channel requires a reboot"));
        RequestReboot(100000);

        DisableRmtInterrupts;
        ClearRmtInterrupts;

        rmt_isr_ThisPtrs[OutputRmtConfig.RmtChannelId] = (c_OutputRmt *)nullptr;
        RMT.data_ch[OutputRmtConfig.RmtChannelId] = 0x00;
    }
}

static void IRAM_ATTR rmt_intr_handler(void *param) {
    RMT_DEBUG_COUNTER(RawIsrCounter++);
#ifdef DEBUG_GPIO

#endif
    if (!InIsr) {
        InIsr = true;

        uint32_t isrFlags = RMT.int_st.val;
        RMT.int_clr.val = uint32_t(-1);

        while (0 != isrFlags) {
            for (auto CurrentRmtChanThisPtr : rmt_isr_ThisPtrs) {
                if (nullptr != CurrentRmtChanThisPtr) {
                    CurrentRmtChanThisPtr->ISR_Handler(isrFlags);
                }
            }

            isrFlags = (volatile uint32_t)RMT.int_st.val;
            RMT.int_clr.val = uint32_t(-1);
        }
        InIsr = false;
    }
#ifdef DEBUG_GPIO

#endif
}

void c_OutputRmt::Begin(OutputRmtConfig_t config, c_OutputCommon *_pParent) {

    do {
        OutputRmtConfig = config;
#if defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
        if ((nullptr == OutputRmtConfig.pPixelDataSource) && (nullptr == OutputRmtConfig.pSerialDataSource))
#else
        if (nullptr == OutputRmtConfig.pPixelDataSource)
#endif

        {
            LOG_PORT.println(F("Invalid RMT configuration parameters. Rebooting"));
            RequestReboot(10000);
            break;
        }

        NumRmtSlotsPerIntensityValue = OutputRmtConfig.IntensityDataWidth + ((OutputRmtConfig.SendInterIntensityBits) ? 1 : 0);
        if (OutputRmtConfig_t::DataDirection_t::MSB2LSB == OutputRmtConfig.DataDirection) {
            TxIntensityDataStartingMask = 1 << (OutputRmtConfig.IntensityDataWidth - 1);
        } else {
            TxIntensityDataStartingMask = 1;
        }

        rmt_config_t RmtConfig;
        RmtConfig.rmt_mode = rmt_mode_t::RMT_MODE_TX;
        RmtConfig.channel = OutputRmtConfig.RmtChannelId;
        RmtConfig.gpio_num = OutputRmtConfig.DataPin;
        RmtConfig.clk_div = RMT_Clock_Divisor;
        RmtConfig.mem_block_num = rmt_reserve_memsize_t::RMT_MEM_64;

        RmtConfig.tx_config.carrier_freq_hz = uint32_t(10);
        RmtConfig.tx_config.carrier_level = rmt_carrier_level_t::RMT_CARRIER_LEVEL_LOW;
        RmtConfig.tx_config.carrier_duty_percent = 50;
        RmtConfig.tx_config.idle_level = OutputRmtConfig.idle_level;
        RmtConfig.tx_config.carrier_en = false;
        RmtConfig.tx_config.loop_en = true;
        RmtConfig.tx_config.idle_output_en = true;

        ResetGpio(OutputRmtConfig.DataPin);
        ESP_ERROR_CHECK(rmt_config(&RmtConfig));

        if (NULL == RMT_intr_handle) {

            for (auto &currentThisPtr : rmt_isr_ThisPtrs) {
                currentThisPtr = nullptr;
            }
            ESP_ERROR_CHECK(rmt_isr_register(rmt_intr_handler, this, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_SHARED, &RMT_intr_handle));
        }

        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.tx_conti_mode = 0;
        RMT.apb_conf.mem_tx_wrap_en = 1;

        ISR_ResetRmtBlockPointers();
        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.apb_mem_rst = 0;
        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.mem_owner = RMT_MEM_OWNER_TX;

        RMT.apb_conf.fifo_mask = RMT_DATA_MODE_MEM;
        memset((void *)&RMTMEM.chan[OutputRmtConfig.RmtChannelId].data32[0], 0x0, sizeof(RMTMEM.chan[0].data32));

        if (nullptr != OutputRmtConfig.CitrdsArray) {

            const ConvertIntensityToRmtDataStreamEntry_t *CurrentTranslation = OutputRmtConfig.CitrdsArray;
            while (CurrentTranslation->Id != RmtDataBitIdType_t::RMT_LIST_END) {

                SetIntensity2Rmt(CurrentTranslation->Translation, CurrentTranslation->Id);
                CurrentTranslation++;
            }
        }

        if (!SendFrameTaskHandle) {

            xTaskCreatePinnedToCore(RMT_Task, "RMT_Task", 4096, NULL, 5, &SendFrameTaskHandle, 0);
            vTaskPrioritySet(SendFrameTaskHandle, 5);
        }

        pParent = _pParent;
        rmt_isr_ThisPtrs[OutputRmtConfig.RmtChannelId] = this;

        ResetGpio(OutputRmtConfig.DataPin);
        rmt_set_gpio(OutputRmtConfig.RmtChannelId, RMT_MODE_TX, OutputRmtConfig.DataPin, false);

        HasBeenInitialized = true;
    } while (false);
}

void c_OutputRmt::GetStatus(ArduinoJson::JsonObject &jsonStatus) {

    jsonStatus[F("NumRmtSlotOverruns")] = NumRmtSlotOverruns;
#ifdef USE_RMT_DEBUG_COUNTERS
    jsonStatus[F("OutputIsPaused")] = OutputIsPaused;
    JsonObject debugStatus = jsonStatus["RMT Debug"].to<JsonObject>();
    debugStatus["RmtChannelId"] = OutputRmtConfig.RmtChannelId;
    debugStatus["GPIO"] = OutputRmtConfig.DataPin;
    debugStatus["conf0"] = String(RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf0.val, HEX);
    debugStatus["conf1"] = String(RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.val, HEX);
    debugStatus["tx_lim_ch"] = String(RMT.tx_lim_ch[OutputRmtConfig.RmtChannelId].limit);

    debugStatus["ErrorIsr"] = ErrorIsr;
    debugStatus["FrameCompletes"] = String(FrameCompletes);
    debugStatus["FrameStartCounter"] = FrameStartCounter;
    debugStatus["FrameTimeouts"] = String(FrameTimeouts);
    debugStatus["FailedToSendAllData"] = String(FailedToSendAllData);
    debugStatus["IncompleteFrame"] = IncompleteFrame;
    debugStatus["IntensityValuesSent"] = IntensityValuesSent;
    debugStatus["IntensityValuesSentLastFrame"] = IntensityValuesSentLastFrame;
    debugStatus["IntensityBitsSent"] = IntensityBitsSent;
    debugStatus["IntensityBitsSentLastFrame"] = IntensityBitsSentLastFrame;
    debugStatus["IntTxEndIsrCounter"] = IntTxEndIsrCounter;
    debugStatus["IntTxThrIsrCounter"] = IntTxThrIsrCounter;
    debugStatus["ISRcounter"] = ISRcounter;
    debugStatus["NumIdleBits"] = OutputRmtConfig.NumIdleBits;
    debugStatus["NumFrameStartBits"] = OutputRmtConfig.NumFrameStartBits;
    debugStatus["NumFrameStopBits"] = OutputRmtConfig.NumFrameStopBits;
    debugStatus["NumRmtSlotsPerIntensityValue"] = NumRmtSlotsPerIntensityValue;
    debugStatus["OneBitValue"] = String(Intensity2Rmt[RmtDataBitIdType_t::RMT_DATA_BIT_ONE_ID].val, HEX);
    debugStatus["RanOutOfData"] = RanOutOfData;
    uint32_t temp = RMT.int_ena.val;
    debugStatus["Raw int_ena"] = "0x" + String(temp, HEX);
    temp = RMT.int_st.val;
    debugStatus["Raw int_st"] = "0x" + String(temp, HEX);
    debugStatus["RawIsrCounter"] = RawIsrCounter;
    debugStatus["RMT_INT_TX_END_BIT"] = "0x" + String(RMT_INT_TX_END_BIT, HEX);
    debugStatus["RMT_INT_THR_EVNT_BIT"] = "0x" + String(RMT_INT_THR_EVNT_BIT, HEX);
    debugStatus["RmtEntriesTransfered"] = RmtEntriesTransfered;
    debugStatus["RmtWhiteDetected"] = String(RmtWhiteDetected);
    debugStatus["RmtXmtFills"] = RmtXmtFills;
    debugStatus["RxIsr"] = RxIsr;
    debugStatus["SendBlockIsrCounter"] = SendBlockIsrCounter;
    debugStatus["SendInterIntensityBits"] = OutputRmtConfig.SendInterIntensityBits;
    debugStatus["SendEndOfFrameBits"] = OutputRmtConfig.SendEndOfFrameBits;
    debugStatus["TX END int_ena"] = "0x" + String(temp & (RMT_INT_TX_END_BIT), HEX);
    debugStatus["TX END int_st"] = "0x" + String(temp & (RMT_INT_TX_END_BIT), HEX);
    debugStatus["TX THRSH int_ena"] = "0x" + String(temp & (RMT_INT_THR_EVNT_BIT), HEX);
    debugStatus["TX THRSH int_st"] = "0x" + String(temp & (RMT_INT_THR_EVNT_BIT), HEX);
    debugStatus["UnknownISRcounter"] = UnknownISRcounter;
    debugStatus["ZeroBitValue"] = String(Intensity2Rmt[RmtDataBitIdType_t::RMT_DATA_BIT_ZERO_ID].val, HEX);

#ifdef IncludeBufferData
    {
        uint32_t index = 0;
        for (auto CurrentCounter : BitTypeCounters) {
            break;
            debugStatus[String("RMT TYPE used Counter ") + String(index++)] = String(CurrentCounter);
        }
    }
    {
        uint32_t index = 0;
        uint32_t *CurrentPointer = (uint32_t *)const_cast<rmt_item32_t *>(&SendBuffer[0]);

        for (index = 0; index < 2; index++) {
            uint32_t data = CurrentPointer[index];
            debugStatus[String("Buffer Data ") + String(index)] = String(data, HEX);
        }
    }
    {
        uint32_t index = 0;
        uint32_t *CurrentPointer = (uint32_t *)const_cast<rmt_item32_t *>(&RMTMEM.chan[OutputRmtConfig.RmtChannelId].data32[0]);

        for (index = 0; index < 2; index++) {
            uint32_t data = CurrentPointer[index];
            debugStatus[String("RMT Data ") + String(index)] = String(data, HEX);
        }
    }
#endif
#endif
}

void IRAM_ATTR c_OutputRmt::ISR_CreateIntensityData() {

    register uint32_t OneBitValue = Intensity2Rmt[RmtDataBitIdType_t::RMT_DATA_BIT_ONE_ID].val;
    register uint32_t ZeroBitValue = Intensity2Rmt[RmtDataBitIdType_t::RMT_DATA_BIT_ZERO_ID].val;

    register uint32_t IntensityValue;
    uint32_t NumAvailableBufferSlotsToFill = NUM_RMT_SLOTS - NumUsedEntriesInSendBuffer;
    while ((NumAvailableBufferSlotsToFill > NumRmtSlotsPerIntensityValue) && ThereIsDataToSend) {
        ThereIsDataToSend = ISR_GetNextIntensityToSend(IntensityValue);
        RMT_DEBUG_COUNTER(IntensityValuesSent++);
#ifdef USE_RMT_DEBUG_COUNTERS
        if (200 < IntensityValue) {
            ++RmtWhiteDetected;
        }
#endif

        uint32_t bitmask = TxIntensityDataStartingMask;
        for (uint32_t BitCount = OutputRmtConfig.IntensityDataWidth; 0 < BitCount; --BitCount) {
            RMT_DEBUG_COUNTER(IntensityBitsSent++);
            ISR_WriteToBuffer((IntensityValue & bitmask) ? OneBitValue : ZeroBitValue);
            if (OutputRmtConfig_t::DataDirection_t::MSB2LSB == OutputRmtConfig.DataDirection) {
                bitmask >>= 1;
            } else {
                bitmask <<= 1;
            }
#ifdef USE_RMT_DEBUG_COUNTERS
            if (IntensityValue & bitmask) {
                BitTypeCounters[int(RmtDataBitIdType_t::RMT_DATA_BIT_ONE_ID)]++;
            } else {
                BitTypeCounters[int(RmtDataBitIdType_t::RMT_DATA_BIT_ZERO_ID)]++;
            }
#endif
        }

        if (OutputRmtConfig.SendEndOfFrameBits && !ThereIsDataToSend) {
            ISR_WriteToBuffer(Intensity2Rmt[RmtDataBitIdType_t::RMT_END_OF_FRAME].val);
            RMT_DEBUG_COUNTER(BitTypeCounters[int(RmtDataBitIdType_t::RMT_END_OF_FRAME)]++);
        } else if (OutputRmtConfig.SendInterIntensityBits) {
            ISR_WriteToBuffer(Intensity2Rmt[RmtDataBitIdType_t::RMT_STOP_START_BIT_ID].val);
            RMT_DEBUG_COUNTER(BitTypeCounters[int(RmtDataBitIdType_t::RMT_STOP_START_BIT_ID)]++);
        }

        NumAvailableBufferSlotsToFill = (NUM_RMT_SLOTS - 1) - NumUsedEntriesInSendBuffer;
    }
}

inline bool IRAM_ATTR c_OutputRmt::ISR_GetNextIntensityToSend(uint32_t &DataToSend) {
    if (nullptr != OutputRmtConfig.pPixelDataSource) {
        return OutputRmtConfig.pPixelDataSource->ISR_GetNextIntensityToSend(DataToSend);
    }
#if defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
    else {
        return OutputRmtConfig.pSerialDataSource->ISR_GetNextIntensityToSend(DataToSend);
    }
#else
    return false;
#endif
}

void IRAM_ATTR c_OutputRmt::ISR_Handler(uint32_t isrFlags) {

    RMT_DEBUG_COUNTER(++ISRcounter);
    if (OutputIsPaused) {
        DisableRmtInterrupts;
    }

    else if (isrFlags & RMT_INT_TX_END_BIT) {
        RMT_DEBUG_COUNTER(++IntTxEndIsrCounter);
        DisableRmtInterrupts;

        if (NumUsedEntriesInSendBuffer) {
            RMT_DEBUG_COUNTER(++FailedToSendAllData);
        }

        vTaskNotifyGiveFromISR(SendFrameTaskHandle, &xHigherPriorityTaskWoken);

    } else if (isrFlags & RMT_INT_THR_EVNT_BIT) {
        RMT_DEBUG_COUNTER(++IntTxThrIsrCounter);

        if (NumUsedEntriesInSendBuffer) {

            RMT_DEBUG_COUNTER(++SendBlockIsrCounter);

            ISR_TransferIntensityDataToRMT(MaxNumRmtSlotsPerInterrupt);

            ISR_CreateIntensityData();

            if (!ThereIsDataToSend && 0 == NumUsedEntriesInSendBuffer) {
                RMT_DEBUG_COUNTER(++RanOutOfData);
                DisableRmtInterrupts;

                vTaskNotifyGiveFromISR(SendFrameTaskHandle, &xHigherPriorityTaskWoken);
            }
        }

    }
#ifdef USE_RMT_DEBUG_COUNTERS
    else {
        RMT_DEBUG_COUNTER(++UnknownISRcounter);
        if (isrFlags & RMT_INT_ERROR_BIT) {
            ErrorIsr++;
        }

        if (isrFlags & RMT_INT_RX_END_BIT) {
            RxIsr++;
        }
    }
#endif
}

inline bool IRAM_ATTR c_OutputRmt::ISR_MoreDataToSend() {
#if defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
    if (nullptr != OutputRmtConfig.pPixelDataSource) {
        return OutputRmtConfig.pPixelDataSource->ISR_MoreDataToSend();
    } else {
        return OutputRmtConfig.pSerialDataSource->ISR_MoreDataToSend();
    }
#else
    return OutputRmtConfig.pPixelDataSource->ISR_MoreDataToSend();
#endif
}

inline void IRAM_ATTR c_OutputRmt::ISR_ResetRmtBlockPointers() {
    RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.mem_rd_rst = 1;
    RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.mem_rd_rst = 0;
    RmtBufferWriteIndex = 0;
    SendBufferWriteIndex = 0;
    SendBufferReadIndex = 0;
    NumUsedEntriesInSendBuffer = 0;
}

inline void IRAM_ATTR c_OutputRmt::ISR_StartNewDataFrame() {
    if (nullptr != OutputRmtConfig.pPixelDataSource) {
        OutputRmtConfig.pPixelDataSource->StartNewFrame();
    }
#if defined(SUPPORT_OutputType_DMX) || defined(SUPPORT_OutputType_Serial) || defined(SUPPORT_OutputType_Renard)
    else {
        OutputRmtConfig.pSerialDataSource->StartNewFrame();
    }
#endif
}

void IRAM_ATTR c_OutputRmt::ISR_TransferIntensityDataToRMT(uint32_t MaxNumEntriesToTransfer) {

    uint32_t NumEntriesToTransfer = min(NumUsedEntriesInSendBuffer, MaxNumEntriesToTransfer);

#ifdef USE_RMT_DEBUG_COUNTERS
    if (NumEntriesToTransfer) {
        ++RmtXmtFills;
        RmtEntriesTransfered = NumEntriesToTransfer;
    }
#endif
    while (NumEntriesToTransfer) {
        RMTMEM.chan[OutputRmtConfig.RmtChannelId].data32[RmtBufferWriteIndex].val = SendBuffer[SendBufferReadIndex].val;
        RmtBufferWriteIndex = (++RmtBufferWriteIndex) & (NUM_RMT_SLOTS - 1);
        SendBufferReadIndex = (++SendBufferReadIndex) & (NUM_RMT_SLOTS - 1);
        --NumEntriesToTransfer;
        --NumUsedEntriesInSendBuffer;
    }

    RMTMEM.chan[OutputRmtConfig.RmtChannelId].data32[RmtBufferWriteIndex].val = uint32_t(0);
}

inline void IRAM_ATTR c_OutputRmt::ISR_WriteToBuffer(uint32_t value) {

    SendBuffer[SendBufferWriteIndex].val = value;
    SendBufferWriteIndex = (++SendBufferWriteIndex) & (NUM_RMT_SLOTS - 1);
    ++NumUsedEntriesInSendBuffer;
}

void c_OutputRmt::PauseOutput(bool PauseOutput) {

    if (OutputIsPaused == PauseOutput) {

    } else if (PauseOutput) {

        DisableRmtInterrupts;
        ClearRmtInterrupts;
    }

    OutputIsPaused = PauseOutput;
}

bool c_OutputRmt::StartNewFrame() {

    bool Response = false;

    do {
        if (OutputIsPaused) {

            DisableRmtInterrupts;
            ClearRmtInterrupts;
            break;
        }

        if (InterrupsAreEnabled) {
            RMT_DEBUG_COUNTER(IncompleteFrame++);
        }

        DisableRmtInterrupts;
        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.tx_start = 0;

        ISR_ResetRmtBlockPointers();

        uint32_t NumInterFrameRmtSlotsCount = 0;
        while (NumInterFrameRmtSlotsCount < OutputRmtConfig.NumIdleBits) {
            ISR_WriteToBuffer(Intensity2Rmt[RmtDataBitIdType_t::RMT_INTERFRAME_GAP_ID].val);
            ++NumInterFrameRmtSlotsCount;
            RMT_DEBUG_COUNTER(BitTypeCounters[int(RmtDataBitIdType_t::RMT_INTERFRAME_GAP_ID)]++);
        }

        uint32_t NumFrameStartRmtSlotsCount = 0;
        while (NumFrameStartRmtSlotsCount++ < OutputRmtConfig.NumFrameStartBits) {
            ISR_WriteToBuffer(Intensity2Rmt[RmtDataBitIdType_t::RMT_STARTBIT_ID].val);
            RMT_DEBUG_COUNTER(BitTypeCounters[int(RmtDataBitIdType_t::RMT_STARTBIT_ID)]++);
        }

#ifdef USE_RMT_DEBUG_COUNTERS
        FrameStartCounter++;
        IntensityValuesSentLastFrame = IntensityValuesSent;
        IntensityValuesSent = 0;
        IntensityBitsSentLastFrame = IntensityBitsSent;
        IntensityBitsSent = 0;
#endif

        ISR_StartNewDataFrame();

        ThereIsDataToSend = ISR_MoreDataToSend();

        ISR_CreateIntensityData();

        ISR_TransferIntensityDataToRMT(NUM_RMT_SLOTS - 1);

        ISR_CreateIntensityData();

        RMT.tx_lim_ch[OutputRmtConfig.RmtChannelId].limit = MaxNumRmtSlotsPerInterrupt;

        ClearRmtInterrupts;
        EnableRmtInterrupts;
        vPortYieldOtherCore(0);

        RMT.conf_ch[OutputRmtConfig.RmtChannelId].conf1.tx_start = 1;
        delay(1);

        Response = true;
    } while (false);

    return Response;
}

#endif
