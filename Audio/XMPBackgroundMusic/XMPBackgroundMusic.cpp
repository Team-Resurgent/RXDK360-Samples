//--------------------------------------------------------------------------------------
// XMPBackgroundMusic.cpp
//
// This sample demonstrates the basic functionality of XMP by showing how to create and
// play playlists using the XMP API.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xmp.h>
#include <AtgApp.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>


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

const DWORD         NUM_SONGS = sizeof( g_SongDescriptors ) / sizeof( g_SongDescriptors[0] );


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Play/Pause" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_1, L"Stop" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_1, L"Select song" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


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
    XMP_HANDLE m_hXMPPlaylist;            // The music player playlist
    XMP_SONGINFO m_songInfo;                // Info about the current song.
    XMP_STATE m_XMPState;                // The current status of the music player

    // System Notifications
    HANDLE m_hNotificationListener;

public:
    HRESULT Initialize();
    HRESULT ShutDown();
    HRESULT Update();
    HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
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

    // Create the font
    if( FAILED( m_Font.Create( "GAME:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "GAME:\\media\\help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the notification listener to listen for XMP notifications
    m_hNotificationListener = XNotifyCreateListener( XNOTIFY_XMP );
    if( !m_hNotificationListener )
        ATG::FatalError( "Error calling XNotifyCreateListener\n" );

    // Initialize XMP state variable
    dwStatus = XMPGetStatus( &m_XMPState );
    assert( dwStatus == ERROR_SUCCESS );

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
                    assert( dwStatus == ERROR_SUCCESS );
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
        m_Font.DrawText( 0, 0, 0xffffffff, L"XMPBackgroundMusic" );
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

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


