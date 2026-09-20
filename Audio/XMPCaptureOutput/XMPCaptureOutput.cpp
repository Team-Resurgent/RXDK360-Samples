//--------------------------------------------------------------------------------------
// XMPCaptureOutput.cpp
//
// This sample demonstrates how to capture an output of the XMP and apply DSP effects
// using the XMP API.
//
// Retrieves output samples from the media player so they can be monitored
// or rendered by the title.  The callback supplied to XMPCaptureOutput
// will receive one audio frame of stereo samples.  The capture buffer will
// contain one frame of stereo floating-point audio samples at 48 kHz.
//
// The fTitleRendering parameter passed to XMPCaptureOutput sets
// whether the title wishes to render the music decoded by XMP.  If
// fTitleRendering is TRUE, the title must render it (using XAudio2 or
// another software audio mixing rendering solution).  As long
// as XMP is playing a title playlist, it will not render any of the audio
// it's decoding.  If the user chooses to override the title music, XMP will
// automatically start rendering music again.  At that point, the capture
// callback will start receiving FALSE as its fTitleRendering parameter and
// the title should immediately stop rendering data, although monitoring it
// is still allowed.  If the title is rendering using an XAudio2 voice tagged as
// XAUDIO2_VOICE_MUSIC, the voice will automatically be muted and
// unmuted when the user takes or relinquishes control of the music playback.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xmp.h>
#include <AtgApp.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#define XAUDIO2_HELPER_FUNCTIONS
#include <xaudio2.h>
#include <xaudio2fx.h>

//--------------------------------------------------------------------------------------
// Global variables and definitions
//--------------------------------------------------------------------------------------
// List of XAudio2 implicit filter types to cycle through
WCHAR*              g_FILTER_TYPES[] =
{
    L"Low Pass Filter",
    L"Band Pass Filter",
    L"High Pass Filter",
    L"Notch Filter",
};

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------

// List of songs to place in the playlist.
XMP_SONGDESCRIPTOR g_SongDescriptors[] =
{
    {
        L"GAME:\\media\\sounds\\music1.wma",    // File path.
        L"Baseball 1",                          // Song title.
        L"",                                    // Song artist.
        L"",                                    // Song album.
        L"",                                    // Song album artist.
        L"",                                    // Genre.
        0,                                      // Track number in the album.
        69000,                                  // Duration in milliseconds.
        XMP_SONGFORMAT_WMA,                     // Format of the song.
    },
    {
        L"GAME:\\media\\sounds\\music2.wma",    // File path.
        L"Baseball 2",                          // Song title.
        L"",                                    // Song artist.
        L"",                                    // Song album.
        L"",                                    // Song album artist.
        L"",                                    // Genre.
        0,                                      // Track number in the album.
        43000,                                  // Duration in milliseconds.
        XMP_SONGFORMAT_WMA,                     // Format of the song.
    },
    {
        L"GAME:\\media\\sounds\\becky.wma",     // File path.
        L"Becky",                               // Song title.
        L"",                                    // Song artist.
        L"",                                    // Song album.
        L"",                                    // Song album artist.
        L"",                                    // Genre.
        0,                                      // Track number in the album.
        166000,                                 // Duration in milliseconds.
        XMP_SONGFORMAT_WMA,                     // Format of the song.
    },
};

const DWORD                     NUM_SONGS = sizeof( g_SongDescriptors ) / sizeof( g_SongDescriptors[0] );

const DWORD                     XMPCAPTURE_NUM_BUFFERS = 2;

// Source voice filter
const FLOAT                     FILTER_FREQ_SCALE = 3.0f;
const FLOAT                     FILTER_ONEOVERQ_SCALE = 1.0f;
const DWORD                     NUM_FILTER_STATES = 4; // for cycling through source voice filter configurations

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Play/Pause" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_1, L"Stop" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_1, L"Select song" },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_1, L"Toggle filter mode" },
    { ATG::HELP_LEFT_STICK,   ATG::HELP_PLACEMENT_2, L"Change\nfilter params" },
};
static const DWORD              NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    // Misc
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    // XMP Stuff
    XMP_HANDLE m_hXMPPlaylist;  // The music player playlist
    XMP_SONGINFO m_songInfo;    // Info about the current song
    XMP_STATE m_XMPState;       // The current status of the music player
    BOOL m_bPlaybackControl;	// Reflects whether game (TRUE) or user (FALSE) is currently controlling XMP

    // System Notifications
    HANDLE m_hNotificationListener;

    // XAudio2 objects
    IXAudio2* m_pXAudio2;
    IXAudio2SourceVoice* m_pSourceVoice;
    IXAudio2MasteringVoice* m_pMasteringVoice;

    // Filter parameters for source voice
    XAUDIO2_FILTER_PARAMETERS m_FilterParams;

    // Volume Meter parameters
    XAUDIO2FX_VOLUMEMETER_LEVELS m_VolumeMeterLevels;

    // XMP Callback
    static void CaptureCallback( LPCXMP_CAPTURE_BUFFER pSampleBuffer,
                                 LPVOID pContext,
                                 BOOL fTitleRendering );

public:
    HRESULT     Initialize();
    HRESULT     ShutDown();
    HRESULT     Update();
    HRESULT     Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: CaptureCallback()
// Desc:
//--------------------------------------------------------------------------------------
void Sample::CaptureCallback( LPCXMP_CAPTURE_BUFFER pSampleBuffer,
                              LPVOID pContext,
                              BOOL fTitleRendering )
{
    if( fTitleRendering && pSampleBuffer )
    {
        IXAudio2SourceVoice* pSourceVoice = ( IXAudio2SourceVoice* )pContext;

        XAUDIO2_VOICE_STATE state;
        pSourceVoice->GetState( &state, XAUDIO2_VOICE_NOSAMPLESPLAYED );
        if( state.BuffersQueued < XMPCAPTURE_NUM_BUFFERS )
        {
            XAUDIO2_BUFFER buffer = { 0 };
            buffer.pAudioData = ( BYTE* )pSampleBuffer;
            buffer.AudioBytes = XMP_CAPTURE_SAMPLE_COUNT * sizeof( FLOAT );
            pSourceVoice->SubmitSourceBuffer( &buffer, 0 );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    DWORD dwStatus;

    m_hXMPPlaylist = NULL;
    memset( &m_songInfo, 0, sizeof( XMP_SONGINFO ) );

    m_bDrawHelp = FALSE;

    // default to filter bypass
    m_FilterParams.Frequency = 1.0f;
    m_FilterParams.OneOverQ = 1.0f;
    m_FilterParams.Type = LowPassFilter;

    // set up volume meter structure
    m_VolumeMeterLevels.ChannelCount = XMP_CAPTURE_CHANNEL_COUNT;
    m_VolumeMeterLevels.pPeakLevels = new FLOAT[XMP_CAPTURE_CHANNEL_COUNT];
    m_VolumeMeterLevels.pRMSLevels = new FLOAT[XMP_CAPTURE_CHANNEL_COUNT];

    // Create the font
    if( FAILED( m_Font.Create( "GAME:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "GAME:\\media\\help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Set up XAudio2 engine
    HRESULT hr = XAudio2Create(&m_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if ( FAILED( hr ) )
        ATG::FatalError( "Error %#X calling XAudio2Create\n", hr );

    // Create a mastering voice
    hr = m_pXAudio2->CreateMasteringVoice(&m_pMasteringVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, NULL);
    if ( FAILED( hr ) )
        ATG::FatalError( "Error %#X calling CreateMasteringVoice\n", hr );

    // Create a source voice with a volume meter effect
    WAVEFORMATEX fmt = {0};
    fmt.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    fmt.nChannels = XMP_CAPTURE_CHANNEL_COUNT;
    fmt.nSamplesPerSec = 48000;
    fmt.wBitsPerSample = 32;
    fmt.nBlockAlign = fmt.nChannels * (fmt.wBitsPerSample /  8);
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    fmt.cbSize = 0;

    IUnknown * pVolumeMeterAPO;
    hr = XAudio2CreateVolumeMeter (&pVolumeMeterAPO);
    if ( FAILED( hr ) )
        ATG::FatalError( "Error %#X calling XAudio2CreateVolumeMeter\n", hr );

    XAUDIO2_EFFECT_DESCRIPTOR descriptor;
    descriptor.InitialState = TRUE;
    descriptor.OutputChannels = 2;
    descriptor.pEffect = pVolumeMeterAPO;

    XAUDIO2_EFFECT_CHAIN chain;
    chain.EffectCount = 1;
    chain.pEffectDescriptors = &descriptor;

    hr = m_pXAudio2->CreateSourceVoice(&m_pSourceVoice, (WAVEFORMATEX*)&fmt, XAUDIO2_VOICE_MUSIC | XAUDIO2_VOICE_USEFILTER, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, &chain);
    if ( FAILED( hr ) )
        ATG::FatalError( "Error %#X calling CreateSubmixVoice\n", hr );

    // Create the notification listener to listen for XMP notifications
    m_hNotificationListener = XNotifyCreateListener( XNOTIFY_XMP );
    if( !m_hNotificationListener )
        ATG::FatalError( "Error calling XNotifyCreateListener\n" );

    // Initialize XMP state variable
    dwStatus = XMPGetStatus( &m_XMPState );
    assert( dwStatus == ERROR_SUCCESS );

    // One-time check to see if user already has control of XMP (for instance, started playing music in Dashboard)
    XMPTitleHasPlaybackControl(&m_bPlaybackControl);
    if (!m_bPlaybackControl) // user already has control, so XMP will render music and we need to mute our render path
    {
        m_pSourceVoice->SetVolume(0.0f, XAUDIO2_COMMIT_NOW);
    }
    else
    {
        m_pSourceVoice->SetVolume(1.0, XAUDIO2_COMMIT_NOW);
    }


    // Create a playlist
    dwStatus = XMPCreateTitlePlaylist( g_SongDescriptors,
                                       NUM_SONGS,
                                       XMP_CREATETITLEPLAYLISTFLAG_NONE,
                                       L"MyPlaylist",
                                       NULL,
                                       &m_hXMPPlaylist );
    assert( dwStatus == ERROR_SUCCESS );

    // Set the playback behavior to be in order and repeat the entire playlist
    XMPSetPlaybackBehavior( XMP_PLAYBACKMODE_INORDER,
                            XMP_REPEATMODE_PLAYLIST,
                            0,
                            NULL );

    // Play the playlist
    XMPPlayTitlePlaylist( m_hXMPPlaylist, NULL, NULL );

    // Set CaptureCallback
    XMPCaptureOutput( CaptureCallback, m_pSourceVoice, TRUE, NULL );

    // Play the source voice
    if( FAILED( hr = m_pSourceVoice->Start( 0, XAUDIO2_COMMIT_NOW ) ) )
        ATG::FatalError( "Error %#X calling Start\n", hr );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ShutDown()
// Desc: This shuts down the media player.  Note that this function is never called in
//       this sample because the sample never shuts down.
//--------------------------------------------------------------------------------------
HRESULT Sample::ShutDown()
{
    DWORD dwStatus;
    HRESULT hr;

    // Stop a source voice
    if( FAILED( hr = m_pSourceVoice->Stop( 0 ) ) )
        ATG::FatalError( "Error %#X calling Stop\n", hr );

    delete[] m_VolumeMeterLevels.pPeakLevels;
    delete[] m_VolumeMeterLevels.pRMSLevels;

    // Stop the music
    XMPStop( NULL );

    // Wait for the music to stop
    dwStatus = XMPGetStatus( &m_XMPState );
    assert( dwStatus == ERROR_SUCCESS );

    while( m_XMPState != XMP_STATE_IDLE )
    {
        Sleep( 1 );
        dwStatus = XMPGetStatus( &m_XMPState );
        assert( dwStatus == ERROR_SUCCESS );
    }

    if( m_hXMPPlaylist )
    {
        // Delete the playlist
        dwStatus = XMPDeleteTitlePlaylist( m_hXMPPlaylist );
        assert( dwStatus == ERROR_SUCCESS );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();
    DWORD dwStatus;

    // Check for new media player notifications.  All we're really looking for is if the
    // state has changed.
    DWORD dwMsgFilter;
    ULONG_PTR param;
    if( XNotifyGetNext( m_hNotificationListener, 0, &dwMsgFilter, &param ) )
    {
        switch( dwMsgFilter )
        {
            case XN_XMP_STATECHANGED:
                XMPGetStatus( &m_XMPState );

                // Get the currently playing song
                if( m_XMPState != XMP_STATE_IDLE )
                {
                    memset( &m_songInfo, 0, sizeof( XMP_SONGINFO ) );

                    dwStatus = XMPGetCurrentSong( &m_songInfo, NULL );
                    //                    assert( dwStatus == ERROR_SUCCESS );
                }

                break;
            case XN_XMP_PLAYBACKCONTROLLERCHANGED:
                m_bPlaybackControl = (BOOL) param;

                // Managing the volume of the voice would not be required if we tagged this source voice
                // as XAUDIO2_VOICE_MUSIC. But since we might want to monitor the music in a
                // read-only manner as it plays, we'll manage muting ourselves.
                if (m_bPlaybackControl) // title regaining control; connect source voice to mastering voice
                {
                    m_pSourceVoice->SetVolume(1.0f, XAUDIO2_COMMIT_NOW);
                }
                else // user taking control of XMP; mute game's source voice
                {
                    m_pSourceVoice->SetVolume(0.0f, XAUDIO2_COMMIT_NOW);
                }
                break;
        }
    }

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Play and Pause
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        switch( m_XMPState )
        {
            case XMP_STATE_IDLE:
                XMPPlayTitlePlaylist( m_hXMPPlaylist, NULL, NULL );  break;
            case XMP_STATE_PLAYING:
                XMPPause( NULL );                                      break;
            case XMP_STATE_PAUSED:
                XMPContinue( NULL );                                   break;
        }
    }

    // Stop
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        XMPStop( NULL );
    }

    // Go to the next song in the playlist
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
    {
        XMPNext( NULL );
    }

    // Go to the previous song in the playlist
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
    {
        XMPPrevious( NULL );
    }

    // Toggle filter mode
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        m_FilterParams.Type = XAUDIO2_FILTER_TYPE((((DWORD)(m_FilterParams.Type))+1) % NUM_FILTER_STATES);
    }

    m_FilterParams.Frequency += pGamepad->fX1 * fElapsedTime * FILTER_FREQ_SCALE;
    m_FilterParams.Frequency = max (m_FilterParams.Frequency, 0.f);
    m_FilterParams.Frequency = min (m_FilterParams.Frequency, XAUDIO2_MAX_FILTER_FREQUENCY);

    m_FilterParams.OneOverQ += pGamepad->fY1 * fElapsedTime * FILTER_ONEOVERQ_SCALE;
    m_FilterParams.OneOverQ = max (m_FilterParams.OneOverQ, 0.01f);
    m_FilterParams.OneOverQ = min (m_FilterParams.OneOverQ, XAUDIO2_MAX_FILTER_ONEOVERQ);

    m_pSourceVoice->SetFilterParameters (&m_FilterParams, XAUDIO2_COMMIT_NOW);

    m_pSourceVoice->GetEffectParameters (0, &m_VolumeMeterLevels, sizeof(XAUDIO2FX_VOLUMEMETER_LEVELS));
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        WCHAR strBuffer[200];
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"XMPCaptureOutput" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Display the state of the media player
        switch( m_XMPState )
        {
            case XMP_STATE_PAUSED:
                m_Font.DrawText( 52, 64, 0xffffffff, L"The Media Player is paused" );
                break;
            case XMP_STATE_PLAYING:
                m_Font.DrawText( 52, 64, 0xffffffff, L"The Media Player is playing: " );

                // If we're playing a song, display it's name
                m_Font.DrawText( 0xffffff00, m_songInfo.wszTitle );
                break;
            default:
                m_Font.DrawText( 52, 64, 0xffffffff, L"The Media Player is idle" );
                break;
        }
        if (!m_bPlaybackControl)
            m_Font.DrawText( 52, 88, 0xffff0000, L"User controlling XMP (all effects bypassed)");

		// Show filter settings
        FLOAT fY = 64.0f;
        m_Font.DrawText( 52, fY += 60, 0xffffffff, L"Implicit source voice filter: " );

        m_Font.SetScaleFactors( 0.8f, 0.8f );
        FLOAT fFreq = XAudio2RadiansToCutoffFrequency(m_FilterParams.Frequency, 48000);
        FLOAT fQ = 1 / m_FilterParams.OneOverQ;

        swprintf_s( strBuffer, L"%s", g_FILTER_TYPES[m_FilterParams.Type]);
        m_Font.DrawText( 52, fY += 30, 0xffffffff, L"Filter mode: " );
        m_Font.DrawText( 188, fY, 0xffffff00, strBuffer );

        swprintf_s( strBuffer, L"%0.0f Hz", fFreq);
        m_Font.DrawText( 52, fY += 20, 0xffffffff, L"Cutoff Freq: " );
        m_Font.DrawText( 188, fY, 0xffffff00, strBuffer );

        swprintf_s( strBuffer, L"%0.2f", fQ);
        m_Font.DrawText( 52, fY += 20, 0xffffffff, L"Q factor: " );
        m_Font.DrawText( 188, fY, 0xffffff00, strBuffer );

        // Show meter effect
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 52, fY += 60, 0xffffffff, L"Volume Meter effect: " );
        m_Font.SetScaleFactors( 0.8f, 0.8f );

        FLOAT fPeakLevelsDB[XMP_CAPTURE_CHANNEL_COUNT];
        FLOAT fRMSLevelsDB[XMP_CAPTURE_CHANNEL_COUNT];
        for (DWORD k=0; k < XMP_CAPTURE_CHANNEL_COUNT; k++)
        {
            if (m_VolumeMeterLevels.pPeakLevels[k] <= 0.0001f)
                fPeakLevelsDB[k] = -96.0f; // clamp to minimum display volume
            else
                fPeakLevelsDB[k] = XAudio2AmplitudeRatioToDecibels(m_VolumeMeterLevels.pPeakLevels[k]);

            if (m_VolumeMeterLevels.pRMSLevels[k] <= 0.0001f)
                fRMSLevelsDB[k] = -96.0f;
            else
                fRMSLevelsDB[k] = XAudio2AmplitudeRatioToDecibels(m_VolumeMeterLevels.pRMSLevels[k]);
        }

        swprintf_s( strBuffer, L"L: %5.1f, R: %5.1f", fPeakLevelsDB[0], fPeakLevelsDB[1] );
        m_Font.DrawText( 52, fY += 30, 0xffffffff, L"Peak Levels: ");
        m_Font.DrawText( 188, fY, 0xffffff00, strBuffer );

        swprintf_s( strBuffer, L"L: %5.1f, R: %5.1f", fRMSLevelsDB[0], fRMSLevelsDB[1] );
        m_Font.DrawText( 52, fY += 20, 0xffffffff, L"RMS Levels: " );
        m_Font.DrawText( 188, fY, 0xffffff00, strBuffer );


        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


