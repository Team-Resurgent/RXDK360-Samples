//--------------------------------------------------------------------------------------
// Achievements.cpp
//
// The sample shows how to read, write and enumerate achievements.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <d3d9.h>
#include <xam.h>
#include <Xgraphics.h>
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgResource.h"
#include "AtgSignIn.h"
#include "AtgUtil.h"
#include "AtgSimpleShaders.h"
#include "AtgDebugDraw.h"

#include "AvatarAwards.spa.h"

//--------------------------------------------------------------------------------------
// Colors used for UI
//--------------------------------------------------------------------------------------
#define MSG_COLOR           0xffff00ff
#define TOP_BACK_COLOR      0xff0000ff          // background gradient colors
#define BOTTOM_BACK_COLOR   0xff000000

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_1, L"Play one game" },
    { ATG::HELP_RIGHT_TRIGGER, ATG::HELP_PLACEMENT_1, L"Hold to cheat" },
    { ATG::HELP_LEFT_SHOULDER, ATG::HELP_PLACEMENT_1, L"Award all assets" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_1, L"Signin" },
};
static CONST DWORD  NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
private:
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    DWORD m_dwGamesPlayed;      // Number of games played total
    DWORD m_dwWinningStreak;    // Number of games won in a row
    WCHAR m_szMessage[128];     // Message to be displayed in UI
    DWORD m_dwMessageDelay;     // Message delay timer

    VOID                        PlayPongGame( BOOL bForceWin );
    VOID                        AwardAvatarAsset( DWORD dwAwardId );

    virtual HRESULT             Initialize();
    virtual HRESULT             Update();
    virtual HRESULT             Render();
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
// Name: Initialize
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Initialize the simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize login
    ATG::SignIn::Initialize( 1, 1, FALSE, 1 );

    // Reset game count variables
    m_dwGamesPlayed = 0;
    m_dwWinningStreak = 0;

    // Clear message string
    m_szMessage[0] = L'\0';

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Update signin
    ATG::SignIn::Update();

    // Check to see if user has signed in
    if( ATG::SignIn::AreUsersSignedIn() )
    {
        // Play simulated pong game and awards items
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            wcscpy_s( m_szMessage, L"Playing Game..." );
            m_dwMessageDelay = GetTickCount();
            PlayPongGame( pGamepad->bRightTrigger );
        }

        // Award everything if the user hits the special key combo
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
        {
            AwardAvatarAsset( AVATARASSETAWARD_FULL );
            AwardAvatarAsset( AVATARASSETAWARD_HELMET );
            AwardAvatarAsset( AVATARASSETAWARD_TORSO );
            AwardAvatarAsset( AVATARASSETAWARD_LEGS );
        }
    }

    // Show the signin UI if prompted
    if( !ATG::SignIn::IsSystemUIShowing() )
    {
        // Show signin UI
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        {
            // Reset game count variables
            m_dwGamesPlayed = 0;
            m_dwWinningStreak = 0;

            ATG::SignIn::ShowSignInUI();
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( TOP_BACK_COLOR, BOTTOM_BACK_COLOR );

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
        m_Font.DrawText( 0, 0, 0xffffffff, L"Avatar Awards" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();

        // Display messages for 2000 ms
        m_Font.Begin();
        m_Font.SetScaleFactors( 0.8f, 0.8f );
        if( GetTickCount() - m_dwMessageDelay < 2000 )
            m_Font.DrawText( 0, -30, MSG_COLOR, m_szMessage );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: PlayPongGame
// Desc: Simulates parameters for a pong game and award items to the currently
//       signed in user profile.
//--------------------------------------------------------------------------------------
VOID Sample::PlayPongGame( BOOL bForceWin )
{
    // generate random wins and losses
    srand( GetTickCount() );

    BOOL bWin;

    if( bForceWin )
        bWin = TRUE;
    else
        bWin = ( rand() % 100 > 25 ) ? TRUE : FALSE;           // win / loose flag

    if( bWin )
    {
        m_dwWinningStreak++;
        lstrcatW( m_szMessage, L" You won!" );
    }
    else
    {
        m_dwWinningStreak = 0;
        lstrcatW( m_szMessage, L" You lost." );

        // award an asset, it's the effort that counts
        AwardAvatarAsset( AVATARASSETAWARD_HELMET );
    }

    m_dwGamesPlayed++;

    // check statistics and award assets as appropriate
    if( m_dwGamesPlayed >= 10 )
        AwardAvatarAsset( AVATARASSETAWARD_LEGS );

    if( m_dwWinningStreak >= 3 )
        AwardAvatarAsset( AVATARASSETAWARD_FULL );

    if( m_dwWinningStreak >= 5 )
        AwardAvatarAsset( AVATARASSETAWARD_TORSO );

}


//--------------------------------------------------------------------------------------
// Name: AwardAvatarAsset
// Desc: Award the specified asset to the player currently signed in and playing games.
//--------------------------------------------------------------------------------------
VOID Sample::AwardAvatarAsset( DWORD dwAwardId )
{	
    // prepare for awarding asset
    HANDLE hEventComplete = CreateEvent( NULL, FALSE, FALSE, NULL );

    if( hEventComplete == NULL )
        ATG::FatalError( "AwardAvatarAsset: Couldn't create event.\n" );

    // prepare the XOVERLAPPED object
    XOVERLAPPED xov;

    ZeroMemory( &xov, sizeof( XOVERLAPPED ) );
    xov.hEvent = hEventComplete;

    // set up the award information object
    XUSER_AVATARASSET  avataraAssetAward;
    avataraAssetAward.dwUserIndex = ATG::SignIn::GetSignedInUser();
    avataraAssetAward.dwAwardId = dwAwardId;  

    // award the object and verify the process
    DWORD dwStatus = XUserAwardAvatarAssets(1, &avataraAssetAward, &xov);

    assert( dwStatus == ERROR_IO_PENDING );

    dwStatus = XGetOverlappedResult( &xov, NULL, TRUE );

    if( dwStatus != ERROR_SUCCESS )
    {
        DWORD dwExtendedError = XGetOverlappedExtendedError( &xov );
        if( dwExtendedError == SPA_E_NOT_LOADED )
            ATG::DebugSpew( "The SPA data was not properly embedded in the XEX.\n" );
        ATG::FatalError( "An error occurred while trying to award an asset.\n\tError code: %d\n\tExtended error: %d\n", dwStatus, dwExtendedError );
    }

    CloseHandle( hEventComplete );
}
