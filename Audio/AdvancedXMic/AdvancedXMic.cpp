//--------------------------------------------------------------------------------------
// AdvancedXMic.cpp
//
// Demonstrates use of the XMic API, currently used for the Xbox360 wireless Microphone.
// Also demonstrates how to draw a spectrometer using XAudio2.
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
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>
#include <XAudio2.h>
#include "MonitorAPOWidget.h"
#include <XMic.h>
#include "XMicInclude.h"

//
// XAudio2 subsystem
//
IXAudio2* g_pXAudio2 = NULL;

//
// Define an LED preset which are the 3 color components along with a friendly name to
// display to the user
//

struct LEDPreset
{
    BYTE                bValues[3];    // red/green/blue byte values
    CHAR                szName[16];    // friendly name for us to work with
};

//
// Some basic known color values that we know about
//
LEDPreset m_LEDPresets[] =
{
    { 0xff, 0x00, 0x00, "Red"    },
    { 0x00, 0xff, 0x00, "Green"  },
    { 0xff, 0x80, 0x00, "Orange" },
    { 0x00, 0xff, 0xff, "Cyan"   },
// unused colors
    { 0x00, 0x00, 0xff, "Blue"   },
    { 0xff, 0xff, 0x00, "Yellow" },
    { 0xff, 0x00, 0xff, "Pink"   },
    { 0xff, 0x80, 0xff, "Violet" },
    { 0x80, 0x40, 0x00, "Brown"  },
};

//
// The number of presets
//
#define LED_PRESET_COUNT                ( ARRAYSIZE(m_LEDPresets) )
#define LED_SAMPLE_FREQUENCY            50


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    XMicSample atgApp;
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    atgApp.Run();
}

//--------------------------------------------------------------------------------------
// Name: XMicSample::XMicSampleInitXAudio2
// Desc: Initialize the XAudio2 subsystem.
//--------------------------------------------------------------------------------------
HRESULT XMicSample::XMicSampleInitXAudio2( void )
{
    HRESULT hr;

    // Create XAudio2 engine with default options
    hr = XAudio2Create( &g_pXAudio2 );

    // Create a 6-channel 48 kHz mastering voice feeding the default device
    IXAudio2MasteringVoice* pMasteringVoice;
    g_pXAudio2->CreateMasteringVoice( &pMasteringVoice, XAUDIO2_DEFAULT_CHANNELS,
        XAUDIO2_DEFAULT_SAMPLERATE);

    if( !SUCCEEDED( hr ) )
    {
        ATG::DebugSpew( "XMicSample::XMicSampleInitXAudio2() failed to initialize XAudio\n" );
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects and does general setup.
//--------------------------------------------------------------------------------------
HRESULT XMicSample::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );
    m_fAsyncLEDReqPending = FALSE;

    // Initialize Audio Subsystem
    XMicSampleInitXAudio2();

    HRESULT hr;
    if( FAILED( hr = m_resource.Create( "game:\\Media\\Resource.xpr" ) ) )
        ATG::FatalError( "Error %#X opening resource file\n", hr );

    m_pTexture = m_resource.GetTexture( "UI_Background" );

    // Initialize each mic (up to XUSER_MAX_COUNT supported)
    for( DWORD dwPlayerIndex = 0; dwPlayerIndex < XUSER_MAX_COUNT; dwPlayerIndex++ )
    {
        m_XMicInstance[dwPlayerIndex].Initialize( dwPlayerIndex, m_pd3dDevice, &m_resource );
    }

    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create a seperate thread to update microphones, as they require an APC callback.
    // We do not recommend running this on the main update thread.
    HANDLE hXMicThread = CreateThread( NULL, 0, UpdateMicThread, this, 0, NULL );
    if( hXMicThread == NULL )
    {
        ATG::FatalError( "Error creating microphones update thread\n" );
    }
    XSetThreadProcessor( hXMicThread, 2 );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: XMicSample::Render()
// Desc: Called once per frame, the call is the entry point for all updates and drawing
//--------------------------------------------------------------------------------------
HRESULT XMicSample::Render()
{
    XINPUT2CONTEXT context;

    ZeroMemory( &context, sizeof(context) );

    // Draw the background
    const D3DRECT g_rcBackground = { 0, 0, 640, 480 };
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( g_rcBackground, m_pTexture );

    m_Timer.MarkFrame();
    m_Font.Begin();

    // Draw title text
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, 0xffffffff, L"Advanced XMic Sample" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

    m_Font.End();

    // Draw all widgets (one per mic)
    for( DWORD dwPlayerIndex = 0; dwPlayerIndex < XUSER_MAX_COUNT; dwPlayerIndex++ )
    {
        m_XMicInstance[dwPlayerIndex].Render( m_pd3dDevice );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: XMicSample::`Thread()
// Desc: Update mics in a separate thread as they require APC callback signal
//--------------------------------------------------------------------------------------
DWORD WINAPI XMicSample::UpdateMicThread( LPVOID lpParameter )
{
    // Give this thread a name
    ATG::SetThreadName( GetCurrentThreadId(), "XMic_Update_Thread" );

    XMicSample* pSample = (XMicSample*)lpParameter;

    for( ;; )
    {
        // Update each connected mic (or attempt to connect, if not connected)
        for( DWORD dwPlayerIndex = 0; dwPlayerIndex < XUSER_MAX_COUNT; dwPlayerIndex++ )
        {
            pSample->m_XMicInstance[dwPlayerIndex].Update( (float)pSample->m_Timer.GetAppTime() );
        }

        // Because this app isn't locked at 30fps, it runs continously never giving a chance
        // for other threads to run.  We must call Sleep once per frame so XAudio2 can call
        // our VoiceCallback class.
        SleepEx( 1, true );
    }

}

//--------------------------------------------------------------------------------------
// Name: XMicSample::Update()
// Desc: Called once per frame, the call is the entry point for all updates
//--------------------------------------------------------------------------------------
HRESULT XMicSample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Handle gamepad
    if( pGamepad->wPressedButtons )
    {
        for( DWORD dwPlayerIndex = 0; dwPlayerIndex < XUSER_MAX_COUNT; dwPlayerIndex++ )
        {
            // Let the widgets handle button presses
            m_XMicInstance[dwPlayerIndex].HandleInput( pGamepad );
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: XMicInstance::Initialize
// Desc: Initialize internal XMic data structure
//--------------------------------------------------------------------------------------
BOOL XMicInstance::Initialize(DWORD dwUserIndex, D3DDevice *pd3ddevice, ATG::PackedResource
                          *presource)
{
    m_dwDeviceIndex               = (DWORD)dwUserIndex;
    m_dwMicrophoneIndex           = XInput2MicDevice( m_dwDeviceIndex );
    m_pVoice                      = NULL;
    m_dwLEDIndex                  = 0;
    m_dwLEDChangeTick             = 0;
    m_DeviceContext               = 0;

    // Initialize visualizer
    m_pMonitor = new ATG::MonitorAPOPipe;
    if (!m_pMonitor)
        ATG::FatalError( "Failed to allocate MonitorAPOPipe\n" );

    m_pWidget = new CMonitorAPOWidget( m_pMonitor );
    if (NULL == m_pWidget)
        ATG::FatalError( "Failed to allocate CMonitorAPOWidget\n" );

    m_pWidget->Init( pd3ddevice, presource );

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: XMicInstance::HandleInput
// Desc: Pass input through to widget
//--------------------------------------------------------------------------------------
void XMicInstance::HandleInput( ATG::GAMEPAD* pGamepad )
{
    m_pWidget->HandleInput( pGamepad );
}


//--------------------------------------------------------------------------------------
// Name: XMicInstance::Connect
// Desc: Query capabilties of mic, and if compatible, start the mic.
//--------------------------------------------------------------------------------------
BOOL XMicInstance::Connect()
{
    HRESULT hr;
    DWORD dwError;

    DWORD dwMicrophoneSampleRate = 0;
    DWORD dwTransferLength = 0;
    XMICCAPABILITIES Capabilities;
    BOOL fWirelessMicrophone = FALSE;
    DWORD dwFrameCount = 0;

    // if not yet connected, connect mic and initialize all sub systems
    hr = XMicSampleProcessCapabilities( m_dwMicrophoneIndex, &Capabilities );

    if( SUCCEEDED( hr ) )
    {
        // Is this a wireless microphone?
        fWirelessMicrophone = ( Capabilities.dwFeatures & XMICFEATURE_WIRELESS ) ? TRUE : FALSE;

        // Determine the sampling rate for this microphone.
        dwMicrophoneSampleRate = ( Capabilities.dwSampleRatesSupported & XMICSAMPLERATE_32000 );
        if( 0 == dwMicrophoneSampleRate )
        {
            dwMicrophoneSampleRate = XMICSAMPLERATE_48000;
        }

        // Create a source voice for this microphone.
        hr = XMicSampleCreateVoice( m_dwMicrophoneIndex, &m_pVoice, dwMicrophoneSampleRate,
            m_pMonitor );
    }

    // We have queried capabilities and have created an appropriate source voice, now start the microphone.
    if( SUCCEEDED( hr ) )
    {
        ATG::DebugSpew( "XMicInstance::Connect() starting microphone (%x)\n", m_dwMicrophoneIndex );

        // Attempt to start the microphone device. We will wait for this request to complete.
        dwError = XMicStart( m_dwMicrophoneIndex, dwMicrophoneSampleRate, &dwTransferLength, NULL );

        if( ( ERROR_SUCCESS != dwError ) )
        {
            ATG::DebugSpew( "XMicInstance::Connect() XMicStart failed with (%d) for microphone (%d)\n", dwError, m_dwMicrophoneIndex );
            hr = E_FAIL;
        }
    }


    // Query the microphone gain, and set the gain to a mid-level value
    if( SUCCEEDED( hr ) )
    {
        hr = XMicSampleProcessGain( m_dwMicrophoneIndex );
    }

    // Prime the pump for audio data acquisition
    if( SUCCEEDED( hr ) )
    {
        ATG::DebugSpew( "XMicInstance::Connect() microphone (%x) making audio data requests\n", m_dwMicrophoneIndex );

        dwFrameCount = fWirelessMicrophone ? XMIC_WIRELESS_FRAME_COUNT : XMIC_WIRED_FRAME_COUNT;

        m_MCD1.m_dwMicrophoneIndex          = m_dwMicrophoneIndex;
        m_MCD1.dwFrameCount               = dwFrameCount;
        m_MCD1.m_pVoice                     = m_pVoice;
        m_MCD1.pOverlap                   = &m_DataRequestOverlapped1;
        m_MCD1.dwAudioFrameBufferLength   = dwTransferLength;
        m_MCD1.pAudioFramesBuffer         = new BYTE[ dwTransferLength * dwFrameCount ];

        m_MCD2.m_dwMicrophoneIndex          = m_dwMicrophoneIndex;
        m_MCD2.dwFrameCount               = dwFrameCount;
        m_MCD2.m_pVoice                     = m_pVoice;
        m_MCD2.pOverlap                   = &m_DataRequestOverlapped2;
        m_MCD2.dwAudioFrameBufferLength   = dwTransferLength;
        m_MCD2.pAudioFramesBuffer         = new BYTE[ dwTransferLength * dwFrameCount ];

        m_DataRequestOverlapped1.dwExtendedError     = 0;
        m_DataRequestOverlapped1.hEvent              = NULL;
        m_DataRequestOverlapped1.pCompletionRoutine  = XMicSampleDataReady;
        m_DataRequestOverlapped1.dwCompletionContext = (DWORD_PTR)&m_MCD1;

        m_DataRequestOverlapped2.dwExtendedError     = 0;
        m_DataRequestOverlapped2.hEvent              = NULL;
        m_DataRequestOverlapped2.pCompletionRoutine  = XMicSampleDataReady;
        m_DataRequestOverlapped2.dwCompletionContext = (DWORD_PTR)&m_MCD2;

        // Send out an initial two requests
        dwError = XMicRequestData( m_MCD1.m_dwMicrophoneIndex, m_MCD1.dwFrameCount,
            m_MCD1.pAudioFramesBuffer, m_MCD1.wFrameLengths, m_MCD1.pOverlap );

        if( ERROR_IO_PENDING != dwError )
        {
            ATG::DebugSpew( "XMicInstance::Connect() XMicRequestData(1) failed for (%x) with (%d)\n",
                m_dwMicrophoneIndex, dwError );
            hr = E_FAIL;
        }

        if( SUCCEEDED( hr ) )
        {
            dwError = XMicRequestData( m_MCD2.m_dwMicrophoneIndex, m_MCD2.dwFrameCount,
                m_MCD2.pAudioFramesBuffer, m_MCD2.wFrameLengths, m_MCD2.pOverlap );

            if( ERROR_IO_PENDING != dwError )
            {
                ATG::DebugSpew( "XMicInstance::Connect() XMicRequestData(2) failed for (%x) with (%d)\n",
                    m_dwMicrophoneIndex, m_dwDeviceState );
                hr = E_FAIL;
            }
        }
    }

    return TRUE;
}

//--------------------------------------------------------------------------------------
// Name: XMicInstance::Update
// Desc: Per-mic general update function
//--------------------------------------------------------------------------------------
BOOL XMicInstance::Update(const float fElapsedTime)
{

    const DWORD m_dwDeviceStateOld = m_dwDeviceState;
    m_dwDeviceState = XMicGetStatus( m_dwMicrophoneIndex );

    // If the mic is attemping to make connection with the xbox, initialize it.
    if( XMICSTATUS_CONNECTED == m_dwDeviceState )
    {
        Connect();
        m_dwDeviceState = XMicGetStatus( m_dwMicrophoneIndex );
    }

    // The mic is now connected and initialized, so run the update loop.
    if( XMICSTATUS_STARTED == m_dwDeviceState )
    {
        // Ensure that the microphone is still connected.
        m_dwDeviceState = XMicGetStatus( m_dwMicrophoneIndex );

        if( XMICSTATUS_STARTED != m_dwDeviceState )
        {
            ATG::DebugSpew( "XMicInstance::Update() m_dwDeviceState is (%d) for microphone (%x)\n",
                m_dwDeviceState, m_dwMicrophoneIndex );
            return FALSE;;
        }

        // Query the device context for the wireless microphone.  Failure is acceptable,
        // it means this Mic is not wireless and does not support XInput2 features.
        BOOL fDeviceChanged = 0;
        if( ( TRUE == XInput2Sample( m_dwMicrophoneIndex, &m_DeviceContext, &fDeviceChanged ) ) )
        {
            // Ensure that this device has LEDs and an accelerometer
            if( ( TRUE == XInput2HasControl( m_DeviceContext, XINPUTID_LEDS ) )
                && ( TRUE == XInput2HasControl( m_DeviceContext, XINPUTID_ACCELERATION ) ) )
            {
                // Query the accelerometer data
                if( FALSE == XInput2GetVector( m_DeviceContext, XINPUTID_ACCELERATION, &m_XMicAccelVector ) )
                {
                    ATG::DebugSpew( "XMicInstance::Update() failed for (%d) with (%d)\n",
                        m_dwDeviceIndex, GetLastError() );
                }

                // Is it time to change the current LED values?
                if( ( GetTickCount() - m_dwLEDChangeTick ) > LED_SAMPLE_FREQUENCY )
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
                            return FALSE;
                       }
                    }

                    // Reset the overlapped structure
                    ZeroMemory( &m_LEDChangeOverlapped, sizeof( XOVERLAPPED ) );

                    // Get the new LED colors
                    XINPUT2_VECTOR LEDValues;
                    const float fSinRangeGlow = 0.7f;
                    const float fColorScale = ( 1.0f - fSinRangeGlow ) + fSinRangeGlow * (0.5f + 0.5f * sin( fElapsedTime * 5.0f + (float)m_dwDeviceIndex ) );

                    LEDValues.x = (FLOAT) ( m_LEDPresets[ m_dwDeviceIndex ].bValues[0] * fColorScale / 255.0f );
                    LEDValues.y = (FLOAT) ( m_LEDPresets[ m_dwDeviceIndex ].bValues[1] * fColorScale / 255.0f  );
                    LEDValues.z = (FLOAT) ( m_LEDPresets[ m_dwDeviceIndex ].bValues[2] * fColorScale / 255.0f  );

                    // Begin an asynchronous update on the microphone LED values.
                    if ( FALSE != XInput2BeginUpdate( m_DeviceContext ) )
                    {
                        // Set the LED colors
                        XInput2SetVector( m_DeviceContext, XINPUTID_LEDS, LEDValues );

                        // XInput2EndUpdate() can fail with an error, or fail if the update is pending...
                        if ( FALSE == XInput2EndUpdate( m_DeviceContext, &m_LEDChangeOverlapped ) )
                        {
                            if ( ERROR_IO_PENDING == GetLastError() )
                            {
                                m_fAsyncLEDReqPending = TRUE;
                            }
                            else
                            {
                                printf( "XMicInstance::Update() XInput2EndUpdate() failed for (%x) with (%d)\n",
                                    m_dwMicrophoneIndex, GetLastError() );
                            }
                        }
                    }


                    // Advance to the next LED preset
                    m_dwLEDIndex++;
                    if( m_dwLEDIndex >= LED_PRESET_COUNT )
                    {
                        m_dwLEDIndex = 0;
                    }

                    m_dwLEDChangeTick = GetTickCount();
                }
            }
        }
    }
    else if( XMICSTATUS_STARTED != m_dwDeviceState && XMICSTATUS_STARTED == m_dwDeviceStateOld )
    {
        //NOTE: You shouldn't do this on your main thread, as this is a blocking call.
        //      ideally, all voices should be shutdown on another "garbage collector" thread.
        m_pVoice->DestroyVoice();
        m_pVoice = NULL;

        m_pWidget->Reset();
    }

	UpdateWidget();

    return TRUE;
}

//--------------------------------------------------------------------------------------
// Name: XMicInstance::Render
// Desc: Per-mic draw function
//-----------------------
BOOL XMicInstance::Render( D3DDevice* pd3ddevice )
{
    const INT g_rackLeft = 19;
    const INT g_rackTop = 100;
    const D3DRECT g_rcRackSpace = { 0, 0, 602, 82 };

    D3DRECT rcWidget = g_rcRackSpace;

    rcWidget.y1 += 82 * m_dwDeviceIndex;
    rcWidget.y2 += 82 * m_dwDeviceIndex;

    OffsetRect( (LPRECT)&rcWidget, g_rackLeft, g_rackTop );
    m_pWidget->Render( pd3ddevice, rcWidget, XMICSTATUS_STARTED == m_dwDeviceState );

    if( XMICSTATUS_STARTED == m_dwDeviceState )
    {
        const XMFLOAT2 fSize = XMFLOAT2( 7.f, 7.0f );
        const XMFLOAT2 fOrigin = XMFLOAT2( (FLOAT)rcWidget.x1, (FLOAT)rcWidget.y1 );
        D3DCOLOR color = D3DCOLOR_RGBA( m_LEDPresets[ m_dwDeviceIndex ].bValues[0],
            m_LEDPresets[ m_dwDeviceIndex ].bValues[1],
            m_LEDPresets[ m_dwDeviceIndex ].bValues[2], 255 );

        ATG::DebugDraw::DrawScreenSpaceRect( fOrigin, fSize, 5.0f, color );
    }

    return TRUE;
}
