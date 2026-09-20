//--------------------------------------------------------------------------------------
//
// SimpleXMV.cpp
//
// Sample showing the simplest way to play XMV movies
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <xaudio2.h>
#include <xmedia2.h>


const CHAR*                 g_strMovieName = "game:\\Media\\Video\\Sample.wmv";

// Get global access to the main D3D device
extern IDirect3DDevice9*    g_pd3dDevice;


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_2, L"Play/Pause\nmovie" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_1, L"Stop movie" },
};
static const DWORD          NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    BOOL m_bFailed;

    IXAudio2* m_pXAudio2;

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
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
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize the XAudio2 Engine. The XAudio2 Engine is needed for movie playback.
    UINT32 flags = 0;
#ifdef _DEBUG
    flags |= XAUDIO2_DEBUG_ENGINE;
#endif

    HRESULT hr = XAudio2Create( &m_pXAudio2, flags );
    if( FAILED( hr ) )
        ATG::FatalError( "Error %#X calling XAudio2Create\n", hr );

    IXAudio2MasteringVoice* pMasteringVoice = NULL;
    hr = m_pXAudio2->CreateMasteringVoice( &pMasteringVoice );
    if( FAILED( hr ) )
        ATG::FatalError( "Error %#X calling CreateMasteringVoice\n", hr );

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    m_bDrawHelp = FALSE;
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    m_bFailed = FALSE;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: VideoCallback()
// Desc: This function is called at the end of each movie frame to process
//       user input. This allows pausing or canceling video playback.
//--------------------------------------------------------------------------------------
VOID VideoCallback( PVOID pMovieData )
{
    assert( pMovieData );
    IXMedia2XmvPlayer* xmvPlayer = ( IXMedia2XmvPlayer* )pMovieData;

    // See if the user wants to pause, un-pause, or cancel movie playback.
    // This function will be called after every frame, including while the
    // movie is paused.

    // Watch for user input.
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // 'A' means pause or unpause the movie.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        XMEDIA_PLAYBACK_STATUS playbackStatus;

        if( SUCCEEDED( xmvPlayer->GetStatus( &playbackStatus ) ) )
        {
            if( XMEDIA_PLAYER_PAUSED == playbackStatus.Status )
            {
                xmvPlayer->Resume();
            }
            else
            {
                xmvPlayer->Pause();
            }
        }
    }

    // 'B' means cancel the movie.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        xmvPlayer->Stop( XMEDIA_STOP_IMMEDIATE );
    }
}

//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//       The movie is played from here.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        IXMedia2XmvPlayer* xmvPlayer;
        XMEDIA_XMV_CREATE_PARAMETERS XmvParams;

        ZeroMemory( &XmvParams, sizeof( XmvParams ) );
        XmvParams.createType = XMEDIA_CREATE_FROM_FILE;
        XmvParams.createFromFile.szFileName = g_strMovieName;

        // Use the default audio and video streams.
        // If using a wmv file with multiple audio or video streams
        // (such as different audio streams for different languages)
        // the dwAudioStreamId & dwVideoStreamId parameters can be used 
        // to select which audio (or video) stream will be played back

        XmvParams.dwAudioStreamId = XMEDIA_STREAM_ID_USE_DEFAULT;
        XmvParams.dwVideoStreamId = XMEDIA_STREAM_ID_USE_DEFAULT;

        if( SUCCEEDED( XMedia2CreateXmvPlayer( m_pd3dDevice, m_pXAudio2, &XmvParams, &xmvPlayer ) ) )
        {
            m_bFailed = FALSE;

            // Set up a callback for processing input. This callback will be called
            // once per frame.
            xmvPlayer->SetCallback( XMEDIA_NOTIFY_END_OF_FRAME, VideoCallback, xmvPlayer );

            // Play the movie.
            xmvPlayer->Play( 0, 0 );

            // Release the movie object
            xmvPlayer->Release();
            xmvPlayer = 0;
            // Movie playback changes various D3D states, so you should reset the
            // states that you need after movie playback is finished.
            m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
        }
        else
        {
            m_bFailed = TRUE;
        }
    }

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

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
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 36, 0xffffffff, L"SimpleXMV" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( -1, 38, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        WCHAR strBuffer[1000];
        if( m_bFailed )
            swprintf_s( strBuffer, L"Failed to load movie\n%S", g_strMovieName );
        else
            swprintf_s( strBuffer, L"Press " GLYPH_A_BUTTON L"to play\n%S", g_strMovieName );
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 100, 0xffffffff, strBuffer );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
