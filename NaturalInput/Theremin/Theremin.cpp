//--------------------------------------------------------------------------------------
// Theremin.cpp
//
// Uses user-selectable control schemes to drive a virtual Theremin (a simple sine-wave
// oscillator with user-controllable pitch and volume).
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xnamath.h>
#include <AtgApp.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgNuiCommon.h>
#include <AtgSimpleShaders.h>
#include <AtgAudio.h>
#include <XAudio2.h>
#include <AtgNuiVisualization.h>

#include <NuiApi.h>

#include "ThereminFilters.h"

#define THEREMIN_TONE_FREQ 8000.0f // unity pitch of audio source waveform that
                                   // simulates the theremin

//--------------------------------------------------------------------------------------
// Call-outs for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_LEFT_BUTTON,    ATG::HELP_PLACEMENT_1, L"Previous Control Scheme" },
    { ATG::HELP_RIGHT_BUTTON,   ATG::HELP_PLACEMENT_1, L"Next Control Scheme" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_1, L"Toggle Linear/Octave Mode" }
};

#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    NUI_SKELETON_FRAME          m_SkeletonFrame;
    BOOL                        m_bPlayerFound;

    CControls           m_ThereminControls;

    IXAudio2*                   m_pXAudio2;
    IXAudio2MasteringVoice*     m_pMasteringVoice;
    IXAudio2SourceVoice*        m_pSourceVoice;

    // Visualization
	ATG::NuiVisualization       m_pip;
	CONST NUI_IMAGE_FRAME*      m_pImageFrame;
	CONST NUI_IMAGE_FRAME*      m_pDepthFrame;
	HANDLE                      m_hImage;
	HANDLE                      m_hDepth;
	HANDLE                      m_hFrameEndEvent;

private:
    HRESULT Initialize();
    HRESULT StartSensorArray();

    HRESULT Update();
    HRESULT UpdateNuiTracking();
    void HandleGamepadInput();
    VOID ApplyTiltCorrection();

    HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth,
                           &atgApp.m_d3dpp.BackBufferHeight );

    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    atgApp.Run();
}

//--------------------------------------------------------------------------------------
// Name: Start Sensor Array
// Desc: Initializes the sensor array, and the Nui libraries.
//--------------------------------------------------------------------------------------
HRESULT Sample::StartSensorArray()
{
    // Create event which will be signaled when frame processing ends
    m_hFrameEndEvent = CreateEvent( NULL,
                                    FALSE,  // auto-reset
                                    FALSE,  // create unsignaled
                                    "NuiFrameEndEvent" );

    if ( !m_hFrameEndEvent )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent\n" );
        return E_FAIL;
    }

    // Initialize the camera on the default worker thread, synchronously
    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON |
                                NUI_INITIALIZE_FLAG_USES_COLOR |
                                NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX,
                                NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );
    if ( FAILED(hr) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

    // Register frame end event with NUI
    hr = NuiSetFrameEndEvent( m_hFrameEndEvent, 0 );
    if( FAILED(hr) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }

    // Open color and depth streams for debug visualizations
    //
    // Color stream
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 0, 1,
                             NULL, &m_hImage );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    // Open the depth stream
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX,
                             NUI_IMAGE_RESOLUTION_320x240, 0, 1, NULL, &m_hDepth );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    hr = NuiSkeletonTrackingEnable( NULL, 0 );

    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr = S_OK;

    // Start the NUI Sensor Array

    hr = StartSensorArray();
    if ( FAILED( hr ) )
        return hr;
    
    // Initialize base member variables    

    m_bDrawHelp = FALSE;

    // Create the font

    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area

    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help

    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;    

    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create XAudio2

    if( FAILED( hr = XAudio2Create( &m_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR ) ) )
        return hr;

    // Create a mastering voice

    if( FAILED( hr = m_pXAudio2->CreateMasteringVoice( &m_pMasteringVoice,
                     XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, NULL
                     ) ) )
    {
        m_pXAudio2->Release();
        return hr;
    }

    // Read the wave file

    ATG::WaveFile WaveFile;
    if( FAILED( hr = WaveFile.Open( "game:\\Media\\Sounds\\Sine8kHz.wav" ) ) )
        ATG::FatalError( "Error %#X opening WAV file\n", hr );

    // Read the format header

    WAVEFORMATEXTENSIBLE wfx = {0};
    if( FAILED( hr = WaveFile.GetFormat( &wfx ) ) )
        ATG::FatalError( "Error %#X reading WAV format\n", hr );

    // Calculate how many bytes and samples are in the wave

    DWORD cbWaveSize = 0;
    WaveFile.GetDuration( &cbWaveSize );

    // Read the sample data into memory

    BYTE* pbWaveData = new BYTE[ cbWaveSize ];
    if( FAILED( hr = WaveFile.ReadSample( 0, pbWaveData, cbWaveSize, &cbWaveSize ) ) )
        ATG::FatalError( "Error %#X reading WAV data\n", hr );

    // Play the wave using a new XAudio2SourceVoice

    // Create the source voice
    
    if( FAILED( hr = m_pXAudio2->CreateSourceVoice( &m_pSourceVoice,
                                                    ( WAVEFORMATEX* )&wfx ) ) )
        ATG::FatalError( "Error %#X creating source voice\n", hr );

    // Submit the wave sample data using an XAUDIO2_BUFFER structure
    
    XAUDIO2_BUFFER buffer = {0};
    buffer.pAudioData = pbWaveData;
    buffer.Flags = XAUDIO2_END_OF_STREAM;  // tell the source voice not to expect any
                                           // data after this buffer
    buffer.AudioBytes = cbWaveSize;
    buffer.LoopCount = XAUDIO2_LOOP_INFINITE;

    if( FAILED( hr = m_pSourceVoice->SubmitSourceBuffer( &buffer ) ) )
        ATG::FatalError( "Error %#X submitting source buffer\n", hr );

    hr = m_pSourceVoice->Start( 0 );

    if( FAILED( m_pip.Initialize( m_pd3dDevice,
                                  NUI_INITIALIZE_FLAG_USES_COLOR |
                                  NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                                  NUI_INITIALIZE_FLAG_USES_SKELETON, 
		                          NUI_IMAGE_RESOLUTION_640x480 ) ) )
    {
        ATG_PrintError ( "Picture in Picture initialization failed" );
	}

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame to update the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    HandleGamepadInput();

    HRESULT hr = UpdateNuiTracking();
    if ( FAILED( hr ) )
        return hr;

    // Pass the latest skeletal data to the control scheme for processing.

    m_ThereminControls.Update( (FLOAT)m_Timer.GetElapsedTime(), &m_SkeletonFrame );

    // Update the volume and pitch of the audio output.

    m_pSourceVoice->SetVolume( m_ThereminControls.GetVolume() , XAUDIO2_COMMIT_NOW);
    m_pSourceVoice->SetFrequencyRatio( m_ThereminControls.GetPitch() /
                                       THEREMIN_TONE_FREQ, XAUDIO2_COMMIT_NOW);

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ApplyTiltCorrection()
// Desc: Corrects the skeleton data so that the up vector (0. 1. 0) matches the
//       real-world value from the hardware accelerometer.
//--------------------------------------------------------------------------------------
VOID Sample::ApplyTiltCorrection()
{
    // In this release only, until final hardware with built in accelerometer ships,
    // we need to check for an invalid up vector (we will synthesize it from
    // the floor plane if that data is present). If we can't get an up
    // vector, we default to 0.0, 1.0, 0.0 instead.

    XMVECTOR& vNormToGrav = m_SkeletonFrame.vNormalToGravity;

    if ( fabs(vNormToGrav.x) < FLT_EPSILON &&
        fabs(vNormToGrav.y) < FLT_EPSILON &&
        fabs(vNormToGrav.z) < FLT_EPSILON )
    {
        static const XMVECTOR c_vUp = { 0.0, 1.0, 0.0, 0.0 };
        vNormToGrav = c_vUp;
    }

    // Generate the leveling matrix and apply it to all points on any skeletons
    // which are currently being tracked. 

    XMMATRIX matLevel = NuiTransformMatrixLevel( vNormToGrav );
    for ( UINT i = 0 ; i < NUI_SKELETON_COUNT ; ++i )
    {
        if ( m_SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED )
        {
            for ( UINT j = 0; j < NUI_SKELETON_POSITION_COUNT ; ++j )
            {
                m_SkeletonFrame.SkeletonData[i].SkeletonPositions[j] = XMVector3Transform( m_SkeletonFrame.SkeletonData[i].SkeletonPositions[j], matLevel );
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame to render the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff00ffff );

    // Set default states
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    
    // Show title, frame rate, and help
    m_Timer.MarkFrame();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Draw the raw depth and image map with skeleton overlaid as visualization.

        const FLOAT drawWidth = 200.0f;
        const FLOAT drawHeight = drawWidth * 3.0f / 4.0f;
        const FLOAT drawX = 50.0f;
        const FLOAT drawY = 720.0f - 50.0f - drawHeight;

        m_pip.BeginRender();
        m_pip.RenderColorStream( drawX, drawY, drawWidth, drawHeight );
        DWORD dwTracked = m_ThereminControls.GetTrackedPlayer();
        
        if ( dwTracked != NO_TRACKED_SKELETON )
        {
            m_pip.RenderSingleSkeleton( m_ThereminControls.GetTrackedPlayer(),
                                         drawX, drawY, drawWidth, drawHeight,
                                         FALSE, TRUE );
        }

        m_pip.RenderDepthStream( drawX + drawWidth + 10, drawY, drawWidth, drawHeight );
		
        if ( dwTracked != NO_TRACKED_SKELETON )
        {
            m_pip.RenderSingleSkeleton( m_ThereminControls.GetTrackedPlayer(),
                                         drawX + drawWidth + 10, drawY, drawWidth,
                                         drawHeight );
        }
        m_pip.EndRender();
        
        WCHAR buf[256];
        m_Font.Begin();
        m_Font.DrawText( 0, 0, 0xffffff00, L"Theremin" );

        swprintf_s( buf, 256, L"Control Scheme: %s",
                    m_ThereminControls.GetControlSchemeName() );
        m_Font.DrawText( 0, 30, 0xFFFFFF00, buf );

        m_Font.DrawText( 500, 30, 0xffffffff,
                         m_ThereminControls.IsOctaveMode() ? L"Octave Mode" :
                                                             L"Linear Mode" );
        m_Font.DrawText( 470, 30, 0xffffffff, GLYPH_X_BUTTON );


        if ( m_ThereminControls.IsTracking() )        
        {
            m_ThereminControls.RenderDebug(m_Font);
        }
        else
        {
            m_Font.DrawText( 0, 60, 0xffffffff, L"(No player detected)" );
        }

        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: HandleGamepadInput
// Desc: Reads the game-pad's current state, and modifies the sensor-array control
//       scheme accordingly.
//--------------------------------------------------------------------------------------
void Sample::HandleGamepadInput()
{
    // Get the current gamepad status

    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help screen on/off

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Cycle the control schemes

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        m_ThereminControls.NextControlScheme();
    }

    else if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
    {
        m_ThereminControls.PrevControlScheme();
    }

    // Toggle between linear and octave pitch modes.

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_ThereminControls.SetOctaveMode( !m_ThereminControls.IsOctaveMode() );
    }
}


//--------------------------------------------------------------------------------------
// Name: UpdateNuiTracking
// Desc: Reads the latest data from the sensor array.
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateNuiTracking()
{
    if( WAIT_OBJECT_0 != WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        return E_FAIL;
    }

    // Gets the latest color buffer image from the device
    HRESULT hrImage = NuiImageStreamGetNextFrame( m_hImage, 0, &m_pImageFrame );
    // Gets the latest depth buffer image from the device
    HRESULT hrDepth = NuiImageStreamGetNextFrame( m_hDepth, 0, &m_pDepthFrame );
    // Gets the latest skeletal tracking data from the device
    HRESULT hrSkeleton = NuiSkeletonGetNextFrame( 0, &m_SkeletonFrame );

    if( SUCCEEDED( hrImage ) )
    {      
        m_pip.SetColorTexture( m_pImageFrame->pFrameTexture );		
        NuiImageStreamReleaseFrame( m_hImage, m_pImageFrame );
    }

    if( SUCCEEDED( hrDepth ) )
    {
        m_pip.SetDepthTexture( m_pDepthFrame->pFrameTexture );	
        NuiImageStreamReleaseFrame( m_hDepth, m_pDepthFrame );
    }

    // If we don't have pending data immediately, just carry on with last frame's data.
    if ( FAILED( hrSkeleton ) && hrSkeleton != E_PENDING )
    {
        return E_FAIL;
    }
 
    m_pip.SetSkeletons( &m_SkeletonFrame );

    // Apply tilt correction to the frame.
  
    ApplyTiltCorrection();

    // Perform data smoothing.

    HRESULT hr = NuiTransformSmooth( &m_SkeletonFrame, NULL );

    // return the first error (other than skeleton failure)
    if( FAILED( hrImage ) )
    {
        hr = hrImage;
    }
    else if( FAILED( hrDepth ) )
    {
        hr = hrDepth;
    }
    
    return hr;
 }
