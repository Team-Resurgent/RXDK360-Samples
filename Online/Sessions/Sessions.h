//--------------------------------------------------------------------------------------
// Sessions.h
//
// Definition of Sample for Sessions sample
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#include "Sessions.spa.h"
#include "ClientInfo.h"
#include "Messages.h"
#include "Session.h"
#include "Voice.h"
#include "HostMigration.h"
#include <AtgHelp.h>


//--------------------------------------------------------------------------------------
// Global constants
//--------------------------------------------------------------------------------------
const DWORD     HEARTBEAT_TIME = 1000;  // send a heartbeat every second
const DWORD     HEARTBEAT_TIMEOUT = 5000;  // timeout if a heartbeat not received for five seconds
const DWORD     WAVE_TIME = 5000;  // wave for five seconds
const DWORD     WAVE_BLINKS = 10;    // blink five times when waving
const DWORD     REGISTRATION_TIME = 10000; // wait ten seconds for everybody to register
const DWORD     MAX_SEARCHRESULTS = 10;    // Maximum search results

const IN_ADDR   NULLADDR = { 0 };       // Null IN_ADDR


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    // Valid app states
    enum APPSTATE
    {
        APPSTATE_MAINMENU,                 // Main Menu
        APPSTATE_CREATEUI,                 // Display the create UI screen
        APPSTATE_CREATE,                   // Create a new session
        APPSTATE_SEARCHUI,                 // Display the search for session UI screen
        APPSTATE_SEARCH,                   // Search for a session
        APPSTATE_CONNECTING,               // Connecting to a session
        APPSTATE_VIEWSTATS,                // Viewing stats
        APPSTATE_DELETING,                 // Deleting the session

        // The below appstates are all "in session"
        APPSTATE_PREGAME        = 0x1000,         // In a session
        APPSTATE_WAITINGFORREGISTRATION,   // Waiting for players to register
        APPSTATE_REGISTERING,              // Registering for arbitration
        APPSTATE_REGISTERED,               // Waiting for the host to start the game
        APPSTATE_STARTING,                 // Starting the session
        APPSTATE_INGAME,                   // Game in progress
        APPSTATE_ENDING,                   // Ending the session
        APPSTATE_POSTGAME,                 // Game over

        APPSTATE_HOSTMIGRATION,            // Begin migrating host

        APPSTATE_INSESSIONFLAG  = 0x1000
    };

    // main menu
    enum MAINMENU
    {
        MAINMENU_CREATE,                   // Create a session
        MAINMENU_SEARCH,                   // Search for a session
        MAINMENU_VIEWSTATS,                // View stats
        MAINMENU_LOGIN,                    // Change logged-in users
        MAINMENU_MAX
    };

    // create menu
    enum CREATEMENU
    {
        CREATEMENU_GAMETYPE,               // Game type
        CREATEMENU_GAMEMODE,               // Game mode
        CREATEMENU_MAP,                    // Map
        CREATEMENU_VICTORYPOINTS,          // Victory points
        CREATEMENU_INVITES,                // Allow invitations
        CREATEMENU_JOINVIAPRESENCE,        // Allow join-via-presence
        CREATEMENU_JOININPROGRESS,         // Allow join-in-progress
        CREATEMENU_CREATE,                 // Create a session
        CREATEMENU_MAX
    };

    // search menu
    enum SEARCHMENU
    {
        SEARCHMENU_GAMETYPE,               // Game type
        SEARCHMENU_GAMEMODE,               // Game mode
        SEARCHMENU_MAP,                    // Map
        SEARCHMENU_MINVICTORYPOINTS,       // Minimum victory points
        SEARCHMENU_MAXVICTORYPOINTS,       // Maximum victory points
        SEARCHMENU_SEARCH,                 // Execute the search
        SEARCHMENU_MAX
    };

    // Colors
    static const D3DCOLOR COLOR_BACKGROUND1   = D3DCOLOR_ARGB( 0xFF, 0x00, 0x00, 0xFF );
    static const D3DCOLOR COLOR_BACKGROUND2   = D3DCOLOR_ARGB( 0xFF, 0x00, 0x00, 0x00 );
    static const D3DCOLOR COLOR_TEXT          = D3DCOLOR_ARGB( 0xFF, 0xFF, 0xFF, 0xFF );
    static const D3DCOLOR GRAY_TEXT           = D3DCOLOR_ARGB( 0x80, 0x80, 0x80, 0x80 );
    static const D3DCOLOR COLOR_HIGHLIGHT     = D3DCOLOR_ARGB( 0xFF, 0xFF, 0xFF, 0x00 );
    static const D3DCOLOR COLOR_HEADER        = D3DCOLOR_ARGB( 0xFF, 0x00, 0xFF, 0xFF );
    static const D3DCOLOR COLOR_LOOPBACK      = D3DCOLOR_ARGB( 0xFF, 0xFF, 0x00, 0x00 );
    static const D3DCOLOR COLOR_TALKING       = D3DCOLOR_ARGB( 0xFF, 0x00, 0xFF, 0x00 );

    // Constants
    static const DWORD  m_anNotificationPosition[];   // notification placement
    static const size_t m_cNotificationPosition;      // number of placements
    static const WCHAR* m_astrNotificationPosition[]; // notification placement labels

    static const WCHAR* m_astrGameTypes[];
    static const DWORD m_cGameTypes;
    static const WCHAR* m_astrGameModes[];
    static const DWORD m_cGameModes;
    static const WCHAR* m_astrMaps[];
    static const DWORD m_cMaps;
    static const DWORD  m_dwSkillViews[];
    static const WCHAR* m_QoSString;

    static const DWORD VICTORY_POINTS_MIN = 1;
    static const DWORD VICTORY_POINTS_MAX = 20;

    static const struct _LEADERBOARD
    {
        WCHAR* m_wchName;
        DWORD m_dwIndex;
        DWORD m_dwSkillIndex;            // index of the associated TrueSkill(TM)
        // skill leaderboard -- if 0, then no skill
        // leaderboard is taken
    }                   m_aLeaderboards[2][4];
    static const DWORD m_cLeaderboards;
    static const DWORD m_nMaxLeaderboardRows;

    CVoice m_Voice;                // Voice handler
    CSession m_Session;              // Session for this sample
    BOOL m_bDrawHelp;            // Draw the help screen

    ATG::Font m_Font16;
    ATG::Font m_Font12;
    ATG::Help m_Help;

    BOOL m_bUseAlphaBlending;     // use alpha blending in the
    // TrueSkill(TM) skill bar display?

    HANDLE m_hLiveListener;         // Live notification listener
    HANDLE m_hSysListener;          // System notification listener
    APPSTATE m_AppState;              // Current application state
    UINT m_nNotificationPosition; // Current notification placement
    UINT m_nMenuItem;             // Current menu item
    XOVERLAPPED m_Overlapped;            // Overlapped task data

    // Networking-related member variables
    SOCKET m_Socket;                    // Socket for communication
    DWORD m_dwConnectionTimer;         // tick count of connection attempt
    DWORD m_dwHeartbeatTimer;          // tick count of last heartbeat check
    DWORD m_dwRegistrationTimer;       // time for other clients to register
    LocalClientInfo m_Local;                     // information about the local client
    ClientInfo* m_pHost;                     // information about the host
    ClientInfoVec m_vecRemote;                 // information about the remote clients

    // Game parameters
    DWORD m_nGameType;
    DWORD m_nGameMode;
    DWORD m_nMap;
    DWORD m_nVictoryPoints;
    DWORD m_nMinVictoryPoints;
    DWORD m_nMaxVictoryPoints;

    // QoS Data
    BOOL m_bQoSTesting;
    XNQOS m_QoSResult;
    XNQOS* m_pQoSResult;
    const XNADDR* m_QoSxnaddr[MAX_SEARCHRESULTS];
    const XNKID* m_QoSxnkid[MAX_SEARCHRESULTS];
    const XNKEY* m_QoSxnkey[MAX_SEARCHRESULTS];

    // Stats
    PXUSER_STATS_READ_RESULTS m_pStats;       // Leaderboard data
    DWORD m_nLeaderboard; // Current leaderboard
    XUSER_STATS_SPEC m_Spec;         // Stats specification
    BOOL m_bReadingLeaderboard;
    UINT m_nNextSelectedRank;    // Rank of user to attempt to select
    XUID m_xuidNextSelectedXuid; // XUID of user to attempt to select

    PXSESSION_SEARCHRESULT_HEADER m_pSearchResults; // Match search results

    // Host Migration
    CHostMigrationHelper m_HostMigration;

private:
    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();

    // State-changing helper
    VOID                SwitchToState( APPSTATE NewState );

    // Display the friends list
    VOID                ShowCommunitySessionsUI( UINT nController );

    // Change notification position
    VOID                ChangeNotificationPosition();

    // Look for a session
    VOID                SearchForSession();

    // Set the user properties for the session
    VOID                InitSessionProperties();

    // Join a found session
    VOID                JoinSession( BOOL bInvited );

    // Send a message to a remote player
    VOID                SendMessage( CMessage* pMessage, IN_ADDR addr );

    // Send a voice message
    VOID                SendVoiceMessage();

    // Receive a message
    VOID                ReceiveMessage();

    // Handle a client leaving the session
    VOID                ClientDropped( IN_ADDR addr );

    // Do heartbeat housework
    VOID                HandleHeartbeat();
    HRESULT             HandleHeartbeat( ClientInfo* pClient );

    // Remove a client from the list
    VOID                RemoveClient( const ClientInfoVec::iterator& i );

    // Recalculate open/filled slots
    VOID                RecalcSlots();

    // Retrieve the TrueSkill(TM) skill values for users
    VOID                ReadTrueSkills( ClientInfo* pClient );

    // Add/remove users to a session
    VOID                AddUsersToSession( ClientInfo* pClient );
    VOID                RemoveUsersFromSession( const ClientInfo* pClient );

    // update the mute lists for all players
    VOID                UpdateMuteLists();

    // Check to see if an invitation has been accepted
    BOOL                CheckForAcceptedInvitation();

    // Start the game
    VOID                StartGame();

    // Arbitration
    VOID                StartRegistration();
    VOID                ProcessRegistrationList();

    // Stats
    VOID                WriteStats();                             // Write stats for the session
    VOID                WriteStats( const ClientInfo* pClient );  // Write stats for a specific client
    VOID                ReadLeaderboard( INT idx, XUID xuid );    // Read a leaderboard

    // Update methods
    HRESULT             UpdateMainMenu();
    HRESULT             UpdateSearchUI();
    HRESULT             UpdateSearch();
    HRESULT             UpdateCreateUI();
    HRESULT             UpdateConnecting();
    HRESULT             UpdateWaitingForRegistration();
    HRESULT             UpdateRegistering();
    HRESULT             UpdateStarting();
    HRESULT             UpdateInSession();
    HRESULT             UpdateEnding();
    HRESULT             UpdateViewStats();
    HRESULT             UpdateDeleting();

    // Render methods
    HRESULT             RenderMainMenu();
    HRESULT             RenderSearchUI();
    HRESULT             RenderSearch();
    HRESULT             RenderCreateUI();
    HRESULT             RenderCreate();
    HRESULT             RenderConnecting();
    HRESULT             RenderInSession();
    HRESULT             RenderViewStats();

    // Draw the players on a given client
    FLOAT               RenderClient( WCHAR* strLabel, FLOAT x, FLOAT y,
                                      ClientInfo* pClient, ClientInfo* pHost );

    // Message processing methods
    VOID                ProcessJoinSession( CMessage* pMsg, IN_ADDR psaFrom );
    VOID                ProcessJoinResponse( CMessage* pMsg, IN_ADDR psaFrom );
    VOID                ProcessPlayerInfo( CMessage* pMsg, IN_ADDR psaFrom );
    VOID                ProcessWave( CMessage* pMsg, IN_ADDR psaFrom );
    VOID                ProcessHeartbeat( CMessage* pMsg, IN_ADDR psaFrom );
    VOID                ProcessGoodbye( CMessage* pMsg, IN_ADDR psaFrom );
    VOID                ProcessStartGame( CMessage* pMsg, IN_ADDR psaFrom );
    VOID                ProcessRegister( CMessage* pMsg, IN_ADDR psaFrom );
    VOID                ProcessRegistered( CMessage* pMsg, IN_ADDR psaFrom );
    VOID                ProcessScorePoint( CMessage* pMsg, IN_ADDR psaFrom );
    VOID                ProcessPointTotal( CMessage* pMsg, IN_ADDR psaFrom );
    VOID                ProcessVoice( CMessage* pMsg, IN_ADDR psaFrom );
    VOID                ProcessMute( CMessage* pMsg, IN_ADDR psaFrom );

public:

                        ~Sample();
    VOID                SessionNotification( SESSION_NOTIFY newNotification );

    const LocalClientInfo* GetLocalClientInfo()
    {
        return &m_Local;
    }
    const ClientInfo* GetHostClientInfo()
    {
        return m_pHost;
    }
    const ClientInfoVec* GetRemoteClientInfoVec()
    {
        return &m_vecRemote;
    }
    const DWORD         GetGameType()
    {
        return m_nGameType;
    }

    friend class CHostMigrationHelper;
};

//--------------------------------------------------------------------------------------
// Name: operator== (IN_ADDR)
// Desc: Little helper operator that will prove useful
//--------------------------------------------------------------------------------------
BOOL operator==( const IN_ADDR& i1, const IN_ADDR& i2 );

