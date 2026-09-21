//--------------------------------------------------------------------------------------
// RichPresence.cpp
//
// The sample illustrates how to use rich presence to send state information between
// two friend users either on the same box (split screen) or between different boxes.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <d3d9.h>
#include "xbox.h"
#include "xonline.h"
#include <xam.h>
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgResource.h"
#include "AtgSignIn.h"
#include "AtgUtil.h"

#include "RichPresence.spa.h"


//--------------------------------------------------------------------------------------
// Color values
//--------------------------------------------------------------------------------------
#define INFO_COLOR          0xffffff00          // information display color
#define SEP_COLOR           0xffffffff          // split screen separator color
#define PRESENCE_COLOR      0xff00ffff          // presence string color

#define TOP_BACK_COLOR      0xff7f0000          // background gradient colors
#define BOTTOM_BACK_COLOR   0xff00007f


//--------------------------------------------------------------------------------------
// Game state data to be sent between two users.
//--------------------------------------------------------------------------------------
struct GAME_STATE_DATA
{
    FLOAT fCursorX;             // cursor coordinates
    FLOAT fCursorY;
};

//--------------------------------------------------------------------------------------
// User specific data.
//--------------------------------------------------------------------------------------
struct USER_DATA
{
    XUID xuid;                     // user id
    GAME_STATE_DATA gameState;               // game state that is sent to remote user/player
    DWORD dwUserIndex;             // user index (0..3)
    DWORD dwLastPresenceEnumerate;
};

static const DWORD  NUM_USERS = 2;
static const DWORD  NUM_FRIENDS_DISPLAY = 3;
static const DWORD  MAX_FRIENDS_DISPLAY_SIZE = XUSER_NAME_SIZE + 2 + MAX_RICHPRESENCE_SIZE;


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_LEFTSTICK, ATG::HELP_PLACEMENT_1, L"Move Cursor" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_1, L"Signin" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer       m_Timer;
    ATG::Font        m_Font;
    ATG::Help        m_Help;
    BOOL             m_bDrawHelp;
    USER_DATA        m_UserData[ NUM_USERS ];  // user specific data
    DWORD            m_dwUserCount;            // number of users signed in
    BOOL             m_bFriendsUIActive[ NUM_USERS ];  // friends UI active
    BOOL             m_bUIActive;              // is any UI active
    HANDLE           m_hNotification;          // UI notification handle
    D3DSURFACE_DESC  m_SurfaceDesc;            // D3D surface
    DWORD            m_dwLastPresenceUpdate;
    DWORD            m_dwAllocAttributes;
    XONLINE_FRIEND*  m_pFriends[ NUM_USERS ];
    DWORD            m_dwPresenceCount;
    WCHAR            m_wszFriendInfo[ NUM_USERS ][ NUM_FRIENDS_DISPLAY ][ MAX_FRIENDS_DISPLAY_SIZE ];  // Rich Presence display strings

    VOID            UpdatePresence();
    VOID            EnumeratePresence( DWORD dwUserIndex );


private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
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

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    ZeroMemory( m_UserData, sizeof( m_UserData ) );
    for( DWORD i = 0; i < NUM_USERS; ++i )
        m_UserData[i].dwLastPresenceEnumerate = GetTickCount();

    // Initialize Live
    if( FAILED( XOnlineStartup() ) )
        return E_FAIL;

    // Start sample by showing the signin UI
    m_bUIActive = FALSE;

    ATG::SignIn::Initialize( NUM_USERS, NUM_USERS, TRUE, NUM_USERS );
    ATG::SignIn::ShowSignInUI();

    // Initialize rich presence. We need to do this for subscribing to extended (string) presence info
    if( ERROR_SUCCESS != XPresenceInitialize( MAX_FRIENDS * NUM_USERS ) )
        return E_FAIL;

    // Create notification listener for UI events
    m_hNotification = XNotifyCreateListener( XNOTIFY_SYSTEM );
    if( m_hNotification == NULL || m_hNotification == INVALID_HANDLE_VALUE )
        return E_FAIL;

    m_dwLastPresenceUpdate = GetTickCount();
    m_dwAllocAttributes = MAKE_XALLOC_ATTRIBUTES( 0, FALSE, FALSE, FALSE, 0,
                                                  XALLOC_ALIGNMENT_DEFAULT, XALLOC_MEMPROTECT_READWRITE,
                                                  FALSE, XALLOC_MEMTYPE_HEAP );
    ZeroMemory( m_pFriends, sizeof( m_pFriends ) );
    m_dwPresenceCount = 0;
    m_dwUserCount = 0;
    ZeroMemory( m_bFriendsUIActive, sizeof( m_bFriendsUIActive ) );

    // Set up Rich Presence display strings
    for ( int dwUser = 0; dwUser < NUM_USERS ; dwUser++ )
    {
        for ( int dwFriendIdx = 0; dwFriendIdx < NUM_FRIENDS_DISPLAY; dwFriendIdx++ )
        {
            ZeroMemory( m_wszFriendInfo[ dwUser ][ dwFriendIdx ], MAX_FRIENDS_DISPLAY_SIZE * sizeof(WCHAR) );
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get input from all the gamepads
    ATG::GAMEPAD Gamepads[XUSER_MAX_COUNT];
    ATG::Input::GetInput( Gamepads );

    // Get merged input to display help
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Retreive UI state
    ATG::SignIn::Update();

    DWORD dwNotificationId;
    ULONG ulParam;

    if( XNotifyGetNext( m_hNotification, 0, &dwNotificationId, &ulParam ) )
    {
        switch( dwNotificationId )
        {
            case XN_SYS_SIGNINCHANGED:
                m_dwUserCount = 0;
                for( DWORD dwCnt = 0; dwCnt < XUSER_MAX_COUNT; ++dwCnt )
                {
                    if( ATG::SignIn::IsUserSignedIn( dwCnt ) )
                    {
                        assert( m_dwUserCount < NUM_USERS );
                        m_UserData[m_dwUserCount].dwUserIndex = dwCnt;
                        XUserGetXUID( dwCnt, &m_UserData[m_dwUserCount].xuid );
                        m_UserData[m_dwUserCount].gameState.fCursorX = 0.5f;
                        m_UserData[m_dwUserCount].gameState.fCursorY = 0.5f;
                        m_dwUserCount++;
                    }
                }
                break;

            case XN_SYS_UI:
                m_bUIActive = ( BOOL )ulParam;
                break;
        }
    }

    if( m_bUIActive == FALSE )
    {
        // Display the Signin UI again
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        {
            ATG::SignIn::ShowSignInUI();
        }

        // Update cursor coordinates
        for( DWORD dwCnt = 0; dwCnt < m_dwUserCount; ++dwCnt )
        {
            // Retreive cursor coordinates from gamepad
            m_UserData[dwCnt].gameState.fCursorX += Gamepads[m_UserData[dwCnt].dwUserIndex].fX1 * 0.001f;
            m_UserData[dwCnt].gameState.fCursorY -= Gamepads[m_UserData[dwCnt].dwUserIndex].fY1 * 0.001f;

            if( m_UserData[dwCnt].gameState.fCursorX < 0.0f )
                m_UserData[dwCnt].gameState.fCursorX = 0.0f;

            if( m_UserData[dwCnt].gameState.fCursorX > 1.0f )
                m_UserData[dwCnt].gameState.fCursorX = 1.0f;

            if( m_UserData[dwCnt].gameState.fCursorY < 0.0f )
                m_UserData[dwCnt].gameState.fCursorY = 0.0f;

            if( m_UserData[dwCnt].gameState.fCursorY > 1.0f )
                m_UserData[dwCnt].gameState.fCursorY = 1.0f;
        }

        // Update presence for all users
        UpdatePresence();
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
        // Get render target dimensions
        IDirect3DSurface9* pRenderTarget = NULL;
        m_pd3dDevice->GetRenderTarget( 0, &pRenderTarget );
        if( !pRenderTarget )
            return S_OK;

        pRenderTarget->GetDesc( &m_SurfaceDesc );
        pRenderTarget->Release();

        // Draw title text
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"RichPresence" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        FLOAT fSurfaceWidth = ( FLOAT )( m_Font.m_rcWindow.x2 - m_Font.m_rcWindow.x1 );
        FLOAT fSurfaceHeight = ( FLOAT )( m_Font.m_rcWindow.y2 - m_Font.m_rcWindow.y1 );

        m_Font.SetScaleFactors( 1.0f, 1.0f );

        m_Font.DrawText( ( FLOAT )( fSurfaceWidth / 2 ),
                         ( FLOAT )( fSurfaceHeight / 2 ) - 12.0f, SEP_COLOR,
                         L"_______________________________________________", ATGFONT_CENTER_X );

        for( DWORD dwCnt = 0; dwCnt < m_dwUserCount; ++dwCnt )
        {
            // Display presence strings
            m_Font.SetScaleFactors( 0.8f, 0.8f );

            // Display friend's presence strings
            for ( int dwFriendsIdx = 0; dwFriendsIdx < NUM_FRIENDS_DISPLAY; dwFriendsIdx++ )
            {
                m_Font.SetCursorPosition( 48.0f,
                    60.0f + ( dwCnt * m_SurfaceDesc.Height / 2 ) + ( dwCnt * 30.0f ) + ( dwFriendsIdx * 20.0f ) );

                m_Font.DrawText( PRESENCE_COLOR, m_wszFriendInfo[ dwCnt ][ dwFriendsIdx ] );
            }

            // Display cursor
            m_Font.SetScaleFactors( 1.0f, 1.0f );
            FLOAT fCursorX = 50 + m_UserData[dwCnt].gameState.fCursorX * ( fSurfaceWidth - 100 );

            FLOAT fCursorY = ( 50 + m_UserData[dwCnt].gameState.fCursorY * ( ( fSurfaceHeight / 2 ) - 100 ) ) +
                ( ( fSurfaceHeight / 2 ) * dwCnt );

            m_Font.DrawText( fCursorX, fCursorY, INFO_COLOR, GLYPH_FILLED_CIRCLE );
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdatePresence
// Desc: Updates presence information for all users.
//--------------------------------------------------------------------------------------
VOID Sample::UpdatePresence()
{
    // Do not update presence information too often to decrease network traffic
    if( ( GetTickCount() - m_dwLastPresenceUpdate ) > 2000 )
    {
        for( DWORD dwCnt = 0; dwCnt < m_dwUserCount; ++dwCnt )
        {
            // Set the active/idle game context value
            XUserSetContext( m_UserData[dwCnt].dwUserIndex, CONTEXT_GAMESTATE, CONTEXT_GAMESTATE_ACTIVE_GAME );

            // Set cursor coordinate properties
            int iCursorX = ( int )( 50 + m_UserData[dwCnt].gameState.fCursorX * ( m_SurfaceDesc.Width - 100 ) );
            int iCursorY = ( int )( 50 + m_UserData[dwCnt].gameState.fCursorY * ( ( m_SurfaceDesc.Height / 2.0f ) -
                                                                                  100 ) );

            XUserSetProperty( m_UserData[dwCnt].dwUserIndex, PROPERTY_CURSORX, sizeof( iCursorX ), &iCursorX );
            XUserSetProperty( m_UserData[dwCnt].dwUserIndex, PROPERTY_CURSORY, sizeof( iCursorY ), &iCursorY );

            // Update the presence mode
            XUserSetContext( m_UserData[dwCnt].dwUserIndex, X_CONTEXT_PRESENCE, CONTEXT_PRESENCE_PRESENCE );

            // update friend's Rich Presence strings
            EnumeratePresence( dwCnt );

            for ( int dwFriendsIdx = 0; dwFriendsIdx < NUM_FRIENDS_DISPLAY; dwFriendsIdx++ )
            {
                if ( wcslen( m_pFriends[ dwCnt ][ dwFriendsIdx ].wszRichPresence ) > 0 &&
                     m_pFriends[ dwCnt ][ dwFriendsIdx ].dwTitleID != 0 )
                {
                    // Friend with Rich Presence information
                    _snwprintf_s( m_wszFriendInfo[ dwCnt ][ dwFriendsIdx ], _TRUNCATE, L"[%S] %s", m_pFriends[ dwCnt ][ dwFriendsIdx ].szGamertag,
                                    m_pFriends[ dwCnt ][ dwFriendsIdx ].wszRichPresence );
                }
                else if ( m_pFriends[ dwCnt ][ dwFriendsIdx ].xuid != 0x0 )
                {
                    // Friend without Rich Presence information
                    _snwprintf_s( m_wszFriendInfo[ dwCnt ][ dwFriendsIdx ], _TRUNCATE, L"[%S] <no presence>", m_pFriends[ dwCnt ][ dwFriendsIdx ].szGamertag );
                }
                else
                {
                    // No friend listed
                    _snwprintf_s( m_wszFriendInfo[ dwCnt ][ dwFriendsIdx ], _TRUNCATE, L"\0" );
                }
            }
        }

        m_dwLastPresenceUpdate = GetTickCount();
    }
}


//--------------------------------------------------------------------------------------
// Name: EnumeratePresence
// Desc: Enumerates peers of given user index.
//--------------------------------------------------------------------------------------
VOID Sample::EnumeratePresence( DWORD dwUserIndex )
{
    DWORD cbBuffer;
    HANDLE hFriendsEnum;

    // Do not enumerate presence information too often to decrease network traffic
    if( ( GetTickCount() - m_UserData[dwUserIndex].dwLastPresenceEnumerate ) > 2000 )
    {
        m_UserData[dwUserIndex].dwLastPresenceEnumerate = GetTickCount();

        // Enumerate presence strings of friends
        DWORD dwRet;
        dwRet = XFriendsCreateEnumerator(
            dwUserIndex,                    // user of whom to enumerate friends
            0,                              // starting index
            MAX_FRIENDS,                    // max number of friends
            &cbBuffer,                      // size of buffer needed
            &hFriendsEnum );

        // Presence information not yet available
        if( dwRet != ERROR_SUCCESS )
            return;

        // Re-allocate the Friends buffer so that it is the exact size requested by XFriendsCreateEnumerator
        XMemFree( m_pFriends[dwUserIndex], m_dwAllocAttributes );
        m_pFriends[ dwUserIndex ] = ( XONLINE_FRIEND* )XMemAlloc( cbBuffer, m_dwAllocAttributes );
        if( m_pFriends[ dwUserIndex ] == NULL )
        {
            CloseHandle( hFriendsEnum );
            return;
        }

        // Zero out the presence data so we don't return garbage if we have no friends
        ZeroMemory( m_pFriends[dwUserIndex], cbBuffer );

        dwRet = XEnumerate( hFriendsEnum, m_pFriends[ dwUserIndex ], cbBuffer, &m_dwPresenceCount, NULL );
        if( dwRet != ERROR_SUCCESS )
        {
            CloseHandle( hFriendsEnum );
            return;
        }

        CloseHandle( hFriendsEnum );
    }
}

