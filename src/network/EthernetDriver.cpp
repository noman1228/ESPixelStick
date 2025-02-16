/*
* pEthernetDriver->cpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2025 Shelby Merrick
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
*/

#include "ESPixelStick.h"

#ifdef SUPPORT_ETHERNET

#include "Ethernet3.h" // For W5500 support
#include <SPI.h>      // For W5500 support
#include "network/NetworkMgr.hpp"
#include "network/EthernetDriver.hpp"

/*****************************************************************************/
/* FSM                                                                       */
/*****************************************************************************/
static fsm_Eth_state_Boot              fsm_Eth_state_Boot_imp;
static fsm_Eth_state_PoweringUp        fsm_Eth_state_PoweringUp_imp;
static fsm_Eth_state_ConnectingToEth   fsm_Eth_state_ConnectingToEth_imp;
static fsm_Eth_state_WaitForIP         fsm_Eth_state_WaitForIP_imp;
static fsm_Eth_state_GotIp             fsm_Eth_state_GotIp_imp;
static fsm_Eth_state_DeviceInitFailed  fsm_Eth_state_DeviceInitFailed_imp;

// W5500 Configuration
#ifdef SUPPORT_W5500
#define W5500_CS_PIN 5  // Define the CS pin for W5500
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // MAC address for W5500
#endif

//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_EthernetDriver::c_EthernetDriver ()
{
    // DEBUG_START;
    // DEBUG_END;
} // c_EthernetDriver

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
c_EthernetDriver::~c_EthernetDriver ()
{
    // DEBUG_START;
    // DEBUG_END;

} // ~c_EthernetDriver

//-----------------------------------------------------------------------------
void c_EthernetDriver::AnnounceState ()
{
    // DEBUG_START;

    String StateName;
    pCurrentFsmState->GetStateName (StateName);
    logcon (String (F ("Entering State: ")) + StateName);

    // DEBUG_END;

} // AnnounceState

//-----------------------------------------------------------------------------
///< Start the module
void c_EthernetDriver::Begin ()
{
    // DEBUG_START;

    fsm_Eth_state_Boot_imp.SetParent(this);
    fsm_Eth_state_PoweringUp_imp.SetParent(this);
    fsm_Eth_state_ConnectingToEth_imp.SetParent(this);
    fsm_Eth_state_WaitForIP_imp.SetParent(this);
    fsm_Eth_state_GotIp_imp.SetParent(this);
    fsm_Eth_state_DeviceInitFailed_imp.SetParent(this);

#ifdef SUPPORT_W5500
    // Initialize W5500
    Ethernet.init(W5500_CS_PIN);
    if (Ethernet.begin(mac) == 0) {
        logcon(F("Failed to configure W5500 using DHCP"));
        fsm_Eth_state_DeviceInitFailed_imp.Init();
    } else {
        logcon(F("W5500 connected using DHCP"));
        fsm_Eth_state_GotIp_imp.Init();
    }
#else
    // Initialize built-in Ethernet
    if (false == ETH.begin(phy_addr, power_pin, mdc_pin, mdio_pin, phy_type, clk_mode)) {
        logcon(F("Failed to configure built-in Ethernet"));
        fsm_Eth_state_DeviceInitFailed_imp.Init();
    } else {
        logcon(F("Built-in Ethernet connected"));
        fsm_Eth_state_GotIp_imp.Init();
    }
#endif

    // Setup Ethernet Handlers
    WiFi.onEvent ([this](WiFiEvent_t event, arduino_event_info_t info) {this->onEventHandler (event, info); });

    // set up the poll interval
    NextPollTimer.StartTimer(PollInterval, false);

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_EthernetDriver::SetEthHostname ()
{
    // DEBUG_START;

    String Hostname;
    NetworkMgr.GetHostname (Hostname);

    // DEBUG_V (String ("Hostname: ") + Hostname);
    if (0 != Hostname.length ())
    {
        // DEBUG_V (String ("Setting ETH hostname: ") + Hostname);
#ifdef SUPPORT_W5500
        Ethernet.setHostname (Hostname.c_str ());
#else
        ETH.setHostname (Hostname.c_str ());
#endif
    }

    logcon (String (F ("Ethernet Connecting as ")) + Hostname);

    // DEBUG_END;
} // StartEth

//-----------------------------------------------------------------------------
void c_EthernetDriver::GetConfig (JsonObject& json)
{
    // DEBUG_START;

    JsonWrite(json, CN_ip,          ip.toString ());
    JsonWrite(json, CN_netmask,     netmask.toString ());
    JsonWrite(json, CN_gateway,     gateway.toString ());
    JsonWrite(json, CN_dnsp,        primaryDns.toString ());
    JsonWrite(json, CN_dnss,        secondaryDns.toString ());
    JsonWrite(json, CN_dhcp,        UseDhcp);

    JsonWrite(json, CN_type,        uint32_t(phy_type));
    JsonWrite(json, CN_addr,        phy_addr);
    JsonWrite(json, CN_power_pin,   power_pin);
    JsonWrite(json, CN_mode,        uint32_t(clk_mode));
    JsonWrite(json, CN_mdc_pin,     mdc_pin);
    JsonWrite(json, CN_mdio_pin,    mdio_pin);
    JsonWrite(json, CN_activevalue, powerPinActiveValue);
    JsonWrite(json, CN_activedelay, powerPinActiveDelayMs);

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_EthernetDriver::GetHostname (String & Name)
{
#ifdef SUPPORT_W5500
    Name = Ethernet.hostname();
#else
    Name = ETH.getHostname ();
#endif
} // GetHostname

//-----------------------------------------------------------------------------
IPAddress c_EthernetDriver::GetIpAddress ()
{
#ifdef SUPPORT_W5500
    return Ethernet.localIP();
#else
    return ETH.localIP ();
#endif
} // GetIpAddress

//-----------------------------------------------------------------------------
IPAddress c_EthernetDriver::GetIpGateway ()
{
#ifdef SUPPORT_W5500
    return Ethernet.gatewayIP();
#else
    return ETH.gatewayIP ();
#endif
} // GetIpGateway

//-----------------------------------------------------------------------------
IPAddress c_EthernetDriver::GetIpSubNetMask ()
{
#ifdef SUPPORT_W5500
    return Ethernet.subnetMask();
#else
    return ETH.subnetMask ();
#endif
} // GetIpSubNetMask

//-----------------------------------------------------------------------------
String c_EthernetDriver::GetMacAddress ()
{
#ifdef SUPPORT_W5500
    return Ethernet.macAddress();
#else
    return ETH.macAddress ();
#endif
} // GetMacAddress

//-----------------------------------------------------------------------------
void c_EthernetDriver::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    String Hostname;
    GetHostname (Hostname);
    JsonWrite(jsonStatus, CN_hostname,  Hostname);
    JsonWrite(jsonStatus, CN_ip,        GetIpAddress ().toString ());
    JsonWrite(jsonStatus, CN_subnet,    GetIpSubNetMask ().toString ());
    JsonWrite(jsonStatus, CN_mac,       GetMacAddress ());
    JsonWrite(jsonStatus, CN_gateway,   GetIpGateway ());
    JsonWrite(jsonStatus, CN_connected, IsConnected ());

    // DEBUG_END;
} // GetStatus

//-----------------------------------------------------------------------------
void c_EthernetDriver::InitPowerPin ()
{
    // DEBUG_START;

    // Set up the power control output
    ResetGpio(power_pin);
    pinMode (power_pin, OUTPUT);
    digitalWrite (power_pin, powerPinActiveValue);

    // DEBUG_END;
} // InitPowerPin

//-----------------------------------------------------------------------------
bool c_EthernetDriver::IsConnected ()
{
    // DEBUG_V("");

    return (pCurrentFsmState == &fsm_Eth_state_GotIp_imp);

} // IsConnected

//------------------------------------------------------------------------------
void c_EthernetDriver::NetworkStateChanged (bool NetworkState)
{
    // DEBUG_START;

    NetworkMgr.SetEthernetIsConnected (NetworkState);

    // DEBUG_END;

} // NetworkStateChanged

//-----------------------------------------------------------------------------
void c_EthernetDriver::onEventHandler (const WiFiEvent_t event, const WiFiEventInfo_t info)
{
    // DEBUG_START;

    switch (event)
    {
        case ARDUINO_EVENT_ETH_START:
        {
            // DEBUG_V ("ARDUINO_EVENT_ETH_START");
            SetEthHostname ();
            break;
        }
        case ARDUINO_EVENT_ETH_CONNECTED:
        {
            // DEBUG_V ("ARDUINO_EVENT_ETH_CONNECTED");
            pCurrentFsmState->OnConnect ();
            break;
        }
        case ARDUINO_EVENT_ETH_GOT_IP:
        {
            // DEBUG_V ("ARDUINO_EVENT_ETH_GOT_IP");
            pCurrentFsmState->OnGotIp ();
            break;
        }
        case ARDUINO_EVENT_ETH_DISCONNECTED:
        {
            // DEBUG_V ("ARDUINO_EVENT_ETH_DISCONNECTED");
            pCurrentFsmState->OnDisconnect ();
            break;
        }
        case ARDUINO_EVENT_ETH_STOP:
        {
            // DEBUG_V ("ARDUINO_EVENT_ETH_STOP");
            pCurrentFsmState->OnDisconnect ();
            break;
        }
        default:
        {
            // DEBUG_V ("some event we are not processing");
            break;
        }
    }
    // DEBUG_END;

} // onEventHandler

//-----------------------------------------------------------------------------
void c_EthernetDriver::Poll ()
{
    // DEBUG_START;

    if (NextPollTimer.IsExpired())
    {
        // DEBUG_V ("Polling");
        NextPollTimer.StartTimer(PollInterval, false);
        pCurrentFsmState->Poll ();
    }

    // DEBUG_END;

} // Poll

//-----------------------------------------------------------------------------
void c_EthernetDriver::reset ()
{
    // DEBUG_START;

    logcon (F ("Ethernet Reset has been requested"));

    NetworkStateChanged (false);
#ifdef SUPPORT_W5500
    Ethernet.begin(mac); // Reinitialize W5500
#else
    ETH.begin(phy_addr, power_pin, mdc_pin, mdio_pin, phy_type, clk_mode); // Reinitialize built-in Ethernet
#endif

    fsm_Eth_state_Boot_imp.Init ();

    // DEBUG_END;
} // reset

//-----------------------------------------------------------------------------
bool c_EthernetDriver::SetConfig (JsonObject & json)
{
    // DEBUG_START;

    bool ConfigChanged = false;
    uint32_t temp = 0;

    String sIP = ip.toString ();
    String sGateway = gateway.toString ();
    String sNetmask = netmask.toString ();
    String sDnsp = primaryDns.toString ();
    String sDnss = secondaryDns.toString ();

    ConfigChanged |= setFromJSON (sIP,      json, CN_ip);
    ConfigChanged |= setFromJSON (sNetmask, json, CN_netmask);
    ConfigChanged |= setFromJSON (sGateway, json, CN_gateway);
    ConfigChanged |= setFromJSON (sDnsp,    json, CN_dnsp);
    ConfigChanged |= setFromJSON (sDnss,    json, CN_dnss);
    ConfigChanged |= setFromJSON (UseDhcp,  json, CN_dhcp);

    ConfigChanged |= setFromJSON (phy_addr,  json, CN_addr);
    ConfigChanged |= setFromJSON (power_pin, json, CN_power_pin);
    ConfigChanged |= setFromJSON (mdc_pin,   json, CN_mdc_pin);
    ConfigChanged |= setFromJSON (mdio_pin,  json, CN_mdio_pin);
    temp = uint32_t (phy_type);
    ConfigChanged |= setFromJSON (temp,  json, CN_type);
    phy_type = eth_phy_type_t (temp);
    temp = uint32_t (clk_mode);
    ConfigChanged |= setFromJSON (temp,  json, CN_mode);
    clk_mode = eth_clock_mode_t (temp);
    ConfigChanged |= setFromJSON (powerPinActiveValue,   json, CN_activevalue);
    ConfigChanged |= setFromJSON (powerPinActiveDelayMs, json, CN_activedelay);

    ip.fromString (sIP);
    gateway.fromString (sGateway);
    netmask.fromString (sNetmask);
    primaryDns.fromString (sDnsp);
    secondaryDns.fromString (sDnss);

    // Eth Driver does not support config updates while it is running.
    if (ConfigChanged && HasBeenPreviouslyConfigured)
    {
        logcon (F ("Configuration change requires system reboot."));
        RequestReboot(100000);
    }

    HasBeenPreviouslyConfigured = true;

    // DEBUG_END;
    return ConfigChanged;

} // SetConfig

//-----------------------------------------------------------------------------
void c_EthernetDriver::SetUpIp ()
{
    // DEBUG_START;
    do // once
    {
        IPAddress temp = INADDR_NONE;

        if (true == UseDhcp)
        {
            logcon (F ("Connecting to Ethernet using DHCP"));
            break;
        }

        if (temp == ip)
        {
            logcon (F ("NETWORK: ERROR: STATIC SELECTED WITHOUT IP. Using DHCP assigned address"));
            break;
        }

        if ((ip      == GetIpAddress ())    &&
            (netmask == GetIpSubNetMask ()) &&
            (gateway == GetIpGateway ()))
        {
            // DEBUG_V ("correct IP is already set");
            break;
        }

        if(primaryDns == INADDR_NONE)
        {
            primaryDns = gateway;
        }

#ifdef SUPPORT_W5500
        Ethernet.begin(mac, ip, primaryDns, gateway, netmask);
#else
        ETH.config (ip, gateway, netmask, primaryDns, secondaryDns);
#endif

        logcon (F ("Connecting to Ethernet with Static IP"));

    } while (false);

    // DEBUG_END;

} // SetUpIp

//-----------------------------------------------------------------------------
void c_EthernetDriver::StartEth ()
{
    // DEBUG_START;

#ifdef SUPPORT_W5500
    Ethernet.begin(mac); // Initialize W5500
#else
    ETH.begin(phy_addr, power_pin, mdc_pin, mdio_pin, phy_type, clk_mode); // Initialize built-in Ethernet
#endif

    // DEBUG_END;
} // StartEth

//-----------------------------------------------------------------------------
int c_EthernetDriver::ValidateConfig ()
{
    // DEBUG_START;

    int response = 0;

    // DEBUG_END;

    return response;

} // ValidateConfig

/*****************************************************************************/
//  FSM Code
/*****************************************************************************/
/*****************************************************************************/
// Waiting for polling to start
void fsm_Eth_state_Boot::Init ()
{
    // DEBUG_START;

    pEthernetDriver->SetFsmState (this);
    pEthernetDriver->GetFsmTimer().StartTimer(10000, false);

    // DEBUG_END;

} // fsm_Eth_state_Boot::Init

/*****************************************************************************/
// Waiting for polling to start
void fsm_Eth_state_Boot::Poll ()
{
    // DEBUG_START;

    if (pEthernetDriver->GetFsmTimer().IsExpired())
    {
        fsm_Eth_state_PoweringUp_imp.Init();
    }

    // DEBUG_END;
} // fsm_Eth_state_boot

/*****************************************************************************/
/*****************************************************************************/
void fsm_Eth_state_PoweringUp::Init ()
{
    // DEBUG_START;

    pEthernetDriver->SetFsmState (this);
    pEthernetDriver->AnnounceState ();
    pEthernetDriver->GetFsmTimer().StartTimer (pEthernetDriver->GetPowerPinActiveDelayMs(), false);

    pEthernetDriver->InitPowerPin ();

    // DEBUG_END;

} // fsm_Eth_state_PoweringUp::Init

/*****************************************************************************/
void fsm_Eth_state_PoweringUp::Poll ()
{
    // DEBUG_START;

    if (pEthernetDriver->GetFsmTimer ().IsExpired())
    {
        fsm_Eth_state_ConnectingToEth_imp.Init ();
        pEthernetDriver->StartEth ();
    }

    // DEBUG_END;
} // fsm_Eth_state_PoweringUp

/*****************************************************************************/
/*****************************************************************************/
void fsm_Eth_state_ConnectingToEth::Init ()
{
    // DEBUG_START;

    pEthernetDriver->SetFsmState (this);
    pEthernetDriver->AnnounceState ();
    pEthernetDriver->GetFsmTimer().StartTimer (5000, false);

    // DEBUG_END;

} // fsm_Eth_state_ConnectingToEthUsingConfig::Init

/*****************************************************************************/
void fsm_Eth_state_ConnectingToEth::Poll()
{
    // DEBUG_START;

    if(GetIpAddress())
    {
        OnGotIp();
    }

    // DEBUG_END;
} // fsm_Eth_state_ConnectingToEth

/*****************************************************************************/
void fsm_Eth_state_ConnectingToEth::OnConnect ()
{
    // DEBUG_START;

    pEthernetDriver->SetUpIp ();
    fsm_Eth_state_WaitForIP_imp.Init ();

    // DEBUG_END;

} // fsm_Eth_state_ConnectingToEth::OnConnect

/*****************************************************************************/
void fsm_Eth_state_ConnectingToEth::OnGotIp ()
{
    // DEBUG_START;

    fsm_Eth_state_GotIp_imp.Init ();
    pEthernetDriver->SetUpIp ();

    // DEBUG_END;

} // fsm_Eth_state_ConnectingToEth::OnConnect

/*****************************************************************************/
/*****************************************************************************/
void fsm_Eth_state_WaitForIP::Init ()
{
    // DEBUG_START;

    pEthernetDriver->SetFsmState (this);
    pEthernetDriver->AnnounceState ();
    pEthernetDriver->GetFsmTimer().StartTimer (5000, false);

    // DEBUG_END;

} // fsm_Eth_state_WaitForIP::Init

/*****************************************************************************/
void fsm_Eth_state_WaitForIP::OnGotIp ()
{
    // DEBUG_START;

    fsm_Eth_state_GotIp_imp.Init ();

    // DEBUG_END;

} // fsm_Eth_state_WaitForIP::OnGotIp

/*****************************************************************************/
void fsm_Eth_state_WaitForIP::OnDisconnect ()
{
    // DEBUG_START;

    fsm_Eth_state_ConnectingToEth_imp.Init ();

    // DEBUG_END;

} // fsm_Eth_state_WaitForIP::OnDisconnect

/*****************************************************************************/
/*****************************************************************************/
void fsm_Eth_state_GotIp::Init ()
{
    // DEBUG_START;

    pEthernetDriver->SetFsmState (this);
    pEthernetDriver->AnnounceState ();
    pEthernetDriver->NetworkStateChanged (true);

    logcon (String (F ("Ethernet Connected with IP: ")) + pEthernetDriver->GetIpAddress ().toString ());

    // DEBUG_END;

} // fsm_Eth_state_GotIp::Init

/*****************************************************************************/
void fsm_Eth_state_GotIp::OnDisconnect ()
{
    // DEBUG_START;

    pEthernetDriver->NetworkStateChanged (false);
    fsm_Eth_state_ConnectingToEth_imp.Init ();

    // DEBUG_END;
} // fsm_Eth_state_GotIp::OnDisconnect

/*****************************************************************************/
/*****************************************************************************/
void fsm_Eth_state_DeviceInitFailed::Init ()
{
    // DEBUG_START;

    pEthernetDriver->SetFsmState (this);
    pEthernetDriver->AnnounceState ();
    pEthernetDriver->NetworkStateChanged (false);

    // DEBUG_END;

} // fsm_Eth_state_DeviceInitFailed::Init

#endif // def SUPPORT_ETHERNET