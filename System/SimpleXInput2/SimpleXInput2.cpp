//--------------------------------------------------------------------------------------
// SimpleXInput2.cpp
//
// The SimpleXInput2 shows how to use the new XInput2 API to read controller inputs
// from gamepads and the new wireless microphone, and set outputs such as vibration and
// LED colors.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <d3d9.h>

#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgInput.h"
#include "AtgResource.h"
#include "AtgUtil.h"
#include <AtgConsole.h>
#include <XInput2.h>
#include <XMic.h>

//
// Color values
//
#define TOP_BACK_COLOR      0xff00007f          // background gradient colors
#define BOTTOM_BACK_COLOR   0xff000000
#define STATUS_TEXT_COLOR   0xffd0d0af
#define TEXT_COLOR          0xffffffff

//
// Some basic known color values for setting LIPS mic LEDs
//
D3DVECTOR g_LEDPresets[] =
{ 
    { 1.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f },
    { 1.0f, 1.0f, 0.0f },
    { 1.0f, 0.0f, 1.0f },
    { 1.0f, 0.5f, 0.0f },
    { 0.0f, 1.0f, 1.0f },
    { 1.0f, 0.5f, 1.0f },
};

#define LED_PRESET_COUNT                ( ARRAYSIZE( g_LEDPresets ) )
#define LED_SAMPLE_FREQUENCY            1000

//
// Table of control id and friendly names for describing the GamePad controls
//
typedef struct 
{
    XINPUTID        ID;
    WCHAR*          szName;
    DWORD           dwDataType;
} CONTROL_DESCRIPTION;

CONTROL_DESCRIPTION g_ControlDescGamepad[] = 
{
    XINPUTID_DPAD_UP,			L"DPAD_UP",		XINPUT2_TYPE_BOOL,
    XINPUTID_DPAD_DOWN,			L"DPAD_DOWN",	XINPUT2_TYPE_BOOL,
    XINPUTID_DPAD_LEFT,			L"DPAD_LEFT",	XINPUT2_TYPE_BOOL,
    XINPUTID_DPAD_RIGHT,		L"DPAD_RIGHT",	XINPUT2_TYPE_BOOL,
    XINPUTID_START,				L"START",		XINPUT2_TYPE_BOOL,
    XINPUTID_BACK,				L"BACK",		XINPUT2_TYPE_BOOL,
    XINPUTID_LEFT_THUMB,		L"L_THUMB",		XINPUT2_TYPE_BOOL,
    XINPUTID_RIGHT_THUMB,		L"R_THUMB",		XINPUT2_TYPE_BOOL,
    XINPUTID_LEFT_SHOULDER,		L"LSHOULDER",	XINPUT2_TYPE_BOOL,
    XINPUTID_RIGHT_SHOULDER,	L"R_SHOULDER",	XINPUT2_TYPE_BOOL,
    XINPUTID_A,					L"A",			XINPUT2_TYPE_BOOL,
    XINPUTID_B,					L"B",			XINPUT2_TYPE_BOOL,
    XINPUTID_X,					L"X",			XINPUT2_TYPE_BOOL,
    XINPUTID_Y,					L"Y",			XINPUT2_TYPE_BOOL,
    XINPUTID_LEFT_TRIGGER,		L"L_TRIGGER",	XINPUT2_TYPE_FLOAT,
    XINPUTID_RIGHT_TRIGGER,		L"R_TRIGGER",	XINPUT2_TYPE_FLOAT,
    XINPUTID_BUTTONS,			L"BUTTONS",		XINPUT2_TYPE_DWORD,
    XINPUTID_LEFT_THUMBSTICK,	L"L_STICK",		XINPUT2_TYPE_POINT,
    XINPUTID_RIGHT_THUMBSTICK,	L"R_STICK",     XINPUT2_TYPE_POINT,
};

const DWORD g_dwNumDescGamepad = ARRAYSIZE( g_ControlDescGamepad );


// Keep track of connected game pads.
const enum EINPUTDEVICESTATE
{
    EINPUTDEVICE_NOTCONNECTED = 0,      // Not gamepad connected
    EINPUTDEVICE_CONNECTEDINCOMPATIBLE, // A gamepad is connected doens't meet req's
    EINPUTDEVICE_CONNECTEDCOMPATIBLE,   // Gamepad is connected and verified compatible
};

// Table of possible properties a device could have
struct DescribeProperty
{
    XINPUTID dwValue;
    WCHAR strName[64];
};

DescribeProperty describeProperties[] =
{
    XINPUTID_GAMEPAD,       L"XINPUTID_GAMEPAD",
    XINPUTID_DRUM_KIT,      L"XINPUTID_DRUM_KIT",
    XINPUTID_GUITAR,        L"XINPUTID_GUITAR",
    XINPUTID_WHEEL,         L"XINPUTID_WHEEL",
    XINPUTID_ARCADE_STICK,  L"XINPUTID_ARCADE_STICK",
    XINPUTID_FLIGHT_STICK,  L"XINPUTID_FLIGHT_STICK",
    XINPUTID_DANCEPAD,      L"XINPUTID_DANCEPAD",
};



//--------------------------------------------------------------------------------------
// Name: class SimpleXInput2
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class SimpleXInput2 : public ATG::Application
{
private:
    ATG::Timer  m_Timer;
    ATG::Font   m_Font;

    // Cache out the microphone accellerometer vector
    D3DVECTOR   m_MicAccelVector[XUSER_MAX_COUNT];

    // Cache controller button inputs in a binary blob
    BYTE        m_bControlDataBlock[XUSER_MAX_COUNT][128];

    // Keep track of connected gamepads
    EINPUTDEVICESTATE m_eInputDeviceState[XUSER_MAX_COUNT];
    
    // Demonstrate asynchronous output
    XOVERLAPPED m_LEDChangeOverlapped;
    BOOL        m_fAsyncLEDReqPending;

    // Print gamepad inputs to screen
    void        PrintControllerSummary( const XINPUT2CONTEXT& context,
                                 const DWORD dwPlayerIndex,
                                 const CONTROL_DESCRIPTION controlDesc[],
                                 const DWORD dwNumControls );

    void        PrintMsg( const DWORD dwPlayerIndex,
                                const INT iQuadrant, 
                                const WCHAR *szMsg);

    // Read capabilities of microphone and output to TTY
    HRESULT     LipsSampleProcessCapabilities( DWORD dwMicrophoneIndex );
    // Output microphone out vector to screen
    void        PrintMicSummary( const DWORD dwPlayerIndex );
    // Set LED colors on microphone
    void        UpdateMicLEDs( const DWORD dwPlayerIndex, const XINPUT2CONTEXT& context );

    virtual     HRESULT Initialize();
    virtual     HRESULT Update()   { return 0; };
    virtual     HRESULT UpdateGamepadInput( const XINPUT2CONTEXT& context, 
                                 const DWORD dwPlayerIndex, 
                                 const CONTROL_DESCRIPTION controlDesc[],
                                 const DWORD dwNumControls );
   
    virtual     HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    SimpleXInput2 atgApp;

    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, 
        &atgApp.m_d3dpp.BackBufferHeight );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects and does general setup.
//--------------------------------------------------------------------------------------
HRESULT SimpleXInput2::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    memset( m_bControlDataBlock, 0x00, sizeof(m_bControlDataBlock) );
    memset( m_eInputDeviceState, 0x00, sizeof(m_eInputDeviceState) );

    m_fAsyncLEDReqPending = FALSE;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SimpleXInput2::UpdateGamepadInput()
// Desc: Called once per frame, update input specific to GamePad
//--------------------------------------------------------------------------------------
HRESULT SimpleXInput2::UpdateGamepadInput(const XINPUT2CONTEXT& context, 
                                   const DWORD dwPlayerIndex, 
                                   const CONTROL_DESCRIPTION controlDesc[],
                                   const DWORD dwNumControls)
{
    // read inputs
    size_t cByteOffset=0;
    BYTE* bControlData = m_bControlDataBlock[dwPlayerIndex];

    for( DWORD dwControl = 0; dwControl < dwNumControls; dwControl++ )
    {
        switch( controlDesc[dwControl].dwDataType )
        {
        case XINPUT2_TYPE_BOOL:
            {
                BOOL bRead;
                if( XInput2GetBool(context, controlDesc[dwControl].ID, &bRead) ) 
                {
                    memcpy(bControlData + cByteOffset, &bRead, sizeof(bRead));
                }

                cByteOffset += sizeof(bRead);
            }
            break;

        case XINPUT2_TYPE_DWORD:
            {
                DWORD dwRead;
                if( XInput2GetDWord(context, controlDesc[dwControl].ID, &dwRead) ) 
                {
                    memcpy(bControlData + cByteOffset, &dwRead, sizeof(dwRead));
                }

                cByteOffset += sizeof(dwRead);
            }
            break;

        case XINPUT2_TYPE_FLOAT:
            {
                FLOAT fRead;
                if( XInput2GetFloat(context, controlDesc[dwControl].ID, &fRead) ) 
                {
                    memcpy(bControlData + cByteOffset, &fRead, sizeof(fRead));
                }

                cByteOffset += sizeof(fRead);
            }
            break;

        case XINPUT2_TYPE_POINT:
            {
                XINPUT2_POINT pointRead;
                if( XInput2GetPoint(context, controlDesc[dwControl].ID, &pointRead) ) 
                {
                    memcpy(bControlData + cByteOffset, &pointRead, sizeof(pointRead));
                }

                cByteOffset += sizeof(pointRead);
            }
            break;

        }
    }

    // Demonstrate how to set output (vibration in this case).  Read the LEFT and
    // RIGHT triggers, and set the vibration to these values.
    XINPUT2_POINT pRead;

    if( XInput2GetFloat(context, XINPUTID_LEFT_TRIGGER, &pRead.x) 
            && XInput2GetFloat(context, XINPUTID_RIGHT_TRIGGER, &pRead.y) ) 
    {
        XInput2SetPoint( context, XINPUTID_VIBRATION, pRead );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: PrintControllerSummary()
// Desc: For each user print summary of key presses/controls
//--------------------------------------------------------------------------------------
void SimpleXInput2::PrintControllerSummary( const XINPUT2CONTEXT& context,
                                           const DWORD dwPlayerIndex,
                                           const CONTROL_DESCRIPTION controlDesc[],
                                           const DWORD dwNumControls )
{
    WCHAR szBuffer[128];
    FLOAT fDrawX = ((m_Font.m_rcWindow.x2 - m_Font.m_rcWindow.x1) / XUSER_MAX_COUNT)
                        * (FLOAT)dwPlayerIndex;
    FLOAT fDrawY = 50.0f;
    const FLOAT fFontScale = 0.5f;
    const FLOAT fLineHeight = m_Font.GetFontHeight() * fFontScale;

    m_Font.SetScaleFactors( fFontScale, fFontScale);

    // Enumerate properties
    for ( INT iProperty = 0; iProperty < sizeof(describeProperties) / 
            sizeof(DescribeProperty); iProperty++ )
    {
        if( XInput2HasProperty(context, describeProperties[iProperty].dwValue) )
        {
            swprintf_s( szBuffer, L"%s ", describeProperties[iProperty].strName );
        }
    }

    m_Font.DrawText( fDrawX, fDrawY, STATUS_TEXT_COLOR, szBuffer, ATGFONT_LEFT );
    fDrawY += fLineHeight;

    // Our control data is cached in a single data block for best D$ coherency
    size_t cByteOffset = 0;
    BYTE* bControlData = m_bControlDataBlock[dwPlayerIndex];

    for( DWORD dwControl = 0; dwControl < dwNumControls; dwControl++ )
    {
        size_t* cBufferedData = (size_t *)( bControlData + cByteOffset );
        switch( controlDesc[dwControl].dwDataType )
        {
        case XINPUT2_TYPE_BOOL:
            swprintf_s( szBuffer, L"%s: %i", controlDesc[dwControl].szName,
                *cBufferedData );
            cByteOffset += sizeof(size_t);
            break;

        case XINPUT2_TYPE_DWORD:
            swprintf_s( szBuffer, L"%s: %i", controlDesc[dwControl].szName, 
                *(DWORD *)cBufferedData );
            cByteOffset += sizeof(DWORD);
            break;

        case XINPUT2_TYPE_FLOAT:
            swprintf_s( szBuffer, L"%s: %0.2f", controlDesc[dwControl].szName,
                *(FLOAT *)cBufferedData );
            cByteOffset += sizeof(FLOAT);
            break;

        case XINPUT2_TYPE_POINT:
            swprintf_s( szBuffer, L"%s: %0.2f, %0.2f", controlDesc[dwControl].szName,
                (*(XINPUT2_POINT *)cBufferedData).x,
                (*(XINPUT2_POINT *)cBufferedData).y );
            cByteOffset += sizeof(XINPUT2_POINT);
            break;
        }

        m_Font.DrawText( fDrawX, fDrawY, TEXT_COLOR, szBuffer, ATGFONT_LEFT );
        fDrawY += fLineHeight;
    }
}


//--------------------------------------------------------------------------------------
// Name: SimpleXInput2::UpdateMicLEDs()
// Desc: Demonstrates usage of XInput2SetVector and how to control mic LED colors.
//--------------------------------------------------------------------------------------
void SimpleXInput2::UpdateMicLEDs( const DWORD dwPlayerIndex, const XINPUT2CONTEXT& context )
{
    static DWORD dwLEDChangeTick = 0;
    static DWORD dwLEDIndex = 0;

    // Is it time to change the current LED values?
    if( ( GetTickCount() - dwLEDChangeTick ) > LED_SAMPLE_FREQUENCY )
    {   
        // First, make sure the last asynchronous update has completed (if one in flight)
        if( m_fAsyncLEDReqPending )
        {
            if( TRUE == XHasOverlappedIoCompleted( &m_LEDChangeOverlapped ) )
            {
                 // Pending IO request has completed, ok to fire off another one.
                m_fAsyncLEDReqPending = FALSE;
             }
            else
            {
               // Just return if it has not completed
                return;
           }
        }

        // Reset the overlapped structure
        ZeroMemory( &m_LEDChangeOverlapped, sizeof( XOVERLAPPED ) );

        // Begin an asynchronous update on the microphone LED values.
        if ( FALSE != XInput2BeginUpdate( context ) )
        {
            // Set the LED colors
            XInput2SetVector( context, XINPUTID_LEDS, g_LEDPresets[dwLEDIndex] );

            // XInput2EndUpdate() can fail with an error, or fail if the update is pending...
            if ( FALSE == XInput2EndUpdate( context, &m_LEDChangeOverlapped ) )
            {
                if ( ERROR_IO_PENDING == GetLastError() )
                {
                    m_fAsyncLEDReqPending = TRUE;
                }
                else
                {
                    printf( "LipsSampleProcessThread() XInput2EndUpdate() failed for (%x) with (%d)\n",
                                          dwPlayerIndex, GetLastError() );
                }
            }
        }

        // Advance to the next LED preset
        dwLEDIndex++;
        if( dwLEDIndex >= LED_PRESET_COUNT )
        {
            dwLEDIndex = 0;
        }

        dwLEDChangeTick = GetTickCount();
    }        
}


//--------------------------------------------------------------------------------------
// Name: SimpleXInput2::Render()
// Desc: Called once per frame, the call is the entry point for all updates and drawing
//--------------------------------------------------------------------------------------
HRESULT SimpleXInput2::Render()
{
    XINPUT2CONTEXT context;
    BOOL changed;

    memset( &context, 0, sizeof(context) );

    // Draw a gradient filled background
    ATG::RenderBackground( TOP_BACK_COLOR, BOTTOM_BACK_COLOR );
    m_Timer.MarkFrame();
    m_Font.Begin();

    for( DWORD dwPlayerIndex = 0; dwPlayerIndex < XUSER_MAX_COUNT; dwPlayerIndex++ )
    {
        // Enumerate gamepads for all users
        if( XInput2Sample( XInput2InputDevice(dwPlayerIndex), &context, &changed ) )
        {
            if( EINPUTDEVICE_NOTCONNECTED == m_eInputDeviceState[dwPlayerIndex] )
            {
                printf( "Gamepad (%i) connected.\n", dwPlayerIndex ); 

                // A gamepad was just plugged in, query if it's compatible.
                if( XInput2HasProperty( context, XINPUTID_GAMEPAD) )
                {
                    m_eInputDeviceState[dwPlayerIndex] = EINPUTDEVICE_CONNECTEDCOMPATIBLE;
                }
                else
                {
                    // This gamepad isn't compatible with the sample, don't update it.
                    // For example it might be a steering wheel.
                    m_eInputDeviceState[dwPlayerIndex] = EINPUTDEVICE_CONNECTEDINCOMPATIBLE;
                }
            }

            if( changed && EINPUTDEVICE_CONNECTEDCOMPATIBLE == m_eInputDeviceState[dwPlayerIndex] )
            {
                // Gamepad detected a state change, update cached info accordingly.
                UpdateGamepadInput( context, dwPlayerIndex, g_ControlDescGamepad, g_dwNumDescGamepad);
                PrintMsg(dwPlayerIndex, 0, L"Unsupported Device.");
            }

            if( EINPUTDEVICE_CONNECTEDCOMPATIBLE== m_eInputDeviceState[dwPlayerIndex] )
            {
                PrintControllerSummary( context, dwPlayerIndex, g_ControlDescGamepad, g_dwNumDescGamepad );
            }
        }
        else
        {
            // No gamepad plugged in
            if( EINPUTDEVICE_NOTCONNECTED != m_eInputDeviceState[dwPlayerIndex] )
            {
                // Handle disconnect
                m_eInputDeviceState[dwPlayerIndex] = EINPUTDEVICE_NOTCONNECTED;
                printf( "Gamepad (%i) disconnected.\n", dwPlayerIndex ); 
            }

            PrintMsg(dwPlayerIndex, 0, L"Connect a Gamepad.");
        }

        // Enumerate Mic devices for all users
        if( XMicGetStatus( XInput2MicDevice( dwPlayerIndex ) ) == XMICSTATUS_STARTED )
        {
            // Mic is connected, read new inputs if available.
            if( XInput2Sample( XInput2MicDevice( dwPlayerIndex ), &context, &changed) && changed )
            {
                // Query the accelerometer data
                XInput2GetVector( context, XINPUTID_ACCELERATION, &m_MicAccelVector[dwPlayerIndex] );
            }

            // Render on screen
            PrintMicSummary( dwPlayerIndex );

            // Demonstrate LED output
            UpdateMicLEDs( dwPlayerIndex, context );
        }
        else if( XMicGetStatus( XInput2MicDevice( dwPlayerIndex ) ) == XMICSTATUS_CONNECTED )
        {
            // Mic is attempting to connect. Establish connection.

            // Query the capabilites of this microphone, print to TTY
            HRESULT hr = LipsSampleProcessCapabilities( XInput2MicDevice( dwPlayerIndex ) );

            // We have queried capabilities of the mic now create a connection
            if( SUCCEEDED( hr ) )
            {
                DWORD dwError, dwTransferLength;

                dwError = XMicStart( XInput2MicDevice( dwPlayerIndex ), 
                             XMICSAMPLERATE_48000, &dwTransferLength, NULL );

                if( ERROR_SUCCESS != GetLastError() )
                {
                    printf( "LipsSampleProcessThread() XMicStart failed with (%d) for microphone (%d)\n", 
                        dwError, dwPlayerIndex );
                }
            }
        }
        else
        {
            PrintMsg( dwPlayerIndex, 1, L"Connect a Mic." );
        }
    }

    // Draw title text
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, 0xffffffff, L"Simple XInput2 Sample" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

    m_Font.End();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SimpleXInput2::PrintMsg()
// Desc: Helper function for drawing text on the screen for specific user
//--------------------------------------------------------------------------------------
void SimpleXInput2::PrintMsg( const DWORD dwPlayerIndex, const INT iQuadrant, 
                                    const WCHAR *szMsg)
{
    FLOAT fDrawX = ((m_Font.m_rcWindow.x2 - m_Font.m_rcWindow.x1) 
        / XUSER_MAX_COUNT) * (FLOAT)dwPlayerIndex;

    FLOAT fDrawY = iQuadrant ? 350.f : 50.0f;
    const FLOAT fFontScale = 0.5f;
    const FLOAT fLineHeight = m_Font.GetFontHeight() * fFontScale;

    m_Font.SetScaleFactors( fFontScale, fFontScale);
    m_Font.DrawText( fDrawX, fDrawY, TEXT_COLOR, szMsg, ATGFONT_LEFT);
    fDrawY += fLineHeight;
}


//--------------------------------------------------------------------------------------
// Name: SimpleXInput2::PrintMicSummary()
// Desc: Draw text on screen for specific player index
//--------------------------------------------------------------------------------------
void SimpleXInput2::PrintMicSummary( const DWORD dwPlayerIndex )
{
    WCHAR szBuffer[128];
    FLOAT fDrawX = ((m_Font.m_rcWindow.x2 - m_Font.m_rcWindow.x1) 
                         / XUSER_MAX_COUNT) * (FLOAT)dwPlayerIndex;
    FLOAT fDrawY = 350.0f;
    const FLOAT fFontScale = 0.5f;
    const FLOAT fLineHeight = m_Font.GetFontHeight() * fFontScale;

    swprintf_s( szBuffer, L"Microphone %x", dwPlayerIndex );
                    
    m_Font.DrawText( fDrawX, fDrawY, TEXT_COLOR, szBuffer, ATGFONT_LEFT );
   
    fDrawY += fLineHeight;

    // Print the microphone accellerometer vector
    swprintf_s( szBuffer, L"Mic Accel: %0.1f %0.1f %0.1f", 
                    m_MicAccelVector[dwPlayerIndex].x,
                    m_MicAccelVector[dwPlayerIndex].y,
                    m_MicAccelVector[dwPlayerIndex].z );

    m_Font.DrawText( fDrawX, fDrawY, TEXT_COLOR, szBuffer, ATGFONT_LEFT );
   
    fDrawY += fLineHeight;
}

//--------------------------------------------------------------------------------------
// Name: SimpleXInput2::LipsSampleProcessCapabilities()
// Desc: Perform sample microphone get capabilities operations
//--------------------------------------------------------------------------------------
HRESULT SimpleXInput2::LipsSampleProcessCapabilities( DWORD dwMicrophoneIndex )
{
    HRESULT hrReturn = S_OK;
    DWORD dwError;
    XMICCAPABILITIES Capabilities;
    XMICCAPABILITIES* pCapabilities = &Capabilities;

    //
    // Query the capabilities of the microphone device
    dwError = XMicGetCapabilities( dwMicrophoneIndex, pCapabilities );
    if( ERROR_SUCCESS != dwError )
    {
        printf( "LipsSampleProcessCapabilities() XMicGetCapabilities() failed with (%d) for microphone (%x)\n", 
                dwError, dwMicrophoneIndex );
        return E_FAIL;
    }

    printf( "\n[XMicGetCapabilities------------------]\n\n" );
    //
    // Print out the audio characteristics of the microphone
    printf( "dwMicrophoneIndex      (%x)\n", dwMicrophoneIndex );
    printf( "wFormatTag             (%d)\n", pCapabilities->wFormatTag );
    printf( "wBitsPerSample         (%d)\n", pCapabilities->wBitsPerSample );
    printf( "nChannels              (%d)\n", pCapabilities->nChannels );
    printf( "dwFrameLength          (%d)\n", pCapabilities->dwFrameLength );

    //
    // Print out the supported sampling rates
    if( XMICSAMPLERATE_32000 & pCapabilities->dwSampleRatesSupported )
    {
        printf( "dwSampleRatesSupported XMICSAMPLERATE_32000\n" );
    }
    if( XMICSAMPLERATE_48000 & pCapabilities->dwSampleRatesSupported )
    {
        printf( "dwSampleRatesSupported XMICSAMPLERATE_48000\n" );
    }            

    //
    // Print out the features of the microphone
    if( XMICFEATURE_WIRELESS & pCapabilities->dwFeatures )
    {
        printf( "dwFeatures             XMICFEATURE_WIRELESS\n" );   
    }
    if( XMICFEATURE_ACCELEROMETER & pCapabilities->dwFeatures )
    {
        printf( "dwFeatures             XMICFEATURE_ACCELEROMETER\n" );   
    }
    else
    {
        hrReturn = FALSE;
    }

    if( XMICFEATURE_OUTPUTLED & pCapabilities->dwFeatures )
    {
        printf( "dwFeatures             XMICFEATURE_OUTPUTLED\n" );   
    }
    else
    {
        hrReturn = FALSE;
    }

    //
    // Print out the color of the microphone
    switch ( pCapabilities->bColor )
    {
    case XMICCOLOR_BLACK:
        printf( "bColor                 XMICCOLOR_BLACK\n" );
        break;   

    case XMICCOLOR_WHITE:
        printf( "bColor                 XMICCOLOR_WHITE\n" );   
        break;

    case XMICCOLOR_UNKNOWN:
        printf( "bColor                 XMICCOLOR_UNKNOWN\n" );
        break;   
    }            

    printf( "\n[-------------------------------------]\n" );                                  

    return hrReturn;
}