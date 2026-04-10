#pragma once
/*
* OutputTLS3001.h - TLS3001 driver code for ESPixelStick
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
#if defined(SUPPORT_OutputProtocol_TLS3001)

#include "OutputPixel.hpp"

/*
The input signals delivered to the SDI pin must adhere to the following definitions:
a. Valid input data must be Manchester-encoded;
        a signal transition from high to low represents a "1,"
        while a transition from low to high represents a "0."
b. After the chip is powered on, a synchronization frame must be transmitted
        first to enable the chip to detect the communication baud rate.
        The format of the synchronization frame is:
            15’b111111111111111 +
            4’b0001 +
            11’b00000000000.
        After transmitting the synchronization frame, a delay period must be observed
        before sending data frames; this ensures that each chip can accurately detect
        the communication baud rate. The delay duration (in microseconds) must be greater
        than: (Number of connected chips) ÷ (Communication baud rate in MHz) × 30.
c. After transmitting a number of data frames, a reset frame must be sent once;
        after waiting for 1 ms, a synchronization frame must be sent again to allow
        the chip to eliminate accumulated errors. The format of the reset frame is:
            15’b111111111111111 +
            4’b0100.
d. The format of a data frame is:
        15’b111111111111111 +
        4’b0010 (Data Header) +
        39-bit data for the first chip +
        39-bit data for the second chip + …… + 39-bit data for the nth chip.

    The first chip is the initial recipient of the data.
    The data format for the chip is as follows:
        1'b0 (Identifier Bit) +
        12'bxxxxxxxxxxxx (Data for Output Port 1) +
        1'b0 (Identifier Bit) +
        12'bxxxxxxxxxxxx (Data for Output Port 2) +
        1'b0 (Identifier Bit) +
        12'bxxxxxxxxxxxx (Data for Output Port 3),
    where 'x' represents either 1 or 0.
f. Data is transmitted Most Significant Bit (MSB) first.
g. The SDI input pin must be held at a low level during the idle state.
h. During the transmission of a single data frame, the data must be sent continuously
        without any interruptions, and the transmission frequency must not change.

*/
class c_OutputTLS3001 : public c_OutputPixel
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputTLS3001 (OM_OutputPortDefinition_t & OutputPortDefinition,
                     c_OutputMgr::e_OutputProtocolType outputType);

    virtual ~c_OutputTLS3001 ();

    // functions to be provided by the derived class
    virtual void Begin () { c_OutputPixel::Begin (); }
    virtual bool SetConfig (ArduinoJson::JsonObject& jsonConfig); ///< Set a new config in the driver
    virtual void GetConfig (ArduinoJson::JsonObject& jsonConfig); ///< Get the current config used by the driver
            void GetDriverName (String& sDriverName) { sDriverName = CN_TLS3001; }
    virtual void GetStatus (ArduinoJson::JsonObject & jsonStatus);
    virtual void SetOutputBufferSize (uint32_t NumChannelsAvailable);

protected:

#define TLS3001_PIXEL_DATA_RATE              500000.0
#define TLS3001_PIXEL_NS_BIT                ((1.0 / TLS3001_PIXEL_DATA_RATE) * NanoSecondsInASecond)

#define TLS3001_PIXEL_NS_IDLE                50000.0 // 50us
#define TLS3001_MIN_IDLE_TIME_US             (TLS3001_PIXEL_NS_IDLE / float(NanoSecondsInAMicroSecond))

#define TLS3001_DEFAULT_INTENSITY_PER_PIXEL  3
/*
    Frame Start = 15 1s Always followed by four bit Frame Type

    Frame Types
        Sync   0b0001 Followed by 15 0s followed by an idle period
        Reset  0b0100 Followed by long idle time (calculation needed)
        Data   0b0010 Followed by 39 bit data times num pixels

    39 bit data
        1 zero
        12 R
        1 zero
        12 g
        1 zero
        12 b
*/
#define TLS3001_MAX_CONSECUTIVE_DATA_FRAMES 50

private:

    uint8_t CurrentLimit = 50;
    struct PreambleData_t
    {
        uint8_t normal[4];
        uint8_t inverted[4];
    };
    PreambleData_t PreambleData;

}; // c_OutputTLS3001

#endif // def SUPPORT_OutputProtocol_TLS3001
