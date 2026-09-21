//--------------------------------------------------------------------------------------
// SimplePartyExperience.cpp
//
// The SimplePartyExperience shows how to use the XParty API to query and display 
// members in your party, send game invites to those members who are in dashboard or 
// another title, and finally bring up the Party UI.
//
// For sake of simplicity, QNET was used to handle Xbox Live session management. QNET is 
// by no means required for XParty.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <d3d9.h>
#include "xbox.h"
#include "xonline.h"
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgResource.h"
#include "AtgSignIn.h"
#include "AtgUtil.h"
#include <XParty.h>
#include <XAudio2.h>
#include <QNet.h>
#include <AtgConsole.h>
#include "SimplePartyExperience.spa.h"


//--------------------------------------------------------------------------------------
// Color values
//--------------------------------------------------------------------------------------
#define TOP_BACK_COLOR      0xff00007f          // background gradient colors
#define BOTTOM_BACK_COLOR   0xff000000
#define PARTY_LIST_COLOR    0xffd0d0af
#define ACTIVE_BUTTON_COLOR 0xffffffff


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
      { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
      { ATG::HELP_A_BUTTON,    ATG::HELP_PLACEMENT_1, L"Start a Party" },
};
const DWORD NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//----------------------------------------------------------------------------------
// Name: class CQNetSimple
// Desc: Simple live functionality via QNet.
//----------------------------------------------------------------------------------
class CQNetSimple : public IQNetCallbacks
{
public:
    CQNetSimple::CQNetSimple() : 
      m_pIQNet(NULL),
      m_dwPrimaryUserIndex(XUSER_INDEX_NONE),
      m_dwLocalUsersMask(0),
      m_fJoinFromInvite(FALSE)
    {
        ZeroMemory( &m_XInviteInfo, sizeof( m_XInviteInfo ) );
    }

    // Required to override pure virtuals for basic QNet functionality
    virtual VOID NotifyStateChanged( IN QNET_STATE OldState, IN QNET_STATE NewState,
                    IN HRESULT hrInfo ) {};
    virtual VOID NotifyPlayerJoined( IN IQNetPlayer *pPlayer ) {};
    virtual VOID NotifyPlayerLeaving( IN IQNetPlayer *pPlayer ) {};
    virtual VOID NotifyNewHost( IN IQNetPlayer *pPlayer ) {};
    virtual VOID NotifyDataReceived( IN IQNetPlayer *pPlayerFrom, 
                    IN DWORD dwNumPlayersTo, IN IQNetPlayer **apPlayersTo, 
                    IN const BYTE *pbData, IN DWORD dwDataSize ) {};
    virtual VOID NotifyWriteStats( IN IQNetPlayer *pPlayer ) {};
    virtual VOID NotifyReadinessChanged( IN IQNetPlayer *pPlayer, IN BOOL bReady ) {};
    virtual VOID NotifyCommSettingsChanged( IN IQNetPlayer *pPlayer ) {};
    virtual VOID NotifyGameSearchComplete( IN IQNetGameSearch *pGameSearch, 
                    IN HRESULT hrComplete, IN DWORD dwNumResults ) {};
    virtual VOID NotifyGameInvite( IN DWORD dwUserIndex, 
                    IN const XINVITE_INFO *pInviteInfo);
    virtual VOID NotifyContextChanged(
                    IN const XUSER_CONTEXT *    pContext ) {};
    virtual VOID NotifyPropertyChanged(
                    IN const XUSER_PROPERTY *   pProperty ) {};

    HRESULT JoinInvitedGame( );
    HRESULT RunStateIdle();

public:
    IQNet* m_pIQNet;             // pointer to QNet interface
    BOOL m_fJoinFromInvite;      // Keep track of join invite so we can correctly join the session

    XINVITE_INFO m_XInviteInfo;  // save invite information to join
    DWORD m_dwPrimaryUserIndex;  // the user index of the first player found
    DWORD m_dwLocalUsersMask;    // bit mask for all local users that are participating
};

//-----------------------------------------------------------------------------
// Name: CQNetSimple::RunStateIdle()
// Desc: Per-frame logic for the QNET_STATE_IDLE state
//-----------------------------------------------------------------------------
HRESULT CQNetSimple::RunStateIdle()
{
    HRESULT hr;

    // If there's a invited game to join, do so.  Otherwise, quit.
    if( m_fJoinFromInvite )
    {
        hr = JoinInvitedGame();
        if( FAILED( hr ) )
        {
            ATG::DebugSpew( "Failed joining invited game (err = 0x%08x)!\n", hr );
            return hr;
        }
    }
    else
    {
        ATG::DebugSpew( "Sample is idle and nothing more to do, quitting.\n" );
    }
    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: CQNetSimple::NotifyGameInvite()
// Desc: Callback invoked when a game invite has been accepted, or the user has
//       requested to join a friend's session.
//-----------------------------------------------------------------------------
VOID
CQNetSimple::NotifyGameInvite(
    IN DWORD                    dwUserIndex,
    IN const XINVITE_INFO *     pInviteInfo
    )
{
    HRESULT hr;

    ATG::DebugSpew( "User index %u received game invite.\n", dwUserIndex );

    // Save the invite info and remember that we need to join it.
    memcpy( &m_XInviteInfo, pInviteInfo, sizeof( m_XInviteInfo ) );
    m_fJoinFromInvite = TRUE;

    // The user received a game invite.  If we were in the middle of a session
    // already, we should prompt the user about terminating the session and the
    // possible data loss, then leave the session.
    if( m_pIQNet->GetState() != QNET_STATE_IDLE )
    {
        // Start leaving the game.
        ATG::DebugSpew( "Leaving game due to invite.\n" );
        hr = m_pIQNet->LeaveGame( TRUE );
        if( FAILED( hr ) )
        {
            ATG::DebugSpew( "Failed leaving game (err = 0x%08x)!\n", hr );
            return;
        }

        // Once we've left the game (may have happened inside the LeaveGame
        // call), we will automatically join the specified game.  See the logic
        // in CQNetSimple::RunStateIdle() andCQNetSimple::JoinInvitedGame().
    }
    else
    {
        // We're not in a session, so we can join right now.
        hr = JoinInvitedGame();
        if( FAILED( hr ) )
        {
            ATG::DebugSpew( "Failed joining invited game (err = 0x%08x)!\n", hr );
            return;
        }
    }
}

//-----------------------------------------------------------------------------
// Name: CQNetSimple::JoinInvitedGame()
// Desc: Routine to join the session identified by a previously accepted game
//       invite
//-----------------------------------------------------------------------------
HRESULT CQNetSimple::JoinInvitedGame( )
{
    HRESULT hr;

    // If we haven't been invited, there's nothing to join.
    if( !m_fJoinFromInvite )
    {
        return S_FALSE;
    }
    // Join the game.
    ATG::DebugSpew( "Joining game from invite.\n" );
    hr = m_pIQNet->JoinGameFromInviteInfo(
        m_dwPrimaryUserIndex, // dwUserIndex
        m_dwLocalUsersMask,   // dwUserMask
        &m_XInviteInfo );     // pInviteInfo
    if( FAILED( hr ) )
    {
        ATG::DebugSpew( "Failed joining game from invite (err = 0x%08x)!\n", hr );
        return hr;
    }

    return S_OK;
}
//--------------------------------------------------------------------------------------
// Name: class SimplePartyExperience
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class SimplePartyExperience : public ATG::Application
{
public:
    SimplePartyExperience();

    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;

    BOOL m_bDrawHelp;            // whether we are drawing help
    XPARTY_USER_LIST m_UserList; // XParty user list
    IQNet* m_pIQNet;             // pointer to QNet interface
    HANDLE m_hNotify;            // Handle for LIVE notifications

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT UpdateInput(const ATG::GAMEPAD* pGamepad);
    virtual HRESULT Render();
    VOID HandleSignIn();
    void RenderParty();

    IXAudio2 *m_pXAudio2;
	CQNetSimple m_CQNetSimple;
};


//--------------------------------------------------------------------------------------
// Name: SimplePartyExperience()
// Desc: Initialize member variables
//--------------------------------------------------------------------------------------
SimplePartyExperience::SimplePartyExperience() :
    m_bDrawHelp(FALSE),
    m_pIQNet(NULL),
    m_hNotify(NULL)
{}

    
//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    SimplePartyExperience atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, 
        &atgApp.m_d3dpp.BackBufferHeight );
    
    atgApp.Run();
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects and Live functionality.
//--------------------------------------------------------------------------------------
HRESULT SimplePartyExperience::Initialize()
{
      memset( &m_UserList, 0x00, sizeof(m_UserList) );

      IXAudio2MasteringVoice* pMasteringVoice = NULL;
      HRESULT hResult = XAudio2Create( &m_pXAudio2, 0 );
      if ( FAILED( hResult ) )
      {
            return E_FAIL;
      }
      else
      {
            hResult = m_pXAudio2->CreateMasteringVoice( &pMasteringVoice, XAUDIO2_DEFAULT_CHANNELS,
                                             XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, NULL );
      }
      if ( FAILED( hResult ) )
      {
            return E_FAIL;
      }

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Start up XNet with default settings.
    if( FAILED( XNetStartup( NULL ) ) )
        return E_FAIL;    // Initialize Live

    // Start up XOnline.
    if( FAILED( XOnlineStartup() ) )
    {        
        XNetCleanup();
        return E_FAIL;
    }

    // Initialize login
    ATG::SignIn::Initialize( 1, 1, TRUE, 1 );

    // Create the QNet object.
    if( FAILED( QNetCreateUsingXAudio2( QNET_SESSIONTYPE_XBOXLIVE_STANDARD, 
		&m_CQNetSimple, NULL, m_pXAudio2, &m_CQNetSimple.m_pIQNet ) ) )
    {
        XOnlineCleanup();
        XNetCleanup();
        return E_FAIL;
    }

    // Create the notify listener
    m_hNotify = XNotifyCreateListener( XNOTIFY_PARTY );
    if( m_hNotify == NULL )
    {
        OutputDebugString( "Failed to create a notification listener.\n" );
        return E_FAIL;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SimplePartyExperience::BlockUntilSignedIn()
// Desc: A simple function that doesn't return until at least one profile is
//       signed in.  Real titles will probably wish to use the more robust
//       ATG::SignIn sample class for managing sign-in, including the various
//       state changes that happen during title startup.
//--------------------------------------------------------------------------------------
VOID SimplePartyExperience::HandleSignIn()
{
    DWORD dwUserIndexLoggedIn = XUSER_INDEX_NONE;
    m_CQNetSimple.m_dwLocalUsersMask = 0;

    // Keep looping until we've found at least one profile to use.
    for( DWORD dwUserIndex = 0; dwUserIndex < XUSER_MAX_COUNT; dwUserIndex++ )
    {
        if( XUserGetSigninState( dwUserIndex ) == eXUserSigninState_SignedInToLive )
        {
            if( dwUserIndexLoggedIn == XUSER_INDEX_NONE )
            {
                dwUserIndexLoggedIn = dwUserIndex;
            }
          
            m_CQNetSimple.m_dwLocalUsersMask |= 1 << dwUserIndex;
        }
    }

    // Check to see if our status changed to logged in
    if( dwUserIndexLoggedIn != m_CQNetSimple.m_dwPrimaryUserIndex && 
        m_CQNetSimple.m_dwPrimaryUserIndex == XUSER_INDEX_NONE )
    {
        // Host a simple QNet session to enable game invides
        const XUSER_CONTEXT aXUserContexts[] =
        {X_CONTEXT_GAME_TYPE, X_CONTEXT_GAME_TYPE_STANDARD,
         X_CONTEXT_GAME_MODE, CONTEXT_GAME_MODE_DEATHMATCH};

        m_CQNetSimple.m_pIQNet->HostGame(
            dwUserIndexLoggedIn,          // dwUserIndex
            m_CQNetSimple.m_dwLocalUsersMask,            // dwUserMask
            8,                             // dwPublicSlots
            2,                             // dwPrivateSlots
            0,                             // cProperties
            NULL,                          // pProperties
            ARRAYSIZE( aXUserContexts ),   // cContexts
            aXUserContexts );              // pContexts
    }

    m_CQNetSimple.m_dwPrimaryUserIndex = dwUserIndexLoggedIn;
}


//--------------------------------------------------------------------------------------
// Name: SimplePartyExperience::Update()
// Desc: Called once per frame, the call is th  e entry point for all updates
//--------------------------------------------------------------------------------------
HRESULT SimplePartyExperience::Update()
{
    // Get merged input to display help
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Update login
    ATG::SignIn::Update();

    // Block until a user logs in
    if( m_CQNetSimple.m_dwPrimaryUserIndex == XUSER_INDEX_NONE )
    {
        HandleSignIn();
        return S_OK;
    }

   // Check for notifications
    DWORD dwNotifyID;
    ULONG_PTR pNotifyParam;
    while( XNotifyGetNext( m_hNotify, 0, &dwNotifyID, &pNotifyParam ) )
    {
        switch( dwNotifyID )
        {
            case XN_PARTY_MEMBERS_CHANGED:
                // XParty: Update the list of current party
                XPartyGetUserList( &m_UserList );
                break;
        }
    }

    // Run the required QNet update
    m_CQNetSimple.m_pIQNet->DoWork();
    
    switch( m_CQNetSimple.m_pIQNet->GetState() )
    {
        HRESULT hr;
        case QNET_STATE_IDLE:                   
            hr = m_CQNetSimple.RunStateIdle();                
            break;
    }
    ;

    // Update xparty specific input
    UpdateInput( pGamepad );

    return S_OK;
}


 //--------------------------------------------------------------------------------------
// Name: SimplePartyExperience::Update()
// Desc: Called once per frame, update input specific to XParty API
//--------------------------------------------------------------------------------------
HRESULT SimplePartyExperience::UpdateInput(const ATG::GAMEPAD* pGamepad)
{
    // Handle input
    if( ATG::SignIn::AreUsersSignedIn() )
    {
        // Show the keyboard UI
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            XShowPartyUI( m_CQNetSimple.m_dwPrimaryUserIndex );
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        {
            XPartySendGameInvites( m_CQNetSimple.m_dwPrimaryUserIndex, NULL );
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        {
            // The CommunitySessionsUI shows players currently in your LIVE party who are
            // also playing the same title as you.
            XShowCommunitySessionsUI( m_CQNetSimple.m_dwPrimaryUserIndex, XSHOWCOMMUNITYSESSION_SHOWPARTY );
        }
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT SimplePartyExperience::Render()
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

        if( ATG::SignIn::AreUsersSignedIn() )
        {
            const FLOAT fDrawX = 32.0f;
            FLOAT fDrawY = 250.0f;

            const FLOAT fLineHeight = m_Font.GetFontHeight() * 1.2f;
            
            // If no user present, present them with option of starting a party
            if( m_UserList.dwUserCount == 0 )
            {
                m_Font.DrawText( fDrawX, fDrawY, ACTIVE_BUTTON_COLOR, 
                    GLYPH_A_BUTTON L"Form A New Live Party", ATGFONT_LEFT );

                fDrawY += fLineHeight;
            }
            else
            {
                m_Font.DrawText( fDrawX, fDrawY, ACTIVE_BUTTON_COLOR, GLYPH_A_BUTTON 
                    L"Manage Live Party", ATGFONT_LEFT );
                fDrawY += fLineHeight;

                m_Font.DrawText( fDrawX, fDrawY,
                    ACTIVE_BUTTON_COLOR, GLYPH_X_BUTTON 
                    L"Send Game Invites To Live Party", ATGFONT_LEFT );
                fDrawY += fLineHeight;
            }

            m_Font.DrawText( fDrawX, fDrawY, ACTIVE_BUTTON_COLOR, GLYPH_Y_BUTTON 
                    L"Show Community UI", ATGFONT_LEFT );

            // Finally draw live party member details
            RenderParty();
        }

        // Draw title text
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"Simple Party Experience" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderParty()
// Desc: Iterate m_UserList and draw gamertags of users in your current party
//--------------------------------------------------------------------------------------
void SimplePartyExperience::RenderParty()
{
    const DWORD dwUserCount = m_UserList.dwUserCount;

    FLOAT yDraw = 80.0f;
    FLOAT xDraw = 20.0f;

    m_Font.DrawText( 0, yDraw, 0xffffffff, L"Live Party:" );
    yDraw += m_Font.GetFontHeight() * 1.5f;

    // if there is no party, make it obvious.
    if( dwUserCount == 0 )
        m_Font.DrawText( xDraw, yDraw, PARTY_LIST_COLOR, L"<No Live Party Active>" );

    // Draw the party members (if any)
    for(DWORD i = 0; i < dwUserCount; i++)
    {
        // convert to WCHAR for our draw function
        WCHAR szGamerTag[XUSER_NAME_SIZE];
        szGamerTag[ XUSER_NAME_SIZE - 1 ] = '\0';
        MultiByteToWideChar( CP_ACP, 0, m_UserList.Users[i].GamerTag, -1, szGamerTag,
            XUSER_NAME_SIZE );


        // draw gamer tag
        m_Font.DrawText( xDraw, yDraw, PARTY_LIST_COLOR, szGamerTag );
        // If user is talking, display an asterix next to their name.
        if( m_UserList.Users[i].dwFlags & XPARTY_USER_ISTALKING )
            m_Font.DrawText( xDraw - 5, yDraw, PARTY_LIST_COLOR, L"*" );

        yDraw += m_Font.GetFontHeight() * 1.2f;
    }
}


