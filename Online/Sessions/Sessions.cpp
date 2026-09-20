//--------------------------------------------------------------------------------------
// Sessions.cpp
//
// Sample for hosting, finding, and joining sessions on Xbox Live, plus sending,
// receiving, and responding to invitations.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgSignIn.h>
#include <AtgUtil.h>
#include "Sessions.h"
#include "Messages.h"
#include "TrueSkillBar.h"
#include "XGUI.h"


//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------

const DWORD     PORT = 1000;          // Port 1000 = most efficient port

const DWORD Sample::m_anNotificationPosition[] =
{
    XNOTIFYUI_POS_TOPLEFT,
    XNOTIFYUI_POS_TOPCENTER,
    XNOTIFYUI_POS_TOPRIGHT,
    XNOTIFYUI_POS_CENTERLEFT,
    XNOTIFYUI_POS_CENTER,
    XNOTIFYUI_POS_CENTERRIGHT,
    XNOTIFYUI_POS_BOTTOMLEFT,
    XNOTIFYUI_POS_BOTTOMCENTER,
    XNOTIFYUI_POS_BOTTOMRIGHT,
};

const size_t    Sample::m_cNotificationPosition =
    sizeof( Sample::m_anNotificationPosition ) /
    sizeof( Sample::m_anNotificationPosition[0] );

const WCHAR*    Sample::m_astrNotificationPosition[] =
{
    L"TOP_LEFT",
    L"TOP_CENTER",
    L"TOP_RIGHT",
    L"CENTER_LEFT",
    L"CENTER",
    L"CENTER_RIGHT",
    L"BOTTOM_LEFT",
    L"BOTTOM_CENTER",
    L"BOTTOM_RIGHT",
};

const WCHAR*    Sample::m_astrGameTypes[] =
{
    L"Ranked",
    L"Player Match",
};

const WCHAR*    Sample::m_QoSString = L"My Game";

const DWORD     Sample::m_cGameTypes = ARRAYSIZE( m_astrGameTypes );

const WCHAR*    Sample::m_astrGameModes[] =
{
    L"Deathmatch",
    L"Cooperative",
    L"Team Battle",
};

const DWORD     Sample::m_cGameModes = ARRAYSIZE( m_astrGameModes );

const WCHAR*    Sample::m_astrMaps[] =
{
    L"Stalingrad",
    L"Leyte Gulf",
    L"Ardennes",
    L"Normandy",
    L"Any"
};

const DWORD     Sample::m_cMaps = 4;

const Sample::_LEADERBOARD Sample::m_aLeaderboards[2][4] =
{
    {
        { L"Ranked Match: All", STATS_VIEW_RANKED_GAMES      , 0                                   },
        { L"Ranked Match: Deathmatch",  STATS_VIEW_RANKED_DEATHMATCH , STATS_VIEW_SKILL_RANKED_DEATHMATCH  },
        { L"Ranked Match: Cooperative", STATS_VIEW_RANKED_COOPERATIVE, STATS_VIEW_SKILL_RANKED_COOPERATIVE },
        { L"Ranked Match: Team Battle", STATS_VIEW_RANKED_TEAM_PLAY  , STATS_VIEW_SKILL_RANKED_TEAM_BATTLE },
    },
    {
        { L"Player Match: All", STATS_VIEW_STANDARD_GAMES      , 0 },        // in standard games, the skill should not be displayed
        { L"Player Match: Deathmatch",  STATS_VIEW_STANDARD_DEATHMATCH , 0 },        // in standard games, the skill should not be displayed
        { L"Player Match: Cooperative", STATS_VIEW_STANDARD_COOPERATIVE, 0 },        // in standard games, the skill should not be displayed
        { L"Player Match: Team Battle", STATS_VIEW_STANDARD_TEAM_PLAY  , 0 },        // in standard games, the skill should not be displayed
    }
};

const DWORD Sample::m_dwSkillViews[] =
{
    STATS_VIEW_SKILL_RANKED_DEATHMATCH,
    STATS_VIEW_SKILL_RANKED_COOPERATIVE,
    STATS_VIEW_SKILL_RANKED_TEAM_BATTLE,
};

const DWORD     Sample::m_cLeaderboards = 4;
const DWORD     Sample::m_nMaxLeaderboardRows = 8;


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: operator== (IN_ADDR)
// Desc: Little helper operator that will prove useful
//--------------------------------------------------------------------------------------
BOOL operator==( const IN_ADDR& i1, const IN_ADDR& i2 )
{
    return memcmp( &i1, &i2, sizeof( IN_ADDR ) ) == 0;
}


//--------------------------------------------------------------------------------------
// Name: ~Sample()
// Desc: Destructor for the sample -- clean up handles
//--------------------------------------------------------------------------------------
Sample::~Sample()
{
    XNetQosRelease( m_pQoSResult );
    CloseHandle( m_hLiveListener );
    CloseHandle( m_hSysListener );
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;

    // Create the fonts
    if( FAILED( m_Font16.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( m_Font12.Create( "game:\\Media\\Fonts\\Arial_12.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Intialise the TrueSkill(TM) skill bar specific render code
    XGUI::Initialise( m_pd3dDevice );
    m_bUseAlphaBlending = FALSE;

    // Confine text drawing to the title safe area
    m_Font12.SetWindow( ATG::GetTitleSafeArea() );
    m_Font16.SetWindow( ATG::GetTitleSafeArea() );

    // Set the sample as the session parent
    m_Session.SetParent( this );

    // Start up Xbox Live functionality using default Secure Network Layer settings
    if( XOnlineStartup() != ERROR_SUCCESS )
    {
        ATG::FatalError( "Failed to start Xbox Live\n" );
    }

    // Register our Live (invitation) listener
    m_hLiveListener = XNotifyCreateListener( XNOTIFY_LIVE );
    if( m_hLiveListener == NULL || m_hLiveListener == INVALID_HANDLE_VALUE )
    {
        ATG::FatalError( "Failed to create Live state notification listener.\n" );
    }

    // Register our System (mute list change) listener
    m_hSysListener = XNotifyCreateListener( XNOTIFY_SYSTEM );
    if( m_hSysListener == NULL || m_hSysListener == INVALID_HANDLE_VALUE )
    {
        ATG::FatalError( "Failed to create System state notification listener.\n" );
    }

    // Initialize signin
    ATG::SignIn::Initialize( 1, 4, TRUE, 4 );

    // Create the socket
    m_Socket = socket( AF_INET, SOCK_DGRAM, IPPROTO_VDP );
    if( m_Socket == INVALID_SOCKET )
    {
        ATG::FatalError( "Failed to create the socket.\n" );
    }

    // Bind the socket
    SOCKADDR_IN sa;
    sa.sin_family = AF_INET;           // IP family
    sa.sin_addr.s_addr = INADDR_ANY;        // Use the only IP that's available to us
    sa.sin_port = htons( PORT );     // Port (should be 1000)

    if( bind( m_Socket, ( SOCKADDR* )&sa, sizeof( sa ) ) != 0 )
    {
        ATG::FatalError( "Failed to bind socket, error %d.\n", WSAGetLastError() );
    }

    // Mark the socket as nonblocking
    DWORD dwNonblocking = 1;

    if( ioctlsocket( m_Socket, FIONBIO, &dwNonblocking ) != 0 )
    {
        ATG::FatalError( "Failed to set the socket to nonblocking.\n" );
    }

    // Initialize local variables
    m_nNotificationPosition = 8;
    m_nMenuItem = 0;
    m_pSearchResults = NULL;
    m_pStats = NULL;
    m_bReadingLeaderboard = FALSE;

    m_nGameType = X_CONTEXT_GAME_TYPE_STANDARD;
    m_nGameMode = 0;

    m_pQoSResult = &m_QoSResult;
    m_bQoSTesting = FALSE;

    // Initialize the leaderboard spec
    m_Spec.dwNumColumnIds = 3;
    m_Spec.rgwColumnIds[ 0 ] = STATS_COLUMN_RANKED_GAMES_GAMES_PLAYED;
    m_Spec.rgwColumnIds[ 1 ] = STATS_COLUMN_RANKED_GAMES_POINTS_SCORED;
    m_Spec.rgwColumnIds[ 2 ] = STATS_COLUMN_RANKED_GAMES_LAST_MAP;

    // Set up the default notification position
    ChangeNotificationPosition();

    // Retrieve our own machine ID
    XNADDR xnaddr;

    while( XNetGetTitleXnAddr( &xnaddr ) == XNET_GET_XNADDR_PENDING );

    XNetXnAddrToMachineId( &xnaddr, &m_Local.id );

    // Initialize XHV
    m_Voice.Initialize( &m_Local );

    SwitchToState( APPSTATE_MAINMENU );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SwitchToState()
// Desc: Changes to a new appstate and performs initialization for the new state
//--------------------------------------------------------------------------------------
VOID Sample::SwitchToState( APPSTATE NewState )
{
    // Clean up from the previous state
    switch( m_AppState )
    {
        case APPSTATE_SEARCH:
            if( m_pSearchResults )
            {
                delete[] m_pSearchResults;
                m_pSearchResults = NULL;
            }
            XNetQosRelease( m_pQoSResult );
            m_bQoSTesting = FALSE;
            break;

        case APPSTATE_VIEWSTATS:
            if( m_pStats )
            {
                delete[] m_pStats;
                m_pStats = NULL;
            }
            break;
    }

    // Initialize the next state
    switch( NewState )
    {
        case APPSTATE_SEARCHUI:
            m_nMinVictoryPoints = VICTORY_POINTS_MIN;
            m_nMaxVictoryPoints = VICTORY_POINTS_MAX;
            m_nGameType = X_CONTEXT_GAME_TYPE_STANDARD;
            m_nGameMode = 0;
            m_nMap = m_cMaps;
            m_Session.SetSessionError( NULL );
            break;

        case APPSTATE_SEARCH:
            SearchForSession();
            break;

        case APPSTATE_CREATEUI:
            m_nVictoryPoints = ( VICTORY_POINTS_MIN + VICTORY_POINTS_MAX ) / 2;
            m_nGameType = X_CONTEXT_GAME_TYPE_STANDARD;
            m_nGameMode = 0;
            m_nMap = 0;
            m_Session.SetSessionFlags( XSESSION_CREATE_LIVE_MULTIPLAYER_STANDARD, TRUE );
            m_Session.SetSessionError( NULL );
            break;

        case APPSTATE_CREATE:
            m_vecRemote.clear();

            InitSessionProperties();

            m_Session.SetHost( TRUE );
            m_pHost = &m_Local;
            m_Session.SwitchToState( SESSION_STATE_CREATING );
            break;

        case APPSTATE_CONNECTING:
            m_vecRemote.clear();
            break;

        case APPSTATE_PREGAME:
            m_dwHeartbeatTimer = GetTickCount();
            break;

        case APPSTATE_REGISTERED:
            // If we're the host, start the game once registration is complete
            if( m_Session.IsHost() )
            {
                if( m_vecRemote.empty() )
                {
                    m_Session.SetSessionError( L"Can't have an arbitrated session with only one player." );
                    m_AppState = APPSTATE_PREGAME;
                    m_Session.SwitchToState( SESSION_STATE_IDLE );
                }
                else
                {
                    StartGame();
                }
            }
            break;

        case APPSTATE_STARTING:
            // Clear out the scores
            ZeroMemory( m_Local.nPoints, sizeof( m_Local.nPoints ) );
            for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
            {
                ClientInfo& Remote = *i;

                ZeroMemory( Remote.nPoints, sizeof( Remote.nPoints ) );
            }
            m_Session.SwitchToState( SESSION_STATE_STARTING );
            break;

        case APPSTATE_ENDING:
            // Write stats before ending the session
            WriteStats();

            m_Session.SwitchToState( SESSION_STATE_ENDING );
            break;

        case APPSTATE_VIEWSTATS:
            m_nLeaderboard = 0;
            m_nGameType = 0;
            ReadLeaderboard( 1, 0 );
            break;

        case APPSTATE_DELETING:
            m_vecRemote.clear();
            m_Voice.EndSession();

            m_Session.SwitchToState( SESSION_STATE_DELETING );
            break;
    }

    m_nMenuItem = 0;
    m_AppState = NewState;
}


//--------------------------------------------------------------------------------------
// Name: ShowFriendsList()
// Desc: Show the friends list
//--------------------------------------------------------------------------------------
VOID Sample::ShowCommunitySessionsUI( UINT nController )
{
    // Start the UI with the appropriate number of users
    DWORD ret = XShowCommunitySessionsUI( nController, XSHOWCOMMUNITYSESSION_SHOWPARTY  );

    if( ret != ERROR_SUCCESS )
    {
        ATG::FatalError( "Unable to launch friends UI, error code %d\n", ret );
    }
}


//--------------------------------------------------------------------------------------
// Name: ChangeNotificationPosition()
// Desc: Update the current notification position
//--------------------------------------------------------------------------------------
VOID Sample::ChangeNotificationPosition()
{
    XNotifyPositionUI( m_anNotificationPosition[ m_nNotificationPosition ] );
}


//--------------------------------------------------------------------------------------
// Name: SearchForSession()
// Desc: Start looking for a session
//--------------------------------------------------------------------------------------
VOID Sample::SearchForSession()
{
    // Figure out the size of the max search results
    DWORD cbResults = 0;
    DWORD ret;

    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

    // Pass in 0 for cbResults to have the buffer size 
    //  calculated for us by XTL
    ret = XSessionSearch(
        SESSION_MATCH_QUERY_FIND_MATCHES, // Procedure index
        m_Session.GetSessionOwner(),      // User index
        MAX_SEARCHRESULTS,                // Maximum results
        0,                                // Number of properties   (ignored)
        0,                                // Number of contexts     (ignored)
        NULL,                             // Properties             (ignored)
        NULL,                             // Contexts               (ignored)
        &cbResults,                       // Size of result buffer
        NULL,                             // Pointer to results     (ignored)
        NULL                              // This call is always synchronous
        );

    if( ( ret != ERROR_INSUFFICIENT_BUFFER ) || ( cbResults == 0 ) )
    {
        ATG::FatalError( "XOnlineMatchSearch failed to get results buffer size with error %d\n", ret );
    }

    m_pSearchResults = ( XSESSION_SEARCHRESULT_HEADER* )new BYTE[ cbResults ];

    if( !m_pSearchResults )
    {
        ATG::FatalError( "Failed to allocate memory for search results." );
    }

    // Initialize the property/context arrays. These must live on beyond the
    // call to XSessionSearch, so they are declared static
    static XUSER_PROPERTY aProperties[2];
    aProperties[0].dwPropertyId = PROPERTY_MIN_VICTORY_POINTS;
    aProperties[0].value.nData = m_nMinVictoryPoints;
    aProperties[0].value.type = XUSER_DATA_TYPE_INT32;
    aProperties[1].dwPropertyId = PROPERTY_MAX_VICTORY_POINTS;
    aProperties[1].value.nData = m_nMaxVictoryPoints;
    aProperties[1].value.type = XUSER_DATA_TYPE_INT32;

    static XUSER_CONTEXT aContexts[3];
    aContexts[0].dwContextId = X_CONTEXT_GAME_TYPE;
    aContexts[0].dwValue = m_nGameType;
    aContexts[1].dwContextId = X_CONTEXT_GAME_MODE;
    aContexts[1].dwValue = m_nGameMode;
    aContexts[2].dwContextId = CONTEXT_MAP;
    aContexts[2].dwValue = m_nMap;

    WORD cContexts = m_nMap == m_cMaps? 2 : 3;

    // Fire off the query
    ret = XSessionSearch(
        SESSION_MATCH_QUERY_FIND_MATCHES, // Procedure index
        m_Session.GetSessionOwner(),      // User index
        MAX_SEARCHRESULTS,                // Maximum results
        2,                                // Number of properties
        cContexts,                        // Number of contexts
        aProperties,                      // Properties
        aContexts,                        // Contexts
        &cbResults,                       // Size of result buffer
        m_pSearchResults,                 // Pointer to results
        &m_Overlapped                     // Overlapped data structure
        );

    if( ret != ERROR_IO_PENDING && ret != ERROR_SUCCESS )
    {
        ATG::FatalError( "XOnlineMatchSearch failed with error %d\n", ret );
    }
}


//--------------------------------------------------------------------------------------
// Name: InitSessionProperties()
// Desc: Host a new session
//--------------------------------------------------------------------------------------
VOID Sample::InitSessionProperties()
{
    // Set up game-related properties
    XUserSetProperty( m_Session.GetSessionOwner(), PROPERTY_VICTORY_POINTS, sizeof( DWORD ), &m_nVictoryPoints );
    XUserSetContext( m_Session.GetSessionOwner(), X_CONTEXT_GAME_MODE, m_nGameMode );
    XUserSetContext( m_Session.GetSessionOwner(), CONTEXT_MAP, m_nMap );
    XUserSetContext( m_Session.GetSessionOwner(), X_CONTEXT_GAME_TYPE, m_nGameType );
}


//--------------------------------------------------------------------------------------
// Name: JoinSession()
// Desc: Connect to an existing session
//--------------------------------------------------------------------------------------
VOID Sample::JoinSession( BOOL bInvited )
{
    m_Local.bInvited = bInvited;
    m_Session.SetHost( FALSE );

    // Set the context for the session
    XUserSetContext( m_Session.GetSessionOwner(), X_CONTEXT_GAME_TYPE, m_nGameType );
    XUserSetContext( m_Session.GetSessionOwner(), X_CONTEXT_GAME_MODE, m_nGameMode );

    m_Session.SetSessionFlags( XSESSION_CREATE_LIVE_MULTIPLAYER_STANDARD, TRUE );
    if( m_nGameType == X_CONTEXT_GAME_TYPE_RANKED )
    {
        m_Session.SetSessionFlags( XSESSION_CREATE_USES_ARBITRATION );
    }

    m_Session.SwitchToState( SESSION_STATE_CREATING );
}


//--------------------------------------------------------------------------------------
// Name: SendMessage()
// Desc: Send a message to a remote player
//--------------------------------------------------------------------------------------
VOID Sample::SendMessage( CMessage* pMessage, IN_ADDR addr )
{
    sockaddr_in sa = {0};
    sa.sin_addr = addr;
    sa.sin_family = AF_INET;
    sa.sin_port = htons( PORT );

    // NULLADDR means self
    if( addr == NULLADDR )
    {
        sa.sin_addr.S_un.S_addr = INADDR_LOOPBACK;
    }

    int ret = sendto( m_Socket, ( CHAR* )pMessage, pMessage->GetSize(),
                      0, ( SOCKADDR* )&sa, sizeof( sa ) );

    if( ret != pMessage->GetSize() )
    {
        // If the send failed, probably the security association is gone; drop the
        // client
        ClientDropped( addr );
    }
}


//--------------------------------------------------------------------------------------
// Name: ReceiveMessage()
// Desc: Receive a message from a remote player
//--------------------------------------------------------------------------------------
VOID Sample::ReceiveMessage()
{
    CMessage msg;
    SOCKADDR_IN sa;
    INT size = sizeof( sa );
    INT ret;

    do
    {
        ret = recvfrom( m_Socket, ( CHAR* )&msg, sizeof( msg ), 0, ( SOCKADDR* )&sa, &size );

        if( ret != SOCKET_ERROR && ret > 0 && ntohs( sa.sin_port ) == PORT )
        {
            switch( msg.GetID() )
            {
                case MSG_JOIN_SESSION:
                    ProcessJoinSession( &msg, sa.sin_addr ); break;
                case MSG_JOIN_RESPONSE:
                    ProcessJoinResponse( &msg, sa.sin_addr ); break;
                case MSG_PLAYER_INFO:
                    ProcessPlayerInfo( &msg, sa.sin_addr ); break;
                case MSG_START_GAME:
                    ProcessStartGame( &msg, sa.sin_addr ); break;
                case MSG_REGISTER:
                    ProcessRegister( &msg, sa.sin_addr ); break;
                case MSG_REGISTERED:
                    ProcessRegistered( &msg, sa.sin_addr ); break;
                case MSG_SCORE_POINT:
                    ProcessScorePoint( &msg, sa.sin_addr ); break;
                case MSG_POINT_TOTAL:
                    ProcessPointTotal( &msg, sa.sin_addr ); break;
                case MSG_WAVE:
                    ProcessWave( &msg, sa.sin_addr ); break;
                case MSG_HEARTBEAT:
                    ProcessHeartbeat( &msg, sa.sin_addr ); break;
                case MSG_GOODBYE:
                    ProcessGoodbye( &msg, sa.sin_addr ); break;
                case MSG_VOICE:
                    // Special case for voice messages: keep track of how much data we received
                    msg.GetVoice().wSize = ( WORD )( ( ret - msg.GetGameDataSize() - sizeof( WORD ) ) & MAXWORD );
                    ProcessVoice( &msg, sa.sin_addr ); break;
                case MSG_MUTE:
                    ProcessMute( &msg, sa.sin_addr ); break;
                case MSG_MIGRATE:
                    m_HostMigration.ProcessMigrateMessage( &msg, sa.sin_addr ); break;
            }
        }
    } while( ret != SOCKET_ERROR && ret > 0 && ntohs( sa.sin_port ) == PORT );
}


//--------------------------------------------------------------------------------------
// Name: ProcessJoinSession()
// Desc: Respond to a join session message
//--------------------------------------------------------------------------------------
VOID Sample::ProcessJoinSession( CMessage* pMsg, IN_ADDR addrFrom )
{
    CMessage msgJoinResponse( MSG_JOIN_RESPONSE );
    MsgJoinSession& JoinSession = pMsg->GetJoinSession();
    MsgJoinResponse& JoinResponse = msgJoinResponse.GetJoinResponse();

    if( m_AppState < APPSTATE_PREGAME || m_AppState > APPSTATE_INGAME || !m_Session.IsHost() )
    {
        JoinResponse.Response = JoinResponse.JOINRESPONSE_NOTHOSTING;
    }
    else if( m_AppState > APPSTATE_PREGAME && m_Session.HasSessionFlags( XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED ) )
    {
        JoinResponse.Response = JoinResponse.JOINRESPONSE_SESSIONINPROGRESS;
    }
    else
    {
        // See if we have the slots to hold this guy
        UINT nSlotsOpen =
            ( m_Session.GetSessionSlots( SLOTS_TOTALPUBLIC ) - m_Session.GetSessionSlots( SLOTS_FILLEDPUBLIC ) ) +
            JoinSession.bInvited * ( m_Session.GetSessionSlots( SLOTS_TOTALPRIVATE ) -
                                     m_Session.GetSessionSlots( SLOTS_FILLEDPRIVATE ) );

        if( JoinSession.cPlayers > nSlotsOpen )
        {
            JoinResponse.Response = JoinResponse.JOINRESPONSE_SESSIONFULL;
        }
        else
        {
            JoinResponse.Response = JoinResponse.JOINRESPONSE_APPROVED;
            JoinResponse.Nonce = m_Session.GetSessionNonce();
            JoinResponse.nVictoryPoints = m_nVictoryPoints;
            JoinResponse.nMap = m_nMap;

            JoinResponse.cPlayers = m_Local.cPlayers;
            memcpy_s( JoinResponse.xuids, sizeof( JoinResponse.xuids ),
                      m_Local.xuids, sizeof( m_Local.xuids ) );
            memcpy_s( JoinResponse.strGamertags, sizeof( JoinResponse.strGamertags ),
                      m_Local.strGamertags, sizeof( m_Local.strGamertags ) );

            m_Voice.GetLocalVoice( JoinResponse.bHasVoice );

            JoinResponse.cPrivateSlots = m_Session.GetSessionSlots( SLOTS_TOTALPRIVATE );
            JoinResponse.cPublicSlots = m_Session.GetSessionSlots( SLOTS_TOTALPUBLIC );

            JoinResponse.id = m_Local.id;

            JoinResponse.dwSessionFlags = m_Session.GetSessionFlags() & ~XSESSION_CREATE_HOST;
        }
    }

    // Send the response
    SendMessage( &msgJoinResponse, addrFrom );

    // If we approved the request, do the proper housekeeping for a new player
    if( JoinResponse.Response == JoinResponse.JOINRESPONSE_APPROVED )
    {
        ClientInfo NewClient;
        NewClient.addr = addrFrom;
        NewClient.id = JoinSession.id;
        NewClient.cPlayers = JoinSession.cPlayers;
        NewClient.dwHeartbeat = GetTickCount();

        XNetInAddrToXnAddr( addrFrom, &NewClient.xnaddr, NULL );

        for( int i = 0; i < XUSER_MAX_COUNT; i++ )
        {
            NewClient.dwWave[ i ] = GetTickCount() - WAVE_TIME;
        }

        memcpy_s( NewClient.xuids, sizeof( NewClient.xuids ),
                  JoinSession.xuids, sizeof( JoinSession.xuids ) );
        memcpy_s( NewClient.strGamertags, sizeof( NewClient.strGamertags ),
                  JoinSession.strGamertags, sizeof( JoinSession.strGamertags ) );
        memcpy_s( NewClient.bHasVoice, sizeof( NewClient.bHasVoice ),
                  JoinSession.bHasVoice, sizeof( JoinSession.bHasVoice ) );

        NewClient.bInvited = JoinSession.bInvited;

        m_Voice.RegisterClient( &NewClient );
        AddUsersToSession( &NewClient );

        // Create a playerinfo message to tell everyone about the new client
        CMessage msgPlayerInfoNew( MSG_PLAYER_INFO );
        MsgPlayerInfo& PlayerInfoNew = msgPlayerInfoNew.GetPlayerInfo();
        PlayerInfoNew.cPlayers = NewClient.cPlayers;
        PlayerInfoNew.id = NewClient.id;
        PlayerInfoNew.xnaddr = NewClient.xnaddr;
        memcpy_s( PlayerInfoNew.strGamertags, sizeof( PlayerInfoNew.strGamertags ),
                  NewClient.strGamertags, sizeof( NewClient.strGamertags ) );
        memcpy_s( PlayerInfoNew.xuids, sizeof( PlayerInfoNew.xuids ),
                  NewClient.xuids, sizeof( NewClient.xuids ) );
        memcpy_s( PlayerInfoNew.bHasVoice, sizeof( PlayerInfoNew.bHasVoice ),
                  NewClient.bHasVoice, sizeof( NewClient.bHasVoice ) );

        // Iterate through and tell the oldbies about the newbie and vice versa
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
        {
            ClientInfo& Remote = *i;

            CMessage msgPlayerInfoOld( MSG_PLAYER_INFO );
            MsgPlayerInfo& PlayerInfoOld = msgPlayerInfoOld.GetPlayerInfo();
            PlayerInfoOld.cPlayers = Remote.cPlayers;
            PlayerInfoOld.id = Remote.id;
            PlayerInfoOld.xnaddr = Remote.xnaddr;
            memcpy_s( PlayerInfoOld.strGamertags, sizeof( PlayerInfoOld.strGamertags ),
                      Remote.strGamertags, sizeof( Remote.strGamertags ) );
            memcpy_s( PlayerInfoOld.xuids, sizeof( PlayerInfoOld.xuids ),
                      Remote.xuids, sizeof( Remote.xuids ) );
            memcpy_s( PlayerInfoOld.bHasVoice, sizeof( PlayerInfoOld.bHasVoice ),
                      Remote.bHasVoice, sizeof( Remote.bHasVoice ) );

            // Send the messages
            SendMessage( &msgPlayerInfoOld, NewClient.addr );
            SendMessage( &msgPlayerInfoNew, Remote.addr );
        }

        // Add the newbie to our list
        m_vecRemote.push_back( NewClient );

        // finally, if the session has already started, let the new player know
        if( m_Session.GetSessionState() > SESSION_STATE_REGISTERED )
        {
            CMessage msgStartGame( MSG_START_GAME );
            SendMessage( &msgStartGame, NewClient.addr );

            // and get them up-to-date on the score
            CMessage msgPointTotal( MSG_POINT_TOTAL );
            for( int i = 0; i < m_Local.cPlayers; ++i )
            {
                if( m_Local.nPoints[ i ] )
                {
                    msgPointTotal.GetPointTotal().id = m_Local.id;
                    msgPointTotal.GetPointTotal().xuid = m_Local.xuids[ i ];
                    msgPointTotal.GetPointTotal().nPoints = m_Local.nPoints[ i ];

                    SendMessage( &msgPointTotal, NewClient.addr );
                }
            }
            for( ClientInfoVec::iterator vecIter = m_vecRemote.begin(); vecIter != m_vecRemote.end(); ++vecIter )
            {
                ClientInfo& Remote = *vecIter;
                for( int i = 0; i < Remote.cPlayers; ++i )
                {
                    if( Remote.nPoints[ i ] )
                    {
                        msgPointTotal.GetPointTotal().id = Remote.id;
                        msgPointTotal.GetPointTotal().xuid = Remote.xuids[ i ];
                        msgPointTotal.GetPointTotal().nPoints = Remote.nPoints[ i ];

                        SendMessage( &msgPointTotal, NewClient.addr );
                    }
                }
            }
        }

        // Recalculate the number of slots
        RecalcSlots();
    }
}


//--------------------------------------------------------------------------------------
// Name: ProcessHeartbeat()
// Desc: Handle a received heartbeat message
//--------------------------------------------------------------------------------------
VOID Sample::ProcessHeartbeat( CMessage* pMsg, IN_ADDR addrFrom )
{
    // If we're not the host, we only expect heartbeats from the host
    if( !m_Session.IsHost() )
    {
        if( addrFrom == m_pHost->addr )
        {
            m_pHost->dwHeartbeat = GetTickCount();
        }
    }
    else
    {
        // If we ARE the host, see whose heart is beating
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
        {
            ClientInfo& Remote = *i;

            if( Remote.addr == addrFrom )
            {
                Remote.dwHeartbeat = GetTickCount();
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: ProcessWave()
// Desc: Handle a received wave message
//--------------------------------------------------------------------------------------
VOID Sample::ProcessWave( CMessage* pMsg, IN_ADDR addrFrom )
{
    if( !( m_AppState & APPSTATE_INSESSIONFLAG ) )
        return;

    MsgWave WaveMsg = pMsg->GetWave();

    // Find the dude that's waving
    ClientInfo* pWaver = NULL;

    if( WaveMsg.id == m_Local.id )
    {
        pWaver = &m_Local;
    }
    else
    {
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
        {
            ClientInfo& Remote = *i;

            if( Remote.id == WaveMsg.id )
            {
                pWaver = &Remote;
            }
        }
    }

    if( pWaver )
    {
        // Flag the player as waving
        for( UINT i = 0; i < pWaver->cPlayers; i++ )
        {
            if( XOnlineAreUsersIdentical( pWaver->xuids[ i ], WaveMsg.xuid ) )
            {
                pWaver->dwWave[ i ] = GetTickCount();
            }
        }

        if( m_Session.IsHost() )
        {
            // If we're the host, pass the message along to everyone else
            for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
            {
                SendMessage( pMsg, i->addr );
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: ProcessJoinResponse()
// Desc: Handle a processed join response
//--------------------------------------------------------------------------------------
VOID Sample::ProcessJoinResponse( CMessage* pMsg, IN_ADDR addrFrom )
{
    if( m_AppState != APPSTATE_CONNECTING )
    {
        return;
    }

    // See what the response was
    MsgJoinResponse JoinResponse = pMsg->GetJoinResponse();

    switch( JoinResponse.Response )
    {
        case JoinResponse.JOINRESPONSE_NOTHOSTING:
            m_Session.SetSessionError( L"This game is no longer available." );
            SwitchToState( APPSTATE_DELETING );
            break;

        case JoinResponse.JOINRESPONSE_SESSIONFULL:
            m_Session.SetSessionError( L"This game is full." );
            SwitchToState( APPSTATE_DELETING );
            break;

        case JoinResponse.JOINRESPONSE_SESSIONINPROGRESS:
            m_Session.SetSessionError( L"This game has already started." );
            SwitchToState( APPSTATE_DELETING );
            break;

        case JoinResponse.JOINRESPONSE_APPROVED:
            // Eureka!
            ClientInfo NewClient;
            m_vecRemote.push_back( NewClient );
            m_pHost = &m_vecRemote.back();
            m_pHost->dwHeartbeat = GetTickCount();
            m_pHost->cPlayers = JoinResponse.cPlayers;
            m_pHost->addr = addrFrom;
            m_pHost->bInvited = TRUE;
            memcpy_s( m_pHost->strGamertags, sizeof( m_pHost->strGamertags ),
                      JoinResponse.strGamertags, sizeof( JoinResponse.strGamertags ) );
            memcpy_s( m_pHost->xuids, sizeof( m_pHost->xuids ),
                      JoinResponse.xuids, sizeof( JoinResponse.xuids ) );
            memcpy_s( m_pHost->bHasVoice, sizeof( m_pHost->bHasVoice ),
                      JoinResponse.bHasVoice, sizeof( JoinResponse.bHasVoice ) );
            m_pHost->id = JoinResponse.id;

            m_nVictoryPoints = JoinResponse.nVictoryPoints;
            m_nMap = JoinResponse.nMap;

            m_Session.SetSessionSlots( SLOTS_TOTALPRIVATE, JoinResponse.cPrivateSlots );
            m_Session.SetSessionSlots( SLOTS_FILLEDPRIVATE, 0 );
            m_Session.SetSessionSlots( SLOTS_TOTALPUBLIC, JoinResponse.cPublicSlots );
            m_Session.SetSessionSlots( SLOTS_FILLEDPUBLIC, 0 );

            // Modify the flags, to get the correct state of invites, join in progress, etc
            m_Session.ModifySessionFlags( JoinResponse.dwSessionFlags );

            // Add the host to the session
            m_Session.SetSessionNonce( JoinResponse.Nonce );

            // Register voice for the host
            m_Voice.RegisterClient( m_pHost );

            AddUsersToSession( m_pHost );
            AddUsersToSession( &m_Local );

            RecalcSlots();

            break;
    }

    SwitchToState( APPSTATE_PREGAME );
}


//--------------------------------------------------------------------------------------
// Name: ProcessPlayerInfo()
// Desc: Handle a player info message
//--------------------------------------------------------------------------------------
VOID Sample::ProcessPlayerInfo( CMessage* pMsg, IN_ADDR addrFrom )
{
    MsgPlayerInfo PlayerInfo = pMsg->GetPlayerInfo();
    ClientInfo* pClient = NULL;

    // Look for the player info in our remote list
    ClientInfoVec::iterator i;
    for( i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
    {
        if( i->id == PlayerInfo.id )
        {
            pClient = &( *i );
            break;
        }
    }

    // If we didn't find it, this must be a new player
    if( pClient == NULL && PlayerInfo.cPlayers != 0 )
    {
        ClientInfo NewClient;
        m_vecRemote.push_back( NewClient );
        pClient = &m_vecRemote.back();
    }

    if( pClient )
    {
        if( PlayerInfo.cPlayers )
        {
            // Copy the new info to the client record
            pClient->cPlayers = PlayerInfo.cPlayers;
            pClient->id = PlayerInfo.id;
            pClient->xnaddr = PlayerInfo.xnaddr;

            XNetXnAddrToInAddr( &PlayerInfo.xnaddr, &m_Session.GetSessionInfo().sessionID, &pClient->addr );

            memcpy_s( pClient->strGamertags, sizeof( pClient->strGamertags ),
                      PlayerInfo.strGamertags, sizeof( PlayerInfo.strGamertags ) );
            memcpy_s( pClient->xuids, sizeof( pClient->xuids ),
                      PlayerInfo.xuids, sizeof( PlayerInfo.xuids ) );
            memcpy_s( pClient->bHasVoice, sizeof( pClient->bHasVoice ),
                      PlayerInfo.bHasVoice, sizeof( PlayerInfo.bHasVoice ) );
            m_Voice.RegisterClient( pClient );
            AddUsersToSession( pClient );
        }
        else
        {
            // Remove the dropped client from the list
            RemoveClient( i );
        }
    }

    // Recalculate the number of consumed slots
    RecalcSlots();
}


//--------------------------------------------------------------------------------------
// Name: RemoveClient
// Desc: Remove a client from the list
//--------------------------------------------------------------------------------------
void Sample::RemoveClient( const ClientInfoVec::iterator& i )
{
    m_Voice.UnregisterClient( &( *i ) );
    RemoveUsersFromSession( &( *i ) );
    m_vecRemote.erase( i );

    RecalcSlots();
}

//--------------------------------------------------------------------------------------
// Name: ProcessGoodbye()
// Desc: Handle a player leaving message
//--------------------------------------------------------------------------------------
VOID Sample::ProcessGoodbye( CMessage* pMsg, IN_ADDR addrFrom )
{
    ClientDropped( addrFrom );
}


//--------------------------------------------------------------------------------------
// Name: ProcessMute()
// Desc: Handle a player notifying another that they are muted
//--------------------------------------------------------------------------------------
VOID Sample::ProcessMute( CMessage* pMsg, IN_ADDR addrFrom )
{
    MsgMute Mute = pMsg->GetMute();
    // iterator for local players
    for( UINT i = 0; i < XUSER_MAX_COUNT; ++i )
    {
        // iterator for remote players
        for( UINT j = 0; j < Mute.cPlayers; ++j )
        {
            m_Local.muteXUIDs[ i ].erase( Mute.xuid[ j ] );
            // iterator for players muted by remote player
            for( UINT k = 0; k < Mute.cMuted[ j ]; ++k )
            {
                if( m_Local.xuids[ i ] == Mute.xuidMuted[ j ][ k ] )
                {
                    m_Local.muteXUIDs[ i ].insert( Mute.xuid[ j ] );
                }
            }
            m_Voice.ProcessMute( m_Local.nController[ i ], Mute.xuid[ j ] );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: ClientDropped()
// Desc: Process a client leaving the session
//--------------------------------------------------------------------------------------
VOID Sample::ClientDropped( IN_ADDR addrFrom )
{
    if( !m_Session.IsHost() )
    {
        // This must have come from the host
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
        {
            if( m_pHost == &( *i ) )
            {
                RemoveClient( i );
                m_HostMigration.BeginHostMigration( this );
                break;
            }
        }


    }
    else
    {
        // Find the dropped client
        ULONGLONG id = 0;

        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
        {
            if( i->addr == addrFrom )
            {
                id = i->id;
            }
        }

        if( id != 0 )
        {
            // Tell everyone this client is gone
            CMessage msgPlayerInfo( MSG_PLAYER_INFO );
            msgPlayerInfo.GetPlayerInfo().id = id;
            msgPlayerInfo.GetPlayerInfo().cPlayers = 0;

            // send the message
            for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
            {
                // Don't send to the dropped player, unless you really like infinite recursion
                if( i->addr == addrFrom )
                    continue;

                SendMessage( &msgPlayerInfo, i->addr );
            }

            // Send it to ourselves, too.
            SendMessage( &msgPlayerInfo, NULLADDR );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: HandleHeartbeat()
// Desc: Do housekeeping tasks for maintaining heartbeats
//--------------------------------------------------------------------------------------
VOID Sample::HandleHeartbeat()
{
    DWORD dwTicks = GetTickCount();

    // If the timer hasn't elapsed, do nothing
    if( dwTicks - m_dwHeartbeatTimer < HEARTBEAT_TIME )
        return;

    // Reset the timer
    m_dwHeartbeatTimer = dwTicks;

    // If we're the host, check everybody's heartbeats, otherwise just check the host
    if( m_Session.IsHost() )
    {
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
        {
            if( FAILED( HandleHeartbeat( &( *i ) ) ) )
                break;
        }
    }
    else
    {
        HandleHeartbeat( m_pHost );
    }
}


//--------------------------------------------------------------------------------------
// Name: HandleHeartbeat()
// Desc: Send a heartbeat and check last received heartbeat
//--------------------------------------------------------------------------------------
HRESULT Sample::HandleHeartbeat( ClientInfo* pClient )
{
    // Send a heartbeat to the client
    CMessage msgHeartbeat( MSG_HEARTBEAT );
    SendMessage( &msgHeartbeat, pClient->addr );

    // Check to see if the client has timed out
    BOOL bTimedOut = GetTickCount() - pClient->dwHeartbeat > HEARTBEAT_TIMEOUT;

    // If so, drop him
    if( bTimedOut )
    {
        ClientDropped( pClient->addr );
    }

    return bTimedOut ? E_FAIL : S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RecalcSlots()
// Desc: Recalculate public/filled slots
//--------------------------------------------------------------------------------------
VOID Sample::RecalcSlots()
{
    UINT cPlayers = m_Local.cPlayers;
    UINT cPrivate = m_Local.bInvited ? m_Local.cPlayers : 0;

    // Loop through the list of remote clients
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
    {
        cPlayers += i->cPlayers;

        if( i->bInvited )
        {
            cPrivate += i->cPlayers;
        }
    }

    // Figure out how many private slots are consumed
    cPrivate = min( cPrivate, m_Session.GetSessionSlots( SLOTS_TOTALPRIVATE ) );

    m_Session.SetSessionSlots( SLOTS_FILLEDPRIVATE, cPrivate );
    m_Session.SetSessionSlots( SLOTS_FILLEDPUBLIC, cPlayers - cPrivate );
}


//--------------------------------------------------------------------------------------
// Name: RemoveUsersFromSession()
// Desc: Remove users from a created/joined session
//--------------------------------------------------------------------------------------
VOID Sample::RemoveUsersFromSession( const ClientInfo* pClient )
{
    if( &m_Local == pClient )
        m_Session.RemoveLocalPlayers( pClient );
    else
        m_Session.RemoveRemotePlayers( pClient );
}


//--------------------------------------------------------------------------------------
// Name: ReadTrueSkills()
// Desc: Reads the TrueSkill(TM) skill for all client players. This function is not
//       guaranteed to fill the Mu/Sigma structures!
//--------------------------------------------------------------------------------------
VOID Sample::ReadTrueSkills( ClientInfo* pClient )
{
    // Throw away old results
    if( m_pStats )
    {
        delete[] m_pStats;
        m_pStats = NULL;
    }

    // copy the XUID's 
    XUID adwXuids[ XUSER_MAX_COUNT ];
    for( UINT i = 0; i < pClient->cPlayers; ++i )
        adwXuids[ i ] = pClient->xuids[ i ];

    // define the specification for a Mu/Sigma skill view
    m_Spec.dwViewId = m_dwSkillViews[ m_nGameMode ];
    m_Spec.dwNumColumnIds = 2;
    m_Spec.rgwColumnIds[ 0 ] = X_STATS_COLUMN_SKILL_MU;
    m_Spec.rgwColumnIds[ 1 ] = X_STATS_COLUMN_SKILL_SIGMA;

    // Get the size of the buffer 
    DWORD cbResults = 0;
    DWORD ret;

    ret = XUserReadStats(
        0,                        // Current title ID
        pClient->cPlayers,        // Number of users
        adwXuids,                 // Buffer of XUID's
        1,                        // Number of stats spec
        &m_Spec,                  // Stats spec
        &cbResults,               // Size of buffer
        NULL,                     // Pointer to results buffer
        NULL );                   // Pointer of an overlapped structure

    if( ret != ERROR_INSUFFICIENT_BUFFER )
        return;

    // Allocate the buffer
    m_pStats = ( PXUSER_STATS_READ_RESULTS )new BYTE[ cbResults ];

    // read the results
    ret = XUserReadStats(
        0,                        // Current title ID
        pClient->cPlayers,        // Number of users
        adwXuids,                 // Buffer of XUID's
        1,                        // Number of stats spec
        &m_Spec,                  // Stats spec
        &cbResults,               // Size of buffer
        m_pStats,                 // Pointer to results buffer
        NULL );                   // Pointer of an overlapped structure

    if( ret != ERROR_SUCCESS )
        return;

    for( UINT i = 0; i < pClient->cPlayers; ++i )
    {
        pClient->dMu[ i ] = m_pStats->pViews[ 0 ].pRows[ i ].pColumns[ 0 ].Value.dblData;
        pClient->dSigma[ i ] = m_pStats->pViews[ 0 ].pRows[ i ].pColumns[ 1 ].Value.dblData;

        // this is a player new to the league
        if( pClient->dMu[ i ] == 0.0 && pClient->dSigma[ i ] == 0.0 )
        {
            pClient->dMu[ i ] = 3.0;
            pClient->dSigma[ i ] = 1.0;
        }
    }

    // done!
    return;
}

//--------------------------------------------------------------------------------------
// Name: AddUsersToSession()
// Desc: Add users to a created/joined session
//--------------------------------------------------------------------------------------
VOID Sample::AddUsersToSession( ClientInfo* pClient )
{
    // first read the TrueSkill(TM) skills of all gamers involved ...
    ReadTrueSkills( pClient );

    if( &m_Local == pClient )
        m_Session.AddLocalPlayers( pClient );
    else
        m_Session.AddRemotePlayers( pClient );

    UpdateMuteLists();
}


//--------------------------------------------------------------------------------------
// Name: StartGame()
// Desc: Notify all peers to begin playing
//--------------------------------------------------------------------------------------
VOID Sample::StartGame()
{
    assert( m_Session.IsHost() );

    // Send a start message to everyone, including ourselves
    CMessage msg( MSG_START_GAME );
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
    {
        SendMessage( &msg, i->addr );
    }

    SendMessage( &msg, NULLADDR );
}


//--------------------------------------------------------------------------------------
// Name: ProcessStartGame()
// Desc: Begin the game
//--------------------------------------------------------------------------------------
VOID Sample::ProcessStartGame( CMessage* pMsg, IN_ADDR addrFrom )
{
    assert( m_AppState == APPSTATE_PREGAME || m_AppState == APPSTATE_REGISTERED );

    // Start the session
    SwitchToState( APPSTATE_STARTING );
}


//--------------------------------------------------------------------------------------
// Name: StartRegistration()
// Desc: Notify all peers to register for arbitration
//--------------------------------------------------------------------------------------
VOID Sample::StartRegistration()
{
    // Start the timer
    m_dwRegistrationTimer = GetTickCount();
    m_AppState = APPSTATE_WAITINGFORREGISTRATION;

    // Send the message to the peers
    CMessage msg( MSG_REGISTER );
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
    {
        i->bRegistered = FALSE;
        SendMessage( &msg, i->addr );
    }
}


//--------------------------------------------------------------------------------------
// Name: ProcessRegister()
// Desc: Receive order from host to register
//--------------------------------------------------------------------------------------
VOID Sample::ProcessRegister( CMessage* pMsg, IN_ADDR addrFrom )
{
    m_Session.SwitchToState( SESSION_STATE_REGISTERING );
    m_AppState = APPSTATE_REGISTERING;
}


//--------------------------------------------------------------------------------------
// Name: ProcessRegistered()
// Desc: Handle a registered message
//--------------------------------------------------------------------------------------
VOID Sample::ProcessRegistered( CMessage* pMsg, IN_ADDR addrFrom )
{
    assert( m_Session.IsHost() );

    bool bAllRegistered = true;

    // Loop through all clients, mark this client as registered and check if all are
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
    {
        ClientInfo& Remote = *i;

        if( Remote.addr == addrFrom )
            Remote.bRegistered = true;

        if( !Remote.bRegistered )
            bAllRegistered = false;
    }

    // If all clients have registered, start registering ourselves
    if( bAllRegistered )
    {
        m_Session.SwitchToState( SESSION_STATE_REGISTERING );
        m_AppState = APPSTATE_REGISTERING;
    }
}


//--------------------------------------------------------------------------------------
// Name: ProcessRegistrationList()
// Desc: Scan the arbitration registration results and boot anyone who didn't register
//--------------------------------------------------------------------------------------
VOID Sample::ProcessRegistrationList()
{
    assert( m_Session.IsHost() );

    // Start off by flagging everybody as unregistered
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
    {
        i->bRegistered = FALSE;
    }

    // Loop through the arbitrated list and flag the machines that showed up
    for( UINT n = 0; n < m_Session.GetRegistrationResults()->wNumRegistrants; n++ )
    {
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
        {
            if( i->id == m_Session.GetRegistrationResults()->rgRegistrants[ n ].qwMachineID )
            {
                i->bRegistered = TRUE;
            }
        }
    }

    // Now loop through the client list one more time looking for unregistered clients. 
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
    {
        if( !i->bRegistered )
        {
            ClientDropped( i->addr );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: ProcessScorePoint()
// Desc: Handle a request to score a point
//--------------------------------------------------------------------------------------
VOID Sample::ProcessScorePoint( CMessage* pMsg, IN_ADDR addrFrom )
{
    // Can only score points when in-game
    if( m_AppState != APPSTATE_INGAME && m_AppState != APPSTATE_STARTING )
        return;

    MsgScorePoint ScorePointMsg = pMsg->GetScorePoint();

    // Find the player that's trying to score
    ClientInfo* pScorer = NULL;

    if( ScorePointMsg.id == m_Local.id )
    {
        pScorer = &m_Local;
    }
    else
    {
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
        {
            ClientInfo& Remote = *i;

            if( Remote.id == ScorePointMsg.id )
            {
                pScorer = &Remote;
            }
        }
    }

    if( pScorer )
    {
        if( m_Session.IsHost() )
        {
            // If we're the host, pass the message along to everyone else
            for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
            {
                SendMessage( pMsg, i->addr );
            }
        }

        // Increment the player's points
        for( UINT i = 0; i < pScorer->cPlayers; i++ )
        {
            if( XOnlineAreUsersIdentical( pScorer->xuids[ i ], ScorePointMsg.xuid ) )
            {
                if( ++pScorer->nPoints[ i ] == m_nVictoryPoints )
                {
                    SwitchToState( APPSTATE_ENDING );
                }
            }
        }

    }
}


//--------------------------------------------------------------------------------------
// Name: ProcessPointTotal()
// Desc: Handle an update to a player's point total
//--------------------------------------------------------------------------------------
VOID Sample::ProcessPointTotal( CMessage* pMsg, IN_ADDR addrFrom )
{
    // Can only score points when in-game
    if( m_AppState != APPSTATE_INGAME && m_AppState != APPSTATE_STARTING )
    {
        return;
    }

    MsgPointTotal PointTotalMsg = pMsg->GetPointTotal();

    // Find the player that's trying to score
    ClientInfo* pScorer = NULL;

    if( PointTotalMsg.id == m_Local.id )
    {
        pScorer = &m_Local;
    }
    else
    {
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
        {
            ClientInfo& Remote = *i;

            if( Remote.id == PointTotalMsg.id )
            {
                pScorer = &Remote;
            }
        }
    }

    if( pScorer )
    {
        // Update the player's points
        for( UINT i = 0; i < pScorer->cPlayers; ++i )
        {
            if( XOnlineAreUsersIdentical( pScorer->xuids[ i ], PointTotalMsg.xuid ) )
            {
                pScorer->nPoints[ i ] = PointTotalMsg.nPoints;
                if( PointTotalMsg.nPoints == m_nVictoryPoints )
                {
                    SwitchToState( APPSTATE_ENDING );
                }
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: WriteStats()
// Desc: Write stats for the session
//--------------------------------------------------------------------------------------
VOID Sample::WriteStats()
{
    WriteStats( &m_Local );

    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
    {
        ClientInfo& Remote = *i;
        WriteStats( &Remote );
    }
}


//--------------------------------------------------------------------------------------
// Name: WriteStats( ClientInfo * )
// Desc: Write stats for a particular user
//--------------------------------------------------------------------------------------
VOID Sample::WriteStats( const ClientInfo* pClient )
{
    DWORD dwRet;

    // Figure out which leaderboards to write to
    DWORD dwLeaderboardTypeMode = 0;
    DWORD dwLeaderboardType = 0;

    dwLeaderboardType = ( m_nGameType == X_CONTEXT_GAME_TYPE_RANKED ) ?
        STATS_VIEW_RANKED_GAMES : STATS_VIEW_STANDARD_GAMES;

    switch( m_nGameType << 8 | m_nGameMode )
    {
        case X_CONTEXT_GAME_TYPE_RANKED << 8 | CONTEXT_GAME_MODE_DEATHMATCH:
            dwLeaderboardTypeMode = STATS_VIEW_RANKED_DEATHMATCH;
            break;
        case X_CONTEXT_GAME_TYPE_RANKED << 8 | CONTEXT_GAME_MODE_COOPERATIVE:
            dwLeaderboardTypeMode = STATS_VIEW_RANKED_COOPERATIVE;
            break;
        case X_CONTEXT_GAME_TYPE_RANKED << 8 | CONTEXT_GAME_MODE_TEAM_BATTLE:
            dwLeaderboardTypeMode = STATS_VIEW_RANKED_TEAM_PLAY;
            break;
        case X_CONTEXT_GAME_TYPE_STANDARD << 8 | CONTEXT_GAME_MODE_DEATHMATCH:
            dwLeaderboardTypeMode = STATS_VIEW_STANDARD_DEATHMATCH;
            break;
        case X_CONTEXT_GAME_TYPE_STANDARD << 8 | CONTEXT_GAME_MODE_COOPERATIVE:
            dwLeaderboardTypeMode = STATS_VIEW_STANDARD_COOPERATIVE;
            break;
        case X_CONTEXT_GAME_TYPE_STANDARD << 8 | CONTEXT_GAME_MODE_TEAM_BATTLE:
            dwLeaderboardTypeMode = STATS_VIEW_STANDARD_TEAM_PLAY;
            break;
        default:
            ATG::FatalError( "Unexpected type/mode pair" );
            break;
    }

    for( UINT i = 0; i < pClient->cPlayers; i++ )
    {
        // Write stats to the skill leaderboard
        // Note that the value for determining Team ID is a quick-and-dirty way of trying
        // to ensure that each user has a unique team ID, since this is an individual game.
        // Titles should use a more robust method of assigning team ID.
        UINT cViews = 1;

        XSESSION_VIEW_PROPERTIES Views[ 3 ];

        XUSER_PROPERTY Skill[ 2 ];
        XUSER_PROPERTY Stats[ 4 ];

        Skill[ 0 ].dwPropertyId = X_PROPERTY_RELATIVE_SCORE;
        Skill[ 0 ].value.nData = pClient->nPoints[ i ];
        Skill[ 0 ].value.type = XUSER_DATA_TYPE_INT32;
        Skill[ 1 ].dwPropertyId = X_PROPERTY_SESSION_TEAM;
        Skill[ 1 ].value.nData = ( LONG )
            ( ( pClient->xuids[ i ] >> 32 ) ^ ( pClient->xuids [ i ] & MAXDWORD ) );
        Skill[ 1 ].value.type = XUSER_DATA_TYPE_INT32;

        Views[ 0 ].dwNumProperties = 2;
        Views[ 0 ].dwViewId = X_STATS_VIEW_SKILL;
        Views[ 0 ].pProperties = Skill;

        // Write stats to the non-skill leaderboards
        // If this is a ranked game, write stats to non-skill leaderboards for every user
        // Otherwise, write stats to non-skill leaderboards only for local user
        if( m_nGameType == X_CONTEXT_GAME_TYPE_RANKED ||
            pClient == &m_Local )
        {
            cViews = 3;

            Stats[ 0 ].dwPropertyId = PROPERTY_GAMES_PLAYED;
            Stats[ 0 ].value.nData = 1;
            Stats[ 0 ].value.type = XUSER_DATA_TYPE_INT32;
            Stats[ 1 ].dwPropertyId = PROPERTY_GAMES_WON;
            Stats[ 1 ].value.i64Data = pClient->nPoints[ i ] == m_nVictoryPoints;
            Stats[ 1 ].value.type = XUSER_DATA_TYPE_INT64;
            Stats[ 2 ].dwPropertyId = PROPERTY_POINTS_SCORED;
            Stats[ 2 ].value.nData = pClient->nPoints[ i ];
            Stats[ 2 ].value.type = XUSER_DATA_TYPE_INT32;
            Stats[ 3 ].dwPropertyId = CONTEXT_MAP;
            Stats[ 3 ].value.nData = m_nMap;
            Stats[ 3 ].value.type = XUSER_DATA_TYPE_CONTEXT;

            Views[ 1 ].dwNumProperties = 4;
            Views[ 1 ].dwViewId = dwLeaderboardTypeMode;
            Views[ 1 ].pProperties = Stats;

            Views[ 2 ].dwNumProperties = 4;
            Views[ 2 ].dwViewId = dwLeaderboardType;
            Views[ 2 ].pProperties = Stats;
        }

        dwRet = XSessionWriteStats(
            m_Session.GetSessionHandle(),
            pClient->xuids[ i ],
            cViews,
            Views,
            NULL               // do it synchronously for simplicity; actual titles should
            );                 // be asynchronous

        if( dwRet != ERROR_SUCCESS )
        {
            HRESULT hr = XGetOverlappedExtendedError( NULL );

            ATG::FatalError( "Failed to write stats, error 0x%08x\n", hr );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: ReadLeaderboard()
// Desc: Read data from the leaderboard
//--------------------------------------------------------------------------------------
VOID Sample::ReadLeaderboard( INT idx, XUID xuid )
{
    // Throw away old results
    if( m_pStats )
    {
        delete[] m_pStats;
        m_pStats = NULL;
    }

    // decide if the TrueSkill(TM) skills should be read, too
    if( m_aLeaderboards[ m_nGameType ][ m_nLeaderboard ].m_dwSkillIndex )
    {
        // Populate the stats spec
        m_Spec.dwViewId = m_aLeaderboards[ m_nGameType ][ m_nLeaderboard ].m_dwSkillIndex;
        m_Spec.dwNumColumnIds = 3;
        m_Spec.rgwColumnIds[ 0 ] = X_STATS_COLUMN_SKILL_MU;
        m_Spec.rgwColumnIds[ 1 ] = X_STATS_COLUMN_SKILL_SIGMA;
        m_Spec.rgwColumnIds[ 2 ] = X_STATS_COLUMN_SKILL_GAMESPLAYED;
    }
    else
    {
        // Populate the stats spec
        m_Spec.dwViewId = m_aLeaderboards[ m_nGameType ][ m_nLeaderboard ].m_dwIndex;
        m_Spec.dwNumColumnIds = 3;
        m_Spec.rgwColumnIds[ 0 ] = STATS_COLUMN_RANKED_GAMES_GAMES_PLAYED;
        m_Spec.rgwColumnIds[ 1 ] = STATS_COLUMN_RANKED_GAMES_POINTS_SCORED;
        m_Spec.rgwColumnIds[ 2 ] = STATS_COLUMN_RANKED_GAMES_LAST_MAP;
    }

    HANDLE hEnumerator;
    DWORD cbResults;
    DWORD ret;

    // Calculate the required buffer size
    if( idx != 0 )
    {
        // Nonzero index means enumerate by rank
        ret = XUserCreateStatsEnumeratorByRank(
            0,                       // Current title ID
            idx,                     // Index to start enumerating from
            m_nMaxLeaderboardRows,   // Number of rows to retrieve
            1,                       // One stats spec
            &m_Spec,                 // Stats spec,
            &cbResults,              // Size of buffer
            &hEnumerator );          // Enumeration handle
    }
    else
    {
        // Zero index means enumerate by XUID
        ret = XUserCreateStatsEnumeratorByXuid(
            0,                       // Current title ID
            xuid,                    // XUID to pivot on
            m_nMaxLeaderboardRows,   // Number of rows to retrieve
            1,                       // One stats spec
            &m_Spec,                 // Stats spec,
            &cbResults,              // Size of buffer
            &hEnumerator );          // Enumeration handle
    }

    if( ret != ERROR_SUCCESS )
    {
        ATG::FatalError(
            "XUserCreateStatsEnumerator...() failed with error %d", ret );
    }

    // Allocate the buffer
    m_pStats = ( PXUSER_STATS_READ_RESULTS )new BYTE[ cbResults ];

    // Enumerate
    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

    ret = XEnumerate(
        hEnumerator,           // Enumeration handle
        m_pStats,              // Buffer
        cbResults,             // Size of buffer
        NULL,                  // Number of rows returned; not used for asynch
        &m_Overlapped );       // Overlapped structure

    if( ret != ERROR_IO_PENDING )
    {
        ATG::FatalError( "XEnumerate() failed with error %d", ret );
    }

    m_bReadingLeaderboard = TRUE;
}


//--------------------------------------------------------------------------------------
// Name: SendVoiceMessage()
// Desc: Send a message containing voice data
//--------------------------------------------------------------------------------------
VOID Sample::SendVoiceMessage()
{
    CMessage msg( MSG_VOICE );

    MsgVoice& msgVoice = msg.GetVoice();
    msgVoice.id = m_Local.id;

    // Retrieve voice data
    msgVoice.wSize = 0;
    WORD wSize = m_Voice.GetVoiceData( msgVoice.bData, MAX_VDP_PACKET - msg.GetSize() );

    msgVoice.wSize = wSize;

    // Send the message to everybody
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
    {
        SendMessage( &msg, i->addr );
    }
}


//--------------------------------------------------------------------------------------
// Name: ProcessVoice()
// Desc: Handle a message containing voice data
//--------------------------------------------------------------------------------------
VOID Sample::ProcessVoice( CMessage* pMsg, IN_ADDR addrFrom )
{
    MsgVoice& msgVoice = pMsg->GetVoice();

    // Find the client this came from
    ClientInfo* pClient = NULL;

    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
    {
        if( i->id == msgVoice.id )
        {
            pClient = &*i;
            break;
        }
    }

    if( pClient )
    {
        // Submit the voice data to the voice class
        m_Voice.SubmitVoiceData( msgVoice.bData, msgVoice.wSize, pClient );
    }
}


//--------------------------------------------------------------------------------------
// Name: SessionNotification()
// Desc: Process notification messages from the session
// TBD -- for multiple session samples, add session id param
//--------------------------------------------------------------------------------------
VOID Sample::SessionNotification( SESSION_NOTIFY newNotification )
{
    switch( newNotification )
    {
        case SESSION_NOTIFY_CREATED:
            if( m_AppState == APPSTATE_CREATE )
            {
                // Join the users to the session
                if( m_Session.IsHost() )
                {
                    m_Local.bInvited = TRUE;
                    m_Session.StartQoSListener( ( BYTE* )m_QoSString, wcslen( m_QoSString ) * sizeof( WCHAR ), 0 );
                }
                AddUsersToSession( &m_Local );

                // Update the slots
                RecalcSlots();

                SwitchToState( APPSTATE_PREGAME );
            }
            else if( !m_Session.IsHost() )
            {
                IN_ADDR host_addr_temp;
                // Resolve the host's IP address
                if( XNetXnAddrToInAddr( &m_Session.GetSessionInfo().hostAddress,
                                        &m_Session.GetSessionInfo().sessionID, &host_addr_temp ) != 0 )
                {
                    m_Session.SetSessionError( L"Could not resolve host's IP address." );
                    SwitchToState( APPSTATE_PREGAME );
                }
                else
                {
                    CHAR szGamerTag[XUSER_NAME_SIZE];

                    // Send a join message to the host
                    CMessage msgJoinSession( MSG_JOIN_SESSION );
                    MsgJoinSession& JoinSession = msgJoinSession.GetJoinSession();

                    JoinSession.id = m_Local.id;
                    JoinSession.cPlayers = m_Local.cPlayers;
                    JoinSession.bInvited = m_Local.bInvited;

                    m_Voice.GetLocalVoice( JoinSession.bHasVoice );

                    for( UINT i = 0, idx = 0; i < XUSER_MAX_COUNT; ++i )
                    {
                        if( ATG::SignIn::IsUserSignedIn( i ) )
                        {
                            JoinSession.nController[ idx ] = ( BYTE )i;
                            XUserGetXUID( i, &JoinSession.xuids[ idx ] );
                            XUserGetName( i, szGamerTag, XUSER_NAME_SIZE );
                            szGamerTag[ XUSER_NAME_SIZE - 1 ] = '\0';
                            MultiByteToWideChar( CP_ACP, 0, szGamerTag, -1,
                                                 JoinSession.strGamertags[ idx ], XUSER_NAME_SIZE );

                            idx++;
                        }
                    }

                    SendMessage( &msgJoinSession, host_addr_temp );
                    m_dwConnectionTimer = GetTickCount();

                    SwitchToState( APPSTATE_CONNECTING );
                }
            }
            break;

        case SESSION_NOTIFY_REGISTERED:
            if( m_AppState == APPSTATE_REGISTERING )
            {
                // we're registered! tell the host
                if( !m_Session.IsHost() )
                {
                    CMessage msg( MSG_REGISTERED );
                    SendMessage( &msg, m_pHost->addr );

                    SwitchToState( APPSTATE_REGISTERED );
                }
                else
                {
                    // we're the host, we have to process the registration list
                    // and reconcile it with the clients
                    ProcessRegistrationList();

                    SwitchToState( APPSTATE_REGISTERED );
                }
            }
            break;

        case SESSION_NOTIFY_FAIL_REGISTER:
            if( m_AppState == APPSTATE_REGISTERING )
            {
                SwitchToState( APPSTATE_PREGAME );
            }
            break;

        case SESSION_NOTIFY_STARTED:
            if( m_AppState == APPSTATE_STARTING )
            {
                SwitchToState( APPSTATE_INGAME );
            }
            break;

        case SESSION_NOTIFY_FAIL_START:
            if( m_AppState == APPSTATE_STARTING )
            {
                SwitchToState( APPSTATE_PREGAME );
            }
            break;

        case SESSION_NOTIFY_ENDED:
            if( m_AppState == APPSTATE_ENDING )
            {
                SwitchToState( APPSTATE_POSTGAME );
            }
            break;

        case SESSION_NOTIFY_FAIL_END:
            if( m_AppState == APPSTATE_ENDING )
            {
                SwitchToState( APPSTATE_INGAME );
            }
            break;

        case SESSION_NOTIFY_DELETED:
            if( m_AppState == APPSTATE_DELETING )
            {
                SwitchToState( APPSTATE_MAINMENU );
            }
            break;
    }
}
