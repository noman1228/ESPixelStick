/*
* OutputDisabled.cpp - OutputDisabled NULL driver code for ESPixelStick
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

#include "output/OutputDisabled.hpp"
#include "output/OutputCommon.hpp"

/*
*/
//----------------------------------------------------------------------------
c_OutputDisabled::c_OutputDisabled (OM_OutputPortDefinition_t & OutputPortDefinition,
                                    c_OutputMgr::e_OutputProtocolType outputType) :
    c_OutputCommon(OutputPortDefinition, outputType)
{
    // DEBUG_START;

    // DEBUG_END;
} // c_OutputDisabled

//----------------------------------------------------------------------------
c_OutputDisabled::~c_OutputDisabled()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputDisabled

//----------------------------------------------------------------------------
// Use the current config to set up the output port
void c_OutputDisabled::Begin()
{
    // DEBUG_START;

    // DEBUG_END;

} // begin

//----------------------------------------------------------------------------
/* Process the config
*
*   needs
*       reference to string to process
*   returns
*       true - config has been accepted
*       false - Config rejected. Using defaults for invalid settings
*/
bool c_OutputDisabled::SetConfig(ArduinoJson::JsonObject & /* jsonConfig */)
{
    // DEBUG_START;

    // DEBUG_END;
    return true;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputDisabled::GetConfig(ArduinoJson::JsonObject & /* jsonConfig */ )
{
    // DEBUG_START;

    // DEBUG_END;

} // GetConfig

//----------------------------------------------------------------------------
uint32_t c_OutputDisabled::Poll()
{
    // DEBUG_START;

    // DEBUG_END;
    return 0;
} // Poll

//----------------------------------------------------------------------------
void c_OutputDisabled::GetStatus (ArduinoJson::JsonObject & jsonStatus)
{
    c_OutputCommon::BaseGetStatus(jsonStatus);
} // GetStatus
