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

#include "achievements.spa.h"


//--------------------------------------------------------------------------------------
// Struct that holds achievements picture texture info
//--------------------------------------------------------------------------------------
struct AchievementPicture
{
    IDirect3DTexture9* Texture;        // Texture of the picture
    DWORD dwImageId;      // Id of the picture
};

//--------------------------------------------------------------------------------------
// Number of achievements.
//--------------------------------------------------------------------------------------
static const DWORD  ACHIEVEMENT_COUNT = 6;

//--------------------------------------------------------------------------------------
// Invliad user index
//--------------------------------------------------------------------------------------
static const DWORD  INVALID_USER_INDEX = 0xffffffff;

//--------------------------------------------------------------------------------------
// Invalid picture id
//--------------------------------------------------------------------------------------
static const DWORD  INVALID_PICTURE_ID = 0xffffffff;

//--------------------------------------------------------------------------------------
// Gamer picture error codes
//--------------------------------------------------------------------------------------
enum GAMER_PICTURE_AWARD_RESULT
{
    GAMER_PICTURE_AWARD_DOES_NOT_EXIST,
    GAMER_PICTURE_AWARD_ERROR,
    GAMER_PICTURE_AWARD_SUCCESS
};

//--------------------------------------------------------------------------------------
// Color values
//--------------------------------------------------------------------------------------
#define MSG_COLOR           0xffffffff          // message color
#define INFO_COLOR          0xffffff00          // information display color

#define TOP_BACK_COLOR      0xff0000ff          // background gradient colors
#define BOTTOM_BACK_COLOR   0xff000000

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_2, L"Achievements of\nCurrent User" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_1, L"Signin" },
    { ATG::HELP_X_BUTTON, ATG::HELP_PLACEMENT_2, L"Achievements UI\n" },
    { ATG::HELP_Y_BUTTON, ATG::HELP_PLACEMENT_2, L"Achievements of\nLast User" },
    { ATG::HELP_DPAD, ATG::HELP_PLACEMENT_1, L"Move Selection" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_2, L"Award Gamer\nPicture" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    BYTE* m_Achievements;
    DWORD m_dwAchievementCount;   // Number of achievements
    DWORD m_dwGamesPlayed;        // Number of games played total
    DWORD m_dwWinningStreak;      // Number of games won in a row
    WCHAR m_szMessage[128];       // Message to be displayed in UI
    DWORD m_dwMessageDelay;       // Message delay timer
    DWORD m_dwUserIndex;          // Signed in user index
    XUID m_LastUserXuid;         // XUID of the last logged in user
    XUID m_CurrentUserXuid;      // XUID of the current user
    AchievementPicture m_AchievementPictures[ACHIEVEMENT_COUNT]; // Picture textures
    DWORD m_dwSelectedAchievement;// Index of selected achievement

    HRESULT                     InitializePictureTextures( DWORD dwTextureCount );
    VOID                        EnumerateAchievements( XUID xuidUser );
    VOID                        PlayPongGame( BOOL fForceWin );
    GAMER_PICTURE_AWARD_RESULT  AwardGamerPicture( VOID );

private:
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

    m_Achievements = new BYTE[XACHIEVEMENT_SIZE_FULL * ACHIEVEMENT_COUNT];
    if( m_Achievements == NULL )
        return E_FAIL;

    if( FAILED( InitializePictureTextures( ACHIEVEMENT_COUNT ) ) )
        return E_FAIL;

    // Initialize Live
    if( FAILED( XOnlineStartup() ) )
        return E_FAIL;

    // Initialize login
    ATG::SignIn::Initialize( 1, 1, FALSE, 1 );

    m_LastUserXuid = m_CurrentUserXuid = INVALID_XUID;
    m_dwUserIndex = ( DWORD )-1;

    // Reset game count variables
    m_dwAchievementCount = 0;
    m_dwGamesPlayed = 0;
    m_dwWinningStreak = 0;

    m_dwSelectedAchievement = 0;

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
        for( DWORD dwCnt = 0; dwCnt < XUSER_MAX_COUNT; ++dwCnt )
        {
            if( ATG::SignIn::IsUserSignedIn( dwCnt ) )
            {
                if( dwCnt != m_dwUserIndex )
                {
                    m_LastUserXuid = m_CurrentUserXuid;
                    XUserGetXUID( dwCnt, &m_CurrentUserXuid );
                }
                m_dwUserIndex = dwCnt;
                break;
            }
        }

        // Play simulated pong game and write user achievements
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            wcscpy_s( m_szMessage, L"Playing Game..." );
            m_dwMessageDelay = GetTickCount();

            PlayPongGame( pGamepad->bRightTrigger );

            EnumerateAchievements( INVALID_XUID );
        }

        // Enumerate the last signed in users's achievements
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        {
            wcscpy_s( m_szMessage, L"Displaying last user's achievements..." );
            m_dwMessageDelay = GetTickCount();

            EnumerateAchievements( m_LastUserXuid );
        }

        // Move selection arrow
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        {
            m_dwSelectedAchievement++;
            if( m_dwSelectedAchievement == m_dwAchievementCount )
                m_dwSelectedAchievement = 0;
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        {
            m_dwSelectedAchievement--;
            if( m_dwSelectedAchievement == -1 )
                m_dwSelectedAchievement = m_dwAchievementCount - 1;
        }

        // Award currently selected picture as the gamer picture
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
        {
            GAMER_PICTURE_AWARD_RESULT dwRes = AwardGamerPicture();

            if( dwRes == GAMER_PICTURE_AWARD_DOES_NOT_EXIST )
                wcscpy_s( m_szMessage, L"Picture is not available for awarding." );
            else if( dwRes == GAMER_PICTURE_AWARD_ERROR )
                wcscpy_s( m_szMessage, L"Failed to award picture to gamer." );
            else if( dwRes == GAMER_PICTURE_AWARD_SUCCESS )
                wcscpy_s( m_szMessage, L"Successfully awarded picture to gamer." );

            m_dwMessageDelay = GetTickCount();
        }
    }

    if( !ATG::SignIn::IsSystemUIShowing() )
    {
        // Show signin UI
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        {
            // Reset game count variables
            m_dwAchievementCount = 0;
            m_dwGamesPlayed = 0;
            m_dwWinningStreak = 0;

            ATG::SignIn::ShowSignInUI();
        }

        // Show Achievements UI
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        {
            DWORD dwErr;

            dwErr = XShowAchievementsUI( ATG::SignIn::GetSignedInUser() );
            assert( dwErr == ERROR_SUCCESS );
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
        m_Font.DrawText( 0, 0, 0xffffffff, L"Achievements" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();

        // Draw list of achievements
        XACHIEVEMENT_DETAILS* Achievements = ( XACHIEVEMENT_DETAILS* )m_Achievements;

        for( DWORD dwIdx = 0; dwIdx < m_dwAchievementCount; ++dwIdx )
        {
            // Draw achievement picture
            D3DRECT rect;
            rect.x1 = ATG::GetTitleSafeArea().x1 + 32;
            rect.y1 = ATG::GetTitleSafeArea().y1 + 58 + dwIdx * 40;
            rect.x2 = rect.x1 + 32;
            rect.y2 = rect.y1 + 32;
            ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect, m_AchievementPictures[dwIdx].Texture );

            m_Font.Begin();
            if( dwIdx == m_dwSelectedAchievement )
                m_Font.DrawText( 0, 60 + ( FLOAT )dwIdx * 40, INFO_COLOR, GLYPH_RIGHT_ARROW );

            // Only display label and time achieved
            m_Font.DrawText( 80, 60 + ( FLOAT )dwIdx * 40, INFO_COLOR, Achievements[dwIdx].pwszLabel );
            m_Font.DrawText( INFO_COLOR, L" ..... " );

            // Check the achieved bit to see if this achievement has been awarded
            if( Achievements[dwIdx].dwFlags & XACHIEVEMENT_DETAILS_ACHIEVED )
            {
                // The achievement was successfully achieved, but the time awarded
                // is only valid if it was achieved while the console was connected
                // to Live.
                if( Achievements[dwIdx].dwFlags & XACHIEVEMENT_DETAILS_ACHIEVED_ONLINE )
                {
                    SYSTEMTIME systemTime;
                    WCHAR strTime[32];

                    FileTimeToSystemTime( &Achievements[dwIdx].ftAchieved, &systemTime );

                    swprintf_s(
                        strTime,
                        L"%d/%d/%d %d:%02d:%02d.%d\n",
                        systemTime.wYear,
                        systemTime.wMonth,
                        systemTime.wDay,
                        systemTime.wHour,
                        systemTime.wMinute,
                        systemTime.wSecond,
                        systemTime.wMilliseconds );

                    m_Font.DrawText( INFO_COLOR, strTime );
                }
                else
                {
                    m_Font.DrawText( INFO_COLOR, L"Achieved offline" );
                }
            }
            else
            {
                // Unachieved... display the "how-to" string
                m_Font.DrawText( INFO_COLOR, Achievements[dwIdx].pwszUnachieved );
            }
            m_Font.End();
        }

        // Display message
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
// Name: InitializePictureTextures
// Desc: Sets up achievements picture textures
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializePictureTextures( DWORD dwTextureCount )
{
    HRESULT hr = S_OK;

    for( DWORD i = 0; i < dwTextureCount; ++i )
    {
        hr = m_pd3dDevice->CreateTexture(
            64,                     // Width
            64,                     // Height
            1,                      // Levels
            0,                      // Usage
            D3DFMT_LIN_A8R8G8B8,    // This is the only supported texture format for ReadPicture
            0,                      // Pool
            &m_AchievementPictures[i].Texture,
            NULL );

        if( FAILED( hr ) )
            break;
    }

    return( hr );
}


//--------------------------------------------------------------------------------------
// Name: EnumerateAchievements
// Desc: Enumerate existing achievements for the supplied user.
//--------------------------------------------------------------------------------------
VOID Sample::EnumerateAchievements( XUID xuidUser )
{
    HANDLE hEnum;
    DWORD cbBuffer;

    // Create enumerator for the default device
    DWORD dwStatus;
    dwStatus = XUserCreateAchievementEnumerator(
        0,                              // Enumerate for the current title
        m_dwUserIndex,
        xuidUser,                       // If INVALID_XUID, the current user's achievements
                                        // are enumerated
        XACHIEVEMENT_DETAILS_ALL,
        0,                              // starting achievement index
        ACHIEVEMENT_COUNT,              // number of achievements to return
        &cbBuffer,                      // bytes needed
        &hEnum );

    assert( dwStatus == ERROR_SUCCESS );

    // Enumerate achievements
    m_dwAchievementCount = 0;
    DWORD dwItems;

    if( XEnumerate( hEnum, m_Achievements, XACHIEVEMENT_SIZE_FULL * ACHIEVEMENT_COUNT,
                    &dwItems, NULL ) == ERROR_SUCCESS )
    {
        m_dwAchievementCount = dwItems;
    }

    // Retrieve achievement pictures
    XACHIEVEMENT_DETAILS* rgAchievements = ( XACHIEVEMENT_DETAILS* )m_Achievements;

    for( DWORD i = 0; i < m_dwAchievementCount; ++i )
    {
        // Assign gamer pictures to up to 2 achievements
        switch( rgAchievements[i].dwId )
        {
            case ACHIEVEMENT_WON10INAROW:
                m_AchievementPictures[i].dwImageId = GAMER_PICTURE_IMAGE_WON10INAROW;
                break;

            case ACHIEVEMENT_PONG_A_THON_I:
                m_AchievementPictures[i].dwImageId = GAMER_PICTURE_IMAGE_PONGATHON1;
                break;

            default:
                m_AchievementPictures[i].dwImageId = INVALID_PICTURE_ID;
                break;
        }

        IDirect3DTexture9* pTexture = m_AchievementPictures[i].Texture;
        D3DLOCKED_RECT rect;
        D3DSURFACE_DESC desc;

        HRESULT hr = pTexture->GetLevelDesc( 0, &desc );

        assert( SUCCEEDED( hr ) );

        hr = pTexture->LockRect(
            0,                      // Level (no mip-map)
            &rect,
            NULL,                   // Lock the whole texture
            0 );                    // Flags

        assert( SUCCEEDED( hr ) );

        dwStatus = XUserReadAchievementPicture(
            m_dwUserIndex,                              // Requesting user
            TITLEID_ACHIEVEMENTS_SAMPLE,                // Title Id
            rgAchievements[i].dwImageId,                // Image Id
            ( BYTE* )rect.pBits,                          // Texture bytes
            rect.Pitch,
            desc.Height,
            NULL );

        assert( dwStatus == ERROR_SUCCESS );

        pTexture->UnlockRect( 0 );
    }

    CloseHandle( hEnum );
}


//--------------------------------------------------------------------------------------
// Name: PlayPongGame
// Desc: Simulates parameters for a pong game and writes achievements to the currently
//       signed in user profile.
//--------------------------------------------------------------------------------------
VOID Sample::PlayPongGame( BOOL fForceWin )
{
    // generate random wins and losses
    srand( GetTickCount() );

    BOOL fWin;

    if( fForceWin )
        fWin = TRUE;
    else
        fWin = ( rand() % 100 > 25 ) ? TRUE : FALSE;           // win / loose flag

    if( fWin )
    {
        m_dwWinningStreak++;
        lstrcatW( m_szMessage, L" You won!" );
    }
    else
    {
        m_dwWinningStreak = 0;
        lstrcatW( m_szMessage, L" You lost." );
    }

    m_dwGamesPlayed++;

    // prepare for writing achievements
    HANDLE hEventComplete = CreateEvent( NULL, FALSE, FALSE, NULL );

    if( hEventComplete == NULL )
        ATG::FatalError( "PlayPongGame: Couldn't create event.\n" );

    XOVERLAPPED xov;

    ZeroMemory( &xov, sizeof( XOVERLAPPED ) );
    xov.hEvent = hEventComplete;

    // determine which achievements have occured
    XUSER_ACHIEVEMENT Achievements[6];
    int nAchievements = 0;

    if( m_dwGamesPlayed == 1 )
    {
        Achievements[nAchievements].dwUserIndex = m_dwUserIndex;
        Achievements[nAchievements].dwAchievementId = ACHIEVEMENT_PLAYED1GAME;
        nAchievements++;
    }

    if( m_dwGamesPlayed == 10 )
    {
        Achievements[nAchievements].dwUserIndex = m_dwUserIndex;
        Achievements[nAchievements].dwAchievementId = ACHIEVEMENT_PLAYED10GAMES;
        nAchievements++;
    }

    if( m_dwGamesPlayed == 100 )
    {
        Achievements[nAchievements].dwUserIndex = m_dwUserIndex;
        Achievements[nAchievements].dwAchievementId = ACHIEVEMENT_PLAYED100GAMES;
        nAchievements++;
    }

    if( m_dwWinningStreak == 3 )
    {
        Achievements[nAchievements].dwUserIndex = m_dwUserIndex;
        Achievements[nAchievements].dwAchievementId = ACHIEVEMENT_WON3INAROW;
        nAchievements++;
    }

    if( m_dwWinningStreak == 10 )
    {
        Achievements[nAchievements].dwUserIndex = m_dwUserIndex;
        Achievements[nAchievements].dwAchievementId = ACHIEVEMENT_WON10INAROW;
        nAchievements++;
    }

    if( m_dwWinningStreak == 20 )
    {
        Achievements[nAchievements].dwUserIndex = m_dwUserIndex;
        Achievements[nAchievements].dwAchievementId = ACHIEVEMENT_PONG_A_THON_I;
        nAchievements++;
    }

    //
    // Write achievements
    //
    // Before writing an achievement, XContentGetCreator must be called on the
    // loaded save game to verify that the current user is in fact the same user
    // who created the save game. Only call XUserWriteAchievements if this is
    // true, otherwise the title is violating TCR 069 [GP No Sharing of Achievements]
    //
    DWORD dwStatus = XUserWriteAchievements( nAchievements, Achievements, &xov );
    assert( dwStatus == ERROR_IO_PENDING );

    dwStatus = XGetOverlappedResult( &xov, NULL, TRUE );
    assert( dwStatus == ERROR_SUCCESS );

    CloseHandle( hEventComplete );

}


//--------------------------------------------------------------------------------------
// Name: AwardGamerPicture
// Desc: Award the currently selected achievement picture as the gamer picture for the
//       currently signed in gamer.
//--------------------------------------------------------------------------------------
GAMER_PICTURE_AWARD_RESULT Sample::AwardGamerPicture( VOID )
{
    DWORD dwErr;

    if( m_AchievementPictures[m_dwSelectedAchievement].dwImageId == INVALID_PICTURE_ID )
        return GAMER_PICTURE_AWARD_DOES_NOT_EXIST;

    dwErr = XUserAwardGamerPicture(
        m_dwUserIndex,          // Currently signed in gamer
        m_AchievementPictures[m_dwSelectedAchievement].dwImageId, // Selected picture
        0,                      // Reserved parameter
        NULL );                 // Not using overlapped I/O

    if( dwErr != ERROR_SUCCESS )
        return GAMER_PICTURE_AWARD_ERROR;

    return GAMER_PICTURE_AWARD_SUCCESS;
}
