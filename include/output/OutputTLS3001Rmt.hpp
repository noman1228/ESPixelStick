#pragma once
/*
* OutputTLS3001Rmt.h - TLS3001 driver code for ESPixelStick RMT Channel
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
*   This is a derived class that converts data in the output buffer into
*   pixel intensities and then transmits them through the configured serial
*   interface.
*
*/
#include "ESPixelStick.h"
#if defined(SUPPORT_OutputProtocol_TLS3001) && defined (ARDUINO_ARCH_ESP32)

#include "OutputTLS3001.hpp"
#include "OutputRmt.hpp"
#include "OutputTLS3001RmtFsm.hpp"

class c_OutputTLS3001Rmt : public c_OutputTLS3001
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputTLS3001Rmt(OM_OutputPortDefinition_t & OutputPortDefinition,
                       c_OutputMgr::e_OutputProtocolType outputType);
    virtual ~c_OutputTLS3001Rmt ();

    // functions to be provided by the derived class
    void    Begin ();                                         ///< set up the operating environment based on the current config (or defaults)
    bool    SetConfig (ArduinoJson::JsonObject& jsonConfig);  ///< Set a new config in the driver
    uint32_t Poll ();                                         ///< Call from loop (),  renders output data
    bool    RmtPoll ();
    void    GetStatus (ArduinoJson::JsonObject& jsonStatus);
    void    SetOutputBufferSize (uint32_t NumChannelsAvailable);
    void    PauseOutput(bool State);
    virtual bool IRAM_ATTR ISR_GetNextBitToSend (uint32_t &DataToSend);

private:
    void    SetBitTimes ();

protected:
    friend class fsm_RMT_state_SendSync;
    friend class fsm_RMT_state_SendReset;
    friend class fsm_RMT_state_SendDataStart;
    friend class fsm_RMT_state_SendData;
    friend class fsm_RMT_state_SendDataIdle;
    friend class fsm_RMT_state;
    fsm_RMT_state * pCurrentFsmState = nullptr;

    fsm_RMT_state_SendSync      fsm_RMT_state_SendSync_imp;
    fsm_RMT_state_SendReset     fsm_RMT_state_SendReset_imp;
    fsm_RMT_state_SendDataStart fsm_RMT_state_SendDataStart_imp;
    fsm_RMT_state_SendData      fsm_RMT_state_SendData_imp;
    fsm_RMT_state_SendDataIdle  fsm_RMT_state_SendDataIdle_imp;

    rmt_item32_t                RmtResetOneMsDelay;
    uint32_t                    NumResetDelayBits = 1;

    rmt_item32_t                RmtSyncIdle;
    uint32_t                    NumSyncIdleBits = 1;

    rmt_item32_t                SendDataIfg;
    uint32_t                    NumIfgBitsPerFrame = 1;

    bool                        SendingData = false;

    c_OutputRmt Rmt;

}; // c_OutputTLS3001Rmt

#endif // defined(SUPPORT_OutputProtocol_TLS3001) && defined (ARDUINO_ARCH_ESP32)
