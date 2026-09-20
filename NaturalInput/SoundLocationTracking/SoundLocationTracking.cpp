//----------------------------------------------------------------------------------------------------------------------
// SoundLocationTracking.cpp
// 
// This sample shows how to use the NuiAudio APIs to capture audio, and to steer the beam-former to improve audio
// fidelity for specific speakers in the room.
// 
// Demonstrated techniques include Manual Beam Steering (to a specific angle), Automated Beam Steering (to the loudest
// audio source), and Beam Steering towards a Tracked player. Also shown is the use of the two audio pipelines, and how
// to switch between them.
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (c) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------


#include <xtl.h>
#include <NuiAudio.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgNuiCommon.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
typedef FLOAT FLOAT32; // 32-bit IEEE float
#include "XDSP.h"
#include <xnamath.h>

#include "Visualization.h"


//----------------------------------------------------------------------------------------------------------------------
// Color values used in this sample
//----------------------------------------------------------------------------------------------------------------------
#define TOP_BACK_COLOR                  0xff00007f  // Background gradient colors
#define BOTTOM_BACK_COLOR               0xff000000
#define TEXT_COLOR                      0xffffffff  // Text rendering color
#define DATA_COLOR                      0xffff00ff  // Data rendering color


//----------------------------------------------------------------------------------------------------------------------
// Direction tracking constants
//----------------------------------------------------------------------------------------------------------------------
#define SPEAKER_LOCK_ANGLE_RANGE             0.2f     // Max difference between angle of skeleton from sensor origin and
                                                      // beam former position from sensor origin to consider it a lock.

CONST FLOAT NUM_COARSE_STEERING_STEPS      = 10.0f;   // Number of steps by which we allow coarse steering on D-Pad.
CONST FLOAT BEAM_STEERING_COARSE_TUNE_STEP = ( NUIAUDIO_MICARRAY_BEAM_ANGLE_MAX - NUIAUDIO_MICARRAY_BEAM_ANGLE_MIN )
                                            / NUM_COARSE_STEERING_STEPS;


//----------------------------------------------------------------------------------------------------------------------
// Audio visualization constants
//----------------------------------------------------------------------------------------------------------------------

// NUIAUDIO_MAX_DURATION is defined explicitly in the docs as ALWAYS being 256. Just to be safe, we'll make sure
// that's the case at compile-time...

#if NUIAUDIO_MAX_DURATION != 256
    #error NUIAUDIO_MAX_DURATION's value has changed since publication. Update LOG2_NUM_AUDIO_SAMPLES below to match.
#endif

CONST SIZE_T NUM_AUDIO_SAMPLES         = NUIAUDIO_MAX_DURATION; // Note: NuiAudio will ALWAYS return 256 values.
CONST UINT32 LOG2_NUM_AUDIO_SAMPLES    = 8;                     // for NUM_AUDIO_SAMPLES = 256, this is 8.
CONST SIZE_T NUM_FLOAT32_PER_XVECTOR   = sizeof(XDSP::XVECTOR) / sizeof(FLOAT32);
CONST SIZE_T NUM_AUDIO_SAMPLES_XVECTOR = NUM_AUDIO_SAMPLES / NUM_FLOAT32_PER_XVECTOR;

CONST SIZE_T CACHE_LINE_SIZE = 128;
#define CACHE_ALIGNED __declspec( align( 128 ) )


//----------------------------------------------------------------------------------------------------------------------
// Forward declarations
//----------------------------------------------------------------------------------------------------------------------

FLOAT FilterAngle( FLOAT fAngle );


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
	{ ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp"         },
	{ ATG::HELP_A_BUTTON,    ATG::HELP_PLACEMENT_2, L"Change Audio pipeline" },
	{ ATG::HELP_B_BUTTON,    ATG::HELP_PLACEMENT_2, L"Toggle Beam steering"  },
	{ ATG::HELP_DPAD,        ATG::HELP_PLACEMENT_2, L"Beam steering (L/R)"   },
};

static CONST DWORD NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

//----------------------------------------------------------------------------------------------------------------------
// Enums
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Beam steering modes for the sample - not API modes
//----------------------------------------------------------------------------------------------------------------------
enum BEAM_STEERING_MODE
{
    BEAM_STEERING_AUTOMATIC,                            // Beam steers towards loudest sound
    BEAM_STEERING_MANUAL,                               // Beam is steered by the user
    BEAM_STEERING_TRACKS_SKELETON,                      // Beam tracks first active skeleton found.
    BEAM_STEERING_MAX = BEAM_STEERING_TRACKS_SKELETON,  // Max value for mode
    BEAM_STEERING_FIRST = BEAM_STEERING_AUTOMATIC       // First value for mode
};


//======================================================================================================================


//----------------------------------------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//----------------------------------------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    static Sample* s_pInstance;     // Needed to bind callbacks to single instance of Sample

private:
    static VOID NuiAudioErrorCallback( HRESULT hr );
    static VOID NuiAudioDataCallback( NUIAUDIO_RESULTS* pNuiAudioResults );
    
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    // NuiAudio functions
    BOOL InitializeNuiAudio();
    VOID ShutdownNuiAudio();
    VOID SetAudioPipeline( DWORD dwNuiAudioPipelineType );
    VOID FollowSkeletonWithBeam();
    VOID ToggleAudioPipeline();
    VOID CycleThroughBeamSteeringModes();

    // Debug rendering
    VOID RenderSkeletonSpeakerOverlay( CONST FLOAT fScreenHeight, CONST FLOAT fScreenWidth );
    VOID TrackSkeletonNearestToBeam( DWORD * TrackingIDs );

    // General sample data
    ATG::Timer          m_Timer;
    ATG::Font           m_Font;
	ATG::Help           m_Help;
	BOOL                m_bDrawHelp;

    // Depth map & skeletal tracking
    HANDLE              m_hDepthStream;
    NUI_SKELETON_FRAME  m_SkeletonFrame;
    XMFLOAT2            m_SkeletonPos[NUI_SKELETON_COUNT];
    DWORD               m_dwActivePlayer;
    BOOL                m_bFirstFrame;
    BOOL                m_bTrackedSpeaker;

    // NuiAudio-specific 
    BOOL                m_bNuiAudioInitialized;
    BEAM_STEERING_MODE  m_BeamSteeringMode;

    NUIAUDIO_HANDLE     m_hNuiAudio;
    volatile HRESULT    m_NuiAudioPipelineStatus;

    DWORD   			m_dwAudioPipeline;      // Current mode - NUIAUDIO_SPEECH_PIPELINE or NUIAUDIO_CHAT_PIPELINE
    
    volatile FLOAT      m_fBeamDirection;       // Reported direction of beam
    volatile FLOAT      m_fConfidence;          // Confidence in beam direction
	FLOAT               m_fBeamAngle;           // Direction to point the beam when using beam steering
    

    // Audio visualization

    SHORT*              m_pAudioBuffer;

    __declspec( align( 16 ) )
    XDSP::XVECTOR m_UnityTable[ NUM_AUDIO_SAMPLES ];      
};

// Initialize static singleton
Sample* Sample::s_pInstance = NULL;


//======================================================================================================================


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::InitializeNuiAudio
// Desc: Initializes the NuiAudio library.
//----------------------------------------------------------------------------------------------------------------------
BOOL Sample::InitializeNuiAudio()
{
    assert( !m_bNuiAudioInitialized && "NuiAudio already initialized");

    DWORD kinectStatus = XNuiGetHardwareStatus();

    // Is the sensor plugged in?

    if ( ( kinectStatus & XNUI_HARDWARE_STATUS_CONNECTED ) == 0 )
    {
        m_NuiAudioPipelineStatus = E_NUI_DEVICE_NOT_CONNECTED;
        return FALSE;
    }
    
    // Is it ready for operation?

    if ( ( kinectStatus & XNUI_HARDWARE_STATUS_READY ) == 0 )
    {
        m_NuiAudioPipelineStatus = E_NUI_DEVICE_NOT_READY;
        return FALSE;
    }
    
    // Initialize the NuiAudio library

    HRESULT hr = NuiAudioCreate( 
        NUIAUDIO_DEFAULT_PROCESSOR, // Initializes NuiAudio on the default HW thread (5)
        NuiAudioErrorCallback,      // The function to report errors to.
        m_dwAudioPipeline,           // The pipeline to use - see head of file
        m_hNuiAudio,                // [out] Handle to the single instance of NuiAudio
        NULL                        // We don't care if NuiAudio is already running on another thread, so pass in NULL
        // and just use that thread anyway.
        );

    if ( FAILED( hr ) )
    {
        ATG::DebugSpew( "NuiAudioCreate failed with error %#X\n", hr );
        m_NuiAudioPipelineStatus = hr;
        return FALSE;
    }

    // Register our callback so we can get data as it arrives.
    // NOTE: this is a low-latency callback (DPC-like). Perform only minimal work in this routine - don't block!

    NuiAudioRegisterCallbacks(
        m_hNuiAudio,                // Handle to the NuiAudio instance
        m_dwAudioPipeline,           // The pipeline to use - see head of file
        NuiAudioDataCallback        // The callback function which will receive new data
        );

    m_bNuiAudioInitialized = TRUE;
    m_NuiAudioPipelineStatus = S_OK;

    return TRUE;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::ShutdownNuiAudio
// Desc: Releases the resources we're using on NuiAudio. Other libraries/code may be using the library as well,
//       including Speech. As a result, depending on your scenario this might not reduce memory use.
//----------------------------------------------------------------------------------------------------------------------
VOID Sample::ShutdownNuiAudio()
{
    assert( m_bNuiAudioInitialized && "NuiAudio was not initialized" );

    // Unregister our data-processing callback.

    NuiAudioUnregisterCallbacks(
        m_hNuiAudio,                // Handle to the NuiAudio instance
        NuiAudioDataCallback        // The callback function we previously registered.
        );

    // Release the instance of the library.

    NuiAudioRelease( m_hNuiAudio );

    m_NuiAudioPipelineStatus = E_FAIL;
    m_bNuiAudioInitialized = FALSE;
}



//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::UpdateAudioPipelineStatus
// Desc: Called when an error occurs in the NuiAudio pipeline. MUST NOT perform lengthy processing or block.
//----------------------------------------------------------------------------------------------------------------------
VOID Sample::NuiAudioErrorCallback( HRESULT hr )
{
    s_pInstance->m_NuiAudioPipelineStatus = hr;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::UpdateBeamPosition
// Desc: Called by NuiAudio when new audio data arrives. MUST not perform lengthy processing or block.
//----------------------------------------------------------------------------------------------------------------------
VOID Sample::NuiAudioDataCallback( NUIAUDIO_RESULTS* pNuiAudioResults )
{
    // Strictly speaking, this should be wrapped in synchronization primitives, but for the purpose of this
    // sample, all we want to do is hand off the data for visualization - we don't care if the BeamAngle and
    // Confidence both arrive at the other thread at the same time; so we just use atomic writes.
    s_pInstance->m_fBeamDirection = pNuiAudioResults->BeamAngle;
    s_pInstance->m_fConfidence = pNuiAudioResults->Confidence;

    if ( pNuiAudioResults->Confidence == 0.0f )
	{
		s_pInstance->m_bTrackedSpeaker = FALSE;
	}
	else
	{
		s_pInstance->m_bTrackedSpeaker = TRUE;
	}

    XMemCpy( s_pInstance->m_pAudioBuffer, pNuiAudioResults->pOutputMic, NUM_AUDIO_SAMPLES * sizeof(SHORT) );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//----------------------------------------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Because we can't bind a this ptr in the Callback registration for NuiAudio, need to work around it.
    s_pInstance = this;

    // Initialize data structures and variables
    XMemSet( &m_SkeletonFrame, 0, sizeof( m_SkeletonFrame ) );

    m_bFirstFrame = TRUE;
    m_dwActivePlayer = 0;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

	m_bDrawHelp = FALSE;
	if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
		return ATGAPPERR_MEDIANOTFOUND;

    // Initialize the simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Initialize the camera and skeleton tracking
    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON |
                                NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX,
                                NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );

    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX,
                             NUI_IMAGE_RESOLUTION_320x240,
                             0, 
                             1, 
                             NULL, 
                             &m_hDepthStream ); 
     
    if ( FAILED( hr ) ) 
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;        
    }

    hr = NuiSkeletonTrackingEnable( NULL, NUI_SKELETON_TRACKING_FLAG_TITLE_SETS_TRACKED_SKELETONS );

    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
    }

    
    hr = InitializeVisualization( m_pd3dDevice, 320, 240 );

    if( FAILED( hr ) )
    {
        ATG::DebugSpew( "Failed to initialize visualization\n" );
        return E_FAIL;
    }

    // Prep for NuiAudio initialization...

    // Allocate buffer for audio samples. Align to cache line.
    m_pAudioBuffer = (SHORT*)_aligned_malloc( NUM_AUDIO_SAMPLES * sizeof( SHORT ), CACHE_LINE_SIZE );

    if ( m_pAudioBuffer == NULL )
    {
        return E_OUTOFMEMORY;
    }

    m_bNuiAudioInitialized = FALSE;
    m_NuiAudioPipelineStatus = S_OK;
	m_fBeamDirection = 0.0f;
	m_fConfidence = 0.0f;
    m_fBeamAngle = 0.0f;
    m_BeamSteeringMode = BEAM_STEERING_AUTOMATIC;
    m_dwAudioPipeline = NUIAUDIO_SPEECH_PIPELINE;
    
    if ( !InitializeNuiAudio() )
    {
        // Note: Initialization can fail at this point if the device isn't ready; we'll pick it up again and check
        // each time around the update loop.
        ATG::DebugSpew( "Failed to initialize NuiAudio - 0x%X\n", m_NuiAudioPipelineStatus );
    }

    NuiAudioSetMicArrayBeamAngle( m_hNuiAudio, m_dwAudioPipeline, NUIAUDIO_MICARRAY_BEAM_ANGLE_AUTO );
   
	XDSP::FFTInitializeUnityTable(m_UnityTable, NUM_AUDIO_SAMPLES );

	XMemSet( m_SkeletonPos, 0, sizeof( m_SkeletonPos ) );

    return S_OK;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: SetAudioPipeline
// Desc: Sets the requested audio pipeline, shutting down and re-initializing if necessary.
//----------------------------------------------------------------------------------------------------------------------
VOID Sample::SetAudioPipeline( DWORD dwNuiAudioPipelineType )
{
    // Already in this mode? Do nothing...

    if ( dwNuiAudioPipelineType == m_dwAudioPipeline )
        return;

    m_dwAudioPipeline = dwNuiAudioPipelineType;

    ShutdownNuiAudio();

    if ( m_dwAudioPipeline == NUIAUDIO_SPEECH_PIPELINE )
    {
        m_BeamSteeringMode = BEAM_STEERING_AUTOMATIC;
        NuiAudioSetMicArrayBeamAngle( m_hNuiAudio, m_dwAudioPipeline, NUIAUDIO_MICARRAY_BEAM_ANGLE_AUTO );
    }
    // Note: Chat pipeline is always automatic; there is no manual mode, so we don't set anything here if we're
    // using the Chat pipe.

    if ( InitializeNuiAudio() )
    {
        ATG::DebugSpew( "Restarted NuiAudio pipeline\n" );
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating the scene.
//----------------------------------------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    CONST NUI_IMAGE_FRAME* pDepthImageFrame;
    HRESULT hr = NuiImageStreamGetNextFrame( m_hDepthStream, NUI_CAMERA_TIMEOUT_DEFAULT, &pDepthImageFrame );
    if( SUCCEEDED( hr ) )
    {
        UpdateDepthTexture( m_pd3dDevice, pDepthImageFrame, m_dwActivePlayer );
        NuiImageStreamReleaseFrame( m_hDepthStream, pDepthImageFrame );
    }

    DWORD TrackingIDs[NUI_SKELETON_MAX_TRACKED_COUNT];
    XMemSet( TrackingIDs, 0, sizeof( TrackingIDs ) );

    hr = NuiSkeletonGetNextFrame( NUI_CAMERA_TIMEOUT_DEFAULT, &m_SkeletonFrame );

    if ( SUCCEEDED( hr ))
    {
        NuiTransformSmooth( &m_SkeletonFrame, NULL );

        if( m_bFirstFrame )
        {
            NuiSkeletonSetTrackedSkeletons( TrackingIDs );

            m_bFirstFrame = FALSE;
        }
        m_dwActivePlayer = 0;
    }

    // If the pipeline has hit an error, we need to shut it down and start it back up.
    if ( m_bNuiAudioInitialized && FAILED( m_NuiAudioPipelineStatus ) )
    {
        ATG::DebugSpew( "Shutting down NuiAudio due to error - 0x%X\n", m_NuiAudioPipelineStatus );
        ShutdownNuiAudio();
    }

    // If the pipeline is not currently initialized, try to start it back up.
    if ( !m_bNuiAudioInitialized )
    {
        // Try to initialize
        if ( InitializeNuiAudio() )
        {
            ATG::DebugSpew( "Restarted NuiAudio pipeline\n" );
        }
        else if ( m_NuiAudioPipelineStatus == E_NUI_DEVICE_NOT_CONNECTED )
        {   
            ATG::DebugSpew( "Restarted NuiAudio pipeline - Hardware Not Connected\n" );
            XShowNuiHardwareRequiredUI( 0 );
        }
    }

    // Get input from all the gamepads (used here only to be able to exit the sample)
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

	if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
		m_bDrawHelp = !m_bDrawHelp;
    }

    if ( !m_bDrawHelp && m_bNuiAudioInitialized )
    {
        if ( m_dwAudioPipeline == NUIAUDIO_SPEECH_PIPELINE )
        {
            // Handle B button on Gamepad:
            // Allow the user to cycle through options.

            if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
            {
                CycleThroughBeamSteeringModes();
            }

            // Handle DPad Left/Right - but only if we're in Manual beam steering mode.
            // Left/Right allows the user to move the beam by coarse steps.

            if ( m_BeamSteeringMode == BEAM_STEERING_MANUAL )
            {
                if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
                {
                    m_fBeamAngle -= BEAM_STEERING_COARSE_TUNE_STEP;
                        
                    if ( m_fBeamAngle < NUIAUDIO_MICARRAY_BEAM_ANGLE_MIN )
                    {
                        m_fBeamAngle = NUIAUDIO_MICARRAY_BEAM_ANGLE_MIN;
                    }

                    NuiAudioSetMicArrayBeamAngle( m_hNuiAudio, m_dwAudioPipeline, m_fBeamAngle );
                }
                else if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
                {
                    m_fBeamAngle += BEAM_STEERING_COARSE_TUNE_STEP;
                        
                    if ( m_fBeamAngle > NUIAUDIO_MICARRAY_BEAM_ANGLE_MAX )
                    {
                        m_fBeamAngle = NUIAUDIO_MICARRAY_BEAM_ANGLE_MAX;
                    }
                        
                    NuiAudioSetMicArrayBeamAngle( m_hNuiAudio, m_dwAudioPipeline, m_fBeamAngle );
                }
            }
        }

        // Handle Gamepad Button A:
        // Switch between audio pipelines

        if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            ToggleAudioPipeline();
        }

        // Collect valid skeletons.

        for( DWORD i = 0; i < NUI_SKELETON_COUNT; ++i )
        {
            if( m_SkeletonFrame.SkeletonData[i].eTrackingState != NUI_SKELETON_NOT_TRACKED )
            {
                m_SkeletonPos[i].x = m_SkeletonFrame.SkeletonData[i].Position.x;
                m_SkeletonPos[i].y = m_SkeletonFrame.SkeletonData[i].Position.z;
            }
            else
            {
                m_SkeletonPos[i].x = m_SkeletonPos[i].y = 0.0f;
            }
        }

        if ( m_dwAudioPipeline == NUIAUDIO_SPEECH_PIPELINE && m_BeamSteeringMode == BEAM_STEERING_TRACKS_SKELETON )
        {
            FollowSkeletonWithBeam();


        }
        else
        {
            // If we've got a likely speaker, highlight the skeleton nearby (by turning on tracking for it)
            if ( m_bTrackedSpeaker )
            {
                TrackSkeletonNearestToBeam( TrackingIDs );
            }
        }
    }

    return S_OK;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//----------------------------------------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    WCHAR pScratchText[256];

    CONST FLOAT fScreenWidth = (FLOAT)m_d3dpp.BackBufferWidth;
    CONST FLOAT fScreenHeight = (FLOAT)m_d3dpp.BackBufferHeight;

    // Draw a gradient filled background
    ATG::RenderBackground( TOP_BACK_COLOR, BOTTOM_BACK_COLOR );

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Timer.MarkFrame();

        // Render on-screen visualizations

        // Render Depth image/segmentation
        VisualizeStreams( m_pd3dDevice );

        // Render Skeleton tracking bones for active skeleton
        VisualizeSkeleton( &m_SkeletonFrame );

        if( m_bTrackedSpeaker )
        {
            VisualizeTalkerPosition( m_pd3dDevice, m_fBeamDirection, m_fConfidence );
        }

        // Render audio waveform.

        if ( m_bNuiAudioInitialized )
        {
            // Display audio centered in bottom 3rd of screen.

            // 10% from each side for title-safety
            FLOAT fWidthTS = fScreenWidth * 0.8f; 
            FLOAT fHeightTS = fScreenHeight * 0.8f;
            FLOAT fRenderHeight = fHeightTS / 3.0f;
            FLOAT fYTSMargin = fScreenHeight * 0.1f;

            // Center horizontal
            FLOAT fXOffset = ( fScreenWidth - fWidthTS ) * 0.5f;

            // Put at bottom of display vertical
            FLOAT fYOffset = fYTSMargin + ( fRenderHeight * 2.0f );

            static CACHE_ALIGNED XDSP::XVECTOR realSamples[ NUM_AUDIO_SAMPLES_XVECTOR ];
            static CACHE_ALIGNED XDSP::XVECTOR imaginarySamples[ NUM_AUDIO_SAMPLES_XVECTOR ];
		    
            CACHE_ALIGNED FLOAT fBandSamples[ NUM_AUDIO_SAMPLES ];

		    XMemSet128( imaginarySamples, 0, sizeof( imaginarySamples ) );

            for( int i = 0; i < NUM_AUDIO_SAMPLES; i++ )
            {
                ( (FLOAT*)realSamples )[i] = (FLOAT) m_pAudioBuffer[i];
            }

		    XDSP::FFT( realSamples, imaginarySamples, m_UnityTable, NUM_AUDIO_SAMPLES );
		    XDSP::FFTPolar( realSamples, realSamples, imaginarySamples, NUM_AUDIO_SAMPLES );
		    XDSP::FFTUnswizzle( (XDSP::XVECTOR*)fBandSamples, realSamples, LOG2_NUM_AUDIO_SAMPLES );

		    VisualizeAudioData( m_pAudioBuffer, fXOffset, fYOffset, fWidthTS, fRenderHeight );
		    VisualizeAudioBand( fBandSamples, fXOffset, fYOffset, fWidthTS, fRenderHeight );
        }

        // Render Debug HUD
        
        m_Font.Begin();

        // Set up title-safe text-rendering area.
        m_Font.SetWindow( ATG::GetTitleSafeArea() );
        m_Font.SetScaleFactors( 1.2f, 1.2f );

        // Show sample name.
        m_Font.DrawText( 0, 0, TEXT_COLOR,  L"Sound Location Tracking" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        // Show current speech pipeline + steering mode.

	    if ( m_dwAudioPipeline == NUIAUDIO_SPEECH_PIPELINE )
	    {
	        m_Font.DrawText( 0, 50, TEXT_COLOR,  L"Speech Pipeline " );

            WCHAR CONST * wstrCurrentModeText = NULL;

            switch( m_BeamSteeringMode )
            {
                case BEAM_STEERING_AUTOMATIC:
                {
                    wstrCurrentModeText = L"w/ Automatic Beam Steering";
                    break;
                }
                
                case BEAM_STEERING_MANUAL:
                {
                    wstrCurrentModeText = L"w/ Manual Beam Steering";
                    break;
                }
                
                case BEAM_STEERING_TRACKS_SKELETON:
                {
                    wstrCurrentModeText = L"w/ Beam Tracking Skeleton";
                    break;
                }
            }
    
            m_Font.DrawText( TEXT_COLOR,  wstrCurrentModeText );
            
	    }
	    else
        {
	        m_Font.DrawText( 0, 50, TEXT_COLOR,  L"Chat Pipeline (only allows Automatic Beam Steering)" );
        }
    
        if ( m_bNuiAudioInitialized )
        {
            if ( !m_bTrackedSpeaker )
            {
                D3DRECT rcWindow;
                m_Font.GetWindow(rcWindow);
                m_Font.DrawText( (rcWindow.x2 - rcWindow.x1) * 0.5f, -40, TEXT_COLOR, L"No Audio Source Found...", ATGFONT_CENTER_X );
            }
        }
        else if ( FAILED( m_NuiAudioPipelineStatus ) )
        {
            HRESULT hr = m_NuiAudioPipelineStatus;
            CONST WCHAR* pText;

            switch(hr)
            {
                case E_NUI_DEVICE_NOT_READY:
                {
                    pText = L"Kinect Sensor is Not Ready";
                    break;
                }
                case E_NUI_DEVICE_NOT_CONNECTED:
                {
                    pText = L"Kinect Sensor is Not Connected";
                    break;
                }
                default:
                {
                    swprintf_s( pScratchText, L"NuiAudio reported an Error: %#X", m_NuiAudioPipelineStatus );
                    pText = pScratchText;
                }
            }
            m_Font.DrawText( 0, 170, TEXT_COLOR, pText );
        }

        // Show beam angle in degrees
        
        swprintf_s( pScratchText, L"Direction = %2.2f degrees", XMConvertToDegrees( m_fBeamDirection ) );
        m_Font.DrawText( 0, 110, DATA_COLOR, pScratchText );

        // Show beam-former confidence

        swprintf_s( pScratchText, L"Confidence = %2.2f", m_fConfidence );
        m_Font.DrawText( 0, 140, DATA_COLOR, pScratchText );

        // Show FPS counter

        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Show A-button meaning

        swprintf_s( pScratchText, GLYPH_A_BUTTON L" Switch To %s", ( m_dwAudioPipeline == NUIAUDIO_SPEECH_PIPELINE ) ?
                                                                L"Chat Pipeline" : L"Speech Pipeline" );
        m_Font.DrawText( 0, 200, TEXT_COLOR, pScratchText );

        // Show control options available when in Speech pipeline mode.

        if ( m_dwAudioPipeline == NUIAUDIO_SPEECH_PIPELINE )
        {
            WCHAR CONST * wstrNextBeamMode = NULL;

            switch( m_BeamSteeringMode )
            {
                //NOTE: These show the current value + 1's meaning. If you change the order in the enum, you'll
                //      need to change them here too...

                case BEAM_STEERING_AUTOMATIC:
                {
                    wstrNextBeamMode = L"Manual Beam Steering";
                    break;
                }
                case BEAM_STEERING_MANUAL:
                {
                    wstrNextBeamMode = L"Beam Tracks Skeleton Mode";
                    break;
                }
                case BEAM_STEERING_TRACKS_SKELETON:
                {
                    wstrNextBeamMode = L"Automatic Beam Steering";
                    break;
                }
            }

            swprintf_s( pScratchText, GLYPH_B_BUTTON L" Switch To %s", wstrNextBeamMode );
            m_Font.DrawText( 0, 230, TEXT_COLOR, pScratchText );


            if ( m_BeamSteeringMode == BEAM_STEERING_MANUAL )
            {
                m_Font.DrawText( 0, 260, TEXT_COLOR, GLYPH_LEFT_TICK L" Steer Beam Left   "
                    GLYPH_RIGHT_TICK L" Steer Beam Right" );
            }

        }

        // Render top-down view 
        
        RenderSkeletonSpeakerOverlay(fScreenHeight, fScreenWidth);

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::CycleThroughBeamSteeringModes
// Desc: Cycles through the active beam steering mode when in the Speech pipeline
//----------------------------------------------------------------------------------------------------------------------

VOID Sample::CycleThroughBeamSteeringModes()
{
    assert( m_dwAudioPipeline == NUIAUDIO_SPEECH_PIPELINE && "Beam steering modes only make sense in speech pipeline");

    // Cycle through, wrapping at end.

    m_BeamSteeringMode = (BEAM_STEERING_MODE)((int)m_BeamSteeringMode + 1);

    if ( m_BeamSteeringMode > BEAM_STEERING_MAX )
    {
        m_BeamSteeringMode = BEAM_STEERING_FIRST;
    }

    switch( m_BeamSteeringMode )
    {
    case BEAM_STEERING_MANUAL:
        {
            // Manual mode

            m_fBeamAngle = 0.0f;
            NuiAudioSetMicArrayBeamAngle( m_hNuiAudio, m_dwAudioPipeline, m_fBeamAngle );
            break;
        }
    
    case BEAM_STEERING_TRACKS_SKELETON:
        {
            // Track the first skeletons that the skeleton tracking system finds for us automatically, by passing in
            // zero for both tracking IDs. This is the naive way of doing it, but a robust mechanism is not the goal of
            // this sample, and it'll work for us.

            DWORD trackingIDs[ NUI_SKELETON_MAX_TRACKED_COUNT ];
            XMemSet( trackingIDs, 0, sizeof(trackingIDs) );
            
            NuiSkeletonSetTrackedSkeletons( trackingIDs );

            // The rest is handled at the end of Sample::Update via a call to FollowSkeletonWithBeam, which is called
            // every frame.
        }
        
    case BEAM_STEERING_AUTOMATIC:
        {
            // Automatically track loudest speaker
            NuiAudioSetMicArrayBeamAngle( m_hNuiAudio, m_dwAudioPipeline, NUIAUDIO_MICARRAY_BEAM_ANGLE_AUTO );
            break;
        }
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::ToggleAudioPipeline
// Desc: Toggles between the speech and chat pipelines.
//----------------------------------------------------------------------------------------------------------------------

VOID Sample::ToggleAudioPipeline()
{
    if ( m_dwAudioPipeline == NUIAUDIO_CHAT_PIPELINE )
    {
        SetAudioPipeline( NUIAUDIO_SPEECH_PIPELINE );
    }
    else
    {
        SetAudioPipeline( NUIAUDIO_CHAT_PIPELINE );
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::TrackSkeletonNearestToBeam
// Desc: Makes the active tracked skeleton to be the one closest to the beam former's beam.
//----------------------------------------------------------------------------------------------------------------------

VOID Sample::TrackSkeletonNearestToBeam( DWORD * TrackingIDs )
{
    FLOAT fDirectionAngle = FilterAngle( m_fBeamDirection );

    for( DWORD i = 0; i < NUI_SKELETON_COUNT; ++i )
    {
        if( m_SkeletonFrame.SkeletonData[i].eTrackingState != NUI_SKELETON_NOT_TRACKED )
        {
            FLOAT fSkeletonAngle = atan2f( m_SkeletonFrame.SkeletonData[i].Position.x,
                m_SkeletonFrame.SkeletonData[i].Position.z );

            // Detect if there is a skeleton within the angle range of the sound direction
            // If there is enough audio coming from this direction, set the skeleton as tracked
            if( fabs( fDirectionAngle - fSkeletonAngle ) < SPEAKER_LOCK_ANGLE_RANGE )
            {
                m_dwActivePlayer = i + 1;

                TrackingIDs[0] = m_SkeletonFrame.SkeletonData[i].dwTrackingID;
                NuiSkeletonSetTrackedSkeletons( TrackingIDs );
            }
        }
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::RenderSkeletonSpeakerOverlay
// Desc: Renders an overlay showing the beam direction, FOV of the sensor, and the current tracked skeletons.
//----------------------------------------------------------------------------------------------------------------------
VOID Sample::RenderSkeletonSpeakerOverlay( CONST FLOAT fScreenHeight, CONST FLOAT fScreenWidth )
{
    // Calculate the position and height/width of the top-down view overlay.

    CONST FLOAT fWidthHeight = fScreenHeight * 0.3f;
    FLOAT fX = fScreenWidth - ( fWidthHeight + fScreenWidth * 0.1f );
    FLOAT fY = fScreenHeight * 0.5f - ( fWidthHeight * 0.5f );

    m_Font.SetWindow( (LONG)fX, (LONG)fY, (LONG)( fX + fWidthHeight ), (LONG)( fY + fWidthHeight ) );
    
    CONST FLOAT fWidthHeightHalf = fWidthHeight * 0.5f;
    
    WCHAR szSkeleton[2];

    for( DWORD i = 0; i < NUI_SKELETON_COUNT; ++i )
    {
        if ( m_SkeletonPos[i].y != 0.0f )
        {
            swprintf_s( szSkeleton, 2, L"%d", i + 1 );
            m_Font.DrawText( fWidthHeightHalf + m_SkeletonPos[i].x / 4.0f * fWidthHeightHalf,
                m_SkeletonPos[i].y / 4.0f * fWidthHeight, TEXT_COLOR, szSkeleton,
                ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
        }
    }
    VisualizeTopView( m_fBeamDirection, fX, fY, fWidthHeight, fWidthHeight, m_bTrackedSpeaker );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::FollowSkeletonWithBeam
// Desc: Finds a player, and follows their head with the beam former. If there aren't any players (or everyone is
//       standing outside of the FOV of the sensor), the beam tracks the loudest audio source.
//----------------------------------------------------------------------------------------------------------------------

VOID Sample::FollowSkeletonWithBeam()
{
    // We're panning the beam based on where the first tracked skeleton is...
    // We don't care if the tracking ID changed since last time, as long as there's a valid skeleton there.
    // Once a skeleton is tracked, its index in NUI_SKELETON_FRAME::SkeletonData is locked.
    if ( m_dwActivePlayer && m_SkeletonFrame.SkeletonData[ m_dwActivePlayer - 1 ].dwTrackingID == 0 )
    {
        m_dwActivePlayer = 0;
    }

    // If we're not tracking anyone, find a skeleton...
    if ( !m_dwActivePlayer )
    {
        for ( SIZE_T i = 0; i < NUI_SKELETON_COUNT; ++i )
        {
            NUI_SKELETON_DATA& skeleton = m_SkeletonFrame.SkeletonData[i];

            if ( ( skeleton.eTrackingState != NUI_SKELETON_NOT_TRACKED ) && ( skeleton.dwTrackingID != 0 ) )
            {
                m_dwActivePlayer = i + 1;
                break;
            }
        }
    }

    // If we now have a tracked skeleton, steer the beam towards it...

    if ( m_dwActivePlayer )
    {
        // If we have a valid head joint (explicit or inferred), steer towards it. If we don't (which should
        // be nearly impossible), use the center of mass of the skeleton.

        NUI_SKELETON_DATA& skeleton = m_SkeletonFrame.SkeletonData[ m_dwActivePlayer - 1 ];

        XMFLOAT2 vBeamTarget;

        if ( skeleton.eTrackingState == NUI_SKELETON_POSITION_ONLY )
        {
            vBeamTarget.x = skeleton.Position.x;
            vBeamTarget.y = skeleton.Position.z;
        }
        else
        {
            assert( skeleton.eTrackingState == NUI_SKELETON_TRACKED );

            if ( skeleton.eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HEAD ] == NUI_SKELETON_NOT_TRACKED )
            {
                vBeamTarget.x = skeleton.Position.x;
                vBeamTarget.y = skeleton.Position.z;
            }
            else
            {
                vBeamTarget.x = skeleton.SkeletonPositions[ NUI_SKELETON_POSITION_HEAD ].x;
                vBeamTarget.y = skeleton.SkeletonPositions[ NUI_SKELETON_POSITION_HEAD ].z;
            }
        }

        // Find angle to skeleton...
        m_fBeamAngle = atan2f( vBeamTarget.x, vBeamTarget.y );
        
        NuiAudioSetMicArrayBeamAngle( m_hNuiAudio, NUIAUDIO_SPEECH_PIPELINE, m_fBeamAngle );
    }
    else
    {
        // Just steer towards the loudest thing the system hears.
        NuiAudioSetMicArrayBeamAngle( m_hNuiAudio, NUIAUDIO_SPEECH_PIPELINE, NUIAUDIO_MICARRAY_BEAM_ANGLE_AUTO );
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: FilterAngle
// Desc: Applies simple filter to incoming speaker angle.
//----------------------------------------------------------------------------------------------------------------------

FLOAT FilterAngle( FLOAT fAngle )
{
    static FLOAT fAverage = 0.0f;
    static FLOAT fLastValue = 0.0f;

    FLOAT fWeight = ( 0.2f - fabs( fLastValue - fAngle ) ) * 5.0f;

    if( fWeight < 0.0f )
    {
        fWeight = 0.0f;
    }
    else if( fWeight > 1.0f )
    {
        fWeight = 1.0f;
    }

    fWeight = 0.1f + ( fWeight * 0.9f );

    fAverage = fWeight * fAngle + ( 1.0f - fWeight ) * fAverage;

    fLastValue = fAngle;

    return fAverage;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program
//----------------------------------------------------------------------------------------------------------------------

VOID __cdecl main()
{
    Sample atgApp;

    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    atgApp.m_d3dpp.EnableAutoDepthStencil = FALSE;
    atgApp.Run();
}

