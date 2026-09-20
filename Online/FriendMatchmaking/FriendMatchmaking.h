//--------------------------------------------------------------------------------------
// FriendMatchmaking.h
//
// Definition of Sample for FriendMatchmaking sample
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <xtl.h>
#include <xrnm.h>
#include <stack>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#pragma warning(disable:4127) // codnitional expression is constant, pops up in STL lists
#include <list>
#pragma warning(default:4127)

//--------------------------------------------------------------------------------------
// Global constants
//--------------------------------------------------------------------------------------
static const UINT   MAX_STRING = 256;
static const UINT   PUBLICSLOTS = 16;
static const UINT   PRIVATESLOTS = 0;
static const UINT   MAXCONSOLES = 16;


#include "TextMenu.h"
#include "Console.h"
#include "Messages.h"


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    // Constants
    static const D3DCOLOR COLOR_BACKGROUND1   = D3DCOLOR_ARGB( 0xFF, 0x00, 0x00, 0xFF );
    static const D3DCOLOR COLOR_BACKGROUND2   = D3DCOLOR_ARGB( 0xFF, 0x00, 0x00, 0x00 );
    static const D3DCOLOR COLOR_TEXT          = D3DCOLOR_ARGB( 0xFF, 0xFF, 0xFF, 0xFF );
    static const D3DCOLOR COLOR_HILIGHT       = D3DCOLOR_ARGB( 0xFF, 0xFF, 0xFF, 0x00 );
    static const D3DCOLOR COLOR_HEADER        = D3DCOLOR_ARGB( 0xFF, 0x00, 0xFF, 0xFF );

    static const DWORD POSITIONUPDATE_INTERVAL = 100; // send ten updates per second


    // Colors and shapes for rendering players
    static const D3DCOLOR       PlayerColors[];
    static const LPCWSTR        PlayerGlyphs[];
    static const UINT PlayerColorCount;
    static const UINT PlayerGlyphCount;

    // Scale for rendering players
    static const UINT PLAYERICONSCALE   = 200;
    static const UINT PLAYERARROWSCALE  = 200;
    static const UINT PLAYERLABELSCALE  = 80;

    // Physics factors
    static const UINT ACCELERATIONFACTOR =   2000; // milliseconds to reach max velocity
    static const UINT VELOCITYFACTOR     =   3000; // milliseconds to cross screen at top speed

private:

    // Buttons that you can press while you play
    static const struct _GameButton
    {
        WORD Button;
        LPCWSTR Glyph;
        DWORD SendFlags;
    }                           GameButtons[];

    static const UINT GameButtonCount;

    // Button rendering parameters
    static const UINT BUTTON_GROW_TIME   = 500;
    static const UINT BUTTON_SHRINK_TIME = 1000;
    static const UINT BUTTON_MAX         = 4;
    static const UINT BUTTON_START       = 1;

    // Application states
    enum APPSTATE
    {
        APPSTATE_MAINMENU,
        APPSTATE_ERROR,
        APPSTATE_HOSTING,
        APPSTATE_SEARCHING,
        APPSTATE_JOINING,
        APPSTATE_JOINED,
        APPSTATE_LEAVING,

        NUM_APPSTATES,

        APPSTATE_NONE
    };

    // State stack
    std::stack <APPSTATE> m_AppState;

    // State stack implementation
    VOID                        PushState( APPSTATE state );
    VOID                        PopState( APPSTATE replace = APPSTATE_NONE );

    static const struct _StateImplementation
    {
        HRESULT ( Sample::*Initialize )( VOID );
        HRESULT ( Sample::*Update )( VOID );
        HRESULT ( Sample::*Render )( VOID );
        HRESULT ( Sample::*Terminate )( VOID );
        BOOL bInitializeOnReturn;
    }                           m_StateImplementation[];

    static const WCHAR* m_AppStateNames [];

    // State implementations
    HRESULT                     InitializeMainMenu( VOID );
    HRESULT                     UpdateMainMenu( VOID );
    HRESULT                     RenderMainMenu( VOID );

    HRESULT                     UpdateError( VOID );
    HRESULT                     RenderError( VOID );
    HRESULT                     TerminateError( VOID );

    HRESULT                     InitializeHosting( VOID );
    HRESULT                     UpdateHosting( VOID );
    HRESULT                     RenderHosting( VOID );
    HRESULT                     TerminateHosting( VOID );

    HRESULT                     InitializeSearching( VOID );
    HRESULT                     UpdateSearching( VOID );
    HRESULT                     RenderSearching( VOID );
    HRESULT                     TerminateSearching( VOID );

    HRESULT                     InitializeJoining( VOID );
    HRESULT                     UpdateJoining( VOID );
    HRESULT                     RenderJoining( VOID );

    HRESULT                     InitializeInSession( VOID );
    HRESULT                     UpdateInSession( VOID );
    HRESULT                     RenderInSession( VOID );

    HRESULT                     InitializeLeaving( VOID );
    HRESULT                     UpdateLeaving( VOID );
    HRESULT                     RenderLeaving( VOID );
    HRESULT                     TerminateLeaving( VOID );


    // Virtual function overrides
    HRESULT                     Initialize( VOID );
    HRESULT                     Update( VOID );
    HRESULT                     Render( VOID );

     // Friend session struct
    struct FriendSession
    {
        XUID   friendXuid;
        WCHAR  wszGamertag[ XUSER_NAME_SIZE ];
        XNKID  sessionID;
        PXSESSION_SEARCHRESULT  pSessionSearchResult;
    };

    // Friend session search state
    enum FRIENDSEARCHSTATE
    {
        FRIENDSEARCHSTATE_ENUMERATEFRIENDS,
        FRIENDSEARCHSTATE_SESSIONSEARCH,
        FRIENDSEARCHSTATE_DONE,

        NUM_FRIENDSEARCHSTATE,

        FRIENDSEARCHSTATE_NONE
    };

    // Friends data
    FRIENDSEARCHSTATE m_FriendSearchState;

    HANDLE m_hFriendsEnum;
    BYTE* m_pFriendsEnumMemory;     // our friends presence data
    DWORD m_dwNumFriends;           // number of friends in m_pFriendsEnumMemory

    XNKID m_SearchSessionIDs[ MAX_FRIENDS ];            // session ids to search for
    XSESSION_SEARCHRESULT_HEADER* m_pSearchResults;     // results of our session search

    FriendSession m_FriendSessions[ MAX_FRIENDS ];      // the list of friend sessions that we can select from
    DWORD m_dwNumFriendSessions;                        // number of friend sessions

	// Friend matchmaking methods
    VOID                        ClearFriendsData( VOID );
    BOOL                        ShouldSearchForFriend( const XONLINE_FRIEND* pFriend );
    DWORD                       EnumerateFriends( VOID );                
    DWORD                       SearchForSessions( DWORD dwEnumFriendsResult );
    VOID                        ProcessSearchResults( VOID );

    // Overlapped IO data structure
    XOVERLAPPED m_Overlapped;

    // XRNM endpoint
    XRNM_HANDLE m_hEndpoint;

    // XRNM methods; these reside in XRNMHelpers.cpp
    HRESULT                     CreateXRNMEndpoint( VOID );
    HRESULT                     AllowInboundLinks( BOOL bAllow );
    HRESULT                     CreateOutboundLink( const XRNM_ADDRESS* addr, _In_opt_ const BYTE* pbData, DWORD cbData, 
                                                    _Inout_ ULONG_PTR pUserData, _Out_ XRNM_HANDLE* pLink );
    HRESULT                     TerminateLink( XRNM_HANDLE pLink );
    HRESULT                     SendMessageToAll( XRNM_CHANNEL_ID id, _Inout_ CMessage* pMsg, DWORD dwFlags,
                                                  const XNADDR* xnaExcept = NULL );
    HRESULT                     SendMessage( XRNM_CHANNEL_ID, XRNM_HANDLE hLink, CMessage* pMsg, DWORD dwFlags );

    HRESULT                     ProcessXRNMEvents( VOID );
    HRESULT                     ProcessDataReceivedEvent( const XRNM_EVENT_DATA_RECEIVED* pEvent );
    HRESULT                     ProcessLinkStatusUpdateEvent( _Inout_ XRNM_EVENT_LINK_STATUS_UPDATE** ppEvent );
    HRESULT                     ProcessInboundLinkRequestEvent( _Inout_ XRNM_EVENT_INBOUND_LINK_REQUEST* pEvent );
    HRESULT                     ProcessReceiptEvent( const XRNM_EVENT_RECEIPT* pEvent );
    HRESULT                     ProcessAlertEvent( const XRNM_EVENT_ALERT* pEvent );

    // Text rendering
    ATG::Font m_Font16;           // Big font
    ATG::Font m_Font12;           // Little font
    FLOAT m_fCenterX;         // X-pos center
    FLOAT m_fCenterY;         // Y-pos center
    FLOAT m_fFontHeight;      // size of a line
    FLOAT m_fScreenWidth;
    FLOAT m_fScreenHeight;

    // Merged gamepad
    ATG::GAMEPAD* m_pGamepad;

    // Draw help
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    // Menu system
    TextMenu m_TextMenu;

    // Console output
    Console m_Console;

    // Global error message
    WCHAR                       m_wstrError[ MAX_STRING ];

    // Player number who pressed the button to advance past the main menu
    UCHAR m_nHost;

    // Main menu
    static       TextMenu::Item MainMenuItems[];
    static const UINT MainMenuItemCount;

    enum
    {
        MAINMENU_HOST,
        MAINMENU_SEARCH
    };

    // Searching menu
    INT m_nSelectedSession;

    // Current session
    HANDLE m_hSession;
    XSESSION_INFO m_SessionInfo;

    // Next available player ID to assign
    UINT m_nNextPlayerID;

    // Miscellaneous timers
    DWORD m_dwLastPhysicsUpdate;
    DWORD m_dwLastPositionUpdate;

    // Players and consoles
    struct SPlayer
    {
        UINT nPlayerID;
        UCHAR nLocalController;
        XUID xuid;
        WCHAR wstrGamertag[ XUSER_NAME_SIZE ];

        FLOAT fXPos;         // X position as a percentage of screen width
        FLOAT fYPos;         // Y position as a percentage of screen height

        FLOAT fXStick;       // X position of controller left thumbstick
        FLOAT fYStick;       // Y position of controller right thumbstick

        FLOAT fXVelocity;    // X velocity (max magnitude = 1)
        FLOAT fYVelocity;    // Y velocity (max magnitude = 1)

        UINT nButton;       // last button pressed
        DWORD dwButtonTime;  // time last button pressed

        BOOL bLocal;        // is local player
    };

    struct SConsole
    {
        XRNM_HANDLE hLink;
        XNADDR xnaddr;

        UCHAR cPlayers;
        SPlayer Players[ XUSER_MAX_COUNT ];
    };

    std::list <SConsole> m_Consoles;

    // Total number of players
    UINT m_cPlayers;

    // Messaging helpers
    HRESULT                     CopyConsoleToMsg( _Inout_ CClientInfoMsg* pMsg, const SConsole* pConsole, UINT nIndex );
    HRESULT                     CopyMsgToConsole( _Inout_ SConsole* pConsole, const CClientInfoMsg* pMsg, UINT nIndex );

    // Reasons why your join request might be denied
    enum JOINRESPONSE
    {
        JOINRESPONSE_APPROVED = 0,  // this shouldn't really get sent back
        JOINRESPONSE_SESSIONFULL,   // not enough empty slots
        JOINRESPONSE_NOTHOSTING,    // I'm not expecting an inbound
        JOINRESPONSE_BADREQUEST     // request was malformed
    };

    HRESULT                     RenderPlayer( _Inout_ SPlayer& p );

    // Session management
    HRESULT                     CreateSession( UCHAR nHostIndex, BOOL bHost );
    HRESULT                     AddLocalUsersToSession( VOID );
    HRESULT                     AddRemoteUsersToSession( const SConsole* pConsole );
    HRESULT                     RemoveLocalUsersFromSession( VOID );
    HRESULT                     RemoveRemoteUsersFromSession( const SConsole* pConsole );
    HRESULT                     CloseSession( VOID );
    
};
