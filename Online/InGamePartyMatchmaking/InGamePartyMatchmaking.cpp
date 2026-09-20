//--------------------------------------------------------------------------------------
// InGamePartyMatchmaking.cpp
//
// Sample for hosting, finding, and joining sessions on Xbox Live, plus sending,
// receiving, and responding to invitations.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma warning( disable: 4355 ) // warning C4355: 'this' : used in base process initializer list

// Disable warning C6211: Leaking memory <pointer> due to an exception.
// We "know" we won't run out of memory in this sample
#pragma warning ( disable : 6211 )


#include "XPlat.h"

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
const DWORD MAX_CONNECTIONWAIT = 10000 * 3;    // wait 3o seconds for response from host
const DWORD PORT               = 1000;         // Port 1000 = most efficient port

const DWORD  Sample::m_anNotificationPosition[] =
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

const size_t Sample::m_cNotificationPosition =
    sizeof( Sample::m_anNotificationPosition ) /
    sizeof( Sample::m_anNotificationPosition[0] );

const WCHAR* Sample::m_astrNotificationPosition[] =
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

const WCHAR* Sample::m_astrGameTypes[] =
{
    L"Ranked",
    L"Player Match",
};

const WCHAR* Sample::m_QoSString = L"My Game";

const DWORD Sample::m_cGameTypes = ARRAYSIZE( m_astrGameTypes );

const WCHAR* Sample::m_astrGameModes[] =
{
    L"Deathmatch",
    L"Cooperative",
    L"Team Battle",
    L"Party Session" // For party session only. Must be last in the enum!
};

const DWORD Sample::m_cGameModes = ARRAYSIZE( m_astrGameModes ) - 1;
const DWORD Sample::m_cPartySessionGameMode = ARRAYSIZE( m_astrGameModes ) - 1;

const WCHAR* Sample::m_astrMaps[] =
{
    L"Stalingrad",
    L"Leyte Gulf",
    L"Ardennes",
    L"Normandy",
    L"Any"
};

const DWORD Sample::m_cMaps = 4;

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

const DWORD Sample::m_cLeaderboards       = 4;
const DWORD Sample::m_nMaxLeaderboardRows = 8;

const char* Sample::m_astrAppStates[] =
{
    "APPSTATE_MAINMENU",
    "APPSTATE_ENUMERATING_PRESENCE",
    "APPSTATE_UPDATING_RICHPRESENCE",
    "APPSTATE_CREATE_PRESENCE_UI",
    "APPSTATE_CREATE_MATCHMAKING_UI",
    "APPSTATE_CREATE_PRESENCE",
    "APPSTATE_CREATE_MATCHMAKING",
    "APPSTATE_CREATE_SESSION_UNHOSTED",
    "APPSTATE_CREATING_SESSION",
    "APPSTATE_ADDING_LOCAL_PLAYERS",
    "APPSTATE_ADDED_LOCAL_PLAYERS",
    "APPSTATE_ADDING_REMOTE_PLAYERS",
    "APPSTATE_ADDED_REMOTE_PLAYERS",
    "APPSTATE_REMOVING_LOCAL_PLAYERS",
    "APPSTATE_REMOVED_LOCAL_PLAYERS",
    "APPSTATE_REMOVING_REMOTE_PLAYERS",
    "APPSTATE_REMOVED_REMOTE_PLAYERS",
    "APPSTATE_SEARCHUI_MATCHMAKING",
    "APPSTATE_SEARCH_MATCHMAKING",
    "APPSTATE_SEARCH_MATCHMAKING_DONE",
    "APPSTATE_CONNECTING_SESSION",
    "APPSTATE_VIEWSTATS",
    "APPSTATE_DELETING_SESSION",
    "APPSTATE_PREGAME",
    "APPSTATE_WAITINGFORREGISTRATION",
    "APPSTATE_REGISTERING",
    "APPSTATE_REGISTERED",
    "APPSTATE_STARTING",
    "APPSTATE_INGAME",
    "APPSTATE_WRITINGSTATS",
    "APPSTATE_ENDING",
    "APPSTATE_POSTGAME",
    "APPSTATE_HOSTMIGRATION",
    "APPSTATE_CANCELLING_OVERLAPPED",
};

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program on Xbox 360
//--------------------------------------------------------------------------------------
#ifdef _XBOX
VOID __cdecl main()
{
    #ifdef _DEBUG
    _CrtSetDbgFlag( _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG     | 
                                    _CRTDBG_LEAK_CHECK_DF   | 
                                    _CRTDBG_ALLOC_MEM_DF ) ); 
    #endif

	// Allocate Sample on the heap as it would otherwise take up too much stack memory
	Sample* pApp = new Sample();
    Sample& app  = *pApp;
    GetVideoSettings( &app.m_d3dpp.BackBufferWidth, &app.m_d3dpp.BackBufferHeight );
    app.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    app.Run();
}
#endif

//--------------------------------------------------------------------------------------
// Name: wmain()
// Desc: Entry point to the program on Games for Windows - Live
//--------------------------------------------------------------------------------------
#ifdef LIVE_ON_WINDOWS
VOID __cdecl wmain()
{
	// Allocate Sample on the heap as it would otherwise take up too much stack memory
	Sample* pApp = new Sample();
    Sample& app  = *pApp;
    app.Run();    
}
#endif

//--------------------------------------------------------------------------------------
// Name: operator== (IN_ADDR)
// Desc: Little helper operator that will prove useful
//--------------------------------------------------------------------------------------
BOOL operator==( const IN_ADDR& i1, const IN_ADDR& i2 )
{
    return memcmp( &i1, &i2, sizeof( IN_ADDR ) ) == 0;
}

//--------------------------------------------------------------------------------------
// Name: operator== (XNKID)
// Desc: Another helper operator that will prove useful
//--------------------------------------------------------------------------------------
BOOL operator==( const XNKID& sid1, const XNKID& sid2 )
{
    return memcmp( &sid1, &sid2, sizeof( XNKID ) ) == 0;
}

//--------------------------------------------------------------------------------------
// Name: operator!= (XNKID)
// Desc: Another helper operator that will prove useful
//--------------------------------------------------------------------------------------
BOOL operator!=( const XNKID& sid1, const XNKID& sid2 )
{
    return !( sid1 == sid2 );
}

//--------------------------------------------------------------------------------------
// Name: Sample()
// Desc: Default ctor the sample
//--------------------------------------------------------------------------------------
Sample::Sample() :
    m_HostMigration(),
    m_CXPlat_Draw( this ),
    m_CXPlat_UserInput( this ),
    m_CXPlat_Signin( this ),
    m_CXPlat_XOnline( this ),
    m_hLiveListener( INVALID_HANDLE_VALUE ),
    m_hFriendsListener( INVALID_HANDLE_VALUE ),
    m_hCustomListener( INVALID_HANDLE_VALUE ),
    m_pTaskSchedulerTaskDataStructs( NULL )
{
}

//--------------------------------------------------------------------------------------
// Name: ~Sample()
// Desc: Destructor for the sample -- clean up
//--------------------------------------------------------------------------------------
Sample::~Sample()
{
    if( m_pQoSResult != NULL )
    {
        XNetQosRelease( m_pQoSResult );
    }

    if( INVALID_HANDLE_VALUE != m_hLiveListener )
    {
        XCloseHandle( m_hLiveListener );
    }

    if( INVALID_HANDLE_VALUE != m_hFriendsListener )
    {
        XCloseHandle( m_hFriendsListener );
    }

    if( INVALID_HANDLE_VALUE != m_hCustomListener )
    {
        XCloseHandle( m_hCustomListener );
    }

    delete[] m_pTaskSchedulerTaskDataStructs;

    // Clean up any lingering sessions
    std::map<ULONGLONG, SessionManager*>::const_iterator citer;
    for ( citer = m_mapSessions.begin(); citer != m_mapSessions.end(); ++citer )
    {
        SessionManager* pSessionMgr = citer->second;
        if( pSessionMgr )
        {
            // Call session manager dtor
            delete pSessionMgr;
        }
    }

    // Cleanup sign-in code
    m_CXPlat_Signin.Cleanup();

    // Tear down network stack
    m_CXPlat_XOnline.Cleanup();

}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_CXPlat_XOnline.Startup();

    if( !m_CXPlat_Draw.Initialize() )
    {
        FatalError( "Failed to initialize m_CXPlat_Draw.\n" );
    }

    #ifdef _XBOX
    // Initialize m_LivePartyUserList
    ZeroMemory( &m_LivePartyUserList, sizeof( XPARTY_USER_LIST ) );
    #endif

    // Register our Live (invitation and XBox LIVE Party) listener
    m_hLiveListener = XNotifyCreateListener( XNOTIFY_LIVE 
                                            #ifdef _XBOX 
                                            | XNOTIFY_PARTY 
                                            #endif 
                                            );
    if( m_hLiveListener == NULL || m_hLiveListener == INVALID_HANDLE_VALUE )
    {
        FatalError( "Failed to create Live state notification listener.\n" );
    }

    // Register our Custom actions listener
    m_hCustomListener = XNotifyCreateListener( XNOTIFY_CUSTOM );
    if( m_hCustomListener == NULL || m_hCustomListener == INVALID_HANDLE_VALUE )
    {
        FatalError( "Failed to create Custom actions notification listener.\n" );
    }

    // Register our Friends notifications listener
    m_hFriendsListener = XNotifyCreateListener( XNOTIFY_FRIENDS );
    if( m_hFriendsListener == NULL || m_hFriendsListener == INVALID_HANDLE_VALUE )
    {
        FatalError( "Failed to create Friends notification listener.\n" );
    }

    // Initialize our Task Scheduler to run on this thread only
    if( !m_TaskScheduler.Initialize( 0 ) )
    {
        FatalError( "Failed to initialize task scheduler.\n" );
    }

    // Get the maximum number of tasks the scheduler can scheduler and use that
    // to allocate an array of TaskSchedulerTaskData structs which we'll use
    // for task data and to pre-fill a stack of free indices into this array
    const UINT cMaxSchedulerTasks = m_TaskScheduler.GetMaxTaskCount();
    m_pTaskSchedulerTaskDataStructs = new TaskSchedulerTaskData[ cMaxSchedulerTasks ];

    for( UINT i = cMaxSchedulerTasks - 1; i > 0; --i )
    {
        m_pTaskSchedulerTaskDataStructs[i].m_index = i;
        m_stackFreeTaskSchedulerTaskDataIndices.push( i ); 
    }

    // Initialize signin
    m_CXPlat_Signin.Initialize( 1, MAX_USER_COUNT, FALSE, MAX_USER_COUNT );

    // Initialize rich presence. We need to do this for subscribing to non-friends' presence info
    if( ERROR_SUCCESS != XPresenceInitialize( MAX_NON_FRIENDS_SUBSCRIPTIONS ) )
    {
        FatalError( "XPresenceInitialize failed!\n" );
    }

    // Create the socket
    m_Socket = Xplat_CreateSocket( AF_INET, SOCK_DGRAM, IPPROTO_VDP );
    if( m_Socket == INVALID_SOCKET )
    {
        FatalError( "Failed to create the socket.\n" );
    }

    // Bind the socket
    SOCKADDR_IN sa;
    sa.sin_family      = AF_INET;               // IP family
    sa.sin_addr.s_addr = INADDR_ANY;            // Use the only IP that's available to us
    sa.sin_port        = Xplat_HTONS( PORT );   // Port (should be 1000)

    if( Xplat_BindSocket( m_Socket, (SOCKADDR*)&sa, sizeof(sa) ) != 0 )
    {
        FatalError( "Failed to bind socket, error %d.\n", Xplat_WSAGetLastError() );
    }

    // Mark the socket as nonblocking
    DWORD dwNonblocking = 1;

    if( Xplat_IOCTLSocket( m_Socket, FIONBIO, &dwNonblocking ) != 0 )
    {
        FatalError( "Failed to set the socket to nonblocking.\n" );
    }

    // Initialize local variables
    m_nNotificationPosition     = 7;
    m_nMenuItem                 = 0;
    m_pSearchResults            = NULL;
    m_pStats                    = NULL;
    m_bReadingLeaderboard       = FALSE;

    m_nGameType                 = X_CONTEXT_GAME_TYPE_STANDARD;
    m_nGameMode                 = 0;

    m_pQoSResult                = &m_QoSResult;
    m_bQoSTesting               = FALSE;

    ZeroMemory( &m_Local, sizeof( LocalClientInfo ) );

    // Initialize the leaderboard spec
    m_Spec.dwNumColumnIds = 3;
    m_Spec.rgwColumnIds[ 0 ] = STATS_COLUMN_RANKED_GAMES_GAMES_PLAYED;
    m_Spec.rgwColumnIds[ 1 ] = STATS_COLUMN_RANKED_GAMES_POINTS_SCORED;
    m_Spec.rgwColumnIds[ 2 ] = STATS_COLUMN_RANKED_GAMES_LAST_MAP;

    // Set up the default notification position
    ChangeNotificationPosition();

    // Retrieve our own machine ID
    XNADDR xnaddr;

    while( XNetGetTitleXnAddr( &xnaddr ) == XNET_GET_XNADDR_PENDING )
        ;

    memcpy_s( &m_Local.xnaddr, sizeof( XNADDR ), &xnaddr, sizeof( XNADDR ) );

    XNetXnAddrToMachineId( &xnaddr, &m_Local.id );

    swprintf_s( m_wszLastSessionError, L"No session errors detected\n" );
    
    // Set initial app states
    m_AppState                = APPSTATE_MAINMENU;
    m_AppStateBeforeMigration = APPSTATE_MAINMENU;

    // Set our custom actions for subscribing/unsubscribing to a player's presence
    XCustomSetAction( CUSTOM_ACTION_SUBSCRIBE_PRESENCE, L"Subscribe Presence", 0 );
    XCustomSetAction( CUSTOM_ACTION_UNSUBSCRIBE_PRESENCE, L"Unsubscribe Presence", 0 );
    
    // Pre-populate signin state of all users
    ZeroMemory( m_SignInInfo, sizeof(XUSER_SIGNIN_INFO) * MAX_USER_COUNT );

    for( UINT i = 0; i < MAX_USER_COUNT; ++i )
    {
        XUserGetSigninInfo( i, 0, &m_SignInInfo[ i ] );
    }

    const BOOL bConsiderPreviousSigninState = FALSE;
    HandleSigninChanges( bConsiderPreviousSigninState );

    return S_OK;
}

#ifdef LIVE_ON_WINDOWS
//--------------------------------------------------------------------------------------
// Device created
//--------------------------------------------------------------------------------------
HRESULT Sample::DeviceCreated()
{
    HRESULT hr;
    if( SUCCEEDED( hr = m_CXPlat_XOnline.OnDeviceCreated() ) )
    {
        hr = m_CXPlat_Draw.CreateBackground();
    }
    return hr;
}
#endif

#ifdef LIVE_ON_WINDOWS
//--------------------------------------------------------------------------------------
// Device destroyed
//--------------------------------------------------------------------------------------
VOID Sample::DeviceDestroyed()
{
    m_CXPlat_XOnline.OnDeviceDestroyed();
    m_CXPlat_Draw.DestroyBackground();
}
#endif


//--------------------------------------------------------------------------------------
// Name: SetLastXSessionError()
// Desc: Set last error from an XSession call
//--------------------------------------------------------------------------------------
BOOL Sample::SetLastXSessionError( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped )
{
    if( pSessionMgr == NULL )
    {
        swprintf_s( m_wszLastSessionError, L"\n" );

        return FALSE;
    }

    DWORD dwRet = XGetOverlappedExtendedError( pXOverlapped );
    HRESULT hr = HRESULT_FROM_WIN32( dwRet );
    const __int64 sessionIDAsInt = pSessionMgr->GetSessionIDAsInt();

    if( hr == S_OK )
    {
        if( pSessionMgr->GetSessionState() == SessionStateDeleted )
        {

            swprintf_s( m_wszLastSessionError, L"\n" );
        }
        else
        {

            swprintf_s( m_wszLastSessionError, L"Session %016I64X has no errors\n", 
                        sessionIDAsInt );
        }
        return FALSE;
    }

    swprintf_s( m_wszLastSessionError, L"Session %016I64X generated error 0x%08x\n", 
                sessionIDAsInt, hr );

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: HandleSigninChanges()
// Desc: Handle signin changes
//--------------------------------------------------------------------------------------
VOID Sample::HandleSigninChanges( const BOOL bConsiderPreviousSigninState )
{
    // Cache old signin info 
    XUSER_SIGNIN_INFO oldSignInInfo[MAX_USER_COUNT];
    memcpy( oldSignInInfo, m_SignInInfo, MAX_USER_COUNT * sizeof( XUSER_SIGNIN_INFO ) );

    // Retrieve new signin info
    for( UINT i = 0; i < MAX_USER_COUNT; ++i )
    {
        XUserGetSigninInfo( i, 0, &m_SignInInfo[ i ] );
    }

    //
    // Process any changes in signin
    //
    if( bConsiderPreviousSigninState && ( memcmp( oldSignInInfo, m_SignInInfo, MAX_USER_COUNT * sizeof( XUSER_SIGNIN_INFO ) ) == 0 ) )
    {
        return;
    }

    //
    // Changes detected. Let's go through each player slot one by one
    //

    // Index of locally signed-in player
    UINT idx = 0;

    // Locally active sessions
    SessionManager* arSessionManagerInstances[] =
    {
        GetMatchmakingSession(),
        GetPresenceSession()
    };

    // Track whether we lost connectivity to LIVE
    BOOL bLostConnectivityToLive = FALSE;

    // For each player slot..
    for( UINT i = 0; i < MAX_USER_COUNT; ++i )
    {
        const XUSER_SIGNIN_INFO& newinfo = m_SignInInfo[ i ];
        const XUSER_SIGNIN_INFO& oldinfo = oldSignInInfo[ i ];
        if( ( newinfo.xuid != INVALID_XUID ) && ( newinfo.UserSigninState != eXUserSigninState_NotSignedIn ) )
        {
            // Signed-in player. Store their info in m_Local
            m_Local.nController[ idx ] = i;
            m_Local.xuids[ idx ]       = newinfo.xuid;
            MultiByteToWideChar( CP_ACP, 0, newinfo.szUserName, -1, m_Local.strGamertags[ idx ], XUSER_NAME_SIZE );

            // If this index is now signed into LIVE but wasn't before, enumerate presence for it
            if( ( newinfo.UserSigninState == eXUserSigninState_SignedInToLive ) && 
                ( oldinfo.UserSigninState != eXUserSigninState_SignedInToLive ) )
            {
                SchedulePresenceEnumTasks( TRUE, i );
            }

            ++idx;
        }

        if( newinfo.UserSigninState == eXUserSigninState_NotSignedIn )
        {
            //
            // Signed-out player. Is this a session owner that signed out? 
            // If so, the system will have deleted the session during signout, and
            // we just clean up our SessionManager instance here.
            for( UINT iXS = 0; iXS < _countof( arSessionManagerInstances ); ++iXS )
            {
                if( arSessionManagerInstances[iXS] )
                {
                    const XUID ownerXuid = arSessionManagerInstances[iXS]->GetSessionOwnerXuid();
                    if( ownerXuid == oldinfo.xuid )
                    {
                        DebugSpew( "HandleSigninChanges: Session %016I64X owner signed out."
                                   "Cleaning up session manager instance\n", 
                                   arSessionManagerInstances[iXS]->GetSessionIDAsInt() );

                        ScheduleSessionMgrDeletionTasks( arSessionManagerInstances[iXS] );
                    }
                }
            }
        }

        // Check for previous LIVE connectivity
        if( ( newinfo.UserSigninState == eXUserSigninState_SignedInLocally ) &&
            ( oldinfo.UserSigninState == eXUserSigninState_SignedInToLive ) )
        {
            //
            // This player is now signed in locally but was signed into LIVE before, so
            // we must have lost connectivity to LIVE. If this player is in a session, 
            // that session is no longer valid, so we'll have to delete it
            for( UINT iXS = 0; iXS < _countof( arSessionManagerInstances ); ++iXS )
            {
                if( arSessionManagerInstances[iXS] )
                {
                    if( arSessionManagerInstances[iXS]->IsPlayerInSession( oldinfo.xuid ) )
                    {
                        DebugSpew( "HandleSigninChanges: Session %016I64X player no longer signed into LIVE."
                                   "LIVE connectivity loss detected, so deleting all local sessions\n", 
                                   arSessionManagerInstances[iXS]->GetSessionIDAsInt() );

                        bLostConnectivityToLive = TRUE;
                        break;
                    }
                }
            }
        }
    }

    // Cache the count of locally signed-in players
    m_Local.cPlayers = idx;

    if( bLostConnectivityToLive )
    {
        DeleteLocalSessions();
    }
}

//--------------------------------------------------------------------------------------
// Name: GetFreeTaskSchedulerTaskData()
// Desc: Returns a pointer to a free TaskSchedulerTaskData struct in our m_pTaskSchedulerTaskDataStructs array
//       that we can use for task scheduling
//--------------------------------------------------------------------------------------
Sample::TaskSchedulerTaskData* Sample::GetFreeTaskSchedulerTaskData()
{
    if( m_stackFreeTaskSchedulerTaskDataIndices.empty() )
    {
        FatalError( "Sample::GetFreeTaskSchedulerTaskData: No more handles!\n" );
    }

    const UINT index = m_stackFreeTaskSchedulerTaskDataIndices.top();
    m_stackFreeTaskSchedulerTaskDataIndices.pop();

    // Explicit call to constructor
    TaskSchedulerTaskData* pTaskData = &m_pTaskSchedulerTaskDataStructs[ index ];
    pTaskData->TaskSchedulerTaskData::TaskSchedulerTaskData();
    
    // Set index
    pTaskData->m_index = index;

    return &m_pTaskSchedulerTaskDataStructs[ index ];
}

//--------------------------------------------------------------------------------------
// Name: ReleaseTaskSchedulerTaskData()
// Desc: Release a TaskSchedulerTaskData struct for use in a future scheduled task
//--------------------------------------------------------------------------------------
VOID Sample::ReleaseTaskSchedulerTaskData( const TaskSchedulerTaskData* pData )
{
    m_stackFreeTaskSchedulerTaskDataIndices.push( pData->m_index );
}

//--------------------------------------------------------------------------------------
// Name: CancelAllPendingXSessionTasksForSession()
// Desc: Cancel all pending XSession tasks for the specified session
//--------------------------------------------------------------------------------------
VOID Sample::CancelAllPendingXSessionTasksForSession( SessionManager* pSessionMgr )
{
    if( !pSessionMgr )
    {
        return;
    }

    std::deque<TASKHANDLE>::iterator iter;
    for( iter = m_listSessionTaskGroupHandles.begin(); iter != m_listSessionTaskGroupHandles.end(); )
    {
        TASKHANDLE& hTaskGroup = (*iter);

        HRESULT hr = m_TaskScheduler.ReleaseTaskGroupEx( &hTaskGroup, ReleaseTaskCallback, pSessionMgr ); 
        if( hr == S_OK )
        {
            DebugSpew( "Sample::CancelAllPendingXSessionTasksForSession - erasing task group at index: %d \n", 
		               hTaskGroup.m_info.m_index );

            // All tasks associated with the task group are released. Removed the taskgroup handle
            // from our m_listSessionTaskGroupHandles
            iter = m_listSessionTaskGroupHandles.erase( iter );
        }
        else
        {
            ++iter;
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: ScheduleTasks()
// Desc: Helper function to schedule tasks given an array of tasks and the APPSTATE they
//       execute in
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleTasks( const WORD* pTaskIDs, 
                            const APPSTATE* pAppStates, 
                            const UINT cTasks, 
                            SessionManager* pSessionMgr,
                            void* pContext )
{
    if( !cTasks || !pTaskIDs || !pAppStates )
    {
        return;
    }

    TASKHANDLE hPrev = {0};
    TASKHANDLE hGroup = m_TaskScheduler.CreateTaskGroup();

    DebugSpew( "Sample::ScheduleTasks - new task group at index: %u\n", 
	           hGroup.m_info.m_index );

    // Check if this is a TASKAREA_XSESSION task. If so, add this task group to our 
    // m_listSessionTaskGroupHandles list and make the first task in this task
    // group dependent on the previous task group in the list. This is an effective
    // way to ensure that all session tasks are serialized.
    if( TASK_AREA( pTaskIDs[0] ) == TASKAREA_XSESSION )
    {
        if( m_listSessionTaskGroupHandles.size() )
        {
            // Get current last handle in the list
            TASKHANDLE hLast = m_listSessionTaskGroupHandles.back();
            
            // Add this group handle to the end of the list
            m_listSessionTaskGroupHandles.push_back( hGroup );

            // Make the first task in this group dependent on hLast
            hPrev = hLast;
        }
        else
        {
            // First task group in the list. Add it.
            m_listSessionTaskGroupHandles.push_back( hGroup );
        }
    }
            
    for( UINT i = 0; i < cTasks; ++i )
    {
        TaskSchedulerTaskData* pTaskData = GetFreeTaskSchedulerTaskData();

        pTaskData->m_wTaskID        = pTaskIDs[i];
        pTaskData->m_pSample        = this;
        pTaskData->m_pSessionMgr    = pSessionMgr;

        // If the first task is asynchronous, then use an overlapped for the task group
        if( TASK_ASYNC( pTaskIDs[0] ) )
        {
            pTaskData->m_pXOverlapped = &m_XSessionOverlapped;
        }
        else if( pTaskIDs[0] == TASK_XOVERLAPPED_CANCEL )
        {
            // If this is a TASK_XOVERLAPPED_CANCEL task, then pContext is a pointer to the XOVERLAPPED that we
            // want to cancel
            pTaskData->m_pXOverlapped = (XOVERLAPPED*)pContext;
        }
        else
        {
            pTaskData->m_pXOverlapped   = NULL;
        }

        pTaskData->m_newAppState    = pAppStates[i];

        //
        // Read in task-specific context
        //
        if( TASK_AREA( pTaskIDs[i] ) == TASKAREA_XSESSION )
        {
            switch( pTaskIDs[i] )
            {
                case TASK_XSESSION_MODIFY:
                {
                    const ModifyFlagsData* pModifyFlagsData = ( const ModifyFlagsData* )pContext;
                    pTaskData->m_ModifyFlagsData = *pModifyFlagsData; 
                    break;
                }
                case TASK_XSESSION_JOIN:
                case TASK_XSESSION_LEAVE:
                {
                    const JoinLeaveData* pJoinLeaveData = ( const JoinLeaveData* )pContext;
                    pTaskData->m_JoinLeaveData = *pJoinLeaveData;
                    break;
                }
            }
        }
        else if( TASK_AREA( pTaskIDs[i] ) == TASKAREA_NETWORKMESSAGE )
        {
            const NetMsgData* pNetMsgData = ( const NetMsgData* )pContext;
            pTaskData->m_NetMsgData.m_pMsg        = pNetMsgData->m_pMsg;
            pTaskData->m_NetMsgData.m_inaddrFrom  = pNetMsgData->m_inaddrFrom;
            pTaskData->m_NetMsgData.m_addrFrom    = pNetMsgData->m_addrFrom;
        }
        else if( TASK_AREA( pTaskIDs[i] ) == TASKAREA_PRESENCE )
        {
            const EnumPresenceData* pEnumPresenceData = ( const EnumPresenceData* )pContext;
            pTaskData->m_EnumPresenceData = *pEnumPresenceData;
        }
        else if( TASK_AREA( pTaskIDs[i] ) == TASKAREA_CONTEXT )
        {
            const UserContextData* pUserContextData = ( const UserContextData* )pContext;
            pTaskData->m_UserContextData = *pUserContextData;
        }

        // AddRef our task data instance
        pTaskData->AddRef();

        TASKHANDLE hTask = m_TaskScheduler.ScheduleTask( pTaskIDs[i], hGroup, hPrev, (PFNTASKHANDLER)Sample::DoTaskCallback, pTaskData );

        DebugSpew( "Sample::ScheduleTasks - taskID: 0x%x; task index: %u; pTaskData: 0x%p; pTaskData->m_NetMsgData.m_pMsg: 0x%p\n", 
                   pTaskIDs[i], hTask.m_info.m_index, pTaskData, pTaskData->m_NetMsgData.m_pMsg );

        DebugSpew( "Sample::ScheduleTasks - task index: %u dependent on task index: %u; bGroup: %d \n", 
		           hTask.m_info.m_index, hPrev.m_info.m_index, hPrev.m_info.m_bIsTaskGroup );

        hPrev = hTask;
    }

    // For session tasks, schedule a final TASK_XSESSION_TASKGROUPDONE task to remove this task group
    // from our m_listSessionTaskGroupHandles list
    if( TASK_AREA( pTaskIDs[0] ) == TASKAREA_XSESSION )
    {
        TaskSchedulerTaskData* pTaskData = GetFreeTaskSchedulerTaskData();
        pTaskData->m_wTaskID             = TASK_XSESSION_TASKGROUPDONE;
        pTaskData->m_pSample             = this;
        pTaskData->m_pSessionMgr         = pSessionMgr;

        pTaskData->AddRef();

        TASKHANDLE hTask = m_TaskScheduler.ScheduleTask( TASK_XSESSION_TASKGROUPDONE, 
                                                          hGroup, 
                                                          hPrev, 
                                                          (PFNTASKHANDLER)Sample::DoTaskCallback, 
                                                          pTaskData );

        DebugSpew( "Sample::ScheduleTasks - taskID: TASK_XSESSION_TASKGROUPDONE; task index: %u; pTaskData: 0x%p\n", 
                   hTask.m_info.m_index, pTaskData );

        DebugSpew( "Sample::ScheduleTasks - task index: %u dependent on task index: %u; bGroup: %d \n", 
		           hTask.m_info.m_index, hPrev.m_info.m_index, hPrev.m_info.m_bIsTaskGroup );
    }
}


//--------------------------------------------------------------------------------------
// Name: ScheduleCancelOverlappedTask()
// Desc: Helper function to schedule a task to cancel an asynchronous operation that takes
//       an XOVERLAPPED
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleCancelOverlappedTask()
{
    // Determine which overlapped operation is underway, and for which SessionManager instance
    SessionManager* pSessionMgr = ( m_AppState == APPSTATE_MAINMENU ) ? GetPresenceSession() : GetMatchmakingSession();
    XOVERLAPPED* pXOverlapped = &m_XSessionOverlapped;

    WORD taskIDs[] =
    {
        TASK_XOVERLAPPED_CANCEL,
        TASK_APP_CHANGE_STATE,
        TASK_XOVERLAPPED_CANCEL_DONE,
        TASK_APP_CHANGE_STATE,
    };

    APPSTATE appStates[4];
    appStates[0] = m_AppState;
    appStates[1] = APPSTATE_CANCELLING_OVERLAPPED;

    // Return to the APPSTATE prior to the overlapped operation
    switch( m_AppState )
    {
    case APPSTATE_SEARCH_MATCHMAKING:
        appStates[2] = APPSTATE_SEARCHUI_MATCHMAKING;
        break;
    case APPSTATE_CREATING_SESSION:
        appStates[2] = APPSTATE_MAINMENU;
        break;
    case APPSTATE_STARTING:
        appStates[2] = ( pSessionMgr->HasSessionFlags( XSESSION_CREATE_USES_ARBITRATION ) ) ? APPSTATE_REGISTERED : APPSTATE_PREGAME;
        break;
    case APPSTATE_ENDING:
        appStates[2] = APPSTATE_INGAME;
        break;
    case APPSTATE_DELETING_SESSION:
        appStates[2] = ( pSessionMgr == GetMatchmakingSession() ) ? APPSTATE_POSTGAME : APPSTATE_MAINMENU;
        break;
    default:
        appStates[2] = APPSTATE_MAINMENU;
        break;
    }

    appStates[3] = appStates[2];

    ScheduleTasks( taskIDs, appStates, 4, pSessionMgr, (void*)pXOverlapped );
}

//--------------------------------------------------------------------------------------
// Name: RescheduleTask()
// Desc: Helper function to reschedule a scheduled task
//--------------------------------------------------------------------------------------
VOID Sample::RescheduleTask( const WORD taskID, const TASKHANDLE* pHandle )
{
    if( !pHandle )
    {
        return;
    }

    DebugSpew( "Sample::RescheduleTask - taskID: 0x%x; pHandle: 0x%p\n", 
               taskID, pHandle );

    m_TaskScheduler.RescheduleTask( pHandle );
}

//--------------------------------------------------------------------------------------
// Name: ScheduleNetMsgTask()
// Desc: Helper function to schedule a task in response to a network message
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleNetMsgTask( const WORD taskID, const CMessage* pMsg, IN_ADDR& inAddrFrom )
{
    const APPSTATE appState = m_AppState;

    NetMsgData netMsgData;
    netMsgData.m_pMsg       = const_cast<CMessage*>(pMsg);
    netMsgData.m_inaddrFrom = inAddrFrom;

    // Make IN_ADDR to XNADDR
    const XNKID& sessionID = pMsg->GetSessionID();
    XNetInAddrToXnAddr( inAddrFrom, &netMsgData.m_addrFrom, const_cast<XNKID*>(&sessionID) );

    // Schedule task
    ScheduleTasks( &taskID, &appState, 1, NULL, &netMsgData );
}

//--------------------------------------------------------------------------------------
// Name: ScheduleChangeStateTask()
// Desc: Helper function to schedule a task to change the APPSTATE
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleChangeStateTask( const APPSTATE newState, SessionManager* pSessionMgr )
{
    const WORD taskID = TASK_APP_CHANGE_STATE;

    ScheduleTasks( &taskID, &newState, 1, pSessionMgr );
}


//--------------------------------------------------------------------------------------
// Name: ScheduleSessionSearchTasks()
// Desc: Helper function to facilitate session search task scheduling
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleSessionSearchTasks( SessionManager* pSessionMgr )
{
    if( !pSessionMgr )
    {
        return;
    }

    WORD taskIDs[] =
    {
        TASK_XSESSION_SEARCH,
        TASK_APP_CHANGE_STATE,
        TASK_XOVERLAPPED_WAIT,
        TASK_XSESSION_SEARCH_DONE,
        TASK_APP_CHANGE_STATE,
    };

    APPSTATE appStates[5];
    appStates[0] = m_AppState;
    appStates[1] = APPSTATE_SEARCH_MATCHMAKING;
    appStates[2] = appStates[1];
    appStates[3] = APPSTATE_SEARCH_MATCHMAKING_DONE;
    appStates[4] = appStates[3];

    ScheduleTasks( taskIDs, appStates, 5, pSessionMgr );
}

//--------------------------------------------------------------------------------------
// Name: SchedulePresenceEnumTasks()
// Desc: Helper function to facilitate presence enumeration tasks
//--------------------------------------------------------------------------------------
VOID Sample::SchedulePresenceEnumTasks( const BOOL bEnumFriends, const DWORD dwUserIndex )
{
    WORD taskIDs[4];
    taskIDs[0] = ( bEnumFriends ) ? TASK_PRESENCE_ENUMFRIENDS : TASK_PRESENCE_ENUM;
    taskIDs[1] = TASK_APP_CHANGE_STATE;
    taskIDs[2] = ( bEnumFriends ) ? TASK_PRESENCE_ENUMFRIENDS_DONE : TASK_PRESENCE_ENUM_DONE;
    taskIDs[3] = TASK_APP_CHANGE_STATE;

    APPSTATE appStates[4];
    appStates[0] = m_AppState;
    appStates[1] = APPSTATE_ENUMERATING_PRESENCE;

    // Determine app state appStates[2] after presence enumeration
    SessionManager* pMatchmakingSessionMgr = GetMatchmakingSession();
    if( pMatchmakingSessionMgr )
    {
        switch( pMatchmakingSessionMgr->GetSessionState() )
        {
        case SessionStateCreated:
            appStates[2] = APPSTATE_PREGAME;
            break;
        case SessionStateInGame:
            appStates[2] = APPSTATE_INGAME;
            break;
        case SessionStateEnding:
        case SessionStateEnded:
            appStates[2] = APPSTATE_POSTGAME;
            break;
        case SessionStateDeleted:
            appStates[2] = APPSTATE_MAINMENU;
            break;
        default:
            appStates[2] = m_AppState;
        }
    }
    else
    {
        appStates[2] = APPSTATE_MAINMENU;
    }

    appStates[3] = appStates[2];

    EnumPresenceData data;
    data.m_dwUserIndex = dwUserIndex;
    ScheduleTasks( taskIDs, appStates, 4, NULL, &data );
}

//--------------------------------------------------------------------------------------
// Name: ScheduleUserContextUpdateTasks()
// Desc: Helper function to facilitate updating user context
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleUserContextUpdateTasks( const DWORD dwUserIndex, const DWORD dwContextID, const DWORD dwContextValue )
{
    WORD taskIDs[] =
    {
        TASKAREA_CONTEXT_UPDATE_RP,
        TASK_APP_CHANGE_STATE,
        TASKAREA_CONTEXT_UPDATE_RP_DONE,
        TASK_APP_CHANGE_STATE
    };

    APPSTATE appStates[4];
    appStates[0] = m_AppState;
    appStates[1] = APPSTATE_UPDATING_RICHPRESENCE;

    // Determine app state appStates[2] after the context update
    SessionManager* pMatchmakingSessionMgr = GetMatchmakingSession();
    if( pMatchmakingSessionMgr )
    {
        switch( pMatchmakingSessionMgr->GetSessionState() )
        {
        case SessionStateCreated:
            appStates[2] = APPSTATE_PREGAME;
            break;
        case SessionStateInGame:
            appStates[2] = APPSTATE_INGAME;
            break;
        case SessionStateEnded:
            appStates[2] = APPSTATE_POSTGAME;
            break;
        case SessionStateDeleted:
            appStates[2] = APPSTATE_MAINMENU;
            break;
        }
    }
    else
    {
        appStates[2] = APPSTATE_MAINMENU;
    }

    appStates[3] = appStates[2];

    UserContextData data;
    data.m_dwUserIndex      = dwUserIndex;
    data.m_dwContextID      = dwContextID;
    data.m_dwContextValue   = dwContextValue;
    ScheduleTasks( taskIDs, appStates, 4, NULL, &data );
}

//--------------------------------------------------------------------------------------
// Name: ScheduleSessionCreationTasks()
// Desc: Helper function to facilitate session creation task scheduling
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleSessionCreationTasks( SessionManager* pSessionMgr )
{
    if( !pSessionMgr )
    {
        return;
    }

    WORD taskIDs[] =
    {
        TASK_XSESSION_CREATE,
        TASK_APP_CHANGE_STATE,
        TASK_XOVERLAPPED_WAIT,
        TASK_XSESSION_CREATED,
    };

    APPSTATE appStates[4];
    appStates[0] = m_AppState;
    appStates[1] = APPSTATE_CREATING_SESSION;
    appStates[2] = appStates[1];

    // Determine app state appStates[3] after the session has been created
    SessionManager* pMatchmakingSessionMgr = GetMatchmakingSession();
    if( pMatchmakingSessionMgr )
    {
        switch( pMatchmakingSessionMgr->GetSessionState() )
        {
        case SessionStateCreated:
            appStates[3] = APPSTATE_PREGAME;
            break;
        case SessionStateRegistered:
            appStates[3] = APPSTATE_REGISTERED;
            break;
        case SessionStateStarting:
            appStates[3] = APPSTATE_STARTING;
            break;
        case SessionStateInGame:
            appStates[3] = APPSTATE_INGAME;
            break;
        case SessionStateEnded:
            appStates[3] = APPSTATE_POSTGAME;
            break;
        case SessionStateDeleted:
            appStates[3] = APPSTATE_MAINMENU;
            break;
        }
    }
    else
    {
        appStates[3] = APPSTATE_MAINMENU;
    }
    
    ScheduleTasks( taskIDs, appStates, 4, pSessionMgr );
}

//--------------------------------------------------------------------------------------
// Name: ScheduleSessionStartTasks()
// Desc: Helper function to facilitate session start task scheduling
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleSessionStartTasks( SessionManager* pSessionMgr )
{
    if( !pSessionMgr )
    {
        return;
    }

    WORD taskIDs[] =
    {
        TASK_XSESSION_START,
        TASK_APP_CHANGE_STATE,
        TASK_XOVERLAPPED_WAIT,
        TASK_XSESSION_STARTED,
        TASK_APP_CHANGE_STATE,
    };

    APPSTATE appStates[5];
    appStates[0] = m_AppState;
    appStates[1] = APPSTATE_STARTING;
    appStates[2] = appStates[1];
    appStates[3] = APPSTATE_INGAME;
    appStates[4] = appStates[3];

    ScheduleTasks( taskIDs, appStates, 5, pSessionMgr );
}

//--------------------------------------------------------------------------------------
// Name: ScheduleSessionEndTasks()
// Desc: Helper function to facilitate session end task scheduling
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleSessionEndTasks( SessionManager* pSessionMgr )
{
    if( !pSessionMgr )
    {
        return;
    }

    WORD taskIDs[] =
    {
        TASK_XSESSION_END,
        TASK_APP_CHANGE_STATE,
        TASK_XOVERLAPPED_WAIT,
        TASK_XSESSION_ENDED,
        TASK_APP_CHANGE_STATE,
    };

    APPSTATE appStates[5];
    appStates[0] = m_AppState;
    appStates[1] = APPSTATE_ENDING;
    appStates[2] = appStates[1];
    appStates[3] = ( GetMatchmakingSession() ) ? APPSTATE_POSTGAME : APPSTATE_MAINMENU;
    appStates[4] = appStates[3];

    ScheduleTasks( taskIDs, appStates, 5, pSessionMgr );
}

//--------------------------------------------------------------------------------------
// Name: ScheduleSessionWriteTasks()
// Desc: Helper function to facilitate session stats writes task scheduling
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleSessionWriteTasks( SessionManager* pSessionMgr )
{
    if( !pSessionMgr )
    {
        return;
    }

    WORD taskIDs[] =
    {
        TASK_XSESSION_WRITESTATS,
        TASK_APP_CHANGE_STATE,
        //TASK_XOVERLAPPED_WAIT,
        TASK_XSESSION_WROTESTATS,
        TASK_APP_CHANGE_STATE
    };

    APPSTATE appStates[4];
    appStates[0] = m_AppState;
    appStates[1] = APPSTATE_WRITINGSTATS;
//    appStates[2] = appStates[1];
    appStates[2] = ( pSessionMgr->HasSessionFlags( XSESSION_CREATE_USES_MATCHMAKING ) ) ? m_AppState : APPSTATE_MAINMENU;
    appStates[3] = appStates[2];

    ScheduleTasks( taskIDs, appStates, 4, pSessionMgr );
}

//--------------------------------------------------------------------------------------
// Name: ScheduleSessionWriteTasks()
// Desc: Helper function to facilitate session stats writes task scheduling
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleSessionWriteTasks( SessionManager* pSessionMgr, 
                                        ClientInfo* pClient, 
                                        const DWORD userMask )
{
    if( !pSessionMgr )
    {
        return;
    }

    // $todo
}


//--------------------------------------------------------------------------------------
// Name: ScheduleSessionDeletionTasks()
// Desc: Helper function to facilitate session deletion task scheduling
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleSessionDeletionTasks( SessionManager* pSessionMgr )
{
    if( !pSessionMgr )
    {
        return;
    }

    WORD taskIDs[] =
    {
        TASK_XSESSION_DELETE,
        TASK_APP_CHANGE_STATE,
        TASK_XOVERLAPPED_WAIT,
        TASK_XSESSION_DELETED,
    };

    APPSTATE appStates[4];
    appStates[0] = m_AppState;
    appStates[1] = APPSTATE_DELETING_SESSION;
    appStates[2] = appStates[1];
    appStates[3] = APPSTATE_MAINMENU;

    ScheduleTasks( taskIDs, appStates, 4, pSessionMgr );
}

//--------------------------------------------------------------------------------------
// Name: ScheduleSessionMgrDeletionTasks()
// Desc: Helper function to facilitate Session Manager deletion task scheduling
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleSessionMgrDeletionTasks( SessionManager* pSessionMgr )
{
    if( !pSessionMgr )
    {
        return;
    }

    WORD taskIDs[] =
    {
        TASK_XSESSION_DELETEMGR,
        TASK_APP_CHANGE_STATE,
    };

    APPSTATE appStates[2];
    appStates[0] = m_AppState;
    appStates[1] = APPSTATE_MAINMENU;

    ScheduleTasks( taskIDs, appStates, 2, pSessionMgr );
}

//--------------------------------------------------------------------------------------
// Name: ScheduleSessionConnectionTasks()
// Desc: Helper function to facilitate connection to the session host
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleSessionConnectionTasks( SessionManager* pSessionMgr )
{
    if( !pSessionMgr )
    {
        return;
    }

    WORD taskIDs[] =
    {
        TASK_XSESSION_CONNECTING,
        TASK_APP_CHANGE_STATE
    };

    APPSTATE appStates[2];
    appStates[0] = m_AppState;
    appStates[1] = APPSTATE_CONNECTING_SESSION;

    ScheduleTasks( taskIDs, appStates, 2, pSessionMgr );
}

//--------------------------------------------------------------------------------------
// Name: ScheduleSessionModifyTasks()
// Desc: Helper function to facilitate modifying session flags
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleSessionMigrationTasks( SessionManager* pSessionMgr )
{
    if( !pSessionMgr )
    {
        return;
    }

    WORD taskIDs[] =
    {
        TASK_XSESSION_MIGRATE,
        TASK_APP_CHANGE_STATE, // APPSTATE_HOSTMIGRATION
        TASK_XSESSION_MIGRATING,
        TASK_XOVERLAPPED_WAIT,
        TASK_XSESSION_MIGRATED,
        TASK_APP_CHANGE_STATE // return to current state
    };

    APPSTATE appStates[6];
    appStates[0] = m_AppState;
    appStates[1] = APPSTATE_HOSTMIGRATION;
    appStates[2] = appStates[1];
    appStates[3] = appStates[1];
    appStates[4] = m_AppState; // return to same APPSTATE before migration began
    appStates[5] = appStates[4];

    ScheduleTasks( taskIDs, appStates, 6, pSessionMgr );
}

//--------------------------------------------------------------------------------------
// Name: ScheduleSessionHostRegisterTasks()
// Desc: Helper function to facilitate the host begin the process of registering itself
//       and clients for arbitration
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleSessionHostRegisterTasks( SessionManager* pSessionMgr )
{
    if( !pSessionMgr )
    {
        return;
    }

    WORD taskIDs[] =
    {
        TASK_XSESSION_REGISTER_HOST_BEGIN,
        TASK_APP_CHANGE_STATE, // APPSTATE_WAITINGFORREGISTRATION 
        TASK_XSESSION_REGISTER_HOST_WAIT,
    };

    APPSTATE appStates[3];
    appStates[0] = m_AppState;
    appStates[1] = APPSTATE_WAITINGFORREGISTRATION;
    appStates[2] = appStates[1];

    ScheduleTasks( taskIDs, appStates, 3, pSessionMgr );
}

//--------------------------------------------------------------------------------------
// Name: ScheduleSessionRegisterTasks()
// Desc: Helper function to facilitate registering a session for arbitration
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleSessionRegisterTasks( SessionManager* pSessionMgr )
{
    if( !pSessionMgr )
    {
        return;
    }

    WORD taskIDs[] =
    {
        TASK_XSESSION_REGISTER,
        TASK_APP_CHANGE_STATE,
        TASK_XOVERLAPPED_WAIT,
        TASK_XSESSION_REGISTERED,
        TASK_APP_CHANGE_STATE
    };

    APPSTATE appStates[5];
    appStates[0] = m_AppState;
    appStates[1] = APPSTATE_REGISTERING;
    appStates[2] = appStates[1];
    appStates[3] = APPSTATE_REGISTERED;
    appStates[4] = appStates[3];

    ScheduleTasks( taskIDs, appStates, 5, pSessionMgr );
}

//--------------------------------------------------------------------------------------
// Name: ScheduleSessionModifyTasks()
// Desc: Helper function to facilitate modifying session flags
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleSessionModifyTasks( SessionManager* pSessionMgr, const DWORD dwFlags, const BOOL bClearFlags )
{
    if( !pSessionMgr )
    {
        return;
    }

    const WORD taskID = TASK_XSESSION_MODIFY;
    const APPSTATE appState = m_AppState;

    ModifyFlagsData data;
    data.m_dwFlags = dwFlags;
    data.m_bClearFlags = bClearFlags;

    ScheduleTasks( &taskID, &appState, 1, pSessionMgr, (void*)&data );
}

//--------------------------------------------------------------------------------------
// Name: ScheduleSessionJoinTasks()
// Desc: Helper function to facilitate session join task scheduling
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleSessionJoinTasks( SessionManager* pSessionMgr, 
                                       ClientInfo* pClient, 
                                       const DWORD dwUserMask )
{
    if( !pSessionMgr || !dwUserMask )
    {
        return;
    }

    WORD taskIDs[4];
    taskIDs[0] = TASK_XSESSION_JOIN;
    taskIDs[1] = TASK_APP_CHANGE_STATE;
    taskIDs[2] = TASK_XOVERLAPPED_WAIT;    
    taskIDs[3] = ( pClient == &m_Local ) ? TASK_XSESSION_JOINED_LOCAL : TASK_XSESSION_JOINED_REMOTE;

    APPSTATE appStates[4];
    appStates[0] = m_AppState;
    appStates[1] = ( pClient == &m_Local ) ? APPSTATE_ADDING_LOCAL_PLAYERS : APPSTATE_ADDING_REMOTE_PLAYERS;
    appStates[2] = appStates[1];
    appStates[3] = ( pClient == &m_Local ) ? APPSTATE_ADDED_LOCAL_PLAYERS : APPSTATE_ADDED_REMOTE_PLAYERS;

    JoinLeaveData data;
    data.m_pClient = pClient;
    data.m_dwUserMask = dwUserMask;

    ScheduleTasks( taskIDs, appStates, 4, pSessionMgr, &data );
}

//--------------------------------------------------------------------------------------
// Name: ScheduleSessionLeaveTasks()
// Desc: Helper function to facilitate session leave task scheduling
//--------------------------------------------------------------------------------------
VOID Sample::ScheduleSessionLeaveTasks( SessionManager* pSessionMgr, 
                                        ClientInfo* pClient, 
                                        const DWORD dwUserMask )
{
    if( !pSessionMgr || !dwUserMask )
    {
        return;
    }

    WORD taskIDs[4];
    taskIDs[0] = TASK_XSESSION_LEAVE;
    taskIDs[1] = TASK_APP_CHANGE_STATE;
    taskIDs[2] = TASK_XOVERLAPPED_WAIT;    
    taskIDs[3] = ( pClient == &m_Local ) ? TASK_XSESSION_LEFT_LOCAL : TASK_XSESSION_LEFT_REMOTE;

    APPSTATE appStates[4];
    appStates[0] = m_AppState;
    appStates[1] = ( pClient == &m_Local ) ? APPSTATE_REMOVING_LOCAL_PLAYERS : APPSTATE_REMOVING_REMOTE_PLAYERS;
    appStates[2] = appStates[1];
    appStates[3] = ( pClient == &m_Local ) ? APPSTATE_REMOVED_LOCAL_PLAYERS : APPSTATE_REMOVED_REMOTE_PLAYERS;

    JoinLeaveData data;
    data.m_pClient = pClient;
    data.m_dwUserMask = dwUserMask;

    ScheduleTasks( taskIDs, appStates, 4, pSessionMgr, &data );
}

//--------------------------------------------------------------------------------------
// Name: ShowFriendsList()
// Desc: Show the friends list
//--------------------------------------------------------------------------------------
VOID Sample::ShowFriendsList( UINT nController )
{
    // Start the UI with the appropriate number of users
    DWORD ret = XShowFriendsUI( nController );

    if( ret != ERROR_SUCCESS )
    {
        FatalError( "Unable to launch friends UI, error code %d\n", ret );
    }
}

//--------------------------------------------------------------------------------------
// Name: CheckForLiveNotifications()
// Desc: Handle any LIVE notifications
//--------------------------------------------------------------------------------------
BOOL Sample::CheckForLiveNotifications()
{
    // Check for system notifications
    DWORD dwNotificationID;
    ULONG_PTR ulParam;

    if( XNotifyGetNext( m_hLiveListener, 0, &dwNotificationID, &ulParam ) )
    {
        switch( dwNotificationID )
        {
            case XN_LIVE_INVITE_ACCEPTED:
            {
                XINVITE_INFO InviteInfo;
                if( ERROR_SUCCESS != XInviteGetAcceptedInfo( ulParam, &InviteInfo ) )
                {
                    FatalError( "CheckForLiveNotifications: XInviteGetAcceptedInfo failed. Last error: %d\n", GetLastError() );
                }
                HandleAcceptedInvitation( InviteInfo );
                break;
            }

            #ifdef _XBOX
            case XN_PARTY_MEMBERS_CHANGED:
            {
                DebugSpew( "CheckForAcceptedInvitation: XBox LIVE Party membeship changed. Getting new user list\n" );
                if( XPartyGetUserList( &m_LivePartyUserList ) == XPARTY_E_NOT_IN_PARTY )
                {
                    // Zero memory. Note that this conveniently sets m_LivePartyUserList.dwUserCount to 0 as well
                    ZeroMemory( &m_LivePartyUserList, sizeof( XPARTY_USER_LIST ) );
                }
                break;
            }
            #endif
        }
        
        return TRUE;
    }

    // No notifications have been received
    return FALSE;
}


//--------------------------------------------------------------------------------------
// Name: CheckForCustomAction()
// Desc: Check to see if any custom action was invoked
//--------------------------------------------------------------------------------------    
BOOL Sample::CheckForCustomAction()
{
    // Check for system notifications
    DWORD dwNotificationID;
    ULONG_PTR ulParam;
    BOOL bRet = FALSE;

    if( XNotifyGetNext( m_hCustomListener, 0, &dwNotificationID, &ulParam ) )
    {
        switch( dwNotificationID )
        {
            case XN_CUSTOM_ACTIONPRESSED:
                {
                    // Custom action pressed
                    DWORD dwUserIndex;
                    DWORD dwActionIndex;
                    XUID xuid;

                    if( !XCustomGetLastActionPress( &dwUserIndex, &dwActionIndex, &xuid ) )
                    {
                        FatalError( "XCustomGetLastActionPress returned false!\n" );
                    }

                    DebugSpew( "User index %d invoked custom action %d!\n", dwUserIndex, dwActionIndex );

                    if( ( dwActionIndex == CUSTOM_ACTION_SUBSCRIBE_PRESENCE ) || 
                        ( dwActionIndex == CUSTOM_ACTION_UNSUBSCRIBE_PRESENCE ) )
                    {
                        BOOL bIsXuidOfFriend = FALSE;
                        XUserAreUsersFriends( dwUserIndex, &xuid, 1, &bIsXuidOfFriend, NULL );
                        const BOOL bPresenceSlotFree = ( m_Local.presenceXuids[dwUserIndex].size() < MAX_NON_FRIENDS_SUBSCRIPTIONS ); 

                        BOOL bIsSubscribedXuid = FALSE;
                        UINT iSubscribedXuid = 0;
                        for( iSubscribedXuid = 0; iSubscribedXuid < m_Local.presenceXuids[dwUserIndex].size(); ++iSubscribedXuid )
                        {
                            if( m_Local.presenceXuids[dwUserIndex].at( iSubscribedXuid ) == xuid )
                            {
                                bIsSubscribedXuid = TRUE;
                                break;
                            }
                        }
                    
                        if( dwActionIndex == CUSTOM_ACTION_SUBSCRIBE_PRESENCE )
                        {
                            if( bIsSubscribedXuid )
                            {
                                DebugSpew( "User index %d already subscribed to presence of XUID %016I64X.\n", dwUserIndex, xuid );
                            }
                            else if( !bPresenceSlotFree )
                            {
                                DebugSpew( "User index %d already has maximum number of allowable subscriptions (%d)\n", 
                                           dwUserIndex, MAX_NON_FRIENDS_SUBSCRIPTIONS );
                            }
                            else if( !bIsXuidOfFriend )
                            {
                                DebugSpew( "Subscribing user index %d to presence of XUID %016I64X.\n", dwUserIndex, xuid );

                                if( ERROR_SUCCESS != XPresenceSubscribe( dwUserIndex, 1, &xuid ) )
                                {
                                    FatalError( "Failed to subscribe to presence of XUID %016I64X for user index %d.\n", xuid, dwUserIndex );
                                }

                                m_Local.presenceXuids[dwUserIndex].push_back( xuid );

                                XONLINE_PRESENCE info = {0};
                                m_Local.presenceInfo[dwUserIndex].push_back( info );
                            }
                            else
                            {
                                DebugSpew( "XUID %016I64X is already a friend of user index %d, so no need to subscribe to their presence.\n", 
                                           xuid, dwUserIndex );
                            }
                        }
                        else if( dwActionIndex == CUSTOM_ACTION_UNSUBSCRIBE_PRESENCE )
                        {
                            if( !bIsXuidOfFriend )
                            {
                                DebugSpew( "Unsubscribing user index %d to presence of XUID %016I64X.\n", dwUserIndex, xuid );

                                if( bIsSubscribedXuid )
                                {
                                    std::vector<XUID>::iterator xuidsIter = m_Local.presenceXuids[dwUserIndex].begin();
                                    xuidsIter += iSubscribedXuid;
                                    m_Local.presenceXuids[dwUserIndex].erase( xuidsIter);

                                    std::vector<XONLINE_PRESENCE>::iterator presenceInfoIter = m_Local.presenceInfo[dwUserIndex].begin();
                                    presenceInfoIter += iSubscribedXuid;
                                    m_Local.presenceInfo[dwUserIndex].erase( presenceInfoIter);

                                    if( ERROR_SUCCESS != XPresenceUnsubscribe( dwUserIndex, 1, &xuid ) )
                                    {
                                        FatalError( "Failed to unsubscribe to presence of XUID %016I64X for user index %d.\n", xuid, dwUserIndex );
                                    }
                                }
                            }
                            else
                            {
                                DebugSpew( "XUID %016I64X is a friend of user index %d, so cannot unsubscribe to their presence.\n", 
                                           xuid, dwUserIndex );
                            }
                        }
                    }

                    bRet = TRUE;
                }
                break;
        }
    }

    return bRet;
}

//-------------------------------------------------------------------------------------
// Name:  CheckForFriendsPresenceChanges
// Desc:  Check to see if the presence state of any friends has changed
//-------------------------------------------------------------------------------------
BOOL Sample::CheckForFriendsPresenceChanges()
{
    DWORD dwNotificationID;
    ULONG_PTR ulParam;
    BOOL bRet = FALSE;

    if( XNotifyGetNext( m_hFriendsListener, 0, &dwNotificationID, &ulParam ) )
    {
        switch( dwNotificationID )
        {
            case XN_FRIENDS_PRESENCE_CHANGED:
                {
                    const DWORD dwUserIndex = (DWORD)ulParam;

                    DebugSpew( "Received presence changed notification for friend of user index %d. Refreshing friend presence info...\n", dwUserIndex );    

                    SchedulePresenceEnumTasks( TRUE, dwUserIndex );

                    bRet = TRUE;
                }
                break;
        }
    }

    return bRet;
}

//-------------------------------------------------------------------------------------
// Name:  CheckForNonFriendsPresenceChanges
// Desc:  Check to see if the presence state of any subscribed non-friends has changed
//-------------------------------------------------------------------------------------
BOOL Sample::CheckForNonFriendsPresenceChanges()
{
    DWORD dwNotificationID;
    ULONG_PTR ulParam;
    BOOL bRet = FALSE;

    if( XNotifyGetNext( m_hLiveListener, 0, &dwNotificationID, &ulParam ) )
    {
        switch( dwNotificationID )
        {
            case XN_LIVE_PRESENCE_CHANGED:
                {
                    DebugSpew( "Received presence changed notification for a subscribed peer.\n");

                    for( UINT i = 0; i < MAX_USER_COUNT; ++i )
                    {
                        const XUSER_SIGNIN_INFO& info = m_SignInInfo[ i ];
                        if( info.UserSigninState != eXUserSigninState_SignedInToLive )
                        {
                            continue;
                        }

                        DebugSpew( "Refreshing presence info subscriptions for user index %d\n", i );
                        if( m_Local.presenceXuids[ i ].size() )
                        {
                            SchedulePresenceEnumTasks( FALSE, i );
                        }
                    }


                    bRet = TRUE;
                }
                break;
        }
    }

    return bRet;
}

//-------------------------------------------------------------------------------------
// Name:  RetrieveFriendsPresenceData
// Desc:  Standalone function to enumerate and retrieve friends' presence data using
//        the XFriendsCreateEnumerator function.
// Notes: When called synchronously (NULL pOverlapped), phFriendsEnum is ignored.
//        When called asynchronously (non-NULL pOverlapped), phFriendsEnum must
//        be non-NULL.
//-------------------------------------------------------------------------------------
HRESULT Sample::RetrieveFriendsPresenceData( const DWORD dwUserIndex, 
                                             XONLINE_FRIEND* pArrayFriends, 
                                             const UINT cbFriendsArraySize, 
                                             DWORD* pcFriendsRetrieved,
                                             XOVERLAPPED* pOverlapped,
                                             HANDLE* phFriendsEnum )
{
    // Just check that pOverlapped and phFriendsEnum are consistent with each other. 
    // Leave all other parameter checking to the system APIs that take those parameters
    if( pOverlapped && !phFriendsEnum )
    {
        FatalError( "If pOverlapped is non-NULL, phFriendsEnum must be non-NULL\n" );
    }

    //
    // Enumerate friends
    //
    HANDLE hFriendsEnum = INVALID_HANDLE_VALUE;
    DWORD cbBuffer = 0;
    DWORD dwErr = ERROR_SUCCESS;
    dwErr = XFriendsCreateEnumerator(
        dwUserIndex,                                    // enumerate friends of this user
        0,                                              // starting index
        cbFriendsArraySize / sizeof( XONLINE_FRIEND ),  // number of friends we're querying for
        &cbBuffer,                                      // size of buffer needed
        &hFriendsEnum );

    // Friends' presence information not yet available
    if( dwErr != ERROR_SUCCESS  )
    {
        FatalError( "XFriendsCreateEnumerator did not return ERROR_SUCCESS. Instead returned %d\n", dwErr );
    }

    //
    // Retrieve all friends' data in one call to XEnumerate.
    //

    // Zero overlapped memory before calling XEnumerate asynchronously
    // and return friends enumeration handle in *phFriendsEnum
    if( pOverlapped )
    {
        ZeroMemory( pOverlapped, sizeof( XOVERLAPPED ) );
        *phFriendsEnum = hFriendsEnum;
    }

    //
    // Call XEnumerate asynchronously or synchronously
    // depending on caller preference.
    // When XEnumerate is called asynchronously,
    // *pcFriendsRetrieved is zero. The number of friend
    // items returned can be retrieved from
    // the InternalHigh of the passed-in XOVERLAPPED structure 
    // when the asynchronous call has completed
    // Note that in the asynchronous case, resources held by
    // hFriendsEnum shouldn't be released until after
    // the overlapped operation has completed.
    //
    if( pcFriendsRetrieved )
    {
        *pcFriendsRetrieved = 0;
    }

    ZeroMemory( pArrayFriends, cbFriendsArraySize );

    dwErr = XEnumerate( hFriendsEnum, 
                        pArrayFriends, 
                        cbBuffer, 
                        pcFriendsRetrieved,
                        pOverlapped );

    if( pOverlapped && ( dwErr != ERROR_IO_PENDING ) )
    {
        FatalError( "XEnumerate did not return ERROR_IO_PENDING when called asynchronously. Instead returned %d\n", dwErr );
    }
    else if( !pOverlapped && dwErr == ERROR_NO_MORE_FILES )
    {
        // No friends
        if( pcFriendsRetrieved )
        {
            *pcFriendsRetrieved = 0;
        }
        dwErr = 0;

    }
    else if( !pOverlapped && dwErr != ERROR_SUCCESS )
    {
        FatalError( "XEnumerate did not return ERROR_SUCCESS when called synchronously. Instead returned %d\n", dwErr );
    }

    if( !pOverlapped )
    {
        // Ok to free resources held by hFriendsEnum
        XCloseHandle( hFriendsEnum );
    }

    return dwErr;
}

//-------------------------------------------------------------------------------------
// Name:  RetrievePresenceData
// Desc:  Standalone function to enumerate and retrieve presence data using
//        the XPresenceCreateEnumerator function.
// Notes: When called synchronously (NULL pOverlapped), phEnumerator is ignored.
//        When called asynchronously (non-NULL pOverlapped), phEnumerator must
//        be non-NULL.
//-------------------------------------------------------------------------------------
HRESULT Sample::RetrievePresenceData( const DWORD dwUserIndex, 
                                      const UINT cPeers,
                                      const XUID* pXuids,
                                      XONLINE_PRESENCE* pArrayPresence,
                                      const UINT cbPresenceArraySize, 
                                      DWORD* pcPresenceItemsRetrieved,
                                      XOVERLAPPED* pOverlapped,
                                      HANDLE* phEnumerator )
{
    // Just check that pOverlapped and phEnumerator are consistent with each other. 
    // Leave all other parameter checking to the system APIs that take those parameters
    if( pOverlapped && !phEnumerator )
    {
        FatalError( "If pOverlapped is non-NULL, phEnumerator must be non-NULL\n" );
    }

    //
    // Enumerate presence
    //

    HANDLE hEnum = INVALID_HANDLE_VALUE;
    DWORD cbBuffer = 0;
    DWORD dwErr = ERROR_SUCCESS;
    dwErr = XPresenceCreateEnumerator(
        dwUserIndex,                    // enumerate friends of this user
        cPeers,                         // number of presence items we're querying for
        pXuids,                         // XUIDs of peers
        0,                              // Starting index
        cPeers,                         // max number of presence items to return
        &cbBuffer,                      // size of buffer needed
        &hEnum );

    // Presence information not yet available
    if( dwErr != ERROR_SUCCESS  )
    {
        FatalError( "XPresenceCreateEnumerator did not return ERROR_SUCCESS. Instead returned %d\n", dwErr );
    }

    //
    // Retrieve all presence data in one call to XEnumerate.
    //

    // Zero overlapped memory before calling XEnumerate asynchronously
    // and return enumeration handle in *phEnumerator
    if( pOverlapped )
    {
        ZeroMemory( pOverlapped, sizeof( XOVERLAPPED ) );
        *phEnumerator = hEnum;
    }

    //
    // Call XEnumerate asynchronously or synchronously
    // depending on caller preference.
    // When XEnumerate is called asynchronously,
    // *pcPresenceItemsRetrieved is zero. The number of presence
    // items returned can be retrieved from
    // the InternalHigh of the passed-in XOVERLAPPED structure 
    // when the asynchronous call has completed
    // Note that in the asynchronous case, resources held by
    // hFriendsEnum shouldn't be released until after
    // the overlapped operation has completed.
    //
    if( pcPresenceItemsRetrieved )
    {
        *pcPresenceItemsRetrieved = 0;
    }

    ZeroMemory( pArrayPresence, cbPresenceArraySize );

    dwErr = XEnumerate( hEnum, 
                        pArrayPresence, 
                        cbBuffer, 
                        pcPresenceItemsRetrieved,
                        pOverlapped );

    if( pOverlapped && ( dwErr != ERROR_IO_PENDING ) )
    {
        FatalError( "XEnumerate did not return ERROR_IO_PENDING when called asynchronously. Instead returned %d\n", dwErr );
    }
    else if( !pOverlapped && dwErr == ERROR_NO_MORE_FILES )
    {
        // No friends
        if( pcPresenceItemsRetrieved )
        {
            *pcPresenceItemsRetrieved = 0;
        }
        dwErr = 0;

    }
    else if( !pOverlapped && dwErr != ERROR_SUCCESS )
    {
        FatalError( "XEnumerate did not return ERROR_SUCCESS when called synchronously. Instead returned %d\n", dwErr );
    }

    if( !pOverlapped )
    {
        // Ok to free resources held by hEnum
        XCloseHandle( hEnum );
    }

    return dwErr;
}

//--------------------------------------------------------------------------------------
// Name: ChangeNotificationPosition()
// Desc: Update the current notification position
//--------------------------------------------------------------------------------------
VOID Sample::ChangeNotificationPosition()
{
    ++m_nNotificationPosition;
    m_nNotificationPosition %= m_cNotificationPosition;

    XNotifyPositionUI( m_anNotificationPosition[ m_nNotificationPosition ] );
}

//------------------------------------------------------------------------
// Name: SessionManagerFromNonce()
// Desc: Retrieves the right SessionManager* given
//       its session nonce
//------------------------------------------------------------------------
const SessionManager* Sample::SessionManagerFromNonce ( ULONGLONG qwSessionNonce )
{
    SessionManager* pSessionMgr = NULL;
    pSessionMgr = m_mapSessions[qwSessionNonce];
    
    if( pSessionMgr == NULL )
    {
        return NULL;
    }

    return (const SessionManager*)pSessionMgr;
}

//------------------------------------------------------------------------
// Name: SessionManagerFromSessionID()
// Desc: Retrieves the right SessionManager* given
//       its session ID
//------------------------------------------------------------------------
const SessionManager* Sample::SessionManagerFromSessionID ( const XNKID& sessionID )
{
    std::map<ULONGLONG, SessionManager*>::const_iterator citer;

    for ( citer = m_mapSessions.begin(); citer != m_mapSessions.end(); ++citer )
    {
        SessionManager* pSessionMgr = citer->second;
        if( !pSessionMgr )
        {
            continue;
        }

        const XNKID& id = pSessionMgr->GetSessionID();
        if( sessionID == id )
        {
            return (const SessionManager*)pSessionMgr;
        }

        // If host migration is active, check the migrated session ID
        // as well for a match
        const SessionState sessionState = pSessionMgr->GetSessionState();
        if( sessionState >= SessionStateMigrateHost &&
            sessionState <= SessionStateMigratedHost )
        {
            const XNKID migratedID = pSessionMgr->GetMigratedSessionID();
            if( sessionID == migratedID )
            {
                return (const SessionManager*)pSessionMgr;
            }
        }
    }

    return NULL;
}

//------------------------------------------------------------------------
// Name: SessionHostFromSessionID()
// Desc: Retrieves the right ClientInfo* given its session ID. This
//       isn't the most performant way to do this, but is fine for
//       the purposes of this sample
//------------------------------------------------------------------------
ClientInfo* Sample::SessionHostFromSessionID( const XNKID& sessionID )
{
    const SessionManager* pSessionMgr = SessionManagerFromSessionID( sessionID );
    if( !pSessionMgr )
    {
        return NULL;
    }

    IN_ADDR hostInAddr = pSessionMgr->GetHostInAddr();

    // Check local client first
    if( m_Local.addr == hostInAddr )
    {
        return &m_Local;
    }

    // Check remote clients
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        IN_ADDR inaddr = {0};
        if( i->GetInAddrForSession( &inaddr, sessionID ) && inaddr == hostInAddr )
        {
            return &(*i);
        }
    }

    return NULL;
}

//------------------------------------------------------------------------
// Name: DeleteLocalSession()
// Desc: Delete  local XSession instances
//------------------------------------------------------------------------
VOID Sample::DeleteLocalSession( SessionManager* pSessionMgr )
{
    if( pSessionMgr )
    {
        // Remove all local users from the session. This ensures that we don't 
        // try to double-join a presence-enabled session once we are connected to LIVE again.
        ScheduleSessionLeaveTasks( pSessionMgr, &m_Local, 0xF );
       
        // Are we the session host?
        if( pSessionMgr->IsSessionHost() )
        {
            // Deleting a hosted XSession requires a live connection to LIVE, so cleanup our
            // pSessionMgr instance. Note that this will also call XCloseHandle on
            // on the session handle
            ScheduleSessionMgrDeletionTasks( pSessionMgr );
        }
        else
        {
            // Not the session host, so safe to delete the XSession
            ScheduleSessionDeletionTasks( pSessionMgr );
        }
    }
}

//------------------------------------------------------------------------
// Name: DeleteLocalSessions()
// Desc: Delete all local XSession instances
//------------------------------------------------------------------------
VOID Sample::DeleteLocalSessions()
{
    SessionManager* arSessionManagerInstances[] =
    {
        GetMatchmakingSession(),
        GetPresenceSession()
    };
    
    for( UINT i = 0; i < _countof( arSessionManagerInstances ); ++ i )
    {
        if( arSessionManagerInstances[i] )
        {
            DeleteLocalSession( arSessionManagerInstances[i] );
        }
    }
}

//------------------------------------------------------------------------
// Name: IsMatchmakingSessionWithPartyMembers()
// Desc: Check if sessionID is of a Matchmaking session with party members
//------------------------------------------------------------------------
BOOL Sample::IsMatchmakingSessionWithPartyMembers( const XNKID& sessionID )
{
    for( std::vector<XNKID>::const_iterator cIter = m_vMatchmakingSessionsWithPartyMembers.begin();
         cIter != m_vMatchmakingSessionsWithPartyMembers.end(); cIter++ )
    {
        if( (*cIter) == sessionID )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//------------------------------------------------------------------------
// Name: AddSessionIDToPartyMatchmakingSessionsList()
// Desc: Add a sessionID to our list of matchmaking sessions we share
//       with party members
//------------------------------------------------------------------------
VOID Sample::AddSessionIDToPartyMatchmakingSessionsList( const XNKID& sessionID )
{
    for( std::vector<XNKID>::const_iterator cIter = m_vMatchmakingSessionsWithPartyMembers.begin();
         cIter != m_vMatchmakingSessionsWithPartyMembers.end(); cIter++ )
    {
        if( (*cIter) == sessionID )
        {
            // Duplicate session ID. Disregard
            return;
        }
    }

    m_vMatchmakingSessionsWithPartyMembers.push_back( sessionID );

    return;
}

//------------------------------------------------------------------------
// Name: RemoveSessionIDFromPartyMatchmakingSessionsList()
// Desc: Remove a sessionID to our list of matchmaking sessions we share
//       with party members
//------------------------------------------------------------------------
VOID Sample::RemoveSessionIDFromPartyMatchmakingSessionsList( const XNKID& sessionID )
{
    for ( std::vector<XNKID>::iterator iter = m_vMatchmakingSessionsWithPartyMembers.begin(); 
          iter != m_vMatchmakingSessionsWithPartyMembers.end(); )
    {
        if( (*iter)== sessionID )
        {
            iter = m_vMatchmakingSessionsWithPartyMembers.erase( iter );
            return;
        }
        else
        {
            iter++;
        }
    }
}

//------------------------------------------------------------------------
// Name: GetMatchmakingSession()
// Desc: Retrieves the right SessionManager* which is our
//       Matchmaking session
//------------------------------------------------------------------------
SessionManager* Sample::GetMatchmakingSession()
{
    // Grab the first session manager from our m_mapSessions map that
    // has the flags specified by our MATCHMAKING_SESSION_FLAGS global set
    return (SessionManager*)FromFlags( MATCHMAKING_SESSION_FLAGS );
}

//------------------------------------------------------------------------
// Name: GetPresenceSession()
// Desc: Retrieves the right SessionManager* which is our
//       Presence session
//------------------------------------------------------------------------
SessionManager* Sample::GetPresenceSession()
{
    // Grab the first session manager from our m_mapSessions map that
    // has the flags specified by our PRESENCE_SESSION_FLAGS global set, and
    // does not have the XSESSION_CREATE_USES_MATCHMAKING flag. We add
    // this extra check because this sample can create a Matchmaking
    // session that uses presence.
    return (SessionManager*)FromFlags( PRESENCE_SESSION_FLAGS, 
                                          XSESSION_CREATE_USES_MATCHMAKING );
}

//------------------------------------------------------------------------
// Name: DeleteSessionGracefully()
// Desc: Gracefully delete a session
//------------------------------------------------------------------------
VOID Sample::DeleteSessionGracefully( SessionManager* pSessionMgr )
{
    if( pSessionMgr )
    {
        const XNKID& sessionID = pSessionMgr->GetSessionID();

        DebugSpew( "DeleteSessionGracefully(%016I64X)\n",
                   XNKIDToInt64( sessionID ) );

        // Remove local users from the session
        ScheduleSessionLeaveTasks( pSessionMgr, &m_Local, 0xF );

        // Remove remote users from the session
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
        {
            if( !i ->IsInSession( sessionID ) )
            {
                continue;
            }

            ScheduleSessionLeaveTasks( pSessionMgr, &(*i), 0xF );
        }

        // Schedule tasks to handle the session deletion
        ScheduleSessionDeletionTasks( pSessionMgr );            
    }
}

//------------------------------------------------------------------------
// Name: FromFlags()
// Desc: Retrieves the right SessionManager* given
//       a set of session flags it should and shouldn't have
//------------------------------------------------------------------------
const SessionManager* Sample::FromFlags ( const DWORD dwExpectedFlags,
                                             const DWORD dwUnexpectedFlags )
{
    for ( std::map<ULONGLONG, SessionManager*>::const_iterator citer = m_mapSessions.begin(); 
          citer != m_mapSessions.end(); 
          ++citer )
    {
        SessionManager* pSessionMgr = citer->second;
        if( pSessionMgr )
        {
            const DWORD sessionFlags = pSessionMgr->GetSessionFlags();

            // If any of dwUnexpectedFlags are in sessionFlags,
            // we don't a match
            if( ( sessionFlags & dwUnexpectedFlags ) != 0 )
            {
                continue;
            }

            // What flags in sessionFlags and dwExpectedFlags are different?
            DWORD dwDiffFlags = sessionFlags ^ dwExpectedFlags;

            // If none of dwDiffFlags are in dwExpectedFlags,
            // we have a match
            if( ( dwDiffFlags & dwExpectedFlags ) == 0 )
            {
                return pSessionMgr;
            }
        }
    }

    return NULL;
}

//--------------------------------------------------------------------------------------
// Name: JoinPartyMembersToSession()
// Desc: We're the host of a Presence session, and we're trying to get
//       party members into a Matchmaking session
//--------------------------------------------------------------------------------------
VOID Sample::JoinPartyMembersToSession( const XSESSION_INFO& matchmakingSessionInfo,
                                        const BOOL bInvited )
{
    SessionManager* pPresenceSessionMgr = GetPresenceSession();
    if( !pPresenceSessionMgr )
    {
        return;
    }

    assert( pPresenceSessionMgr->IsSessionHost() );
    const XNKID& presenceSessionID = pPresenceSessionMgr->GetSessionID();

    CMessage msg( MSG_FOUND_SESSION );
    msg.SetSessionID( presenceSessionID );

    MsgFoundSession& FoundSession   = msg.GetFoundSession();
    FoundSession.info               = matchmakingSessionInfo;
    FoundSession.bInvited           = bInvited;

    // loop through the list of clients and look for party members
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        IN_ADDR inaddr = {0};
        if( i->GetInAddrForSession( &inaddr, presenceSessionID ) )
        {
            SendMessage( &msg, inaddr );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: JoinPartyMemberToSession()
// Desc: We're the host of a Presence session, and we're trying to get
//       party member into a Matchmaking session
//--------------------------------------------------------------------------------------
VOID Sample::JoinPartyMemberToSession( const XSESSION_INFO& matchmakingSessionInfo,
                                       const ClientInfo* pClient,
                                       const BOOL bInvited ) 
{
    SessionManager* pPresenceSessionMgr = GetPresenceSession();
    if( !pPresenceSessionMgr || !pClient )
    {
        return;
    }

    assert( pPresenceSessionMgr->IsSessionHost() );
    const XNKID& presenceSessionID = pPresenceSessionMgr->GetSessionID();

    CMessage msg( MSG_FOUND_SESSION );
    msg.SetSessionID( presenceSessionID );

    MsgFoundSession& FoundSession   = msg.GetFoundSession();
    FoundSession.info               = matchmakingSessionInfo;
    FoundSession.bInvited           = bInvited;

    IN_ADDR inaddr = {0};
    if( pClient->GetInAddrForSession( &inaddr, presenceSessionID ) )
    {
        SendMessage( &msg, inaddr );
    }
}


//--------------------------------------------------------------------------------------
// Name: JoinSession()
// Desc: Clears local client info about presence session affiliation
//--------------------------------------------------------------------------------------
VOID Sample::ClearPresenceSessionAffiliation()
{

    // Local player
    ZeroMemory( m_Local.presenceSessionNonces, MAX_USER_COUNT * sizeof( ULONGLONG ) );

    // Remote players
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        ZeroMemory( i->presenceSessionNonces, MAX_USER_COUNT * sizeof( ULONGLONG ) );
    }
}

//--------------------------------------------------------------------------------------
// Name: JoinSession()
// Desc: Join a session from an XN_LIVE_INVITE_ACCEPTED notification
//--------------------------------------------------------------------------------------
VOID Sample::JoinSessionFromInviteInfoPreamble( XINVITE_INFO InviteInfo )
{
    // Before we can create a local copy of the session we're joining,
    // we need to query the session host for more details about the
    // session

    // Resolve the host's IP address
    XNetRegisterKey( &InviteInfo.hostInfo.sessionID, &InviteInfo.hostInfo.keyExchangeKey );

    IN_ADDR in_addr;
    if( XNetXnAddrToInAddr( &InviteInfo.hostInfo.hostAddress,
                            &InviteInfo.hostInfo.sessionID, 
                            &in_addr ) != 0 )
    {
        FatalError( "Could not resolve host's IP address.\n" );
    }

    // Send a MSG_QUERY_SESSION message to the host to retrieve details
    // about the session
    CMessage msg(MSG_QUERY_SESSION);
    msg.SetSessionID( InviteInfo.hostInfo.sessionID );

    DebugSpew( "JoinSessionFromInviteInfoPreamble(%016I64X): Sending MSG_QUERY_SESSION to host.\n",
               XNKIDToInt64( InviteInfo.hostInfo.sessionID ) );

    SendMessage( &msg, in_addr );
}

#ifdef _XBOX
//--------------------------------------------------------------------------------------
// Name: ShouldSendGameInvitesToLiveParty()
// Desc: Determines if game invites should be sent to the XBox LIVE Party
//--------------------------------------------------------------------------------------
BOOL Sample::ShouldSendGameInvitesToLiveParty()
{
    // We'll send out invites if any LIVE Party member isn't
    // in any presence-enabled session that we're in. Basically, 
    // we're inviting them to come join us in our in 
    // our "in-game experience"
    for( std::map<ULONGLONG, SessionManager*>::const_iterator iter = 
          m_mapSessions.begin(); iter != m_mapSessions.end(); ++iter )
    {
        SessionManager* pSessionMgr = iter->second;

        if( pSessionMgr && 
            pSessionMgr->HasSessionFlags( XSESSION_CREATE_USES_PRESENCE ) )
        {
            const XNKID& gameSessionID = pSessionMgr->GetSessionID();
            for( DWORD i = 0; i < m_LivePartyUserList.dwUserCount; ++i )
            {
                // If any LIVE Party member isn't in our  session, then
                // we want to send invites out
                if( m_LivePartyUserList.Users[i].SessionInfo.sessionID != gameSessionID )
                {
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}
#endif

//--------------------------------------------------------------------------------------
// Name: ShouldSendGameInvitesToFriends()
// Desc: Determines if game invites should be sent to friends
//--------------------------------------------------------------------------------------
BOOL Sample::ShouldSendGameInvitesToFriends( const DWORD dwUserIndex )
{
    // We'll send out invites if any friend isn't
    // in any presence-enabled session that we're in
    for( std::map<ULONGLONG, SessionManager*>::const_iterator iter = 
          m_mapSessions.begin(); iter != m_mapSessions.end(); ++iter )
    {
        SessionManager* pSessionMgr = iter->second;

        if( pSessionMgr && 
            pSessionMgr->HasSessionFlags( XSESSION_CREATE_USES_PRESENCE ) )
        {
            const XNKID& gameSessionID = pSessionMgr->GetSessionID();
            for( DWORD iFriend = 0; iFriend < m_Local.cFriends[dwUserIndex]; ++iFriend )
            {
                // If any friend isn't in our  session, then
                // we want to send invites out
                if( m_Local.friends[dwUserIndex][iFriend].sessionID != gameSessionID )
                {
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

//--------------------------------------------------------------------------------------
// Name: SendMessage()
// Desc: Send a message to a remote player
//--------------------------------------------------------------------------------------
VOID Sample::SendMessage( CMessage* pMessage, IN_ADDR addr )
{
    sockaddr_in sa  = {0};
    sa.sin_addr     = addr;
    sa.sin_family   = AF_INET;
    sa.sin_port     = Xplat_HTONS( PORT );

    // NULLADDR means self
    if( addr == NULLADDR )
    {
        #ifdef _XBOX
        sa.sin_addr.S_un.S_addr = INADDR_LOOPBACK;
        #else if LIVE_ON_WINDOWS
        sa.sin_addr = m_Local.addr;
        #endif
    }

    // If this client is already in the process of being dropped, don't send
    // to to them
    const XNKID& sessionID = pMessage->GetSessionID();
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        IN_ADDR inaddr = {0};
        if( i->GetInAddrForSession( &inaddr, sessionID ) )
        {
            if( i->bDroppingClient )
            {
                return;
            }
            else
            {
                break;
            }
        }
    }

    int nSize = pMessage->GetSize();

    // GFW-L clients run on little-endian architecture, and network traffic is big-endian,
    // so we need to byte-swap before sending messages across the wire
    pMessage->EndianSwap();

    INT ret = Xplat_SendTo( m_Socket, (CHAR*)pMessage, pMessage->GetSize(),
        0, (SOCKADDR*)&sa, sizeof( sa ) );

    // Byte-swap our message payload to what it was before sending out across the wire
    pMessage->EndianSwap();

    if( ret != nSize )
    {
        if( ret == SOCKET_ERROR )
        {
            DebugSpew( "SendMessage(%016I64X): sendto error: %d\n", XNKIDToInt64( pMessage->GetSessionID() ), Xplat_WSAGetLastError() );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: ReceiveMessage()
// Desc: Receive a message from a remote player
//--------------------------------------------------------------------------------------
VOID Sample::ReceiveMessage()
{
    SOCKADDR_IN sa;
    INT size = sizeof( sa );
    INT ret;

    do
    {
        CMessage* pMsg = new CMessage();

        ret = Xplat_RecvFrom( m_Socket, (CHAR*)pMsg, sizeof(CMessage), 0, (SOCKADDR*)&sa, &size );

        if( ret != SOCKET_ERROR && ret > 0 && Xplat_NTOHS( sa.sin_port ) == PORT )
        {
            // GFW-L clients run on little-endian architecture, and network traffic is big-endian,
            // so we need to byte-swap before receiving messages from the wire
            pMsg->EndianSwap();
            
            switch( pMsg->GetID() )
            {
                case MSG_START_SESSION:         ScheduleNetMsgTask( TASK_NETMSG_START_SESSION, pMsg, sa.sin_addr ); break;
                case MSG_QUERY_SESSION:         ScheduleNetMsgTask( TASK_NETMSG_QUERY_SESSION, pMsg, sa.sin_addr ); break;
                case MSG_RESP_SESSION:          ScheduleNetMsgTask( TASK_NETMSG_RESP_SESSION, pMsg, sa.sin_addr ); break;
                case MSG_JOIN_SESSION:          ScheduleNetMsgTask( TASK_NETMSG_JOIN_SESSION, pMsg, sa.sin_addr ); break;
                case MSG_JOIN_SESSION_PARTY:    ScheduleNetMsgTask( TASK_NETMSG_JOIN_SESSION_PARTY, pMsg, sa.sin_addr ); break;
                case MSG_FOUND_SESSION:         ScheduleNetMsgTask( TASK_NETMSG_FOUND_SESSION, pMsg, sa.sin_addr ); break;
                case MSG_LEFT_SESSION:          ScheduleNetMsgTask( TASK_NETMSG_LEFT_SESSION, pMsg, sa.sin_addr ); break;
                case MSG_JOIN_RESPONSE:         ScheduleNetMsgTask( TASK_NETMSG_JOIN_RESPONSE, pMsg, sa.sin_addr ); break;
                case MSG_JOIN_RESPONSE_PARTY:   ScheduleNetMsgTask( TASK_NETMSG_JOIN_RESPONSE_PARTY, pMsg, sa.sin_addr ); break;
                case MSG_PLAYER_INFO:           ScheduleNetMsgTask( TASK_NETMSG_PLAYER_INFO, pMsg, sa.sin_addr ); break;
                case MSG_WAVE:                  ScheduleNetMsgTask( TASK_NETMSG_WAVE, pMsg, sa.sin_addr ); break;
                case MSG_HEARTBEAT:             ScheduleNetMsgTask( TASK_NETMSG_HEARTBEAT, pMsg, sa.sin_addr ); break;
                case MSG_GOODBYE:               ScheduleNetMsgTask( TASK_NETMSG_GOODBYE, pMsg, sa.sin_addr ); break;
                case MSG_END_SESSION:           ScheduleNetMsgTask( TASK_NETMSG_END_SESSION, pMsg, sa.sin_addr ); break;
                case MSG_REGISTER:              ScheduleNetMsgTask( TASK_NETMSG_REGISTER, pMsg, sa.sin_addr ); break;
                case MSG_REGISTERED:            ScheduleNetMsgTask( TASK_NETMSG_REGISTERED, pMsg, sa.sin_addr ); break;
                case MSG_SCORE_POINT:           ScheduleNetMsgTask( TASK_NETMSG_SCORE_POINT, pMsg, sa.sin_addr ); break;
                case MSG_POINT_TOTAL:           ScheduleNetMsgTask( TASK_NETMSG_POINT_TOTAL, pMsg, sa.sin_addr ); break;
                case MSG_MIGRATE:               ScheduleNetMsgTask( TASK_NETMSG_MIGRATE, pMsg, sa.sin_addr ); break;
            }
        }
        else
        {
            delete pMsg;
        }
    } while( ret != SOCKET_ERROR && ret > 0 && Xplat_NTOHS( sa.sin_port ) == PORT );
}

//--------------------------------------------------------------------------------------
// Name: DoNetMsgQuerySession()
// Desc: Respond to a query session message
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgQuerySession ( const NetMsgData& netMsgData )
{
    const XNKID sessionID = netMsgData.m_pMsg->GetSessionID();
    const BOOL bInvited   = netMsgData.m_pMsg->GetQuerySession().bInvited;

    CMessage msgResponseSession( MSG_RESP_SESSION );
    msgResponseSession.SetSessionID( sessionID );

    MsgResponseSession& QueryResponse = msgResponseSession.GetRespSession();
    QueryResponse.bInvited            = bInvited;

    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID ); 
    const BOOL bIsPresenceSession = ( pSessionMgr == GetPresenceSession() );

    DebugSpew( "DoNetMsgQuerySession(%016I64X): Received MSG_QUERY_SESSION.\n",
               XNKIDToInt64( sessionID ) );

    if( pSessionMgr && pSessionMgr->IsSessionHost() )
    {
        DebugSpew( "DoNetMsgQuerySession(%016I64X): Sending QUERYRESPONSE_HOSTING.\n",
                   XNKIDToInt64( sessionID ) );

        QueryResponse.Response = QueryResponse.QUERYRESPONSE_HOSTING;
        QueryResponse.dwFlags = pSessionMgr->GetSessionFlags();
        pSessionMgr->GetMaxSlotCounts( QueryResponse.dwMaxPublicSlots, QueryResponse.dwMaxPrivateSlots );

        if ( bIsPresenceSession )
        {
            QueryResponse.dwGameMode = m_cPartySessionGameMode;
            QueryResponse.dwGameType = X_CONTEXT_GAME_TYPE_STANDARD;
        }
        else // Matchmaking session
        {
            QueryResponse.dwGameMode = m_nGameMode;
            QueryResponse.dwGameType = m_nGameType;
        }

        QueryResponse.qwSessionNonce = pSessionMgr->GetSessionNonce();

        const XSESSION_INFO& session_info = pSessionMgr->GetSessionInfo();
        QueryResponse.session_info        = session_info;
    }
    else
    {
        DebugSpew( "DoNetMsgQuerySession(%016I64X): Sending QUERYRESPONSE_NOTHOSTING.\n",
                   XNKIDToInt64( sessionID ) );

        QueryResponse.Response = QueryResponse.QUERYRESPONSE_NOTHOSTING;
    }

    // Send the response
    SendMessage( &msgResponseSession, netMsgData.m_inaddrFrom );
}

//--------------------------------------------------------------------------------------
// Name: IsPresenceSessionHostInAddr()
// Desc: Is IN_ADDR from Presence session host?
//--------------------------------------------------------------------------------------
BOOL Sample::IsPresenceSessionHostInAddr( const IN_ADDR& addr )
{
    SessionManager* pPresenceSessionMgr = GetPresenceSession();
    if( !pPresenceSessionMgr || pPresenceSessionMgr->IsSessionHost() )
    {

        DebugSpew( "IsPresenceSessionHostInAddr: IN_ADDR not from Presence session host\n" );
        return FALSE;
    }

    IN_ADDR hostInAddr = pPresenceSessionMgr->GetHostInAddr();

    return ( hostInAddr == addr );
}

//--------------------------------------------------------------------------------------
// Name: DoNetMsgFoundSession()
// Desc: Respond to a found session message sent from a Presence session host
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgFoundSession( const NetMsgData& netMsgData )
{
    const XSESSION_INFO& session_info = netMsgData.m_pMsg->GetFoundSession().info;
    const BOOL bInvited               = netMsgData.m_pMsg->GetFoundSession().bInvited;

    // Make sure this message is coming from our Presence session host. If 
    // not, disregard
    if( !IsPresenceSessionHostInAddr( netMsgData.m_inaddrFrom ) )
    {
        DebugSpew( "DoNetMsgFoundSession(%016I64X): Received MSG_FOUND_SESSION from unknown host.\n",
                   XNKIDToInt64( session_info.sessionID ) );

        return;
    }

    DebugSpew( "DoNetMsgFoundSession(%016I64X): Received MSG_FOUND_SESSION from Presence session host.\n",
               XNKIDToInt64( session_info.sessionID ) );

    // Send a MSG_QUERY_SESSION message to the host to initiate the
    // process of joining this session

    // Resolve the host's IP address
    XNetRegisterKey( &session_info.sessionID, &session_info.keyExchangeKey );

    IN_ADDR in_addr;
    if( XNetXnAddrToInAddr( &session_info.hostAddress,
                            &session_info.sessionID, 
                            &in_addr ) != 0 )
    {
        FatalError( "Could not resolve host's IP address." );
    }

    // We get this message from our Presence session host telling us to join a
    // Matchmaking session. We track all Matchmaking sessions that we are
    // in with party members, so add this sessionID to our tracking list
    AddSessionIDToPartyMatchmakingSessionsList( session_info.sessionID );

    // Send a MSG_QUERY_SESSION message to the host to retrieve details
    // about the session
    CMessage msg(MSG_QUERY_SESSION);
    msg.SetSessionID( session_info.sessionID );
    msg.GetQuerySession().bInvited = bInvited;

    SendMessage( &msg, in_addr );
}

//--------------------------------------------------------------------------------------
// Name: DoNetMsgLeftSession()
// Desc: Respond to a left session message sent from a Presence session host
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgLeftSession ( const NetMsgData& netMsgData )
{
    const XNKID& sessionID = netMsgData.m_pMsg->GetSessionID();

    // Make sure this message is coming from our Presence session host. If 
    // not, disregard
    if( !IsPresenceSessionHostInAddr( netMsgData.m_inaddrFrom ) )
    {
        DebugSpew( "DoNetMsgLeftSession(%016I64X): Received MSG_LEFT_SESSION from unknown host.\n",
                   XNKIDToInt64( sessionID ) );

        return;
    }

    DebugSpew( "DoNetMsgLeftSession(%016I64X): Received MSG_LEFT_SESSION from Presence session host.\n",
               XNKIDToInt64( sessionID ) );

    // If we're not in a Matchmaking session with the session ID sent with the message,
    // then disregard message
    SessionManager* pMatchmakingSessionMgr = GetMatchmakingSession();
    if( !pMatchmakingSessionMgr )
    {
        DebugSpew( "DoNetMsgLeftSession(%016I64X): Unknown Matchmaking session.\n",
                   XNKIDToInt64( sessionID ) );

        return;
    }

    const XNKID& matchmakingSessionID = pMatchmakingSessionMgr->GetSessionID();
    if( matchmakingSessionID == sessionID )
    {
        // If we made it this far, the message is valid. Gracefully leave 
        // the Matchmakign session
        DebugSpew( "DoNetMsgLeftSession(%016I64X): Gracefully deleting Matchmaking session.\n",
                   XNKIDToInt64( sessionID ) );
        DeleteSessionGracefully( pMatchmakingSessionMgr );
    }
}

//--------------------------------------------------------------------------------------
// Name: DoNetMsgRespSession()
// Desc: Respond to a respond session message
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgRespSession( const NetMsgData& netMsgData )
{
    const MsgResponseSession& RespSession = netMsgData.m_pMsg->GetRespSession();
    const XNKID sessionID           = netMsgData.m_pMsg->GetSessionID();

    DebugSpew( "DoNetMsgRespSession(%016I64X): Received MSG_RESP_SESSION.\n",
               XNKIDToInt64( sessionID ) );

    if( RespSession.Response == RespSession.QUERYRESPONSE_NOTHOSTING )
    {
        DebugSpew( "DoNetMsgRespSession(%016I64X): Received QUERYRESPONSE_NOTHOSTING.\n",
                   XNKIDToInt64( sessionID ) );

        // The peer that we sent the MSG_RESP_SESSION message to 
        // isn't hosting the session, so do nothing
        return;
    }
    
    // Don't allow joining a session of a type we're already in
    const BOOL bIsMatchmakingSession = ( RespSession.dwFlags & XSESSION_CREATE_USES_MATCHMAKING );

    if( bIsMatchmakingSession && GetMatchmakingSession() )
    {
        DebugSpew( "DoNetMsgRespSession(%016I64X): Already in a Matchmaking session, so not joining\n",
                   XNKIDToInt64( sessionID ) );

        return;
    }
    else if( !bIsMatchmakingSession && GetPresenceSession() ) 
    {
        DebugSpew( "DoNetMsgRespSession(%016I64X): Already in a Presence session, so not joining\n",
                   XNKIDToInt64( sessionID ) );

        return;
    }

    // Clear our client list of anyone not in our Presence session
    SessionManager* pPresenceSessionMgr = GetPresenceSession();

    if( pPresenceSessionMgr )
    {
        const XNKID presenceSessionID = pPresenceSessionMgr->GetSessionID();
        ClientInfoVec::iterator i;
        for( i = m_vecRemote.begin(); i != m_vecRemote.end(); )
        {
            if( i->IsInSession( presenceSessionID ) )
            {
                ++i;
            }
            else
            {
                i = m_vecRemote.erase( i );
            }
        }
    }
    else
    {
        m_vecRemote.clear();
    }

    // Create and initialize our session manager
    SessionManager* pSessionMgr = new SessionManager();

    SessionManagerInitParams initParams;
    
    initParams.m_SessionCreationReason  = 
        ( !RespSession.bInvited ) ? SessionCreationReasonJoinFromSearch : 
                                    SessionCreationReasonAmbiguous;
    
    initParams.m_bIsHost                = FALSE;
    initParams.m_dwSessionFlags         = RespSession.dwFlags;

    // If the gametype is ranked, but sure to add the 
    // XSESSION_CREATE_USES_ARBITRATION and XSESSION_CREATE_USES_STATS flags
    if( RespSession.dwGameType == X_CONTEXT_GAME_TYPE_RANKED )
    {
        initParams.m_dwSessionFlags |= XSESSION_CREATE_USES_ARBITRATION
                                    |  XSESSION_CREATE_USES_STATS;
    }

    initParams.m_dwMaxPublicSlots   = RespSession.dwMaxPublicSlots;
    initParams.m_dwMaxPrivateSlots  = RespSession.dwMaxPrivateSlots;

    // If we're currently in a Presence session, then strip the
    // XSESSION_CREATE_USES_PRESENCE flag from our creation flags, 
    // since the Presence session by definition already specifies 
    // this flag and a player can only be in 
    // one Presence session at a time
    if( pPresenceSessionMgr )
    {
        initParams.m_dwSessionFlags &= ~XSESSION_CREATE_USES_PRESENCE;
    }

    pSessionMgr->Initialize( initParams );

    // Give it the XSESSION_INFO from the host
    pSessionMgr->SetSessionInfo( RespSession.session_info );

    // Store the host's IN_ADDR
    pSessionMgr->SetHostInAddr( netMsgData.m_inaddrFrom );

    const DWORD dwOwnerController = m_CXPlat_Signin.GetLowestSignedInControllerID();
    pSessionMgr->SetSessionOwner( dwOwnerController );

    //
    // Set the session nonce, which will be used for reporting
    // arbitrated results

    // First clear out the temporary session nonce from our map.
    // Remember that for non-hosted sessions we had to create one
    // to get our bookkeeping right..
    m_mapSessions.erase( pSessionMgr->GetSessionNonce() ); 

    // Next set the true session nonce for this session manager
    assert( RespSession.qwSessionNonce );
    pSessionMgr->SetSessionNonce( RespSession.qwSessionNonce );

    // Now insert into our map
    m_mapSessions.insert( NONCE_SESSION_PAIR( RespSession.qwSessionNonce, pSessionMgr ) ); 

    // Set up game type and game mode contexts
    const BOOL bIsPresenceSession = ( pSessionMgr->HasSessionFlags( PRESENCE_SESSION_FLAGS ) &&
                                      !pSessionMgr->HasSessionFlags( MATCHMAKING_SESSION_FLAGS ) );
    if( bIsPresenceSession )
    {
        assert( RespSession.dwGameMode == m_cPartySessionGameMode );

        m_nGameMode = m_cPartySessionGameMode;
        m_nGameType = X_CONTEXT_GAME_TYPE_STANDARD;
    }
    else // Matchmaking session
    {
        m_nGameMode = RespSession.dwGameMode;
        m_nGameType = RespSession.dwGameType;
    }
    
    XUserSetContext( dwOwnerController, 
                     X_CONTEXT_GAME_TYPE, 
                     m_nGameType );

    XUserSetContext( dwOwnerController, 
                     X_CONTEXT_GAME_MODE, 
                     m_nGameMode );

    // Create the session. Note that the flags specified by RespSession.dwFlags
    // and passed in front the session host already takes care of differentiating
    // this session as a presence session or matchmaking session for us
    ScheduleChangeStateTask( APPSTATE_CREATE_SESSION_UNHOSTED, pSessionMgr );
}


//--------------------------------------------------------------------------------------
// Name: DoNetMsgJoinSession()
// Desc: Respond to a join session message
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgJoinSession( const NetMsgData& netMsgData )
{
    const XNKID& sessionID       = netMsgData.m_pMsg->GetSessionID();
    IN_ADDR inaddrFrom           = netMsgData.m_inaddrFrom;
    const MsgJoinSession& JoinSession = netMsgData.m_pMsg->GetJoinSession();

    // Ignore this request if we've already mapped this inaddr to a client for this session
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        IN_ADDR inaddr = {0};
        if( i->GetInAddrForSession( &inaddr, sessionID ) && inaddr == inaddrFrom )
        {
            DebugSpew( "DoNetMsgJoinSession(%016I64X): Ignoring known client ID's request: %I64u\n",
                       XNKIDToInt64( sessionID ),
                       i->id );
            return;
        }
    }

    CMessage msgJoinResponse( MSG_JOIN_RESPONSE );
    msgJoinResponse.SetSessionID( sessionID );
    MsgJoinResponse& JoinResponse = msgJoinResponse.GetJoinResponse();

    for ( DWORD i=0; i<JoinSession.cPlayers; ++i )
    {
        DebugSpew( "DoNetMsgJoinSession(%016I64X): Joining: 0x%016I64X; bInvited: %d\n",
                   XNKIDToInt64( sessionID ),
                   JoinSession.xuids[ i ], 
                   JoinSession.bInvited );
    }

    SessionManager* pMatchmakingSessionMgr = GetMatchmakingSession();    

    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID ); 
    if( !pSessionMgr )
    {
        DebugSpew( "DoNetMsgJoinSession(%016I64X): Returning JOINRESPONSE_NOTHOSTING for unknown session\n", XNKIDToInt64( sessionID ) );

        JoinResponse.Response = JoinResponse.JOINRESPONSE_NOTHOSTING;

    }
    else if( !pSessionMgr->IsSessionHost() )
    {
        DebugSpew( "DoNetMsgJoinSession(%016I64X): Returning JOINRESPONSE_NOTHOSTING\n", XNKIDToInt64( sessionID ) );
                  
        JoinResponse.Response = JoinResponse.JOINRESPONSE_NOTHOSTING;
    }
    else if( pSessionMgr == pMatchmakingSessionMgr && m_AppState > APPSTATE_PREGAME && 
             pSessionMgr->HasSessionFlags( XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED ) )
    {
        DebugSpew( "DoNetMsgJoinSession(%016I64X): Returning JOINRESPONSE_SESSIONINPROGRESS\n", XNKIDToInt64( sessionID ) );

        JoinResponse.Response = JoinResponse.JOINRESPONSE_SESSIONINPROGRESS;
    }
    else
    {
        // See if we have the slots to hold this guy

        DWORD dwMaxPublicSlots, dwMaxPrivateSlots, dwFilledPublicSlots, dwFilledPrivateSlots;

        pSessionMgr->GetMaxSlotCounts( dwMaxPublicSlots, dwMaxPrivateSlots );
        pSessionMgr->GetFilledSlotCounts( dwFilledPublicSlots, dwFilledPrivateSlots );
        
        UINT nSlotsOpen = dwMaxPublicSlots - dwFilledPublicSlots 
                        + JoinSession.bInvited * ( dwMaxPrivateSlots - dwFilledPrivateSlots);

        if( JoinSession.cPlayers > nSlotsOpen )
        {
            DebugSpew( "DoNetMsgJoinSession(%016I64X): Returning JOINRESPONSE_SESSIONFULL\n", XNKIDToInt64( sessionID ) );

            JoinResponse.Response = JoinResponse.JOINRESPONSE_SESSIONFULL;
        }
        else
        {
            JoinResponse.Response       = JoinResponse.JOINRESPONSE_APPROVED;
            JoinResponse.Nonce          = pSessionMgr->GetSessionNonce();
            JoinResponse.nVictoryPoints = m_nVictoryPoints;
            JoinResponse.nMap           = m_nMap;

            // Return the local player info
            UINT idx = 0;
            for( UINT i = 0; i < m_Local.cPlayers; ++i )
            {
                // Return online non-idle users only
                const XUSER_SIGNIN_INFO& info = m_SignInInfo[ m_Local.nController[ i ] ];
                if( info.UserSigninState == eXUserSigninState_SignedInToLive && !m_Local.bIsIdle[ i ] )
                {
                    JoinResponse.xuids[ idx ] = info.xuid;
                    MultiByteToWideChar( CP_ACP, 0, info.szUserName, -1,
                        JoinResponse.strGamertags[ idx ], XUSER_NAME_SIZE );

                    DebugSpew( "DoNetMsgJoinSession(%016I64X): Returning local user: 0x%016I64X\n",
                               XNKIDToInt64( sessionID ),
                               JoinResponse.xuids[ idx ] );

                    idx++;
                }
            }
            
            // Set count of players we're attempting to join
            JoinResponse.cPlayers = idx;

            JoinResponse.cMaxPrivateSlots     = dwMaxPrivateSlots;
            JoinResponse.cMaxPublicSlots      = dwMaxPublicSlots;
            JoinResponse.cFilledPublicSlots   = dwFilledPublicSlots;
            JoinResponse.cFilledPrivateSlots  = dwFilledPrivateSlots;

            JoinResponse.id = m_Local.id;

            JoinResponse.dwSessionFlags = pSessionMgr->GetSessionFlags() & ~XSESSION_CREATE_HOST;

            for ( DWORD i=0; i<idx; ++i )
            {
                DebugSpew( "DoNetMsgJoinSession(%016I64X): 0x%016I64X\n",
                           XNKIDToInt64( sessionID ), 
                           JoinResponse.xuids[ i ] );
            }
        }
    }

    // Send the response
    SendMessage( &msgJoinResponse, inaddrFrom );

    // If we approved the request, do the proper housekeeping for a new player
    if( pSessionMgr != NULL && JoinResponse.Response == JoinResponse.JOINRESPONSE_APPROVED )
    {
        DebugSpew( "DoNetMsgJoinSession(%016I64X): Sending JOINRESPONSE_APPROVED\n", pSessionMgr->GetSessionIDAsInt() );

        // Note - Because of how the ClientInfo struct is defined, it's easier to 
        // create a new ClientInfo instance each time we approve a new client, 
        // even if that physical client machine is already in an active session on this console
        ClientInfo* pNewClient = NULL;
        ClientInfo NewClient;
        m_vecRemote.push_back( NewClient );
        pNewClient = &m_vecRemote.back();

        pNewClient->addr        = inaddrFrom;
        pNewClient->JoinedSession( sessionID, m_Local.addr, inaddrFrom );

        pNewClient->id          = JoinSession.id;
        pNewClient->cPlayers    = JoinSession.cPlayers;
        pNewClient->dwHeartbeat = GetTickCount();

        XNetInAddrToXnAddr( inaddrFrom, &pNewClient->xnaddr, NULL );

        for( UINT i = 0; i < MAX_USER_COUNT; ++i )
        {
            pNewClient->dwWave[ i ] = GetTickCount() - WAVE_TIME;
        }

        memcpy_s( pNewClient->xuids, sizeof( pNewClient->xuids ),
                  JoinSession.xuids, sizeof( JoinSession.xuids ) );
        memcpy_s( pNewClient->strGamertags, sizeof( pNewClient->strGamertags ),
                  JoinSession.strGamertags, sizeof( JoinSession.strGamertags ) );
        memcpy_s( pNewClient->bHasVoice, sizeof( pNewClient->bHasVoice ),
                  JoinSession.bHasVoice, sizeof( JoinSession.bHasVoice ) );
        ZeroMemory( pNewClient->bIsIdle, MAX_USER_COUNT * sizeof( BOOL ) );
        ZeroMemory( pNewClient->bToAdd, MAX_USER_COUNT * sizeof( BOOL ) );
        ZeroMemory( pNewClient->bToRemove, MAX_USER_COUNT * sizeof( BOOL ) );
        ZeroMemory( pNewClient->presenceSessionNonces, MAX_USER_COUNT * sizeof( ULONGLONG ) );

        pNewClient->bInvited = JoinSession.bInvited;

        for ( DWORD i = 0; i < pNewClient->cPlayers; ++i )
        {
            DebugSpew( "DoNetMsgJoinSession(%016I64X): New Client: 0x%016I64X; bInvited: %d\n", 
                       XNKIDToInt64( sessionID ),
                       pNewClient->xuids[ i ], 
                       pNewClient->bInvited );
        }

        // Add the client to our session
        ScheduleSessionJoinTasks( pSessionMgr, pNewClient, 0xF );

        // Create a playerinfo message to tell everyone about the new client
        CMessage msgPlayerInfoNew( MSG_PLAYER_INFO );
        msgPlayerInfoNew.SetSessionID( sessionID );

        MsgPlayerInfo& PlayerInfoNew    = msgPlayerInfoNew.GetPlayerInfo();
        PlayerInfoNew.cPlayers          = pNewClient->cPlayers;
        PlayerInfoNew.bInvited          = pNewClient->bInvited;
        PlayerInfoNew.id                = pNewClient->id;
        PlayerInfoNew.xnaddr            = pNewClient->xnaddr;
        memcpy_s( PlayerInfoNew.strGamertags, sizeof( PlayerInfoNew.strGamertags ),
                  pNewClient->strGamertags, sizeof( pNewClient->strGamertags ) );
        memcpy_s( PlayerInfoNew.xuids,        sizeof( PlayerInfoNew.xuids ),
                  pNewClient->xuids,        sizeof( pNewClient->xuids ) );
        memcpy_s( PlayerInfoNew.bHasVoice,    sizeof( PlayerInfoNew.bHasVoice ),
                  pNewClient->bHasVoice,    sizeof( pNewClient->bHasVoice ) );

        // Iterate through and tell the oldbies about the newbie and vice versa
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
        {
            ClientInfo& Remote = *i;

            IN_ADDR remoteInAddr = {0};
            if( !i->GetInAddrForSession( &remoteInAddr, sessionID ) )
            {
                continue;
            }

            // Don't tell newbie about newbie!
            if( Remote.id == PlayerInfoNew.id )
            {
                continue;
            }

            CMessage msgPlayerInfoOld( MSG_PLAYER_INFO );
            msgPlayerInfoOld.SetSessionID( sessionID );

            MsgPlayerInfo& PlayerInfoOld = msgPlayerInfoOld.GetPlayerInfo();
            PlayerInfoOld.cPlayers = Remote.cPlayers;
            PlayerInfoOld.bInvited = Remote.bInvited;
            PlayerInfoOld.id       = Remote.id;
            PlayerInfoOld.xnaddr   = Remote.xnaddr;
            memcpy_s( PlayerInfoOld.strGamertags, sizeof( PlayerInfoOld.strGamertags ),
                      Remote.       strGamertags, sizeof( Remote.       strGamertags ) );
            memcpy_s( PlayerInfoOld.xuids,        sizeof( PlayerInfoOld.xuids ),
                      Remote.       xuids,        sizeof( Remote.       xuids ) );
            memcpy_s( PlayerInfoOld.bHasVoice,    sizeof( PlayerInfoOld.bHasVoice ),
                      Remote.       bHasVoice,    sizeof( Remote.       bHasVoice ) );

            // Send the messages
            DebugSpew( "DoNetMsgJoinSession(%016I64X): Sending MSG_PLAYER_INFO to " \
                       "tell existing remote ID %I64u about new remote ID %I64u\n",
                       XNKIDToInt64( sessionID ), PlayerInfoOld.id, PlayerInfoNew.id );
            
            SendMessage( &msgPlayerInfoOld, inaddrFrom );

            DebugSpew( "DoNetMsgJoinSession(%016I64X): Sending MSG_PLAYER_INFO to " \
                       "tell new remote ID %I64u about existing remote ID %I64u \n",
                       XNKIDToInt64( sessionID ), PlayerInfoNew.id, PlayerInfoOld.id );

            SendMessage( &msgPlayerInfoNew, remoteInAddr );
        }

        // Let the player know what state the session is in
        const SessionState sessionState = pSessionMgr->GetSessionState();
        switch( sessionState )
        {
        case SessionStateRegistering:
        case SessionStateRegistered:
            {
            CMessage msgRegister( MSG_REGISTER );
            msgRegister.SetSessionID( sessionID );

            DebugSpew( "DoNetMsgJoinSession(%016I64X): Sending MSG_REGISTER to " \
                       "new remote ID %I64u\n",
                       XNKIDToInt64( sessionID ), PlayerInfoNew.id );
            
            SendMessage( &msgRegister, inaddrFrom );
            }
            break;
        case SessionStateStarting:
        case SessionStateInGame:
            {
            CMessage msgStartGame( MSG_START_SESSION );
            msgStartGame.SetSessionID( sessionID );

            DebugSpew( "DoNetMsgJoinSession(%016I64X): Sending MSG_START_SESSION to " \
                       "new remote ID %I64u\n",
                       XNKIDToInt64( sessionID ), PlayerInfoNew.id );
            
            SendMessage( &msgStartGame, inaddrFrom );
            }
            break;
        case SessionStateEnd:
        case SessionStateEnding:
        case SessionStateEnded:
            {
            CMessage msgEndGame( MSG_END_SESSION );
            msgEndGame.SetSessionID( sessionID );

            DebugSpew( "DoNetMsgJoinSession(%016I64X): Sending MSG_END_SESSION to " \
                       "new remote ID %I64u\n",
                       XNKIDToInt64( sessionID ), PlayerInfoNew.id );
            
            SendMessage( &msgEndGame, inaddrFrom );
            }
            break;
        }

        // If we've started the game, get them the up-to-date scores
        if( sessionState > SessionStateRegistered )
        {
            CMessage msgPointTotal( MSG_POINT_TOTAL );
            msgPointTotal.SetSessionID( sessionID );
            MsgPointTotal& PointTotal  = msgPointTotal.GetPointTotal();

            for( UINT i = 0; i < m_Local.cPlayers; ++i )
            {
                if( m_Local.nPoints[ i ] )
                {
                    PointTotal.id      = m_Local.id;
                    PointTotal.xuid    = m_Local.xuids[ i ];
                    PointTotal.nPoints = m_Local.nPoints[ i ];

                    DebugSpew( "DoNetMsgJoinSession(%016I64X): Sending MSG_POINT_TOTAL to " \
                               "new remote ID %I64u about local XUID 0x%016I64X with " \
                               "point total %d\n",
                               XNKIDToInt64( sessionID ), 
                               PlayerInfoNew.id,
                               m_Local.xuids[ i ],
                               m_Local.nPoints[ i ] );

                    SendMessage( &msgPointTotal, inaddrFrom );
                }
            }
            
            for( ClientInfoVec::iterator vecIter = m_vecRemote.begin(); 
                 vecIter != m_vecRemote.end(); ++vecIter )
            {
                if( !vecIter->IsInSession( sessionID ) )
                {
                    continue;
                }

                ClientInfo& Remote = *vecIter;
                for( UINT i = 0; i < Remote.cPlayers; ++i )
                {
                    if( Remote.nPoints[ i ] )
                    {
                        PointTotal.id      = Remote.id;
                        PointTotal.xuid    = Remote.xuids[ i ];
                        PointTotal.nPoints = Remote.nPoints[ i ];

                        DebugSpew( "DoNetMsgJoinSession(%016I64X): Sending MSG_POINT_TOTAL to " \
                                   "new remote ID %I64u about remote ID %I64u and XUID 0x%016I64X with " \
                                   "point total %d\n",
                                   XNKIDToInt64( sessionID ), 
                                   PlayerInfoNew.id,
                                   Remote.id,
                                   Remote.xuids[ i ],
                                   Remote.nPoints[ i ] );

                        SendMessage( &msgPointTotal, inaddrFrom );
                    }
                }
            }
        }

        // Finally, if this session is a Presence session and we're also
        // the host of a Matchmaking session, let this joiner into the 
        // Matchmaking session if there's room
        if ( pSessionMgr == GetPresenceSession() && pMatchmakingSessionMgr && 
             pMatchmakingSessionMgr->IsSessionHost() )
        {
            DWORD dwMaxPublicSlots, dwMaxPrivateSlots, dwFilledPublicSlots, dwFilledPrivateSlots;

            pMatchmakingSessionMgr->GetMaxSlotCounts( dwMaxPublicSlots, dwMaxPrivateSlots );
            pMatchmakingSessionMgr->GetFilledSlotCounts( dwFilledPublicSlots, dwFilledPrivateSlots );
            
            UINT nSlotsOpen = dwMaxPublicSlots - dwFilledPublicSlots 
                            + JoinSession.bInvited * ( dwMaxPrivateSlots - dwFilledPrivateSlots);

            if( JoinSession.cPlayers <= nSlotsOpen )
            {
                DebugSpew( "DoNetMsgJoinSession(%016I64X): Letting joiner into hosted Matchmaking session %016I64X\n", 
                           XNKIDToInt64( sessionID ),
                           pMatchmakingSessionMgr->GetSessionIDAsInt() );

                const BOOL bInvited = TRUE;
                JoinPartyMemberToSession( pMatchmakingSessionMgr->GetSessionInfo(),
                                          pNewClient,
                                          bInvited );
            }
        }        
    }
}

//--------------------------------------------------------------------------------------
// Name: DoNetMsgJoinSessionParty()
// Desc: Respond to a join session message for a Party belonging to a Presence session
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgJoinSessionParty( const NetMsgData& netMsgData )
{
    const XNKID sessionID    = netMsgData.m_pMsg->GetSessionID();
    const IN_ADDR inaddrFrom = netMsgData.m_inaddrFrom;

    DebugSpew( "DoNetMsgJoinSessionParty(%016I64X): Receiving MSG_JOIN_SESSION_PARTY\n", XNKIDToInt64( sessionID ) );

    const MsgJoinSessionParty& JoinSessionParty = netMsgData.m_pMsg->GetJoinSessionParty();

    CMessage msgJoinResponseParty( MSG_JOIN_RESPONSE_PARTY );
    msgJoinResponseParty.SetSessionID( sessionID );
    MsgJoinResponseParty& JoinResponseParty = msgJoinResponseParty.GetJoinResponseParty();

    SessionManager* pMatchmakingSessionMgr = GetMatchmakingSession();

    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID ); 
    if( !pSessionMgr )
    {
        DebugSpew( "DoNetMsgJoinSessionParty(%016I64X): Returning JOINRESPONSEPARTY_NOTHOSTING for unknown session\n", 
                   XNKIDToInt64( sessionID ) );

        JoinResponseParty.Response = JoinResponseParty.JOINRESPONSEPARTY_NOTHOSTING;
    }
    else if( !pSessionMgr->IsSessionHost() )
    {
        DebugSpew( "DoNetMsgJoinSessionParty(%016I64X): Returning JOINRESPONSEPARTY_NOTHOSTING\n", 
                   XNKIDToInt64( sessionID ) );

        JoinResponseParty.Response = JoinResponseParty.JOINRESPONSEPARTY_NOTHOSTING;
    }
    else if( pSessionMgr == pMatchmakingSessionMgr && 
             m_AppState < APPSTATE_PREGAME || m_AppState > APPSTATE_POSTGAME )
    {
        DebugSpew( "DoNetMsgJoinSessionParty(%016I64X): Returning JOINRESPONSEPARTY_NOTHOSTING for session in appstate %d\n", 
                   XNKIDToInt64( sessionID ), m_AppState );
                  
        JoinResponseParty.Response = JoinResponseParty.JOINRESPONSEPARTY_NOTHOSTING;
    }
    else if( pSessionMgr == pMatchmakingSessionMgr && m_AppState > APPSTATE_PREGAME && 
             pSessionMgr->HasSessionFlags( XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED ) )
    {
        DebugSpew( "DoNetMsgJoinSessionParty(%016I64X): Returning JOINRESPONSEPARTY_SESSIONINPROGRESS\n", 
                   XNKIDToInt64( sessionID ) );

        JoinResponseParty.Response = JoinResponseParty.JOINRESPONSEPARTY_SESSIONINPROGRESS;
    }
    else
    {
        // See if we have the slots to hold this guy

        DWORD dwMaxPublicSlots, dwMaxPrivateSlots, dwFilledPublicSlots, dwFilledPrivateSlots;

        pSessionMgr->GetMaxSlotCounts( dwMaxPublicSlots, dwMaxPrivateSlots );
        pSessionMgr->GetFilledSlotCounts( dwFilledPublicSlots, dwFilledPrivateSlots );
        
        UINT nSlotsOpen = dwMaxPublicSlots - dwFilledPublicSlots 
                        + JoinSessionParty.bInvited * ( dwMaxPrivateSlots - dwFilledPrivateSlots);

        if( JoinSessionParty.cPlayers > nSlotsOpen )
        {
            DebugSpew( "DoNetMsgJoinSessionParty(%016I64X): Returning JOINRESPONSEPARTY_SESSIONFULL. Requested slots: %d; Available: %d\n", 
                       XNKIDToInt64( sessionID ), JoinSessionParty.cPlayers, nSlotsOpen );

            JoinResponseParty.Response = JoinResponseParty.JOINRESPONSEPARTY_SESSIONFULL;
        }
        else
        {
            DebugSpew( "DoNetMsgJoinSessionParty(%016I64X): Returning JOINRESPONSEPARTY_APPROVED\n", 
                       XNKIDToInt64( sessionID ) );

            JoinResponseParty.Response = JoinResponseParty.JOINRESPONSEPARTY_APPROVED;
        }
    }

    // Send the response
    SendMessage( &msgJoinResponseParty, inaddrFrom );
}


//--------------------------------------------------------------------------------------
// Name: DoNetMsgHeartbeat()
// Desc: Handle a received heartbeat message
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgHeartbeat( const NetMsgData& netMsgData )
{
    const XNKID sessionID    = netMsgData.m_pMsg->GetSessionID();
    const IN_ADDR inaddrFrom = netMsgData.m_inaddrFrom;
    const _int64 sessionIDInt64 = XNKIDToInt64( sessionID );

    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID ); 
    if( !pSessionMgr )
    {
        // We don't care about this message
        DebugSpew( "DoNetMsgHeartbeat(%016I64X, %I64u): Heartbeat for unknown session. Ignoring\n",
                   sessionIDInt64, 
                   m_Local.id );

        return;
    }

//    const BOOL bIsFromHost = ( addrFrom == pSessionMgr->GetHostInAddr() );
    
    // See whose heart is beating
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        IN_ADDR inaddr = {0};
        if( i->GetInAddrForSession( &inaddr, sessionID ) && inaddr == inaddrFrom )
        {
//            const DWORD dwLastHeartbeat = i->dwHeartbeat;
            i->dwHeartbeat = GetTickCount();

            //DebugSpew( "DoNetMsgHeartbeat(%016I64X, %I64u): Heartbeat from ID %I64u (%s). " \
            //           "Last heartbeat %d ticks ago..\n",
            //           sessionIDInt64, 
            //           m_Local.id,
            //           i->id,
            //           ( bIsFromHost ) ? "host" : "client",
            //           i->dwHeartbeat - dwLastHeartbeat );
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: DoNetMsgWave()
// Desc: Handle a received wave message
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgWave( const NetMsgData& netMsgData )
{
    const XNKID sessionID = netMsgData.m_pMsg->GetSessionID();
    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID ); 
    if( !pSessionMgr )
    {
        // We don't care about this message
        return;
    }

    if( m_AppState < APPSTATE_PREGAME )
        return;

    MsgWave& WaveMsg = netMsgData.m_pMsg->GetWave();

    // Find the dude that's waving
    ClientInfo* pWaver = NULL;

    if( WaveMsg.id == m_Local.id )
    {
        pWaver = &m_Local;
    }
    else
    {
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
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
        for( UINT i = 0; i < pWaver->cPlayers; ++i )
        {
            if( XOnlineAreUsersIdentical( pWaver->xuids[ i ], WaveMsg.xuid ) )
            {
                pWaver->dwWave[ i ] = GetTickCount();
            }
        }

        if( pSessionMgr->IsSessionHost() )
        {
            // If we're the host, pass the message along to everyone else
            for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
            {
                IN_ADDR inaddr = {0};
                if( i->GetInAddrForSession( &inaddr, sessionID ) )
                {
                    SendMessage( netMsgData.m_pMsg, inaddr );
                }
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: DoNetMsgJoinResponse()
// Desc: Handle a processed join response
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgJoinResponse( const NetMsgData& netMsgData )
{
    const XNKID sessionID    = netMsgData.m_pMsg->GetSessionID();
    const IN_ADDR inaddrFrom = netMsgData.m_inaddrFrom;
    
    DebugSpew( "DoNetMsgJoinResponse(%016I64X): Received JOINRESPONSE\n", XNKIDToInt64( sessionID ) );

    // See what the response was
    const MsgJoinResponse& JoinResponse = netMsgData.m_pMsg->GetJoinResponse();

    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID ); 
    if( !pSessionMgr )
    {
        DebugSpew( "DoNetMsgJoinResponse(%016I64X): Unknown session ID, so ignoring JOINRESPONSE message\n", XNKIDToInt64( sessionID ) );
        return;
    }

    // Handle response
    switch( JoinResponse.Response )
    {
        case JoinResponse.JOINRESPONSE_NOTHOSTING:
            {
            DebugSpew( "DoNetMsgJoinResponse(%016I64X): Receiving JOINRESPONSE_NOTHOSTING\n",
                       pSessionMgr->GetSessionIDAsInt() );

            // Schedule tasks to handle the session deletion
            ScheduleSessionDeletionTasks( pSessionMgr );            

            pSessionMgr->SetSessionError( L"This game is no longer available." );
            }
            break;

        case JoinResponse.JOINRESPONSE_SESSIONFULL:
            {
            DebugSpew( "DoNetMsgJoinResponse(%016I64X): Receiving JOINRESPONSE_SESSIONFULL\n",
                       pSessionMgr->GetSessionIDAsInt() );

            // Schedule tasks to handle the session deletion
            ScheduleSessionDeletionTasks( pSessionMgr );            

            pSessionMgr->SetSessionError( L"This game is full." );
            }
            break;

        case JoinResponse.JOINRESPONSE_SESSIONINPROGRESS:
            {
            DebugSpew( "DoNetMsgJoinResponse(%016I64X): Receiving JOINRESPONSE_SESSIONINPROGRESS\n",
                       pSessionMgr->GetSessionIDAsInt() );

            // Schedule tasks to handle the session deletion
            ScheduleSessionDeletionTasks( pSessionMgr );            

            pSessionMgr->SetSessionError( L"This game has already started." );
            }
            break;

        case JoinResponse.JOINRESPONSE_APPROVED:
            DebugSpew( "DoNetMsgJoinResponse(%016I64X): Receiving JOINRESPONSE_APPROVED for session\n",
                       pSessionMgr->GetSessionIDAsInt() );

            // Eureka!

            // Note - Because of how the ClientInfo struct is defined, it's easier to 
            // create a new ClientInfo instance each time we join a session, 
            // even if that physical client machine is already in an active session on this console
            ClientInfo NewClient;
            m_vecRemote.push_back( NewClient );

            pSessionMgr->SetHostInAddr( inaddrFrom );

            //
            // Set the session nonce, which will be used for reporting
            // arbitrated results

            // First clear out the temporary session nonce from our map.
            // Remember that for non-hosted sessions we had to create one
            // to get our bookkeeping right..
            m_mapSessions.erase( pSessionMgr->GetSessionNonce() ); 

            // Next set the true session nonce for this session manager
            assert( JoinResponse.Nonce );
            pSessionMgr->SetSessionNonce( JoinResponse.Nonce );

            // Now insert into our map
            m_mapSessions.insert( NONCE_SESSION_PAIR( JoinResponse.Nonce, pSessionMgr ) ); 

            ClientInfo* pHost   = &m_vecRemote.back();
            pHost->dwHeartbeat  = GetTickCount();
            pHost->bInvited     = ( pSessionMgr->GetSessionCreationReason() 
                                        != SessionCreationReasonJoinFromSearch );

            pHost->cPlayers     = JoinResponse.cPlayers;
            pHost->addr         = inaddrFrom;
            if( XNetInAddrToXnAddr( inaddrFrom, &pHost->xnaddr, NULL ) )
            {
                FatalError( "DoNetMsgJoinResponse(%016I64X): XNetInAddrToXnAddr failed!\n",
                            pSessionMgr->GetSessionIDAsInt() );
            }

            pHost->JoinedSession( sessionID, inaddrFrom, pHost->addr );

            // Add a local reference to this session as well
            m_Local.JoinedSession( sessionID, inaddrFrom, m_Local.addr );
            
            memcpy_s( pHost->strGamertags,       sizeof( pHost->strGamertags ),
                      JoinResponse.strGamertags, sizeof( JoinResponse.strGamertags ) );
            memcpy_s( pHost->xuids,       sizeof( pHost->xuids ),
                      JoinResponse.xuids, sizeof( JoinResponse.xuids ) );
            memcpy_s( pHost->bHasVoice,          sizeof( pHost->bHasVoice ),
                      JoinResponse.bHasVoice,    sizeof( JoinResponse.bHasVoice ) );
            pHost->id = JoinResponse.id;

            m_nVictoryPoints = JoinResponse.nVictoryPoints;
            m_nMap           = JoinResponse.nMap;
        
            pSessionMgr->SetMaxSlotCounts( JoinResponse.cMaxPublicSlots, JoinResponse.cMaxPrivateSlots );

            // Modify the flags, to get the correct state of invites, join in progress, etc
            // Note that you can't modify flags for ranked games
            if( GetGameType() != X_CONTEXT_GAME_TYPE_RANKED )
            {
                DebugSpew( "DoNetMsgJoinResponse(%016I64X): Modifying session flags with flags from host: %d\n",
                           pSessionMgr->GetSessionIDAsInt(),
                           JoinResponse.dwSessionFlags );

                if( JoinResponse.dwSessionFlags & XSESSION_CREATE_USES_PRESENCE )
                {
                    DebugSpew( "DoNetMsgJoinResponse(%016I64X): Discarding XSESSION_CREATE_USES_PRESENCE from host flags...\n",
                               pSessionMgr->GetSessionIDAsInt() );
                }
                ScheduleSessionModifyTasks( pSessionMgr, JoinResponse.dwSessionFlags & ~XSESSION_CREATE_USES_PRESENCE );
            }

            // Add the host to the session. Note that we would have already added
            // local players when we created the session
            DebugSpew( "DoNetMsgJoinResponse(%016I64X): Adding remote host to session\n",
                       pSessionMgr->GetSessionIDAsInt() );

            ScheduleSessionJoinTasks( pSessionMgr, pHost, 0xF );

            // If we're joining a Matchmaking session and are in a Presence session:
            // 1. If we're the host of the Presence session, 
            //    nothing to do at this point. We'll add players to the session once
            //    we get notification from the Matchmaking host to do so (through
            //    a MSG_PLAYER_INFO message ).
            // 2.   a. If we're not the host AND 
            //      b. the host of the Presence session and Matchmaking session are different, AND
            //      c. we're not joining this Matchmaking session along with our party, THEN,
            //    leave the Presence session gracefully. Note that it's important to check for 
            //    condition b. since we might be in the middle of processing back-to-back
            //    join operations for a Presence and Matchmaking session owned by the same host. And
            //    it's important to check condition c since we might be joining the Matchmaking session
            //    from instruction for our Presence host.
            SessionManager* pPresenceSessionMgr = GetPresenceSession();
            if( pSessionMgr == GetMatchmakingSession() && pPresenceSessionMgr )
            {
                const ClientInfo* pMatchmakingHostClient = SessionHostFromSessionID( pSessionMgr->GetSessionID() );
                const ClientInfo* pPresenceHostClient    = SessionHostFromSessionID( pPresenceSessionMgr->GetSessionID() );

                const XNADDR& hostXnaddrPresenceSession    = pPresenceHostClient->xnaddr;
                const XNADDR& hostXnaddrMatchmakingSession = pMatchmakingHostClient->xnaddr;

                const BOOL bAreHostsSame = ( memcmp( &hostXnaddrPresenceSession, 
                                                     &hostXnaddrMatchmakingSession, 
                                                     sizeof( XNADDR ) ) == 0 );
                
                const BOOL bJoiningMatchmakingSessionWithParty = 
                    IsMatchmakingSessionWithPartyMembers( sessionID );

                if( pPresenceSessionMgr->IsSessionHost() && m_vecRemote.size() )
                {
                    // 1. If we're the host of the Presence session, 
                    //    nothing to do at this point. We'll add players to the session once
                    //    we get notification from the Matchmaking host to do so (through
                    //    a MSG_PLAYER_INFO message ).
                }
                else if( !pPresenceSessionMgr->IsSessionHost() && 
                         !bAreHostsSame && 
                         !bJoiningMatchmakingSessionWithParty )
                {
                    DebugSpew( "DoNetMsgJoinResponse(%016I64X): Joining Matchmaking session "
                               "with a different Presence session host, so leaving "
                               "Presence session %016I64X\n",                               
                               pSessionMgr->GetSessionIDAsInt(),
                               pPresenceSessionMgr->GetSessionIDAsInt() );

                    DeleteSessionGracefully( pPresenceSessionMgr );
                }
            }           

            // If we're joining a Presence session and are in a Matchmaking session, leave
            // the Matchmaking session
            SessionManager* pMatchmakingSessionMgr = GetMatchmakingSession();
            if( pSessionMgr == GetPresenceSession() && pMatchmakingSessionMgr )
            {
                DeleteSessionGracefully( pMatchmakingSessionMgr );
            }

            break; 
    }
}

//--------------------------------------------------------------------------------------
// Name: DoNetMsgJoinResponseParty()
// Desc: Handle a processed join response for a party of Presence session members
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgJoinResponseParty( const NetMsgData& netMsgData )
{
    const XNKID sessionID = netMsgData.m_pMsg->GetSessionID();

    DebugSpew( "DoNetMsgJoinResponseParty(%016I64X): Received JOINRESPONSE_PARTY\n", XNKIDToInt64( sessionID ) );

    // See what the response was
    const MsgJoinResponseParty& JoinResponseParty = netMsgData.m_pMsg->GetJoinResponseParty();

    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID ); 
    if( !pSessionMgr )
    {
        return;
    }

    // Handle response
    switch( JoinResponseParty.Response )
    {
        case JoinResponseParty.JOINRESPONSEPARTY_NOTHOSTING:
            {
            DebugSpew( "DoNetMsgJoinResponseParty(%016I64X): Received JOINRESPONSEPARTY_NOTHOSTING\n", pSessionMgr->GetSessionIDAsInt() );
                        
            pSessionMgr->SetSessionError( L"This game is no longer available." );

            // Schedule tasks to handle the session deletion
            ScheduleSessionDeletionTasks( pSessionMgr );            
            }
            break;

        case JoinResponseParty.JOINRESPONSEPARTY_SESSIONFULL:
            {
            DebugSpew( "DoNetMsgJoinResponseParty(%016I64X): Received JOINRESPONSEPARTY_SESSIONFULL\n", pSessionMgr->GetSessionIDAsInt() );
                        
            pSessionMgr->SetSessionError( L"This game is full." );

            // Schedule tasks to handle the session deletion
            ScheduleSessionDeletionTasks( pSessionMgr );            
            }
            break;

        case JoinResponseParty.JOINRESPONSEPARTY_SESSIONINPROGRESS:
            {
            DebugSpew( "DoNetMsgJoinResponseParty(%016I64X): Received JOINRESPONSEPARTY_SESSIONINPROGRESS\n", pSessionMgr->GetSessionIDAsInt() );
                        
            pSessionMgr->SetSessionError( L"This game has already started." );

            // Schedule tasks to handle the session deletion
            ScheduleSessionDeletionTasks( pSessionMgr );            
            }
            break;

        case JoinResponseParty.JOINRESPONSEPARTY_APPROVED:
            DebugSpew( "DoNetMsgJoinResponseParty(%016I64X): Received JOINRESPONSEPARTY_APPROVED\n", pSessionMgr->GetSessionIDAsInt() );

            // Eureka! Let the host know we want to join the session...
            const XSESSION_INFO info = pSessionMgr->GetSessionInfo();
            const IN_ADDR inaddr     = pSessionMgr->GetHostInAddr();
            const BOOL bInvited      = ( pSessionMgr->GetSessionCreationReason() 
                                         != SessionCreationReasonJoinFromSearch );

            JoinSession( info.sessionID, inaddr, bInvited );

            // ... and have our party members do the same
            JoinPartyMembersToSession( pSessionMgr->GetSessionInfo(), bInvited ); 

            break; 
    }
}


//--------------------------------------------------------------------------------------
// Name: DoNetMsgPlayerInfo()
// Desc: Handle a player info message
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgPlayerInfo( const NetMsgData& netMsgData )
{
    const XNKID sessionID = netMsgData.m_pMsg->GetSessionID();
    const MsgPlayerInfo& PlayerInfo = netMsgData.m_pMsg->GetPlayerInfo();

    ClientInfo* pClient = NULL;

    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID ); 
    if( !pSessionMgr )
    {
        return;
    }

    DebugSpew( "DoNetMsgPlayerInfo(%016I64X): Processing MSG_PLAYER_INFO\n", XNKIDToInt64( sessionID ) );
    for ( UINT i = 0; i < PlayerInfo.cPlayers; ++i )
    {
        DebugSpew( "0x%016I64X\n", PlayerInfo.xuids[ i ] );
    }

    // Look for the player info in our remote list
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        if( i->id == PlayerInfo.id && i->IsInSession( sessionID ) )
        {
            pClient = &(*i);
            break;
        }
    }

    // If we didn't find it, this must be a new player
    if( pClient == NULL && PlayerInfo.cPlayers != 0 )
    {
        ClientInfo NewClient;
        m_vecRemote.push_back( NewClient );
        pClient = &m_vecRemote.back();

        DebugSpew( "DoNetMsgPlayerInfo(%016I64X): Creating new client %p for id %I64u\n", 
                   XNKIDToInt64( sessionID ), 
                   pClient,
                   PlayerInfo.id );
    }

    if( pClient && PlayerInfo.id )
    {
        DebugSpew( "DoNetMsgPlayerInfo(%016I64X): Found client %p for id %I64u\n", 
                   XNKIDToInt64( sessionID ), 
                   pClient,
                   PlayerInfo.id );

        if( PlayerInfo.cPlayers )
        {
            // Since we support players dropping in/out of sessions, we 
            // need to consider individual players on the client. First
            // cull out old players from the session
            DWORD dwToRemoveMask = 0xFFFF;
            if( pClient->cPlayers == 0 )
            {
                dwToRemoveMask = 0;
            }
            for( UINT i = 0; i < pClient->cPlayers; ++i )
            {
                for ( UINT j = 0; j < PlayerInfo.cPlayers; ++j )
                {
                    if( pClient->xuids[ i ] == PlayerInfo.xuids[j] )
                    {
                        dwToRemoveMask ^= 1 << i;
                        break;
                    }
                }
            }

            DebugSpew( "DoNetMsgPlayerInfo(%016I64X): dwToRemoveMask value is %d\n", XNKIDToInt64( sessionID ), dwToRemoveMask );

            if( dwToRemoveMask )
            {
                // Write stats for the players we're removing...
                ScheduleSessionWriteTasks( pSessionMgr, pClient, dwToRemoveMask );
                
                // ... and then remove these players from session
                ScheduleSessionLeaveTasks( pSessionMgr, pClient, dwToRemoveMask );
            }

            // Now figure out who the new players are
            DWORD dwToAddMask = 0xFFFF;
            for( UINT i = 0; i < PlayerInfo.cPlayers; ++i )
            {
                for ( UINT j = 0; j < pClient->cPlayers; ++j )
                {
                    if( PlayerInfo.xuids[ i ] == pClient->xuids[j] )
                    {
                        dwToAddMask ^= 1 << i;
                        break;
                    }
                }
            }

            DebugSpew( "DoNetMsgPlayerInfo(%016I64X): dwToAddMask value is %d\n", XNKIDToInt64( sessionID ), dwToAddMask );

            if( dwToAddMask )
            {
                // Copy the new info to the client record and add new players
                // to the session
                pClient->cPlayers = PlayerInfo.cPlayers;
                pClient->bInvited = PlayerInfo.bInvited;
                pClient->id       = PlayerInfo.id;
                pClient->xnaddr   = PlayerInfo.xnaddr;

                DebugSpew( "DoNetMsgPlayerInfo(%016I64X): Resolving address of client %I64u\n", XNKIDToInt64( sessionID ), pClient->id );

                if( XNetXnAddrToInAddr( &PlayerInfo.xnaddr, &sessionID, &pClient->addr ) )
                {
                    FatalError( "DoNetMsgPlayerInfo(%016I64X): XNetXnAddrToInAddr failed for client %I64u\n", XNKIDToInt64( sessionID ), pClient->id );
                }

                pClient->JoinedSession( pSessionMgr->GetSessionID(), 
                                        pSessionMgr->GetHostInAddr(), 
                                        pClient->addr );

                pClient->dwHeartbeat = GetTickCount();
                                        

                memcpy_s( pClient->strGamertags, sizeof( pClient->  strGamertags ),
                          PlayerInfo.strGamertags, sizeof( PlayerInfo.strGamertags ) );
                memcpy_s( pClient->xuids, sizeof( pClient->  xuids ),
                          PlayerInfo.xuids, sizeof( PlayerInfo.xuids ) );
                memcpy_s( pClient->bHasVoice, sizeof( pClient-> bHasVoice ),
                          PlayerInfo.bHasVoice, sizeof( PlayerInfo.bHasVoice ) );
                ZeroMemory( pClient->bIsIdle, MAX_USER_COUNT * sizeof( BOOL ) );
                ZeroMemory( pClient->bToAdd, MAX_USER_COUNT * sizeof( BOOL ) );
                ZeroMemory( pClient->bToRemove, MAX_USER_COUNT * sizeof( BOOL ) );
                ZeroMemory( pClient->presenceSessionNonces, MAX_USER_COUNT * sizeof( ULONGLONG ) );

                // Add new users to the session immediately
                ScheduleSessionJoinTasks( pSessionMgr, pClient, dwToAddMask );
            }
        }
        else
        {
            DebugSpew( "DoNetMsgPlayerInfo(%016I64X): Removing all users for client %I64u\n", XNKIDToInt64( sessionID ), pClient->id );
    
            ScheduleSessionLeaveTasks( pSessionMgr, pClient, 0xF );

            pClient->LeftSession( pSessionMgr->GetSessionID() );
        }
    }
}

//
//--------------------------------------------------------------------------------------
// Name: DoNetMsgGoodbye()
// Desc: Handle a player leaving message
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgGoodbye( const NetMsgData& netMsgData )
{
    const XNKID sessionID = netMsgData.m_pMsg->GetSessionID();
    const IN_ADDR inaddrFrom = netMsgData.m_inaddrFrom;

    DebugSpew( "DoNetMsgGoodbye(%016I64X): Received MSG_GOODBYE\n", XNKIDToInt64( sessionID ) );

    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID );
    if( !pSessionMgr )
    {
        DebugSpew( "DoNetMsgGoodbye(%016I64X): Unknown session ID, so ignoring MSG_GOODBYE\n", XNKIDToInt64( sessionID ) );
        return;
    }

    //// If we currently are deleting this session, then ignore this message. We would
    //// already be deleting this session, if say, we received a MSG_LEFT_SESSION
    //// message from our Presence session host for this session
    //if( GetSessionManagerTaskData( pSessionMgr, SessionManagementTask_DELETE ) )
    //{
    //    DebugSpew( "DoNetMsgGoodbye(%016I64X): Already deleting session, so ignoring MSG_GOODBYE\n", XNKIDToInt64( sessionID ) );
    //    return;
    //}

    const MsgGoodbye& GoodBye = netMsgData.m_pMsg->GetGoodbye();

    if( !pSessionMgr->IsSessionHost() )
    {
        // This must have come from the host
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
        {
            IN_ADDR inaddr = {0};
            if( !i->GetInAddrForSession( &inaddr, sessionID ) )
            {
                continue;
            }
        
            DWORD dwUserMask = 0;
            for ( UINT j = 0; j < GoodBye.cXuids; ++j )
            {
                if( i->IndexPlayer( GoodBye.xuids[j] ) != -1 )
                {
                    dwUserMask |= 1<<j;
                }
            }

            if( dwUserMask )
            {
                DebugSpew( "DoNetMsgGoodbye(%016I64X): Removing remote userMask %d \n", XNKIDToInt64( sessionID ), dwUserMask );

                // Flag this client as being dropped so we don't try to send messages to him
                i->bDroppingClient = TRUE;

                // Write stats for the leaving players
                ScheduleSessionWriteTasks( pSessionMgr, &(*i), dwUserMask );

                // If the session host is leaving, initiate host migration
                if( pSessionMgr->GetHostInAddr() == i->addr )
                {
                    DebugSpew( "DoNetMsgGoodbye(%016I64X): Client %I64u is the host! " 
                               "Beginning host migration...\n", XNKIDToInt64( sessionID ), i->id );

                    ScheduleSessionMigrationTasks( pSessionMgr );
                }

                // Remove the leaving players from the session
                ScheduleSessionLeaveTasks( pSessionMgr, &(*i), dwUserMask );
            }
        }
    }
    else
    {
        // Find the client who left
        ULONGLONG id = 0;

        ClientInfoVec::iterator it;
        for( it = m_vecRemote.begin(); it != m_vecRemote.end(); ++it )
        {
            IN_ADDR inaddr = {0};
            if( !it->GetInAddrForSession( &inaddr, sessionID ) )
            {
                continue;
            }

            if( inaddr == inaddrFrom )
            {
                id = it->id;
                break;
            }
        }

        if( id != 0 && it != m_vecRemote.end() )
        {
            // Send a MSG_PLAYER_INFO to all clients with the XUIDs of the
            // remaining players in the session
            CMessage msgPlayerInfo( MSG_PLAYER_INFO );
            msgPlayerInfo.SetSessionID( sessionID );
            MsgPlayerInfo& PlayerInfo = msgPlayerInfo.GetPlayerInfo();
            PlayerInfo.id = id;

            // Determine which players are sticking around
            UINT cPlayers = 0;
            BYTE dwUserMask = 0;
            for ( UINT i = 0; i < it->cPlayers; ++i )
            {                
                BOOL bToKeep = TRUE;
                for ( UINT j = 0; j < GoodBye.cXuids; ++j )
                {
                    if( it->xuids[ i ] == GoodBye.xuids[j] )
                    {
                        bToKeep = FALSE;
                    }
                }

                if( bToKeep )
                {
                    memcpy_s( &PlayerInfo.strGamertags[cPlayers], XUSER_NAME_SIZE, &it->strGamertags[ i ], XUSER_NAME_SIZE );
                    memcpy_s( &PlayerInfo.xuids[cPlayers], sizeof( XUID ), &it->xuids[ i ], sizeof( XUID ) );
                    cPlayers++;

                    dwUserMask |= 1 << it->nController[ i ];
                }
            }
            
            PlayerInfo.cPlayers = cPlayers;

            DebugSpew( "DoNetMsgGoodbye(%016I64X): Sending MSG_PLAYER_INFO for userMask %d\n",
                       XNKIDToInt64( sessionID ), dwUserMask );

            // Forward this message to all clients in the session
            for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
            {
                IN_ADDR inaddr = {0};
                if( !i->GetInAddrForSession( &inaddr, sessionID ) )
                {
                    continue;
                }
                
                // Don't send to the dropped player
                if( inaddr == inaddrFrom )
                {
                    continue;
                }

                // Only send to clients in the session
                if( i->IsInSession( sessionID ) )
                {
                    SendMessage( &msgPlayerInfo, inaddrFrom );
                }
            }

            // Forward to ourselves
            SendMessage( &msgPlayerInfo, NULLADDR );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: ClientDropped()
// Desc: Process a client leaving the session
//--------------------------------------------------------------------------------------
VOID Sample::ClientDropped( IN_ADDR addrClient, const XNKID& sessionID, DWORD userMask )
{
    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID );
    if( !pSessionMgr )
    {
        return;
    }

    //// If we currently are deleting this session, then we're already going to handle
    //// removing the client, so do nothing here
    //if( GetSessionManagerTaskData( pSessionMgr, SessionManagementTask_DELETE ) )
    //{
    //    DebugSpew( "ClientDropped(%016I64X): Already deleting session, so ignoring...\n", XNKIDToInt64( sessionID ) );
    //    return;
    //}

    const IN_ADDR hostInAddr = pSessionMgr->GetHostInAddr();

    if( !pSessionMgr->IsSessionHost() )
    {
        // This must have come from the host

        BOOL bDropping = FALSE;

        ClientInfoVec::iterator i = m_vecRemote.end();
        for( i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
        {
            IN_ADDR in_addr = {0};
            if( i->GetInAddrForSession( &in_addr, sessionID ) && 
                in_addr == addrClient && 
                !i->bDroppingClient )
            {
                bDropping = TRUE;

                // Flag this client as being dropped. We use this to prevent ourselves
                // from sending the client messages, and to remove the client from
                // our list of remote clients after removing its players from the session
                i->bDroppingClient = TRUE;

                DebugSpew( "ClientDropped(%016I64X): Processing dropped client %I64u...\n", XNKIDToInt64( sessionID ), i->id );

                // Client found, so break out of for loop
                break;
            }
        }

        if( bDropping && i != m_vecRemote.end() )
        {
            if( hostInAddr == addrClient )
            {
                DebugSpew( "ClientDropped(%016I64X): Client %I64u is the host! " 
                           "Beginning host migration...\n", XNKIDToInt64( sessionID ), i->id );

                ScheduleSessionMigrationTasks( pSessionMgr );
            }            

            ScheduleSessionLeaveTasks( pSessionMgr, &(*i), 0xF );
        }
    }
    else
    {
        // Find the dropped client
        ULONGLONG id = 0;

        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
        {
            IN_ADDR inaddr = {0};
            if( i->GetInAddrForSession( &inaddr, sessionID ) && inaddr == addrClient )
            {
                id = i->id;
                break;
            }
        }

        if( id != 0 )
        {
            // Tell everyone this client is gone
            CMessage msgPlayerInfo( MSG_PLAYER_INFO );
            msgPlayerInfo.SetSessionID( sessionID );
            msgPlayerInfo.GetPlayerInfo().id       = id;
            msgPlayerInfo.GetPlayerInfo().cPlayers = 0;

            // send the message
            for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
            {
                IN_ADDR inaddr = {0};
                if( i->GetInAddrForSession( &inaddr, sessionID ) && i->id != id )
                {
                    DebugSpew( "ClientDropped(%016I64X): Sending MSG_PLAYER_INFO for client ID %I64u "  \
                               "to remote ID %I64u\n",
                               XNKIDToInt64( sessionID ), id, i->id );

                    SendMessage( &msgPlayerInfo, inaddr );
                }
            }

            // Send it to ourselves, too.
            DebugSpew( "ClientDropped(%016I64X): Sending MSG_PLAYER_INFO for client ID %I64u "  \
                       "to ourself\n",
                       XNKIDToInt64( sessionID ), id );

            SendMessage( &msgPlayerInfo, NULLADDR );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: HandleHeartbeats()
// Desc: Do housekeeping tasks for maintaining heartbeats for all sessions that
//       we're in
//--------------------------------------------------------------------------------------
VOID Sample::HandleHeartbeats()
{
    for( std::map<ULONGLONG, SessionManager*>::iterator iter = 
          m_mapSessions.begin(); iter != m_mapSessions.end(); iter++ )
    {
        SessionManager* pSessionMgr = iter->second;

        if( pSessionMgr )
        {
            HandleHeartbeat( pSessionMgr );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: HandleHeartbeat()
// Desc: Do housekeeping tasks for maintaining heartbeats
//--------------------------------------------------------------------------------------
VOID Sample::HandleHeartbeat( SessionManager* pSessionMgr )
{
    if( !pSessionMgr )
    {
        return;
    }

    XNKID sessionID = {0};

    // Are we in the middle of host migration
    const SessionState sessionState = pSessionMgr->GetSessionState();
    const BOOL bMigratingHost = ( sessionState >= SessionStateMigrateHost &&
                                  sessionState <= SessionStateMigratedHost );

    // If we're in the middle of host migration, then use
    // the migrated session ID; otherwise use the current session ID
    if( bMigratingHost )
    {
        sessionID = pSessionMgr->GetMigratedSessionID();
    }
    else
    {
        sessionID = pSessionMgr->GetSessionID();
    }

    // Check everybody's heartbeats
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        IN_ADDR inaddr = {0};
        if( !i->GetInAddrForSession( &inaddr, sessionID ) )
        {
            continue;
        }

        if( i->bDroppingClient )
        {
            continue;
        }

        // If we're in the middle of host migration, then don't count
        // this time against the client. Simply update its heartbeat
        if( bMigratingHost )
        {
            i->dwHeartbeat = GetTickCount();
            continue;
        }

        // If heartbeat test fails, drop the client and break out of our
        // for-loop
        if( FAILED( HandleHeartbeat( &(*i), sessionID ) ) )
        {
            ClientDropped( inaddr, sessionID );
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: HandleHeartbeat()
// Desc: Send a heartbeat and check last received heartbeat
//--------------------------------------------------------------------------------------
HRESULT Sample::HandleHeartbeat( ClientInfo* pClient, const XNKID& sessionID )
{
    if ( !pClient )
    {
        return E_FAIL;
    }

    // If we're already dropping the client return S_OK
    if( pClient->bDroppingClient )
    {
        return S_OK;
    }

    const __int64 sessionIDInt64 = XNKIDToInt64( sessionID );

    // Is this client in this session? If not, exit
    if( !pClient->IsInSession( sessionID ) )
    {
        //DebugSpew( "HandleHeartbeat(%016I64X, %I64u): Client ID %I64u not in session, so not sending heartbeat\n",
        //           sessionIDInt64, 
        //           m_Local.id,
        //           pClient->id );

        return S_OK;
    }

    const DWORD dwTicks = GetTickCount();

    // If the timer for this client hasn't elapsed, do nothing
    if( dwTicks - pClient->dwHeartbeatTimer < HEARTBEAT_TIME )
        return S_OK;

    // Reset the timer
    pClient->dwHeartbeatTimer = dwTicks;

    // Send a heartbeat to the client
    CMessage msgHeartbeat( MSG_HEARTBEAT );
    msgHeartbeat.SetSessionID( sessionID );

    IN_ADDR inaddr = {0};
    if( !pClient->GetInAddrForSession( &inaddr, sessionID ) )
    {
        return S_OK;
    }

    //DebugSpew( "HandleHeartbeat(%016I64X, %I64u): Sending hearbeat to ID %I64u\n",
    //           sessionIDInt64, 
    //           m_Local.id,
    //           pClient->id );

    SendMessage( &msgHeartbeat, inaddr );

    // Check to see if the client has timed out
    const DWORD dwTickCount = GetTickCount();
    BOOL bTimedOut = ( dwTickCount - pClient->dwHeartbeat > HEARTBEAT_TIMEOUT );

    if( bTimedOut )
    {
        DebugSpew( "HandleHeartbeat(%016I64X, %I64u): Client ID %I64u failed heartbeat test for session\n" \
                   "Last heartbeat %d ticks ago..\n",
                   sessionIDInt64, 
                   m_Local.id,
                   pClient->id, 
                   dwTickCount - pClient->dwHeartbeat );
    }

    return bTimedOut ? E_FAIL : S_OK;
}

//--------------------------------------------------------------------------------------
// Name: HandleIdle()
// Desc: Check and update idle status of all local controllers
//--------------------------------------------------------------------------------------
void Sample::HandleIdle()
{
    DWORD dwTicks = GetTickCount();

    for( UINT i = 0; i < MAX_USER_COUNT; ++i )
    {
        BOOL bIdle = dwTicks - m_Local.lastActivity[ i ] > IDLE_TIMEOUT;
        if( bIdle )
        {
            m_Local.bIsIdle[ i ] = TRUE;
        }
        else
        {
            m_Local.bIsIdle[ i ] = FALSE;
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: CalculateTeamTrueSkill()
// Desc: Calculate the aggregate TrueSkill(TM) of party members in the same
//       Presence session
//--------------------------------------------------------------------------------------
VOID Sample::CalculateTeamTrueSkill( DOUBLE& teamMu, DOUBLE& teamSigma )
{
    SessionManager* pSessionMgr = (SessionManager*)GetPresenceSession();
    if( !pSessionMgr )
    {
        return;
    }

    const size_t estimateMaxPlayers = m_Local.cPlayers + m_vecRemote.size() * MAX_USER_COUNT ;

    double* pMu     = new double[estimateMaxPlayers];
    double* pSigma  = new double[estimateMaxPlayers];
    
    if( !pMu || !pSigma )
    {
        FatalError( "new failed!" );
    }

    UINT idx = 0;

    // get values from our local non-idle, signed-in players
    for ( UINT i = 0; i < m_Local.cPlayers; ++i )
    {
        const XUSER_SIGNIN_INFO& info = m_SignInInfo[ m_Local.nController[ i ] ];
        if( info.UserSigninState != eXUserSigninState_NotSignedIn && !m_Local.bIsIdle[ i ] )
        {
            pMu[idx]    = m_Local.dMu[ i ];
            pSigma[idx] = m_Local.dSigma[ i ];
            idx++;
        }
    }

    // loop through the list of clients. Since m_vecRemote can in theory
    // contain both Presence and Matchmaking session clients, 
    // we err on the side of caution and only consider the TrueSkill(TM) of 
    // clients in our Presence session
    const XNKID sessionID = pSessionMgr->GetSessionID();
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        if( !i->IsInSession( sessionID ) )
        {
            continue;
        }

        for ( UINT j = 0; j < i->cPlayers; ++j )
        {
            pMu[idx]    = i->dMu[ j ];
            pSigma[idx] = i->dSigma[ j ];
            idx++;
        }
    }

    DWORD ret = XSessionCalculateSkill( idx, 
                                        pMu, 
                                        pSigma, 
                                        &teamMu, 
                                        &teamSigma );

    if( ret != ERROR_SUCCESS )
    {
        FatalError( "XSessionCalculateSkill failed: %d\n", ret );
    }

    delete[] pMu;
    delete[] pSigma;
}

//--------------------------------------------------------------------------------------
// Name: SetSessionTrueSkill()
// Desc: Set the TrueSkill(TM) of a session
//--------------------------------------------------------------------------------------
VOID Sample::SetSessionTrueSkill( SessionManager* pSessionMgr )
{
    const XUSER_SIGNIN_INFO& ownerSigninInfo = m_SignInInfo[ pSessionMgr->GetSessionOwner() ];
    if( !pSessionMgr || ownerSigninInfo.UserSigninState == eXUserSigninState_NotSignedIn )
    {
        return;
    }

    const XNKID& sessionID = pSessionMgr->GetSessionID();

    std::vector< XUID > xuids;

    // get XUIDS from our local non-idle, signed-in players
    for ( UINT i = 0; i < m_Local.cPlayers; ++i )
    {
        const XUSER_SIGNIN_INFO& localSigninInfo = m_SignInInfo[ m_Local.nController[ i ] ];
        if( localSigninInfo.UserSigninState != eXUserSigninState_NotSignedIn && !m_Local.bIsIdle[ i ] )
        {
            xuids.push_back( m_Local.xuids[ i ] );
        }
    }

    // loop through the list of clients. Since m_vecRemote can in theory
    // contain both Presence and Matchmaking session clients, 
    // we err on the side of caution and only consider the TrueSkill(TM) of 
    // clients in our session
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        if( !i->IsInSession( sessionID ) )
        {
            continue;
        }

        for ( UINT j = 0; j < i->cPlayers; ++j )
        {
            xuids.push_back( m_Local.xuids[ j ] );
        }
    }

    pSessionMgr->ModifySkill( xuids.size(), 
                              &(*xuids.begin()),
                              NULL );
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
    DWORD cNonIdlePlayers = 0;
    XUID adwXuids[ MAX_USER_COUNT ];
    for( UINT i = 0; i < pClient->cPlayers; ++i )
    {
        // Only consider non-idle players
        if( pClient->bIsIdle[ i ] )
        {
            continue;
        }
        adwXuids[ cNonIdlePlayers ] = pClient->xuids[ i ];
        cNonIdlePlayers++;
    }

    // Early out if all players are idle
    if( !cNonIdlePlayers )
    {
        return;
    }

    // define the specification for a Mu/Sigma skill view
    m_Spec.dwViewId = m_dwSkillViews[ m_nGameMode ];
    m_Spec.dwNumColumnIds = 2;
    m_Spec.rgwColumnIds[ 0 ] = X_STATS_COLUMN_SKILL_MU;
    m_Spec.rgwColumnIds[ 1 ] = X_STATS_COLUMN_SKILL_SIGMA;

    // Get the size of the buffer 
    DWORD  cbResults = 0;
    DWORD  ret;

    ret = XUserReadStats(
        0,                        // Current title ID
        cNonIdlePlayers,          // Number of users
        adwXuids,                 // Buffer of XUID's
        1,                        // Number of stats spec
        &m_Spec,                  // Stats spec
        &cbResults,               // Size of buffer
        NULL,                     // Pointer to results buffer
        NULL );                   // Pointer of an overlapped structure

    if( ret != ERROR_INSUFFICIENT_BUFFER )
        return;

    // Allocate the buffer
    m_pStats = ( PXUSER_STATS_READ_RESULTS ) new BYTE[ cbResults ];

    // read the results
    ret = XUserReadStats(
        0,                        // Current title ID
        cNonIdlePlayers,          // Number of users
        adwXuids,                 // Buffer of XUID's
        1,                        // Number of stats spec
        &m_Spec,                  // Stats spec
        &cbResults,               // Size of buffer
        m_pStats,                 // Pointer to results buffer
        NULL );                   // Pointer of an overlapped structure

    if( ret != ERROR_SUCCESS )
        return;

    DWORD iPlayer = 0;
    for( UINT i = 0; i < pClient->cPlayers; ++i )
    {
        // Only consider non-idle players
        if( pClient->bIsIdle[ i ] )
        {
            // Zero out Mu/Sigma of idle players first
            pClient->dMu[ i ] = 0.0;
            pClient->dSigma[ i ] = 0.0;

            continue;
        }

        pClient->dMu[ i ] = m_pStats->pViews[ 0 ].pRows[ iPlayer ].pColumns[ 0 ].Value.dblData;
        pClient->dSigma[ i ] = m_pStats->pViews[ 0 ].pRows[ iPlayer ].pColumns[ 1 ].Value.dblData;

        // this is a player new to the league
        if( pClient->dMu[ i ] == 0.0 && pClient->dSigma[ i ] == 0.0 )
        {
            pClient->dMu[ i ] = 3.0;
            pClient->dSigma[ i ] = 1.0;
        }

        iPlayer++;
    }

    // done!
    return;
}
//--------------------------------------------------------------------------------------
// Name: DebugDumpSessionMembers()
// Desc: Debug method to report session members
//--------------------------------------------------------------------------------------
VOID Sample::DebugDumpSessionMembers( SessionManager* pSessionMgr )
{
    #ifdef _DEBUG

    const XNKID& sessionID = pSessionMgr->GetSessionID();

    DebugSpew( "***************** DebugDumpSessionMembers *****************\n" \
               "instance: %p\n" \
               "sessionID: %016I64X\n",
               pSessionMgr,
               XNKIDToInt64( sessionID ) );

    if( m_Local.IsInSession( sessionID ) )
    {
        DebugSpew( "********* Console ID(local): %I64u; cPlayers: %d; bInvited: %d *********\n",
                   m_Local.id,
                   m_Local.cPlayers,
                   m_Local.bInvited );

        for( UINT i = 0; i < m_Local.cPlayers; ++i )
        {
            DebugSpew( "Player ID: 0x%016I64X; bUsesPrivateSlot: %d\n", 
                       m_Local.xuids[i],
                       m_Local.bUsesPrivateSlot[i] );
        }
    }

    for( ClientInfoVec::const_iterator it = m_vecRemote.begin(); 
         it != m_vecRemote.end(); ++it )
    {
        if( it->IsInSession( sessionID ) )
        {
            DebugSpew( "********* Console ID(remote): %I64u; cPlayers: %d; bInvited: %d *********\n",
                       it->id,
                       it->cPlayers,
                       it->bInvited );

            for( UINT i = 0; i < it->cPlayers; ++i )
            {
                DebugSpew( "Player ID: 0x%016I64X; bUsesPrivateSlot: %d\n", 
                           it->xuids[i],
                           it->bUsesPrivateSlot[i] );
            }
        }
        else
        {
            DebugSpew( "********* Console ID(remote): %I64u not in session!; cPlayers: %d; bInvited: %d *********\n",
                       it->id,
                       it->cPlayers,
                       it->bInvited );
        }
    }
    DebugSpew( "***********************************************************\n" );
    
    #endif
}

//--------------------------------------------------------------------------------------
// Name: StartGame()
// Desc: Notify all peers to begin playing
//--------------------------------------------------------------------------------------
VOID Sample::StartGame( const XNKID& sessionID )
{
    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID ); 
    if( !pSessionMgr )
    {
        return;
    }

    assert( pSessionMgr->IsSessionHost() );

    // Send a start message to everyone, including ourselves
    CMessage msg( MSG_START_SESSION );
    msg.SetSessionID( sessionID );

    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        IN_ADDR inaddr = {0};
        if( i->GetInAddrForSession( &inaddr, sessionID ) )
        {
            DebugSpew( "Sending client %p MSG_START_SESSION for session %016I64X\n", &(*i), XNKIDToInt64( sessionID ) );
            SendMessage( &msg, inaddr );
        }
    }

    DebugSpew( "Sending self MSG_START_SESSION for session %016I64X\n", XNKIDToInt64( sessionID ) );
    SendMessage( &msg, NULLADDR );
}

//--------------------------------------------------------------------------------------
// Name: EndGame()
// Desc: Notify all peers to end playing
//--------------------------------------------------------------------------------------
VOID Sample::EndGame( const XNKID& sessionID )
{
    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID ); 
    if( !pSessionMgr )
    {
        return;
    }

    assert( pSessionMgr->IsSessionHost() );

    // Send a end message to everyone, including ourselves
    CMessage msg( MSG_END_SESSION );
    msg.SetSessionID( sessionID );

    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        IN_ADDR inaddr = {0};
        if( i->GetInAddrForSession( &inaddr, sessionID ) )
        {
            DebugSpew( "Sending client %p MSG_END_SESSION for session %016I64X\n", &(*i), XNKIDToInt64( sessionID ) );
            SendMessage( &msg, inaddr );
        }
    }

    DebugSpew( "Sending self MSG_END_SESSION for session %016I64X\n", XNKIDToInt64( sessionID ) );
    SendMessage( &msg, NULLADDR );
}

//--------------------------------------------------------------------------------------
// Name: DoNetMsgStartGame()
// Desc: Begin the game
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgStartGame( const NetMsgData& netMsgData )   
{
    const XNKID sessionID = netMsgData.m_pMsg->GetSessionID();

    DebugSpew( "Received MSG_START_SESSION for session %016I64X\n", XNKIDToInt64( sessionID ) );

    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID ); 
    if( pSessionMgr )
    {
        ScheduleSessionStartTasks( pSessionMgr );
    }
}

//--------------------------------------------------------------------------------------
// Name: DoNetMsgEndGame()
// Desc: End the game
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgEndGame( const NetMsgData& netMsgData )   
{
    const XNKID sessionID = netMsgData.m_pMsg->GetSessionID();

    DebugSpew( "Received MSG_END_SESSION for session %016I64X\n", XNKIDToInt64( sessionID ) );

    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID ); 
    if( pSessionMgr )
    {
        // If this is an arbitrated session, we need
        // to write stats for ourselves and our peers. The same also
        // applies if we're the session host for any session type
        if( ( pSessionMgr->HasSessionFlags( XSESSION_CREATE_USES_ARBITRATION ) ) ||
            ( pSessionMgr->IsSessionHost() ) )
        {
            ScheduleSessionWriteTasks( pSessionMgr );
        }

        ScheduleSessionEndTasks( pSessionMgr );
    }
}

//--------------------------------------------------------------------------------------
// Name: DoNetMsgRegister()
// Desc: Receive order from host to register
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgRegister( const NetMsgData& netMsgData )
{
    const XNKID sessionID = netMsgData.m_pMsg->GetSessionID();

    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID ); 
    if( pSessionMgr )
    {
        ScheduleSessionRegisterTasks( pSessionMgr );
    }
}


//--------------------------------------------------------------------------------------
// Name: DoNetMsgRegistered()
// Desc: Handle a registered message
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgRegistered( const NetMsgData& netMsgData )
{
    const XNKID sessionID = netMsgData.m_pMsg->GetSessionID();
    const IN_ADDR inaddrFrom = netMsgData.m_inaddrFrom;

    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID ); 
    if( !pSessionMgr )
    {
        return;
    }

    assert( pSessionMgr->IsSessionHost() );

    // Loop through all clients in the session, mark this client as registered and check if all are
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        IN_ADDR inaddr = {0};
        if( i->GetInAddrForSession( &inaddr, sessionID ) )
        {
            if( inaddr == inaddrFrom )
            {
                i->bRegistered = TRUE;
            }
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: ProcessRegistrationList()
// Desc: Scan the arbitration registration results and boot anyone who didn't register
//--------------------------------------------------------------------------------------
VOID Sample::ProcessRegistrationList()
{
    SessionManager* pSessionMgr = GetMatchmakingSession();
    const XNKID& sessionID = pSessionMgr->GetSessionID();

    assert( pSessionMgr->IsSessionHost() );

    PXSESSION_REGISTRATION_RESULTS pRegistrationResults = 
        pSessionMgr->GetRegistrationResults();

    if( !pRegistrationResults )
    {
        return;
    }

    // Loop through the arbitrated list and flag the machines that showed up
    for( UINT n = 0; n < pRegistrationResults->wNumRegistrants; n++ )
    {
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
        {
            if( !i->IsInSession( sessionID ) )
            {
                continue;
            }

            assert( pRegistrationResults->rgRegistrants );
            const ULONGLONG qwMachineID = pRegistrationResults->rgRegistrants[ n ].qwMachineID;

            if( i->id == qwMachineID )
            {
                i->bRegistered = TRUE;
            }
        }
    }

    // Now loop through the client list one more time looking for unregistered clients and drop
    // them from the session
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        if( i->IsInSession( sessionID ) && !i->bRegistered )
        {
            IN_ADDR inaddr = {0};
            if( i->GetInAddrForSession( &inaddr, sessionID ) )
            {
                ClientDropped( inaddr, pSessionMgr->GetSessionID() );
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: DoNetMsgScorePoint()
// Desc: Handle a request to score a point
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgScorePoint( const NetMsgData& netMsgData )  
{
    const XNKID sessionID = netMsgData.m_pMsg->GetSessionID();

    // Can only score points when in-game
    if( m_AppState != APPSTATE_INGAME && m_AppState != APPSTATE_POSTGAME )
    {
        DebugSpew( "DoNetMsgScorePoint(%016I64X): Received MSG_SCORE_POINT while not in-game or in-postgame.\n", XNKIDToInt64( sessionID ) );
    }
    else
    {
        DebugSpew( "DoNetMsgScorePoint(%016I64X): Received MSG_SCORE_POINT\n", XNKIDToInt64( sessionID ) );
    }

    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID ); 
    if( !pSessionMgr )
    {
        return;
    }

    const MsgScorePoint ScorePointMsg = netMsgData.m_pMsg->GetScorePoint();

    // Find the player that's trying to score
    ClientInfo* pScorer = NULL;

    if( ScorePointMsg.id == m_Local.id )
    {
        pScorer = &m_Local;
    }
    else
    {
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
        {
            ClientInfo& Remote = *i;

            if( ( Remote.id == ScorePointMsg.id ) && Remote.IsInSession( sessionID ) )
            {
                pScorer = &Remote;
                break;
            }
        }
    }

    if( pScorer )
    {
        if( pSessionMgr->IsSessionHost() )
        {
            // If we're the host, pass the message along to everyone else
            for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
            {
                IN_ADDR inaddr = {0};
                if( i->GetInAddrForSession( &inaddr, sessionID ) )
                {
                    DebugSpew( "DoNetMsgScorePoint(%016I64X - host): Forwarding MSG_SCORE_POINT to client %I64u.\n", 
                               XNKIDToInt64( sessionID ),
                               i->id );

                    SendMessage( netMsgData.m_pMsg, inaddr );
                }
            }
        }

        // Increment the player's points
        for( UINT i = 0; i < pScorer->cPlayers; ++i )
        {
            if( XOnlineAreUsersIdentical( pScorer->xuids[ i ], ScorePointMsg.xuid ) )
            {
                ++pScorer->nPoints[ i ];

                DebugSpew( "DoNetMsgScorePoint(%016I64X): Player 0x%016I64X on client %I64u scored a point. New point total: %d\n", 
                           XNKIDToInt64( sessionID ),
                           pScorer->xuids[ i ],
                           pScorer->id,
                           pScorer->nPoints[ i ] );

                if( pScorer->nPoints[ i ] == m_nVictoryPoints )
                {
                    // Write stats
                    ScheduleSessionWriteTasks( pSessionMgr );

                    // If we're the session host, then end the session
                    // and tell all clients to do the same
                    if( pSessionMgr->IsSessionHost() )
                    {          
                        DebugSpew( "DoNetMsgScorePoint(%016I64X - host): Player 0x%016I64X on client %I64u won! Ending game\n", 
                                   XNKIDToInt64( sessionID ),
                                   pScorer->xuids[ i ],
                                   pScorer->id );

                        EndGame( sessionID );
                    }

                    return;
                }
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: DoNetMsgPointTotal()
// Desc: Handle an update to a player's point total
//--------------------------------------------------------------------------------------
VOID Sample::DoNetMsgPointTotal( const NetMsgData& netMsgData )
{
    const XNKID sessionID               = netMsgData.m_pMsg->GetSessionID();
    const MsgPointTotal PointTotalMsg   = netMsgData.m_pMsg->GetPointTotal();

    SessionManager* pSessionMgr = (SessionManager*)SessionManagerFromSessionID( sessionID ); 
    if( !pSessionMgr )
    {
        return;
    }

    // Find the player that's trying to score
    ClientInfo* pScorer = NULL;

    if( PointTotalMsg.id == m_Local.id )
    {
        pScorer = &m_Local;
    }
    else
    {
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); 
             i != m_vecRemote.end(); ++i )
        {
            if( !i->IsInSession( sessionID ) )
            {
                continue;
            }

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
                    // Write stats
                    ScheduleSessionWriteTasks( pSessionMgr );

                    // If we're the session host, then end the session
                    // and tell all clients to do the same
                    if( pSessionMgr->IsSessionHost() )
                    {
                        EndGame( sessionID );
                    }
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: WriteStats()
// Desc: Write stats for a particular user
//--------------------------------------------------------------------------------------
HRESULT Sample::WriteStats( SessionManager* pSessionMgr, 
                            const ClientInfo* pClient, 
                            const DWORD userMask,
                            XOVERLAPPED* pXOverlapped )
{
    if( !pSessionMgr || !userMask )
    {
        return S_OK;
    }

    // Nothing to do if stats are reported for this session
    if( !pSessionMgr->HasSessionFlags( XSESSION_CREATE_USES_STATS ) )
    {
        DebugSpew( "No stats reported for session %016I64X. Is that intentional?\n", pSessionMgr->GetSessionIDAsInt() );
        ScheduleChangeStateTask( APPSTATE_MAINMENU);
        return S_OK;
    }

    // Can only write stats between session start and session end
    const SessionState sessionState = pSessionMgr->GetSessionState();
    if( sessionState < SessionStateInGame || sessionState > SessionStateEnd )
    {
        DebugSpew( "Session %016I64X in wrong state to report stats: %s\n", 
                   pSessionMgr->GetSessionIDAsInt(),
                   pSessionMgr->GetSessionStateString() );
        ScheduleChangeStateTask( APPSTATE_MAINMENU);
        return S_OK;
    }

    // Figure out which leaderboards to write to
    DWORD dwLeaderboardTypeMode = 0;
    DWORD dwLeaderboardType     = 0;

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
        FatalError( "Unexpected type/mode pair" );
        break;
    }

    for( UINT i = 0; i < pClient->cPlayers; ++i )
    {
        if( ( userMask & 1 << pClient->nController[ i ] ) == 0 ) 
        {
            continue;
        }

        // Write stats to the skill leaderboard
        // Note that the value for determining Team ID is a quick-and-dirty way of trying
        // to ensure that each user has a unique team ID, since this is an individual game.
        // Titles should use a more robust method of assigning team ID.
        UINT cViews = 1;

        // Allocate Views and Properties on the heap since the SessionManager
        // class calls XSessionWriteStats asynchronously        
        XSESSION_VIEW_PROPERTIES* Views = 
            ( XSESSION_VIEW_PROPERTIES* )new BYTE[ sizeof( XSESSION_VIEW_PROPERTIES ) * 3 ];

        XUSER_PROPERTY* Skill = 
            ( XUSER_PROPERTY* )new BYTE[ sizeof( XUSER_PROPERTY ) * 2 ];

        XUSER_PROPERTY* Stats = 
            ( XUSER_PROPERTY* )new BYTE[ sizeof( XUSER_PROPERTY ) * 4 ];

        Skill[ 0 ].dwPropertyId = X_PROPERTY_RELATIVE_SCORE;
        Skill[ 0 ].value.nData  = pClient->nPoints[ i ];
        Skill[ 0 ].value.type   = XUSER_DATA_TYPE_INT32;
        Skill[ 1 ].dwPropertyId = X_PROPERTY_SESSION_TEAM;
        Skill[ 1 ].value.nData  = ( LONG )
            ( ( pClient->xuids[ i ] >> 32 ) ^ ( pClient->xuids [ i ] & MAXDWORD ) );
        Skill[ 1 ].value.type   = XUSER_DATA_TYPE_INT32;

        Views[ 0 ].dwNumProperties = 2;
        Views[ 0 ].dwViewId        = X_STATS_VIEW_SKILL;
        Views[ 0 ].pProperties     = Skill;

        // Write stats to the non-skill leaderboards
        // If this is a ranked game, write stats to non-skill leaderboards for every user
        // Otherwise, write stats to non-skill leaderboards only for local user
        if( m_nGameType == X_CONTEXT_GAME_TYPE_RANKED ||
            pClient     == &m_Local )
        {
            cViews = 3;

            Stats[ 0 ].dwPropertyId  = PROPERTY_GAMES_PLAYED;
            Stats[ 0 ].value.nData   = 1;
            Stats[ 0 ].value.type    = XUSER_DATA_TYPE_INT32;
            Stats[ 1 ].dwPropertyId  = PROPERTY_GAMES_WON;
            Stats[ 1 ].value.i64Data = pClient->nPoints[ i ] == m_nVictoryPoints;
            Stats[ 1 ].value.type    = XUSER_DATA_TYPE_INT64;
            Stats[ 2 ].dwPropertyId  = PROPERTY_POINTS_SCORED;
            Stats[ 2 ].value.nData   = pClient->nPoints[ i ];
            Stats[ 2 ].value.type    = XUSER_DATA_TYPE_INT32;
            Stats[ 3 ].dwPropertyId  = CONTEXT_MAP;
            Stats[ 3 ].value.nData   = m_nMap;
            Stats[ 3 ].value.type    = XUSER_DATA_TYPE_CONTEXT;

            Views[ 1 ].dwNumProperties = 4;
            Views[ 1 ].dwViewId        = dwLeaderboardTypeMode;
            Views[ 1 ].pProperties     = Stats;

            Views[ 2 ].dwNumProperties = 4;
            Views[ 2 ].dwViewId        = dwLeaderboardType;
            Views[ 2 ].pProperties     = Stats;
        }

        return pSessionMgr->WriteStats( pClient->xuids[ i ], cViews, Views, pXOverlapped );
    }

    return S_OK;
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
    DWORD  cbResults;
    DWORD  ret;

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
        FatalError( "XUserCreateStatsEnumerator...() failed with error %d", ret );
    }

    // Allocate the buffer
    m_pStats = ( PXUSER_STATS_READ_RESULTS ) new BYTE[ cbResults ];

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
        FatalError( "XEnumerate() failed with error %d", ret );
    }

    m_bReadingLeaderboard = TRUE;
}

const LocalClientInfo*  Sample::GetLocalClientInfo()
{
    return &m_Local; 
}

const ClientInfoVec* Sample::GetRemoteClientInfoVec() 
{ 
    return &m_vecRemote; 
}

DWORD Sample::GetGameType()
{ 
    return m_nGameType; 
}

DWORD Sample::GetGameMode()
{ 
    return m_nGameMode;
}

DWORD Sample::GetMap()
{ 
    return m_nMap; 
}

DWORD Sample::GetNumVictoryPoints()
{ 
    return m_nVictoryPoints; 
}

DWORD Sample::GetNumMinVictoryPoints()
{ 
    return m_nMinVictoryPoints; 
}

DWORD Sample::GetNumMaxVictoryPoints()
{ 
    return m_nMaxVictoryPoints; 
}

HRESULT Sample::SayGoodbye( SessionManager* pSessionMgr,
                            const DWORD dwXuidCount,
                            const XUID* pXuids,
                            const IN_ADDR& inaddr )
{
    CMessage msgGoodbye( MSG_GOODBYE );
    MsgGoodbye& GoodBye = msgGoodbye.GetGoodbye();
    msgGoodbye.SetSessionID( pSessionMgr->GetSessionID() );

    GoodBye.cXuids = dwXuidCount;
    memcpy_s( &GoodBye.xuids, MAX_USER_COUNT * sizeof( XUID ), pXuids, dwXuidCount * sizeof( XUID ) );

    SendMessage( &msgGoodbye, inaddr );

    return S_OK;
}

HRESULT Sample::JoinSession( const XNKID& sessionID, const IN_ADDR& hostInAddr, const BOOL bInvited )
{
    // Send a join message to the host
    CMessage msgJoinSession( MSG_JOIN_SESSION );
    msgJoinSession.SetSessionID( sessionID );

    MsgJoinSession& JoinSession = msgJoinSession.GetJoinSession();

    JoinSession.id       = m_Local.id;
    JoinSession.bInvited = bInvited;

    UINT idx = 0;
    for( UINT i = 0; i < m_Local.cPlayers; ++i )
    {
        // Add online non-idle users only
        const XUSER_SIGNIN_INFO& info = m_SignInInfo[ m_Local.nController[ i ] ];
        if( info.UserSigninState != eXUserSigninState_NotSignedIn && !m_Local.bIsIdle[ i ] )
        {
            JoinSession.nController[ idx ] = m_Local.nController[ i ];
            JoinSession.xuids[ idx ]       = info.xuid;

            MultiByteToWideChar( CP_ACP, 0, info.szUserName, -1,
                JoinSession.strGamertags[ idx ], XUSER_NAME_SIZE );

            DebugSpew( "Attempting join local user 0x%016I64X to remote session %016I64X\n", 
                       JoinSession.xuids[ idx ],
                       XNKIDToInt64( sessionID ) );

            idx++;
        }
    }
    
    // Set count of players we're attempting to join
    JoinSession.cPlayers = idx;

    // Send message to session host
    SendMessage( &msgJoinSession, hostInAddr );

    // Schedule a task to process connecting to the session
    ScheduleSessionConnectionTasks( const_cast<SessionManager*>( SessionManagerFromSessionID( sessionID ) ) );

    return S_OK;
}

HRESULT Sample::JoinSessionPartyHost( SessionManager* pPresenceSessionMgr,
                                      const XNKID& sessionID, 
                                      const IN_ADDR& hostInAddr, 
                                      const BOOL bInvited )
{
    const XNKID presenceSessionID = pPresenceSessionMgr->GetSessionID();

    // Send a join party message to the host
    CMessage msgJoinSessionParty( MSG_JOIN_SESSION_PARTY );
    msgJoinSessionParty.SetSessionID( sessionID );

    int cPartyMembers = m_Local.cPlayers;
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        if( i->IsInSession( presenceSessionID ) )
        {
            cPartyMembers = cPartyMembers + i->cPlayers;
        }
    }
    msgJoinSessionParty.GetJoinSessionParty().cPlayers = cPartyMembers;
    msgJoinSessionParty.GetJoinSessionParty().bInvited = bInvited;

    DebugSpew( "Sending MSG_JOIN_SESSION_PARTY for session %016I64X for %d players\n", XNKIDToInt64( sessionID ), cPartyMembers );

    // Send the message
    SendMessage( &msgJoinSessionParty, hostInAddr );

    // Schedule a task to process connecting to the session
    ScheduleSessionConnectionTasks( pPresenceSessionMgr );

    return S_OK;
}

HRESULT Sample::LeaveSession( SessionManager* pSessionMgr )
{
    const XNKID& sessionID = pSessionMgr->GetSessionID();

    // Send a MSG_LEFT_SESSION message to all members in our Presence session. But
    // only do so if we're the host of the Presence session
    SessionManager* pPresenceSessionMgr = GetPresenceSession(); 
    if( !pPresenceSessionMgr ||
        !pPresenceSessionMgr->IsSessionHost() )
    {
        return S_OK;
    }

    const XNKID& presenceSessionID = pPresenceSessionMgr->GetSessionID();

    CMessage msgLeftSession( MSG_LEFT_SESSION );
    msgLeftSession.SetSessionID( sessionID );

    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        // Send message to clients of our Presence session
        if( i->IsInSession( presenceSessionID ) )
        {
            DebugSpew( "Sending MSG_LEFT_SESSION for session %016I64X " \
                       "to Presence session client ID %I64u\n",
                       XNKIDToInt64( sessionID ),
                       i->id );

            SendMessage( &msgLeftSession, i->addr );
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: ReleaseTaskCallback()
// Desc: Callback to determine if a task should be deleted or not
//--------------------------------------------------------------------------------------
BOOL Sample::ReleaseTaskCallback( const TASKHANDLE* phTask, const void* pTaskData, void* pUserContext )
{
    // User context is a SessionManager instance
    SessionManager* pSessionMgr = ( SessionManager* )pUserContext;

    // pTaskData is a TaskSchedulerTaskData instance
    TaskSchedulerTaskData* pTaskSchedulerTaskData = (TaskSchedulerTaskData*)pTaskData;

    // If task data references pSessionMgr, then release this task. Otherwise don't
    return ( pTaskSchedulerTaskData->m_pSessionMgr == pSessionMgr ) ? TRUE : FALSE;
}

//--------------------------------------------------------------------------------------
// Name: DoTaskCallback()
// Desc: Top-level task handler for all TaskScheduler-scheduled tasks
//--------------------------------------------------------------------------------------
HRESULT Sample::DoTaskCallback( const WORD taskID, const TASKHANDLE* pHandle, void* pTaskData )
{
    TaskSchedulerTaskData* pTSTaskData = (TaskSchedulerTaskData*)pTaskData;

    Sample* pSample                 = pTSTaskData->m_pSample;
    SessionManager* pSessionMgr     = pTSTaskData->m_pSessionMgr;
    XOVERLAPPED* pXOverlapped       = pTSTaskData->m_pXOverlapped;
    const APPSTATE newAppState      = pTSTaskData->m_newAppState;

	HRESULT hr = S_OK;

	DebugSpew( "Sample::DoTaskCallback(threadID 0x%x) - taskID:0x%x; pTaskData:0x%p; ", 
			   GetCurrentThreadId(), taskID, pTaskData );

    switch( TASK_AREA( taskID ) )
    {
    case TASKAREA_APP:
		DebugSpew( "TASKAREA_APP\n" ); 
        if( taskID == TASK_APP_CHANGE_STATE )
        {
            hr = pSample->DoSwitchAppStateTask( newAppState, pSessionMgr );
        }
        break;
    case TASKAREA_XSESSION:
		DebugSpew( "TASKAREA_XSESSION\n" ); 
        hr = pSample->DoSessionTask( taskID, pHandle, pTSTaskData, pSessionMgr, pXOverlapped );
        break;
    case TASKAREA_NETWORKMESSAGE:
		DebugSpew( "TASKAREA_NETWORKMESSAGE\n" ); 
        hr = pSample->DoNetMsgTask( taskID, pTSTaskData->m_NetMsgData );
        break;
    case TASKAREA_XOVERLAPPED:
		DebugSpew( "TASKAREA_XOVERLAPPED\n" ); 
        hr = pSample->DoOverlappedTask( taskID, pHandle, pTSTaskData, pSessionMgr, pXOverlapped );
        break;
    case TASKAREA_PRESENCE:
		DebugSpew( "TASKAREA_PRESENCE\n" ); 
        hr = pSample->DoPresenceTask( taskID, 
                                      pTSTaskData->m_EnumPresenceData.m_dwUserIndex, 
                                      &pTSTaskData->m_EnumPresenceData.m_hEnum, 
                                      pXOverlapped );
        break;
    case TASKAREA_CONTEXT:
		DebugSpew( "TASKAREA_CONTEXT\n" ); 
        hr = pSample->DoUpdateUserContextTask( taskID, 
                                               pTSTaskData->m_UserContextData.m_dwUserIndex, 
                                               pTSTaskData->m_UserContextData.m_dwContextID,
                                               pTSTaskData->m_UserContextData.m_dwContextValue,
                                               pXOverlapped );
        break;
	default:
		DebugSpew( "\n" ); 
		break;
	}

    // If synchronous task and not TASK_XOVERLAPPED_WAIT, immediately release held task data
    if( !TASK_ASYNC( taskID ) && ( taskID != TASK_XOVERLAPPED_WAIT ) )
    {
        pTSTaskData->Release();
    }

	return hr;
}

//--------------------------------------------------------------------------------------
// Name: DoSwitchAppStateTask()
// Desc: Handle app state switch tasks
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSwitchAppStateTask( const APPSTATE NewState, SessionManager* pSessionMgr )
{
    assert(m_AppState < _countof(m_astrAppStates));
    DebugSpew( "Switching from app state %s to %s\n", 
                m_astrAppStates[m_AppState], 
                m_astrAppStates[NewState] );

    // Clean up from the previous state
    switch( m_AppState )
    {
        case APPSTATE_SEARCH_MATCHMAKING_DONE:
            delete[] m_pSearchResults;
            m_pSearchResults = NULL;
            if( m_pQoSResult )
            {
                XNetQosRelease( m_pQoSResult );
            }
            m_pQoSResult = NULL;
            m_bQoSTesting = FALSE;
            break;

        #ifdef _XBOX
        case APPSTATE_VIEWSTATS:
            if( m_pStats )
            {
                delete[] m_pStats;
                m_pStats = NULL;
            }
            break;
        #endif

        case APPSTATE_POSTGAME:
        {
            SessionManager* pMatchmakingSessionMgr = GetMatchmakingSession(); 
            if( !pMatchmakingSessionMgr )
            {
                break;
            }            
        }
    }

    // Initialize the next state
    switch( NewState )
    {
        case APPSTATE_SEARCHUI_MATCHMAKING:
            {
            m_pSessionMgrCtx    = NULL;
            m_nMinVictoryPoints = VICTORY_POINTS_MIN;
            m_nMaxVictoryPoints = VICTORY_POINTS_MAX;
            m_nGameType = X_CONTEXT_GAME_TYPE_STANDARD;
            m_nGameMode = 0;
            m_nMap      = m_cMaps;
            }
            break;

        case APPSTATE_SEARCH_MATCHMAKING:
            m_pQoSResult = NULL;
            break;

        case APPSTATE_CREATE_PRESENCE_UI:
            m_pSessionMgrCtx = NULL;
            m_nGameType      = X_CONTEXT_GAME_TYPE_STANDARD;
            break;

        case APPSTATE_CREATE_MATCHMAKING_UI:
            {
            m_pSessionMgrCtx = NULL;
            m_nVictoryPoints = ( VICTORY_POINTS_MIN + VICTORY_POINTS_MAX ) / 2;
            m_nGameType      = X_CONTEXT_GAME_TYPE_STANDARD;
            m_nGameMode      = 0;
            m_nMap           = 0;
            }
            break;

        case APPSTATE_CREATE_MATCHMAKING:
            {
            DWORD dwOwnerController = pSessionMgr->GetSessionOwner();

            // Clear our client list of anyone not in our Presence session
            SessionManager* pPresenceSessionMgr = GetPresenceSession();
            if( pPresenceSessionMgr == NULL )
            {
                m_vecRemote.clear();
            }
            else
            {
                const XNKID sessionID = pPresenceSessionMgr->GetSessionID();
                ClientInfoVec::iterator i;
                for( i = m_vecRemote.begin(); i != m_vecRemote.end(); )
                {
                    if( i->IsInSession( sessionID ) )
                    {
                        ++i;
                    }
                    else
                    {
                        i = m_vecRemote.erase( i );
                    }
                }
            }

            // Initialize our session manager
            SessionManagerInitParams initParams;
            initParams.m_SessionCreationReason  = SessionCreationReasonHosting;
            initParams.m_bIsHost                = TRUE;
            initParams.m_dwSessionFlags         = pSessionMgr->GetSessionFlags() | MATCHMAKING_SESSION_FLAGS;
            initParams.m_dwMaxPublicSlots       = MATCHMAKING_SESSION_PUBLICSLOTS;
            initParams.m_dwMaxPrivateSlots      = MATCHMAKING_SESSION_PRIVATESLOTS;

            // If the gametype is ranked, but sure to add the 
            // XSESSION_CREATE_USES_ARBITRATION and XSESSION_CREATE_USES_STATS flags
            if( m_nGameType == X_CONTEXT_GAME_TYPE_RANKED )
            {
                initParams.m_dwSessionFlags |= XSESSION_CREATE_USES_ARBITRATION
                                            |  XSESSION_CREATE_USES_STATS;
            }

            // If we're not currently in a Presence session, then add the
            // XSESSION_CREATE_USES_PRESENCE flag so that this session can
            // be joined via invites and presence
            if( !GetPresenceSession() )
            {
                initParams.m_dwSessionFlags |= XSESSION_CREATE_USES_PRESENCE;
            }

            // Initialize our session manager
            pSessionMgr->Initialize( initParams );

            pSessionMgr->SetHostInAddr( m_Local.addr );

            // Set up game-related properties
            XUserSetProperty( dwOwnerController, PROPERTY_VICTORY_POINTS, sizeof( DWORD ), &m_nVictoryPoints );
            XUserSetContext ( dwOwnerController, X_CONTEXT_GAME_MODE, m_nGameMode );
            XUserSetContext ( dwOwnerController, CONTEXT_MAP, m_nMap );
            XUserSetContext ( dwOwnerController, X_CONTEXT_GAME_TYPE, m_nGameType );
            
            // Schedule session creation task
            ScheduleSessionCreationTasks( pSessionMgr );
           }

            break;

        case APPSTATE_CREATE_PRESENCE:
            {
            DWORD dwOwnerController = pSessionMgr->GetSessionOwner();
            m_vecRemote.clear();

            // Initialize our session manager
            SessionManagerInitParams initParams;
            initParams.m_SessionCreationReason  = SessionCreationReasonHosting;
            initParams.m_bIsHost                = TRUE;
            initParams.m_dwSessionFlags         = pSessionMgr->GetSessionFlags() | PRESENCE_SESSION_FLAGS;
            initParams.m_dwMaxPublicSlots       = PRESENCE_SESSION_PUBLICSLOTS;
            initParams.m_dwMaxPrivateSlots      = PRESENCE_SESSION_PRIVATESLOTS;

            // If we're currently in a Matchmaking session that's presence-enabled, 
            // then our Presence session can't have the XSESSION_CREATE_USES_PRESENCE flag. 
            // This is because a user can only be in one presence-enabled session at a time.
            SessionManager* pMatchmakingSessionMgr = GetMatchmakingSession();
            if( pMatchmakingSessionMgr != NULL &&
                pMatchmakingSessionMgr->HasSessionFlags( XSESSION_CREATE_USES_PRESENCE ) )
            {
                initParams.m_dwSessionFlags &= ~XSESSION_CREATE_USES_PRESENCE;
            }

            // Initialize our session manager
            pSessionMgr->Initialize( initParams );

            pSessionMgr->SetHostInAddr( m_Local.addr );

            // Set up game type and game mode contexts
            // For a Presence session we always create an standard
            // session, and we always use its own, dedicated
            // game mode
            XUserSetContext ( dwOwnerController, 
                              X_CONTEXT_GAME_TYPE, 
                              X_CONTEXT_GAME_TYPE_STANDARD );

            XUserSetContext ( dwOwnerController, 
                              X_CONTEXT_GAME_MODE, 
                              Sample::m_cPartySessionGameMode );

            // Schedule session creation task
            ScheduleSessionCreationTasks( pSessionMgr );
            }
            
            break;

        case APPSTATE_CREATE_SESSION_UNHOSTED:
            {
            // Schedule session creation task
            ScheduleSessionCreationTasks( pSessionMgr );
            }
            break;

        case APPSTATE_PREGAME:
            // Do nothing
            break;

        case APPSTATE_REGISTERED:
            {
                // If we're the host, start the game once registration is complete
                SessionManager* pMatchmakingSessionMgr = GetMatchmakingSession(); 
                if( !pMatchmakingSessionMgr )
                {
                    FatalError( "Unexpectedly found no matchmaking session!\n" );
                }

                if( pMatchmakingSessionMgr->IsSessionHost() )
                {
                    if( m_vecRemote.empty() )
                    {
                        pMatchmakingSessionMgr->SetSessionError( L"Can't have an arbitrated session with only one player." );
                        m_AppState = APPSTATE_PREGAME;
                    }
                    else
                    {
                        const XNKID sessionID = pMatchmakingSessionMgr->GetSessionID();
                        StartGame( sessionID );
                    }
                }
            }
            break;

        case APPSTATE_STARTING:
            break;

        #ifdef _XBOX
        case APPSTATE_VIEWSTATS:
            m_nLeaderboard = 0;
            m_nGameType    = 0;
            ReadLeaderboard( 1, 0 );
            break;
        #endif
    }

    m_nMenuItem = 0;
    m_AppState = NewState;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DoPresenceTask()
// Desc: Handle presence enumeration tasks
//--------------------------------------------------------------------------------------
HRESULT Sample::DoPresenceTask( const WORD taskID,
                                const DWORD dwUserIndex,
                                HANDLE* phEnum,
                                XOVERLAPPED* pXOverlapped  )
{
    HRESULT hr = S_OK;

    DebugSpew( "Sample::DoPresenceTask(threadID 0x%x) - taskID: 0x%x; dwUserIndex: %d; pXOverlapped: 0x%p; ", 
		       GetCurrentThreadId(), taskID, dwUserIndex, pXOverlapped );

    switch( taskID )
    {
    case TASK_PRESENCE_ENUMFRIENDS:
		DebugSpew( "TASK_PRESENCE_ENUMFRIENDS\n" );
        hr = DoPresenceEnumFriends( dwUserIndex, phEnum, pXOverlapped );
        break;
    case TASK_PRESENCE_ENUM:
		DebugSpew( "TASK_PRESENCE_ENUM\n" );
        hr = DoPresenceEnumNonFriends( dwUserIndex, phEnum, pXOverlapped );
        break;
    case TASK_PRESENCE_ENUMFRIENDS_DONE:
		DebugSpew( "TASK_PRESENCE_ENUMFRIENDS_DONE\n" );
        hr = DoPresenceEnumFriendsDone( dwUserIndex, phEnum, pXOverlapped );
        break;
    case TASK_PRESENCE_ENUM_DONE:
		DebugSpew( "TASK_PRESENCE_ENUM_DONE\n" );
        hr = DoPresenceEnumNonFriendsDone( dwUserIndex, phEnum, pXOverlapped );
        break;

	default:
		DebugSpew( "\n" ); 
		break;
	}

	return hr;
}
//--------------------------------------------------------------------------------------
// Name: DoUpdateUserContextTask()
// Desc: Handle context updates
//--------------------------------------------------------------------------------------
HRESULT Sample::DoUpdateUserContextTask( const WORD taskID,
                                         const DWORD dwUserIndex, 
                                         const DWORD dwContextID, 
                                         const DWORD dwContextValue,
                                         XOVERLAPPED* pXOverlapped )
{
    HRESULT hr = S_OK;

    DebugSpew( "Sample::DoUpdateUserContextTask(threadID 0x%x) - taskID: 0x%x; dwUserIndex: %d; dwContextID: %d; dwContextValue: %d; pXOverlapped: 0x%p; ", 
		       GetCurrentThreadId(), taskID, dwUserIndex, dwContextID, dwContextValue, pXOverlapped );

    switch( taskID )
    {
    case TASKAREA_CONTEXT_UPDATE_RP:
		DebugSpew( "TASKAREA_CONTEXT_UPDATE_RP\n" );
        XUserSetContextEx( dwUserIndex, dwContextID, dwContextValue, pXOverlapped );  ;
        break;
    case TASKAREA_CONTEXT_UPDATE_RP_DONE:
		DebugSpew( "TASKAREA_CONTEXT_UPDATE_RP_DONE\n" );
        break;

	default:
		DebugSpew( "\n" ); 
		break;
	}

	return hr;}


//--------------------------------------------------------------------------------------
// Name: DoOverlappedTask()
// Desc: Handle overlapped tasks
//--------------------------------------------------------------------------------------
HRESULT Sample::DoOverlappedTask( const WORD taskID, 
                                  const TASKHANDLE* pHandle,
                                  TaskSchedulerTaskData* pTSTaskData,
                                  SessionManager* pSessionMgr, 
                                  XOVERLAPPED* pXOverlapped  )
{
    HRESULT hr = S_OK;

    DebugSpew( "Sample::DoOverlappedTask(threadID 0x%x) - taskID: 0x%x; pXOverlapped: 0x%p; pSessionMgr: 0x%p", 
		       GetCurrentThreadId(), taskID, pXOverlapped, pSessionMgr );

    DWORD dwTickCountSinceExecutionStart = 0;

    switch( taskID )
    {
    case TASK_XOVERLAPPED_WAIT:
		DebugSpew( "TASK_XOVERLAPPED_WAIT\n" );

        // Is task done? If so, free its resources. If not, reschedule it.
        if( XHasOverlappedIoCompleted( pXOverlapped ) )
        {
            m_TaskScheduler.ReleaseTask( const_cast<TASKHANDLE*>(pHandle) );

            // release held task data
            pTSTaskData->Release();
        }
        else if( m_TaskScheduler.IsTaskExecuting( pHandle, &dwTickCountSinceExecutionStart ) )
        {
            // Task is still executing. If it's exceeded its max allowable execution time,
            // kill it. This is likely due to losing network connectivity to LIVE
            if( dwTickCountSinceExecutionStart > MAX_OVERLAPPED_TIME ) 
            {
                DebugSpew( "Sample::DoOverlappedTask(threadID 0x%x) - Overlapped task execution at index %u exceeded timeout. Releasing task\n", 
		                   GetCurrentThreadId(), pHandle->m_info.m_index );

                m_TaskScheduler.ReleaseTask( const_cast<TASKHANDLE*>(pHandle) );

                // release held task data
                pTSTaskData->Release();

                // Whatever SessionManager instance this task was working on is no longer valid. Force
                // immediate deletion of this session
                DeleteLocalSessions();
            }
            else
            {
                // Task still good. Reschedule and update bit in our taskdata to indicate this
                const_cast<TASKHANDLE*>(pHandle)->m_info.m_bRescheduled = TRUE;
                RescheduleTask( TASK_XOVERLAPPED_WAIT, pHandle );

                // Still need the task data
                pTSTaskData->AddRef();
            }
        }
        break;
    case TASK_XOVERLAPPED_CANCEL:
        {
        DebugSpew( "TASK_XOVERLAPPED_CANCEL\n" );
        
        // Cancel the overlapped. Note that this is a blocking call. It is safe
        // to schedule this task on any thread.
        if( ERROR_IO_INCOMPLETE == XGetOverlappedResult( pXOverlapped, NULL, FALSE ) )
        {
           XCancelOverlapped( pXOverlapped );
        }

        // Zero out the overlapped memory so that any scheduled TASK_XOVERLAPPED_WAIT task
        // waiting on this overlapped can be flushed out of the Scheduler's queue
        ZeroMemory( pXOverlapped, sizeof( XOVERLAPPED ) );

        if( !m_listSessionTaskGroupHandles.empty() )
        {
            // All overlapped calls are serialized with m_listSessionTaskGroupHandles, so
            // the overlapped operation we just cancelled is at the front of the queue.
            // Instruct the scheduler to cancel the task group and all associated tasks
            TASKHANDLE& hTaskGroup = m_listSessionTaskGroupHandles.front();

            DebugSpew( "Sample::DoOverlappedTask(threadID 0x%x) - Cancelling task group index at index %u\n", 
		               GetCurrentThreadId(), hTaskGroup.m_info.m_index );

            m_TaskScheduler.ReleaseTaskGroup( &hTaskGroup );

            // Remove front element
            m_listSessionTaskGroupHandles.pop_front();
        }

        break;
        }
    case TASK_XOVERLAPPED_CANCEL_DONE:
		DebugSpew( "TASK_XOVERLAPPED_CANCEL_DONE\n" );

        // Roll back pSessionMgr to the correct state it was in before the overlapped operation was started
        if( pSessionMgr )
        {
            pSessionMgr->NotifyOverlappedOperationCancelled( pXOverlapped );
        }

        break;

	default:
		DebugSpew( "\n" ); 
		break;
	}

	return hr;
}

//--------------------------------------------------------------------------------------
// Name: DoNetMsgTask()
// Desc: Handle network-related tasks
//--------------------------------------------------------------------------------------
HRESULT Sample::DoNetMsgTask( const WORD taskID, 
                              const NetMsgData& netMsgData )
{
	DebugSpew( "Sample::DoNetMsgTask(threadID 0x%x) - taskID: 0x%x; netMsgData.m_pMsg: 0x%p; ", 
			   GetCurrentThreadId(), taskID, netMsgData.m_pMsg );

    switch( taskID )
    {
    case TASK_NETMSG_START_SESSION:
		DebugSpew( "TASK_NETMSG_START_SESSION\n" );
        DoNetMsgStartGame( netMsgData );
        break;  
    case TASK_NETMSG_QUERY_SESSION:
		DebugSpew( "TASK_NETMSG_QUERY_SESSION\n" );
        DoNetMsgQuerySession( netMsgData );
        break;  
    case TASK_NETMSG_RESP_SESSION:
		DebugSpew( "TASK_NETMSG_RESP_SESSION\n" );
        DoNetMsgRespSession( netMsgData );
        break;  
    case TASK_NETMSG_JOIN_SESSION:
		DebugSpew( "TASK_NETMSG_JOIN_SESSION\n" );
        DoNetMsgJoinSession( netMsgData );
        break;  
    case TASK_NETMSG_JOIN_SESSION_PARTY:
		DebugSpew( "TASK_NETMSG_JOIN_SESSION_PARTY\n" );
        DoNetMsgJoinSessionParty( netMsgData );
        break;  
    case TASK_NETMSG_FOUND_SESSION:
		DebugSpew( "TASK_NETMSG_FOUND_SESSION\n" );
        DoNetMsgFoundSession( netMsgData );
        break;  
    case TASK_NETMSG_LEFT_SESSION:
		DebugSpew( "TASK_NETMSG_LEFT_SESSION\n" );
        DoNetMsgLeftSession( netMsgData );
        break;  
    case TASK_NETMSG_JOIN_RESPONSE:
		DebugSpew( "TASK_NETMSG_JOIN_RESPONSE\n" );
        DoNetMsgJoinResponse( netMsgData );
        break;  
    case TASK_NETMSG_JOIN_RESPONSE_PARTY:
		DebugSpew( "TASK_NETMSG_JOIN_RESPONSE_PARTY\n" );
        DoNetMsgJoinResponseParty( netMsgData );
        break;  
    case TASK_NETMSG_PLAYER_INFO:
		DebugSpew( "TASK_NETMSG_PLAYER_INFO\n" );
        DoNetMsgPlayerInfo( netMsgData );
        break;  
    case TASK_NETMSG_WAVE:
		DebugSpew( "TASK_NETMSG_WAVE\n" );
        DoNetMsgWave( netMsgData );
        break;  
    case TASK_NETMSG_HEARTBEAT:
		DebugSpew( "TASK_NETMSG_HEARTBEAT\n" );
        DoNetMsgHeartbeat( netMsgData );
        break;  
    case TASK_NETMSG_GOODBYE:
		DebugSpew( "TASK_NETMSG_GOODBYE\n" );
        DoNetMsgGoodbye( netMsgData );
        break;  
    case TASK_NETMSG_END_SESSION:
		DebugSpew( "TASK_NETMSG_END_SESSION\n" );
        DoNetMsgEndGame( netMsgData );
        break;  
    case TASK_NETMSG_REGISTER:
		DebugSpew( "TASK_NETMSG_REGISTER\n" );
        DoNetMsgRegister( netMsgData );
        break;  
    case TASK_NETMSG_REGISTERED:
		DebugSpew( "TASK_NETMSG_REGISTERED\n" );
        DoNetMsgRegistered( netMsgData );
        break;  
    case TASK_NETMSG_SCORE_POINT:
		DebugSpew( "TASK_NETMSG_SCORE_POINT\n" );
        DoNetMsgScorePoint( netMsgData );
        break;  
    case TASK_NETMSG_POINT_TOTAL:
		DebugSpew( "TASK_NETMSG_POINT_TOTAL\n" );
        DoNetMsgPointTotal( netMsgData );
        break;  
    case TASK_NETMSG_MIGRATE:
		DebugSpew( "TASK_NETMSG_MIGRATE\n" );
        m_HostMigration.ProcessMigrateMessage( netMsgData.m_pMsg, netMsgData.m_inaddrFrom );
        break; 
    default:
		DebugSpew( "\n" ); 
		break;
	}

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoPresenceEnumFriends()
// Desc: Handle enumerating friends presence
//--------------------------------------------------------------------------------------
HRESULT Sample::DoPresenceEnumFriends( const DWORD dwUserIndex, HANDLE* phEnum, XOVERLAPPED* pXOverlapped )
{
    // Only refresh presence data if online
    if( XUserGetSigninState( dwUserIndex ) == eXUserSigninState_SignedInToLive )
    {
        // Retrieve friends presence data synchronously
        if( FAILED( RetrieveFriendsPresenceData( dwUserIndex,
                                                 m_Local.friends[dwUserIndex],
                                                 sizeof( m_Local.friends[dwUserIndex] ),
                                                 &m_Local.cFriends[dwUserIndex],
                                                 pXOverlapped,
                                                 phEnum ) ) )
        {
            FatalError( "Sample::DoPresenceEnumFriends failed!\n" );
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoPresenceEnumNonFriends()
// Desc: Handle enumerating non-friends presence
//--------------------------------------------------------------------------------------
HRESULT Sample::DoPresenceEnumNonFriends( const DWORD dwUserIndex, HANDLE* phEnum, XOVERLAPPED* pXOverlapped )
{
    DebugSpew( "User index %d invoked non-friends presence update!\n", dwUserIndex );


    // Only refresh presence data if online
    if( XUserGetSigninState( dwUserIndex ) == eXUserSigninState_SignedInToLive )
    {
        const UINT cPeers = m_Local.presenceXuids[ dwUserIndex ].size();
        
        if( cPeers == 0 )
        {
            return S_OK;
        }

        // Pointers to the start of our XUIDs and XONLINE_PRESENCE arrays for this user index
        const XUID* pXuids = &( m_Local.presenceXuids[ dwUserIndex ][ 0 ] );
        XONLINE_PRESENCE* pArrayPresence = &( m_Local.presenceInfo[ dwUserIndex ][ 0 ] );
        const UINT cbPresenceArraySize = m_Local.presenceInfo[ dwUserIndex ].size() * sizeof( XONLINE_PRESENCE ); 

        // Retrieve non-friends presence data synchronously
        DWORD dwPresenceItemsRetrieved;
        if( FAILED( RetrievePresenceData( dwUserIndex,
                                          cPeers,
                                          pXuids,
                                          pArrayPresence,
                                          cbPresenceArraySize,
                                          &dwPresenceItemsRetrieved,
                                          pXOverlapped,
                                          phEnum ) ) )
        {
            FatalError( "Sample::DoPresenceEnumNonFriends failed!\n" );
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoPresenceEnumFriendsDone()
// Desc: Handle enumerated friends presence
//--------------------------------------------------------------------------------------
HRESULT Sample::DoPresenceEnumFriendsDone( const DWORD dwUserIndex, const HANDLE* phEnum, XOVERLAPPED* pXOverlapped )
{
    if( XUserGetSigninState( dwUserIndex ) == eXUserSigninState_SignedInToLive )
    {
        //
        // When enumerating presence is called asynchronously,
        // the number of presence
        // items returned can be retrieved from
        // the InternalHigh of the passed-in XOVERLAPPED structure 
        // when the asynchronous call has completed
        //
        if( pXOverlapped )
        {
            m_Local.cFriends[dwUserIndex] = pXOverlapped->InternalHigh;
        }

        // Cache friends xuid info for quick retrieval later
        for( DWORD iFriend = 0; iFriend < m_Local.cFriends[ dwUserIndex ]; ++iFriend )
        {
            m_Local.friendXuids[ dwUserIndex ][ iFriend ] = m_Local.friends[ dwUserIndex ][ iFriend ].xuid;
        }

        // Close enumerator handle if we enumerated presence async
        if( pXOverlapped )
        {
            XCloseHandle( *phEnum );
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoPresenceEnumFriends()
// Desc: Handle enumerated non-friends presence
//--------------------------------------------------------------------------------------
HRESULT Sample::DoPresenceEnumNonFriendsDone( const DWORD dwUserIndex, const HANDLE* phEnum, XOVERLAPPED* pXOverlapped )
{
    if( XUserGetSigninState( dwUserIndex ) == eXUserSigninState_SignedInToLive )
    {
        #ifdef _DEBUG
        const UINT cPeers = m_Local.presenceXuids[ dwUserIndex ].size();

        if( cPeers == 0 )
        {
            return S_OK;
        }

        XONLINE_PRESENCE* pArrayPresence = &( m_Local.presenceInfo[ dwUserIndex ][ 0 ] );
        DebugSpew( "***************** UpdateNonFriendsPresence *****************\n" );
        for( UINT i = 0; i < cPeers; ++i )
        {
            DebugSpew( "***************** XONLINE_PRESENCE %d *****************\n" \
                       "XUID: 0x%016I64X\n" \
                       "dwState: %d\n" \
                       "online?: %s\n",
                       i,
                       pArrayPresence[ i ].xuid,
                       pArrayPresence[ i ].dwState,
                       ( pArrayPresence[ i ].dwState & XONLINE_FRIENDSTATE_FLAG_ONLINE ) ? "yes" : "no" );            
        }
        #endif
    }

    // Close enumerator handle if we enumerated presence async
    if( pXOverlapped )
    {
        XCloseHandle( *phEnum );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoSessionTask()
// Desc: Handle session-related tasks
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionTask( const WORD taskID, 
                               const TASKHANDLE* pHandleTask, 
                               TaskSchedulerTaskData* pTSTaskData,
                               SessionManager* pSessionMgr,
                               XOVERLAPPED* pXOverlapped )
{
	HRESULT hr = S_OK;

	DebugSpew( "Sample::DoSessionTask(threadID 0x%x) - taskID:0x%x; pSessionMgr:0x%p; ", 
			   GetCurrentThreadId(), taskID, pSessionMgr );

    // If task is async, then initialize our overlapped. Remember
    // that we are serializing all XSession tasks (see ScheduleTasks() function), so there
    // is no danger of zero'ing out *pXOverlapped while it is being used for another
    // XSession call
    if( TASK_ASYNC( taskID ) && pXOverlapped )
    {
        ZeroMemory( pXOverlapped, sizeof( XOVERLAPPED ) );
    }

    switch( taskID )
    {
    case TASK_XSESSION_SEARCH:
		DebugSpew( "TASK_XSESSION_SEARCH\n" );
        hr = DoSessionSearchTask( pSessionMgr, pXOverlapped );
        break;
    case TASK_XSESSION_SEARCH_DONE:
		DebugSpew( "TASK_XSESSION_SEARCH_DONE\n" );
        hr = DoSessionSearchDoneTask( pSessionMgr, pXOverlapped );
        if( FAILED( hr ) )
        {
            delete pSessionMgr;
            m_pSessionMgrCtx = NULL;
        }
        break;
    case TASK_XSESSION_CREATE:
		DebugSpew( "TASK_XSESSION_CREATE\n" );
        hr = pSessionMgr->CreateSession( pXOverlapped );
        break;
    case TASK_XSESSION_CREATED:
		DebugSpew( "TASK_XSESSION_CREATED\n" );
        hr = DoSessionCreatedTask( pSessionMgr, pXOverlapped );
        break;
    case TASK_XSESSION_START:
		DebugSpew( "TASK_XSESSION_START\n" );
        hr = pSessionMgr->StartSession( pXOverlapped );
        break;
    case TASK_XSESSION_STARTED:
		DebugSpew( "TASK_XSESSION_STARTED\n" );
        hr = DoSessionStartedTask( pSessionMgr, pXOverlapped );
        break;
    case TASK_XSESSION_END:
		DebugSpew( "TASK_XSESSION_END\n" );
        if( pSessionMgr->GetSessionState() == SessionStateInGame )
        {
            hr = pSessionMgr->EndSession( pXOverlapped );
        }
        break;
    case TASK_XSESSION_ENDED:
		DebugSpew( "TASK_XSESSION_ENDED\n" );
        hr = DoSessionEndedTask( pSessionMgr, pXOverlapped );
        break;
    case TASK_XSESSION_WRITESTATS:
		DebugSpew( "TASK_XSESSION_WRITESTATS\n" );
        hr = DoSessionStatsWriteTask( pSessionMgr, pXOverlapped );
        break;
    case TASK_XSESSION_WROTESTATS:
		DebugSpew( "TASK_XSESSION_WROTESTATS\n" );
        hr = DoSessionStatsWrittenTask( pSessionMgr, pXOverlapped );
        break;
    case TASK_XSESSION_MODIFY:
		DebugSpew( "TASK_XSESSION_MODIFY\n" );
        // Do nothing if session is already deleted
        if( pSessionMgr->GetSessionState() < SessionStateDeleted )
        {
            hr = pSessionMgr->ModifySessionFlags( pTSTaskData->m_ModifyFlagsData.m_dwFlags, 
                                                  pTSTaskData->m_ModifyFlagsData.m_bClearFlags, 
                                                  pXOverlapped );
        }
        else
        {
	        DebugSpew( "Sample::DoSessionTask(threadID 0x%x) - taskID:0x%x; pSessionMgr:0x%p already in deleted state, so ignoring task\n", 
			           GetCurrentThreadId(), taskID, pSessionMgr );
        }
        break;
    case TASK_XSESSION_JOIN:
		DebugSpew( "TASK_XSESSION_JOIN\n" );
        DoSessionAddUsersTask( pTSTaskData->m_JoinLeaveData.m_pClient, 
                               pSessionMgr,
                               const_cast<DWORD*>( pTSTaskData->m_JoinLeaveData.m_aIndices ), 
                               const_cast<XUID*>( pTSTaskData->m_JoinLeaveData.m_aXuids ), 
                               const_cast<BOOL*>( pTSTaskData->m_JoinLeaveData.m_abPrivate ), 
                               pTSTaskData->m_JoinLeaveData.m_pClient->cPlayers,
                               pXOverlapped, 
                               pTSTaskData->m_JoinLeaveData.m_dwUserMask );
        break;
    case TASK_XSESSION_LEAVE:
		DebugSpew( "TASK_XSESSION_LEAVE\n" );
        DoSessionRemoveUsersTask( pTSTaskData->m_JoinLeaveData.m_pClient, 
                                  pSessionMgr, 
                                  const_cast<DWORD*>( pTSTaskData->m_JoinLeaveData.m_aIndices ), 
                                  const_cast<XUID*>( pTSTaskData->m_JoinLeaveData.m_aXuids ), 
                                  const_cast<BOOL*>( pTSTaskData->m_JoinLeaveData.m_abPrivate ),
                                  pTSTaskData->m_JoinLeaveData.m_pClient->cPlayers,
                                  pXOverlapped, 
                                  pTSTaskData->m_JoinLeaveData.m_dwUserMask );
        break;
    case TASK_XSESSION_JOINED_LOCAL:
		DebugSpew( "TASK_XSESSION_JOINED_LOCAL\n" );
        hr = DoSessionLocalPlayersAddedTask( pSessionMgr, pXOverlapped );
        break;
    case TASK_XSESSION_JOINED_REMOTE:
		DebugSpew( "TASK_XSESSION_JOINED_REMOTE\n" );
        hr = DoSessionRemotePlayersAddedTask( pSessionMgr, pXOverlapped );
        break;
    case TASK_XSESSION_LEFT_LOCAL:
		DebugSpew( "TASK_XSESSION_LEFT_LOCAL\n" );
        hr = DoSessionLocalPlayersRemovedTask( pSessionMgr, pXOverlapped );
        break;
    case TASK_XSESSION_LEFT_REMOTE:
		DebugSpew( "TASK_XSESSION_LEFT_REMOTE\n" );
        hr = DoSessionRemotePlayersRemovedTask( pSessionMgr, pXOverlapped );
        break;
    case TASK_XSESSION_DELETE:
		DebugSpew( "TASK_XSESSION_DELETE\n" );
        hr = pSessionMgr->DeleteSession( pXOverlapped );
        break;
    case TASK_XSESSION_DELETED:
		DebugSpew( "TASK_XSESSION_DELETED\n" );
        hr = DoSessionDeletedTask( pSessionMgr, pXOverlapped );
        break;
    case TASK_XSESSION_DELETEMGR:
		DebugSpew( "TASK_XSESSION_DELETEMGR\n" );
        hr = DoSessionMgrDeleteTask( pSessionMgr );
        break;
    case TASK_XSESSION_CONNECTING:
		DebugSpew( "TASK_XSESSION_CONNECTING\n" );
        //If we've never rescheduled this task, then start connection timer
        if( !pHandleTask->m_info.m_bRescheduled )
        {
            m_dwConnectionTimer = GetTickCount();
        }

        if( SUCCEEDED( DoSessionConnectingTask( pSessionMgr ) ) )
        {
            // Task done
            m_TaskScheduler.ReleaseTask( const_cast<TASKHANDLE*>(pHandleTask) );
        }
        else
        {
            // Task not done. Reschedule and update bit in our taskdata to indicate this
            const_cast<TASKHANDLE*>(pHandleTask)->m_info.m_bRescheduled = TRUE;
            RescheduleTask( TASK_XSESSION_CONNECTING, pHandleTask );

            // Still need the task data
            pTSTaskData->AddRef();
        }
        break;
    case TASK_XSESSION_MIGRATE:
		DebugSpew( "TASK_XSESSION_MIGRATE\n" );
        hr = DoSessionMigrateHostTask( pSessionMgr );
        break;
    case TASK_XSESSION_MIGRATING:
		DebugSpew( "TASK_XSESSION_MIGRATING\n" );
        if( SUCCEEDED( DoSessionMigratingHostTask( pSessionMgr ) ) )
        {
            // Task done
            m_TaskScheduler.ReleaseTask( const_cast<TASKHANDLE*>(pHandleTask) );
        }
        else
        {
            // Task not done. Reschedule and update bit in our taskdata to indicate this
            const_cast<TASKHANDLE*>(pHandleTask)->m_info.m_bRescheduled = TRUE;
            RescheduleTask( TASK_XSESSION_MIGRATING, pHandleTask );

            // Still need the task data
            pTSTaskData->AddRef();
        }
        break;
    case TASK_XSESSION_MIGRATED:
		DebugSpew( "TASK_XSESSION_MIGRATED\n" );
        hr = DoSessionMigratedHostTask( pSessionMgr );
        break;
    case TASK_XSESSION_REGISTER_HOST_BEGIN:
		DebugSpew( "TASK_XSESSION_REGISTER_HOST_BEGIN\n" );
        hr = DoSessionHostBeginRegisterTask( pSessionMgr );
        break;
    case TASK_XSESSION_REGISTER_HOST_WAIT:
		DebugSpew( "TASK_XSESSION_REGISTER_HOST_WAIT\n" );
        if( SUCCEEDED( DoSessionHostWaitRegisterTask( pSessionMgr ) ) )
        {
            // Task done. Release and schedule task to register ourselves
            m_TaskScheduler.ReleaseTask( const_cast<TASKHANDLE*>(pHandleTask) );
            ScheduleSessionRegisterTasks( pSessionMgr );
        }
        else
        {
            // Task not done. Reschedule and update bit in our taskdata to indicate this
            const_cast<TASKHANDLE*>(pHandleTask)->m_info.m_bRescheduled = TRUE;
            RescheduleTask( TASK_XSESSION_REGISTER_HOST_WAIT, pHandleTask );

            // Still need the task data
            pTSTaskData->AddRef();
        }
        break;
    case TASK_XSESSION_REGISTER:
		DebugSpew( "TASK_XSESSION_REGISTER\n" );
        hr = pSessionMgr->RegisterArbitration( pXOverlapped );
        break;
    case TASK_XSESSION_REGISTERED:
		DebugSpew( "TASK_XSESSION_REGISTERED\n" );
        hr = DoSessionRegisteredTask( pSessionMgr );
        break;  
    case TASK_XSESSION_TASKGROUPDONE:
        {
            DebugSpew( "TASK_XSESSION_TASKGROUPDONE\n" );
            if( !m_listSessionTaskGroupHandles.empty() )
            {
                // pHandleTaskGroup must refer to the first handle in our 
                // m_listSessionTaskGroupHandles list
                TASKHANDLE& hTaskGroup = m_listSessionTaskGroupHandles.front();
                assert( hTaskGroup.m_info.m_index == pHandleTask->m_indexGroup );

                DebugSpew( "Sample::DoSessionTask(threadID 0x%x) - Task group index %u completed execution\n", 
                           GetCurrentThreadId(), hTaskGroup.m_info.m_index );

                // Remove front element
                m_listSessionTaskGroupHandles.pop_front();
            }

            break;
        }
    default:
		DebugSpew( "\n" ); 
		break;
	}

	return hr;
}

//--------------------------------------------------------------------------------------
// Name: DoSessionCreatedTask()
// Desc: Handle creation of an XSession
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionCreatedTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped )
{
    if( SetLastXSessionError( pSessionMgr, pXOverlapped ) )
    {
        ScheduleSessionMgrDeletionTasks( pSessionMgr );
        ScheduleChangeStateTask( APPSTATE_MAINMENU, pSessionMgr );
        return S_OK;
    }

    // Add local console to the session
    m_Local.JoinedSession( pSessionMgr->GetSessionID(), 
                           pSessionMgr->GetHostInAddr(), 
                           m_Local.addr );

    // Update session state
    pSessionMgr->SwitchToState( SessionStateCreated );

    // Make sure the host hasn't forgotten about us
    HandleHeartbeat( pSessionMgr );

    // Store session nonce/session manager pair for quick lookup later.
    // For hosted sessions, from now on we will get this session manager instance by looking up
    // its nonce. For non-hosted sessions, the current session nonce is a temporary, randomly
    // generated nonce that we'll replace with the true session nonce from the session host
    // when we get it
    const ULONGLONG qwSessionNonce = pSessionMgr->GetSessionNonce();
    m_mapSessions.insert( NONCE_SESSION_PAIR( qwSessionNonce, pSessionMgr ) ); 

    SessionManager* pPresenceSessionMgr = GetPresenceSession();

    const BOOL bIsMatchmakingSession     = ( pSessionMgr->HasSessionFlags( MATCHMAKING_SESSION_FLAGS ) );
    const BOOL bInPresenceSession        = ( pPresenceSessionMgr ) ? TRUE : FALSE;
    const BOOL bInPresenceSessionAndHost = ( bInPresenceSession && pPresenceSessionMgr->IsSessionHost() );
    const BOOL bInvited                  = ( pSessionMgr->GetSessionCreationReason() != SessionCreationReasonJoinFromSearch );

    // If this is a Matchmaking session and we are the
    // host, then set up our QoS listener
    if(  bIsMatchmakingSession && pSessionMgr->IsSessionHost() )
    {
        pSessionMgr->StartQoSListener( ( BYTE* )m_QoSString, wcslen( m_QoSString ) * sizeof( WCHAR ), 0 );
    }

    // If this is a Matchmaking session and we're also hosting a Presence session, 
    // check the Matchmaking session for any modifiers flags
    // ( bits in XSESSION_CREATE_MODIFIERS_MASK ). Then, apply these flags to the Presence session
    if( bIsMatchmakingSession && bInPresenceSessionAndHost )
    {
        const DWORD dwModifiersToApply = pSessionMgr->GetSessionFlags() & XSESSION_CREATE_MODIFIERS_MASK; 
        ScheduleSessionModifyTasks( pPresenceSessionMgr, dwModifiersToApply );

    }

    const XSESSION_INFO& session_info = pSessionMgr->GetSessionInfo();

    #ifdef LIVE_ON_WINDOWS
    // Register local address as a secure address. We need this because
    // the local loopback address is not supported on GFW-L
    if( XNetXnAddrToInAddr( &m_Local.xnaddr,
                            &session_info.sessionID, &m_Local.addr ) != 0 )
    {
        DebugSpew( "XNetXnAddrToInAddr failed for local address." );
    }
    // Update the IN_ADDR of the host
    else if ( pSessionMgr->GetHostInAddr().S_un.S_addr == 0 )
    {
        pSessionMgr->SetHostInAddr( m_Local.addr );
    }
    #endif

    const IN_ADDR in_addr = pSessionMgr->GetHostInAddr();

    // If we're not hosting the session, then we'll have to join the remote session
    // and tell the host to remotely join us too
    if( !pSessionMgr->IsSessionHost() )
    {
        // Resolve the host's IP address
        if( XNetXnAddrToInAddr( &session_info.hostAddress,
                                &session_info.sessionID, (IN_ADDR*)&in_addr ) != 0 )
        {
            DebugSpew( "Could not resolve host's IP address." );
        }

        // If this is a Matchmaking session, we have to get ourselves,
        // and possibly, our Presence session members, into this session
        if( bIsMatchmakingSession )
        {
            if( bInPresenceSession )
            {
                // Get ourselves and our party members into this session
                if( bInPresenceSessionAndHost )
                {
                    JoinSessionPartyHost( pPresenceSessionMgr, session_info.sessionID, in_addr, bInvited );
                }
                else
                {
                    // Just get ourselves into this session
                    JoinSession( session_info.sessionID, in_addr, bInvited );
                }
            }
            else
            {
                // Just get ourselves into this session
                JoinSession( session_info.sessionID, in_addr, bInvited );
            }
        }
        else
        {
            // Not a Matchmaking session. Just get ourselves into this session
            JoinSession( session_info.sessionID, in_addr, bInvited );
        }
    }
    else
    {
        // Hosting this session. If this is a Matchmaking session and
        // we're also hosting a Presence session, get our Party members
        // in here
        if( bIsMatchmakingSession && bInPresenceSessionAndHost )
        {
            JoinPartyMembersToSession( session_info, bInvited ); 
        }
    }

    // Add local players to the session
    ScheduleSessionJoinTasks( pSessionMgr, &m_Local, 0xF );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoSessionStartedTask()
// Desc: Handle start of an XSession
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionStartedTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped )
{
    if( SetLastXSessionError( pSessionMgr, pXOverlapped ) )
    {
        ScheduleSessionMgrDeletionTasks( pSessionMgr );
        ScheduleChangeStateTask( APPSTATE_MAINMENU, pSessionMgr );
        return S_OK;
    }

    pSessionMgr->SwitchToState( SessionStateInGame );
    
    SessionManager* pPresenceSessionMgr = GetPresenceSession();

    const BOOL bIsMatchmakingSession     = ( pSessionMgr->HasSessionFlags( MATCHMAKING_SESSION_FLAGS ) );
    const BOOL bInPresenceSession        = ( pPresenceSessionMgr ) ? TRUE : FALSE;
    const BOOL bInPresenceSessionAndHost = ( bInPresenceSession && pPresenceSessionMgr->IsSessionHost() );

    // Is this a Matchmaking session?
    if( bIsMatchmakingSession )
    {
        // If we're the host of a Presence session, start it. The Presence session 
        // start/end is used as the indicator for correctly reporting data 
        // from the Live service to a title's publisher
        if( bInPresenceSessionAndHost && pPresenceSessionMgr->GetSessionState() < SessionStateStart )
        {
            XNKID presenceSessionID = pPresenceSessionMgr->GetSessionID();
            StartGame( presenceSessionID );
        }

        // Update rich presence for anyone in the session
        for( UINT i = 0; i < m_Local.cPlayers; ++i )
        {
            if( pSessionMgr->IsPlayerInSession( m_Local.xuids[ i ] ) )
            {
                ScheduleUserContextUpdateTasks( m_Local.nController[ i ], X_CONTEXT_PRESENCE, GetRichPresenceIDForSession( pSessionMgr ) ); 
            }
        }
    }

    // If this is an arbitrated Matchmaking session and we're in a presence-enabled session,
    // then disable join-in-progress in the presence-enabled session, as arbitrated sessions
    // cannot be joined in progress
    if(  bIsMatchmakingSession && 
         pSessionMgr->HasSessionFlags( XSESSION_CREATE_USES_ARBITRATION ) )
    {
        if( pSessionMgr->HasSessionFlags( XSESSION_CREATE_USES_PRESENCE ) )
        {
            ScheduleSessionModifyTasks( pSessionMgr, XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED );
        }
        else if( bInPresenceSession )
        {
            ScheduleSessionModifyTasks( pPresenceSessionMgr, XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED );
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DoSessionEndedTask()
// Desc: Handle end of an XSession
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionEndedTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped )
{
    if( SetLastXSessionError( pSessionMgr, pXOverlapped ) )
    {
        ScheduleSessionMgrDeletionTasks( pSessionMgr );
        ScheduleChangeStateTask( APPSTATE_MAINMENU, pSessionMgr );
        return S_OK;
    }

    pSessionMgr->SwitchToState( SessionStateEnded );

    // If we're the host of a Presence session, also end that session.
    // The Presence session start/end is used as the indicator for
    // correctly reporting data from the Live service to a title's
    // publisher
    SessionManager* pPresenceSessionMgr = GetPresenceSession(); 
    if( pPresenceSessionMgr != NULL && pPresenceSessionMgr->IsSessionHost() &&
         pPresenceSessionMgr->GetSessionState() > SessionStateStart && 
         pPresenceSessionMgr->GetSessionState() < SessionStateEnd )
    {
        const XNKID presenceSessionID = pPresenceSessionMgr->GetSessionID();
        EndGame( presenceSessionID );
    }


    // Update rich presence if this is the Matchmaking session
    if( pSessionMgr == GetMatchmakingSession() )
    {
        for( UINT i = 0; i < m_Local.cPlayers; ++i )
        {
            if( pSessionMgr->IsPlayerInSession( m_Local.xuids[ i ] ) )
            {
                ScheduleUserContextUpdateTasks( m_Local.nController[ i ], X_CONTEXT_PRESENCE, GetRichPresenceIDForSession( pSessionMgr ) ); 
            }
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoSessionDeletedTask()
// Desc: Handle deletion of an XSession
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionDeletedTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped )
{
    if( SetLastXSessionError( pSessionMgr, pXOverlapped ) )
    {
        ScheduleSessionMgrDeletionTasks( pSessionMgr );
        ScheduleChangeStateTask( APPSTATE_MAINMENU, pSessionMgr );
        return S_OK;
    }

    pSessionMgr->SwitchToState( SessionStateDeleted );

    // Update rich presence if this is the Matchmaking session
    if( pSessionMgr == GetMatchmakingSession() )
    {
        for( UINT i = 0; i < MAX_USER_COUNT; ++i )
        {
            XUserSetContext( i, X_CONTEXT_PRESENCE, CONTEXT_PRESENCE_PARTIESSAMPLEPRESENCE ); 
        }
    }

    // If this is an arbitrated Matchmaking session and we're in a presence-enabled session,
    // then re-enable join-in-progress in the presence-enabled session
    SessionManager* pPresenceSessionMgr = GetPresenceSession();
    const BOOL bInAlivePresenceSession = ( pPresenceSessionMgr != NULL && pPresenceSessionMgr->GetSessionState() < SessionStateDelete ) 
                                            ? TRUE : FALSE;

    if(  pSessionMgr == GetMatchmakingSession() && 
         pSessionMgr->HasSessionFlags( XSESSION_CREATE_USES_ARBITRATION ) &&
         bInAlivePresenceSession )
    {
        ScheduleSessionModifyTasks( pPresenceSessionMgr, XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED, TRUE );
    }   
    
    // If this is a Presence session then remove affiliation to
    // it from our local players
    if( pSessionMgr->HasSessionFlags( PRESENCE_SESSION_FLAGS ) &&
        !pSessionMgr->HasSessionFlags( MATCHMAKING_SESSION_FLAGS ) )
    {
        for( UINT i = 0; i < m_Local.cPlayers; ++i )
        {
            m_Local.presenceSessionNonces[ i ] = 0;
        }

        // Do the same for clients
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
        {
            IN_ADDR inaddr = {0};
            if( !i->GetInAddrForSession( &inaddr, pSessionMgr->GetSessionID() ) )
            {
                continue;

            }

            for( UINT j = 0; j < i->cPlayers; ++j )
            {
                i->presenceSessionNonces[ j ] = 0;
            }
        }
    }

    // Delete our session manager instance
    ScheduleSessionMgrDeletionTasks( pSessionMgr );

    return S_OK;

}


//--------------------------------------------------------------------------------------
// Name: DoSessionMgrDeleteTask()
// Desc: Delete Session Manager instance
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionMgrDeleteTask( SessionManager* pSessionMgr )
{
    const ULONGLONG qwSessionNonce = pSessionMgr->GetSessionNonce();

    // No-op if Session Manager instance isn't in our m_mapSessions
    if( !SessionManagerFromNonce( qwSessionNonce ) )
    {
        DebugSpew( "ProcessDeleteSessionMgr: Instance %p already deleted, so no-op\n",
                   pSessionMgr );

        return S_OK;
    }

    // If this is a Presence session, get rid of local client affiliation with this session
    if( pSessionMgr->HasSessionFlags( PRESENCE_SESSION_FLAGS ) &&
        !pSessionMgr->HasSessionFlags( MATCHMAKING_SESSION_FLAGS ) )
    {
        ClearPresenceSessionAffiliation();
    }

    // Remove from our m_mapSessions map
    m_mapSessions.erase( qwSessionNonce ); 

    // Remove from party matchmaking sessions list
    RemoveSessionIDFromPartyMatchmakingSessionsList( pSessionMgr->GetSessionID() );

    // Clear out error messages
//    SetLastXSessionError( NULL );

    // Null out m_pSessionMgrCtx in case it's holding a reference to this instance
    m_pSessionMgrCtx = NULL;

    // Close all pending task scheduler tasks that reference this session
    CancelAllPendingXSessionTasksForSession( pSessionMgr );
    
    // Delete the session
    if( pSessionMgr )
    {
        // If the session has been created, call our
        // SessionManager's destructor. This will clean
        // up any system resources held by the SessionManager
        if( pSessionMgr->GetSessionState() >= SessionStateCreated )
        {
            DebugSpew( "ProcessDeleteSessionMgr: Calling delete on instance %p\n",
                       pSessionMgr );

            delete pSessionMgr;
        }
    }

    // Clear out the scores before the next game
    ZeroMemory( m_Local.nPoints, sizeof( m_Local.nPoints ) );
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        ClientInfo& Remote = *i;
        ZeroMemory( Remote.nPoints, sizeof( Remote.nPoints ) );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoSessionAddUsersTask()
// Desc: Add users to a created/joined session
//--------------------------------------------------------------------------------------
VOID Sample::DoSessionAddUsersTask ( ClientInfo* pClient, 
                                     SessionManager* pSessionMgr, 
                                     __inout_ecount(dwCountPlayers) DWORD* paIndices,
                                     __inout_ecount(dwCountPlayers) XUID* paXuids,
                                     __inout_ecount(dwCountPlayers) BOOL* pabPrivate,
                                     const DWORD dwCountPlayers,
                                     XOVERLAPPED* pXOverlapped,
                                     const DWORD userMask )
{
    // first read the TrueSkill(TM) skills of all gamers involved ...
    ReadTrueSkills( pClient );

    const ULONGLONG qwSessionNonce = pSessionMgr->GetSessionNonce();

    // Get current slot counts
    DWORD dwMaxPublicSlots, dwMaxPrivateSlots, dwFilledPublicSlots, dwFilledPrivateSlots, dwPrivateSlotsUsed = 0;
    
    pSessionMgr->GetMaxSlotCounts( dwMaxPublicSlots, dwMaxPrivateSlots );
    pSessionMgr->GetFilledSlotCounts( dwFilledPublicSlots, dwFilledPrivateSlots );

    if( &m_Local == pClient )
    {
        // For local players, if session has started and 
        // join-in-progress is disabled then do nothing
        SessionState sessionState = pSessionMgr->GetSessionState();
        if( sessionState >= SessionStateInGame && 
             pSessionMgr->HasSessionFlags( XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED ) )
        {
            return;
        }

        DWORD cToAdd = 0;
        for( UINT i = 0; i < dwCountPlayers; ++i )
        {
            if( pClient->xuids[ i ] == INVALID_XUID )
            {
                continue;
            }
    
            // Don't re-add a player already in the session. Pass in both XUID and controller index.
            // In case the XUID is an offline XUID, we'll need the controller index for the check
            if( pSessionMgr->IsPlayerInSession( pClient->xuids[ i ], m_Local.nController[ i ] ) )
            {
                continue;
            }

            if( ( 1 << pClient->nController[ i ] & userMask ) == 0 )
            {
                pClient->bToAdd[ i ] = 0;
                continue;
            }

            // Only add player if not idle and signed in
            const XUSER_SIGNIN_INFO& info = m_SignInInfo[ m_Local.nController[ i ] ];
            if( info.UserSigninState == eXUserSigninState_NotSignedIn || m_Local.bIsIdle[ i ] )
            {
                pClient->bToAdd[ i ] = 0;
                continue;
            }

            // If this is a presence session, then add session nonce for
            // this client
            if( pSessionMgr->HasSessionFlags( PRESENCE_SESSION_FLAGS ) &&
                !pSessionMgr->HasSessionFlags( MATCHMAKING_SESSION_FLAGS ) )
            {
                // Nothing to do if we're already in the session
                if( pClient->presenceSessionNonces[ i ] == qwSessionNonce )
                {
                    pClient->bToAdd[ i ] = 0;
                    continue;
                }
                else
                {
                    pClient->presenceSessionNonces[ i ] = qwSessionNonce;
                }
            }

            // Session owner and invited clients take occupy a private slot if there is 
            // one available; otherwise, they take up a public slot
            if( ( pClient->bInvited || pSessionMgr->GetSessionOwner() == pClient->nController[ i ] ) && 
                dwPrivateSlotsUsed < dwMaxPrivateSlots - dwFilledPrivateSlots ) 
            {
                dwPrivateSlotsUsed++;
                pClient->bUsesPrivateSlot[ i ] = TRUE;
            }
            else
            {
                pClient->bUsesPrivateSlot[ i ] = FALSE;
            }

            DebugSpew( "%016I64X: AddUsersToSession(local): 0x%016I64X; bInvited: %d; bUsesPrivateSlot: %d; id: %I64u\n", 
                       pSessionMgr->GetSessionIDAsInt(), 
                       pClient->xuids[ i ], 
                       pClient->bInvited,
                       pClient->bUsesPrivateSlot[ i ], 
                       pClient->id );


            // Mark this user for adding
            pClient->bToAdd[ i ]  = 1;
            paIndices[ cToAdd ]   = pClient->nController[ i ];
            pabPrivate[ cToAdd ]  = pClient->bUsesPrivateSlot[ i ];
            cToAdd++;
        }

        if( cToAdd )
        {
            pSessionMgr->AddLocalPlayers( cToAdd, paIndices, pabPrivate, pXOverlapped );
        }
    }
    else
    {
        DWORD cToAdd = 0;
        for( UINT i = 0; i < dwCountPlayers; ++i )
        {
            if( ( 1 << pClient->nController[ i ] & userMask ) == 0 )
            {
                pClient->bToAdd[ i ] = 0;
                continue;
            }

            // Don't re-add a player already in the session
            if( pSessionMgr->IsPlayerInSession( pClient->xuids[ i ] ) )
            {
                continue;
            }

            // If this is a presence session, then add session nonce for
            // this client
            if( pSessionMgr->HasSessionFlags( PRESENCE_SESSION_FLAGS ) &&
                !pSessionMgr->HasSessionFlags( MATCHMAKING_SESSION_FLAGS ) )
            {
                // Nothing to do if we're already in the session
                if( pClient->presenceSessionNonces[ i ] == qwSessionNonce )
                {
                    pClient->bToAdd[ i ] = 0;
                    continue;
                }
                else
                {
                    pClient->presenceSessionNonces[ i ] = qwSessionNonce;
                }
            }

            // Invited clients take occupy a private slot if there is 
            // one available; otherwise, they take up a public slot
            if( pClient->bInvited && dwPrivateSlotsUsed < dwMaxPrivateSlots - dwFilledPrivateSlots ) 
            {
                dwPrivateSlotsUsed++;
                pClient->bUsesPrivateSlot[ i ] = TRUE;
            }
            else
            {
                pClient->bUsesPrivateSlot[ i ] = FALSE;
            }

            DebugSpew( "%016I64X: AddUsersToSession(remote): 0x%016I64X; bInvited: %d; bUsesPrivateSlot: %d; id: %I64u\n", 
                       pSessionMgr->GetSessionIDAsInt(), 
                       pClient->xuids[ i ], 
                       pClient->bInvited,
                       pClient->bUsesPrivateSlot[ i ], 
                       pClient->id );

            // Mark this user for adding
            pClient->bToAdd[ i ] = 1;
            paXuids[ cToAdd ]    = pClient->xuids[ i ];
            pabPrivate[ cToAdd ] = pClient->bUsesPrivateSlot[ i ];
            cToAdd++;
        }

        if( cToAdd )
        {
            pSessionMgr->AddRemotePlayers( cToAdd, paXuids, pabPrivate, pXOverlapped );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: DoSessionRemoveUsersTask()
// Desc: Removes users from a session
//--------------------------------------------------------------------------------------
VOID Sample::DoSessionRemoveUsersTask ( ClientInfo* pClient, 
                                        SessionManager* pSessionMgr, 
                                        __inout_ecount(dwCountPlayers) DWORD* paIndices,
                                        __inout_ecount(dwCountPlayers) XUID* paXuids,
                                        __inout_ecount(dwCountPlayers) BOOL* pabPrivate,
                                        const DWORD dwCountPlayers,
                                        XOVERLAPPED* pXOverlapped,
                                        const DWORD userMask )
{
    const ULONGLONG qwSessionNonce = pSessionMgr->GetSessionNonce();
    const XNKID& sessionID = pSessionMgr->GetSessionID();

    if( &m_Local == pClient )
    {
        DWORD cToRemove = 0;
        
        for( UINT i = 0; i < dwCountPlayers; ++i )
        {
            if( ( 1 << pClient->nController[ i ] & userMask ) == 0 )
            {
                pClient->bToRemove[ i ] = 0;
                continue;
            }

            // Can only remove a player already in the session. Pass in both XUID and controller index.
            // In case the XUID is an offline XUID, we'll need the controller index for the check
            if( !pSessionMgr->IsPlayerInSession( pClient->xuids[ i ], pClient->nController[ i ] ) )
            {
                continue;
            }

            // If this is a Presence session, then first check if
            // we're in this session
            if( pSessionMgr->HasSessionFlags( PRESENCE_SESSION_FLAGS ) &&
                !pSessionMgr->HasSessionFlags( MATCHMAKING_SESSION_FLAGS ) )
            {
                // Nothing to do if we're not in the session
                if( pClient->presenceSessionNonces[ i ] != qwSessionNonce )
                {
                    pClient->bToRemove[ i ] = 0;
                    continue;
                }
                else
                {
                    // Clear nonce
                    pClient->presenceSessionNonces[ i ] = 0;
                }
            }
            
            // Mark this user for removal
            pClient->bToRemove[ i ] = 1;
            paIndices [ cToRemove ] = pClient->nController[ i ];
            pabPrivate[ cToRemove ] = pClient->bUsesPrivateSlot[ i ];                                    
            cToRemove++;

            DebugSpew( "RemoveUsersFromSession(%016I64X, local): 0x%016I64X; bInvited: %d; bUsesPrivateSlot: %d; id: %I64u\n", 
                       XNKIDToInt64( sessionID ), 
                       pClient->xuids[ i ], 
                       pClient->bInvited,
                       pClient->bUsesPrivateSlot[ i ], 
                       pClient->id );
        }

        if( cToRemove )
        {
            pClient->LeftSession( sessionID );
            pSessionMgr->RemoveLocalPlayers( cToRemove, paIndices, pabPrivate, pXOverlapped );
        }
    }
    else
    {
        DWORD cToRemove = 0;
        for( UINT i = 0; i < dwCountPlayers; ++i )
        {
            if( ( 1 << pClient->nController[ i ] & userMask ) == 0 )
            {
                pClient->bToRemove[ i ] = 0;
                continue;
            }

            // Can only remove a player already in the session
            if( !pSessionMgr->IsPlayerInSession( pClient->xuids[ i ] ) )
            {
                continue;
            }

            // If this is a presence session, then first check if
            // we're in this session
            if( pSessionMgr->HasSessionFlags( PRESENCE_SESSION_FLAGS ) &&
                !pSessionMgr->HasSessionFlags( MATCHMAKING_SESSION_FLAGS ) )
            {
                // Nothing to do if we're not in the session
                if( pClient->presenceSessionNonces[ i ] != qwSessionNonce )
                {
                    pClient->bToRemove[ i ] = 0;
                    continue;
                }
            }

            DebugSpew( "RemoveUsersFromSession(%016I64X, remote): 0x%016I64X; bInvited: %d; bUsesPrivateSlot: %d; id: %I64u\n", 
                       XNKIDToInt64( sessionID ), 
                       pClient->xuids[ i ], 
                       pClient->bInvited,
                       pClient->bUsesPrivateSlot[ i ], 
                       pClient->id );

            // Mark this user for removal
            pClient->bToRemove[ i ] = 1;
            paXuids [ cToRemove ]   = pClient->xuids[ i ];
            pabPrivate[ cToRemove ] = pClient->bUsesPrivateSlot[ i ];
            cToRemove++;
        }

        if( cToRemove )
        {
            pSessionMgr->RemoveRemotePlayers( cToRemove, paXuids, pabPrivate, pXOverlapped );

            // If we've removed all players on the client, remove the client
            // from the session
            if( cToRemove == dwCountPlayers )
            {
                pClient->LeftSession( sessionID );
            }
        }

        if( pClient->bDroppingClient )
        {
            for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
            {
                if( ( &(*i) == pClient ) && ( i->id == pClient->id ) )
                {
                    if( pSessionMgr->GetHostInAddr() == i->addr )
                    {
                        DebugSpew( "RemoveUsersFromSession(%016I64X, remote): Client %I64u is session host and host migration underway, so not dropping...\n" , 
                                   XNKIDToInt64( sessionID ), i->id );
                        break;
                    }

                    DebugSpew( "RemoveUsersFromSession(%016I64X, remote): Removing client %I64u from our list of remote clients...\n" , 
                               XNKIDToInt64( sessionID ), i->id );

                    m_vecRemote.erase( i );
                    break;
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: GetRichPresenceIDForSession()
// Desc: Returns rich presence string context value that maps to the session state
//--------------------------------------------------------------------------------------
DWORD Sample::GetRichPresenceIDForSession( const SessionManager* pSessionMgr )
{
    DWORD dwContextValue = CONTEXT_PRESENCE_PARTIESSAMPLEPRESENCE;
    if( pSessionMgr )
    {
        switch( pSessionMgr->GetSessionState() )
        {
        case SessionStateNone:
        case SessionStateCreated:
        case SessionStateIdle:
        case SessionStateWaitingForRegistration:
        case SessionStateRegister:
        case SessionStateRegistering:
        case SessionStateRegistered:
        case SessionStateStart:
        case SessionStateStarting:
            dwContextValue = CONTEXT_PRESENCE_PARTIESSAMPLEINGAMELOBBY;
            break;
        case SessionStateInGame:
        case SessionStateMigrateHost:
        case SessionStateMigratingHost:
        case SessionStateMigratedHost:
        case SessionStateEnd:
        case SessionStateEnding:
            dwContextValue = CONTEXT_PRESENCE_PARTIESSAMPLEINGAME;
            break;
        case SessionStateEnded:
        case SessionStateDelete:
        case SessionStateDeleting:
            dwContextValue = CONTEXT_PRESENCE_PARTIESSAMPLEINPOSTGAMELOBBY;
            break;
        case SessionStateDeleted:
            dwContextValue = CONTEXT_PRESENCE_PARTIESSAMPLEPRESENCE;
            break;
        }
    }

    return dwContextValue;
}


//--------------------------------------------------------------------------------------
// Name: SwitchToPostJoinLeaveAppState()
// Desc: Determine what APPSTATE to switch to following the task
//--------------------------------------------------------------------------------------
VOID Sample::SwitchToPostJoinLeaveAppState( SessionManager* pSessionMgr )
{
    APPSTATE newAppState;

    if( pSessionMgr )
    {
        switch( pSessionMgr->GetSessionState() )
        {
        case SessionStateCreated:
            newAppState = APPSTATE_PREGAME;
            break;
        case SessionStateRegistered:
            newAppState = APPSTATE_REGISTERED;
            break;
        case SessionStateStarting:
            newAppState = APPSTATE_STARTING;
            break;
        case SessionStateInGame:
            newAppState = APPSTATE_INGAME;
            break;
        case SessionStateEnded:
            newAppState = APPSTATE_POSTGAME;
            break;
        case SessionStateDeleted:
            newAppState = APPSTATE_MAINMENU;
            break;
        default:
            newAppState = APPSTATE_MAINMENU;
            break;
        }
    }
    else
    {
        newAppState = APPSTATE_MAINMENU;
    }

    ScheduleChangeStateTask( newAppState, pSessionMgr );    
}

//--------------------------------------------------------------------------------------
// Name: DoSessionLocalPlayersAddedTask()
// Desc: Handle local players added to the session
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionLocalPlayersAddedTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped )
{
    // $TODO - For release builds, getting an 0x80070490 error (ERROR_NOT_FOUND) when creating
    // a matchmaking session after a presence session and vice-versa. Need to investigate why.
    // Interestingly this error can be ignored in the case of calling XSessionJoinLocal
    // asynchronously..
    #ifdef _DEBUG
    if( SetLastXSessionError( pSessionMgr, pXOverlapped ) )
    {
        ScheduleSessionMgrDeletionTasks( pSessionMgr );
        ScheduleChangeStateTask( APPSTATE_MAINMENU, pSessionMgr );
        return S_OK;
    }
    #endif

    DebugDumpSessionMembers( pSessionMgr );

    for( UINT i = 0; i < m_Local.cPlayers; ++i )
    {
        // Unmark player for adding
        if( m_Local.bToAdd[ i ] )
        {
            m_Local.bToAdd[ i ] = 0;
        }
    }

    // Update rich presence for anyone in the session if this is the Matchmaking session
    if( pSessionMgr == GetMatchmakingSession() )
    {
        for( UINT i = 0; i < m_Local.cPlayers; ++i )
        {
            if( pSessionMgr->IsPlayerInSession( m_Local.xuids[ i ] ) )
            {
                ScheduleUserContextUpdateTasks( m_Local.nController[ i ], X_CONTEXT_PRESENCE, GetRichPresenceIDForSession( pSessionMgr ) ); 
            }
        }
    }

    // If this is a current Matchmaking session and we are the session
    // host, we need to modify the TrueSkill(TM) of the session
    if( pSessionMgr == GetMatchmakingSession() && 
        pSessionMgr->IsSessionHost() &&
        pSessionMgr->GetSessionState() < SessionStateDelete )
    {
        SetSessionTrueSkill( pSessionMgr );
    }

    SwitchToPostJoinLeaveAppState( pSessionMgr );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoSessionRemotePlayersAddedTask()
// Desc: Handle remote players added to the session
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionRemotePlayersAddedTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped )
{
    if( SetLastXSessionError( pSessionMgr, pXOverlapped ) )
    {
        ScheduleSessionMgrDeletionTasks( pSessionMgr );
        ScheduleChangeStateTask( APPSTATE_MAINMENU, pSessionMgr );
        return S_OK;
    }

    DebugDumpSessionMembers( pSessionMgr );

    for( UINT i = 0; i < m_Local.cPlayers; ++i )
    {
        // Unmark player for adding
        if( m_Local.bToAdd[ i ] )
        {
            m_Local.bToAdd[ i ] = 0;
        }
    }

    // If this is a current Matchmaking session and we are the session
    // host, we need to modify the TrueSkill(TM) of the session
    if( pSessionMgr == GetMatchmakingSession() && 
        pSessionMgr->IsSessionHost() &&
        pSessionMgr->GetSessionState() < SessionStateDelete )
    {
        SetSessionTrueSkill( pSessionMgr );
    }

    SwitchToPostJoinLeaveAppState( pSessionMgr );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoSessionLocalPlayersRemovedTask()
// Desc: Handle local players removed from the session
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionLocalPlayersRemovedTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped )
{
    const XNKID sessionID = pSessionMgr->GetSessionID();

    if( SetLastXSessionError( pSessionMgr, pXOverlapped ) )
    {
        ScheduleSessionMgrDeletionTasks( pSessionMgr );
        ScheduleChangeStateTask( APPSTATE_MAINMENU, pSessionMgr );
        return S_OK;
    }

    DebugDumpSessionMembers( pSessionMgr );

    // If this is a current Matchmaking session and we are the session
    // host, we need to modify the TrueSkill(TM) of the session
    if( pSessionMgr == GetMatchmakingSession() && 
        pSessionMgr->IsSessionHost() &&
        pSessionMgr->GetSessionState() < SessionStateDelete )
    {
        SetSessionTrueSkill( pSessionMgr );
    }

    BOOL bSessionOwnerRemoved   = FALSE;
    INT bPlayerRemoved          = FALSE;
    
    XUID removed[MAX_USER_COUNT];
    BYTE cRemoved           = 0;
    DWORD dwRemovedUserMask = 0;
    
    for( UINT i = 0; i < m_Local.cPlayers; ++i )
    {
        // Is player marked for removal?
        if( m_Local.bToRemove[ i ] )
        {
            dwRemovedUserMask |= 1 << m_Local.nController[ i ];

            if( m_Local.nController[ i ] == pSessionMgr->GetSessionOwner() )
            {
                bSessionOwnerRemoved = TRUE;
                removed[ cRemoved++ ] = m_Local.xuids[ i ];
            }
            else
            {
                bPlayerRemoved = TRUE;
                removed[ cRemoved++ ] = m_Local.xuids[ i ];
            }
            
            // If this is a presence session, then remove
            // the player from it
            if( pSessionMgr->HasSessionFlags( PRESENCE_SESSION_FLAGS ) &&
                !pSessionMgr->HasSessionFlags( MATCHMAKING_SESSION_FLAGS ) )
            {
                m_Local.presenceSessionNonces[ i ] = 0;
            }

            // Clear the remove flag for this player
            m_Local.bToRemove[ i ] = 0;
        }
    }

    // If we removed a player, then we go ahead
    // and send a message to all peers to remove the player
    if( bPlayerRemoved || bSessionOwnerRemoved )
    {
        if( !pSessionMgr->IsSessionHost() )
        {
            DebugSpew( "Sample::DoSessionLocalPlayersRemovedTask(%016I64X): Sending host MSG_GOODBYE\n", XNKIDToInt64( sessionID ) );
            for ( UINT i = 0; i < cRemoved; ++i )
            {
                DebugSpew( "MSG_GOODBYE for player %016I64X\n", removed[ i ] );
            }

            SayGoodbye( pSessionMgr,
                        cRemoved,
                        removed,
                        pSessionMgr->GetHostInAddr() );
        }
        else
        {
            DebugSpew( "Sample::DoSessionLocalPlayersRemovedTask(%016I64X): Sending clients MSG_GOODBYE\n", XNKIDToInt64( sessionID ) );
            for ( UINT j = 0; j < cRemoved; ++j )
            {
                DebugSpew( "MSG_GOODBYE for player %016I64X\n", removed[j] );
            }

            for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
            {
                IN_ADDR inaddr = {0};
                if( i->GetInAddrForSession( &inaddr, sessionID ) )
                {
                    SayGoodbye( pSessionMgr,
                                cRemoved,
                                removed,
                                inaddr );
                }
            }
        }
    }

    SwitchToPostJoinLeaveAppState( pSessionMgr );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoSessionLocalPlayersRemovedTask()
// Desc: Handle remote players removed from the session
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionRemotePlayersRemovedTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped )
{
    if( SetLastXSessionError( pSessionMgr, pXOverlapped ) )
    {
        ScheduleSessionMgrDeletionTasks( pSessionMgr );
        ScheduleChangeStateTask( APPSTATE_MAINMENU, pSessionMgr );
        return S_OK;
    }

    DebugDumpSessionMembers( pSessionMgr );

    // If this is an arbitrated session and we're the only player in the session, end the session
    if( pSessionMgr->HasSessionFlags( XSESSION_CREATE_USES_ARBITRATION ) && 
        pSessionMgr->GetSessionState() < SessionStateEnd )
    {
        DWORD dwFilledPublicSlots, dwFilledPrivateSlots;
        pSessionMgr->GetFilledSlotCounts( dwFilledPublicSlots, dwFilledPrivateSlots );
        
        if( dwFilledPublicSlots + dwFilledPrivateSlots == 1 )
        {
            DebugSpew( "Sample::DoSessionRemotePlayersRemovedTask(%016I64X): Arbitrated session now has only 1 player in it, so ending\n", 
                       pSessionMgr->GetSessionIDAsInt() );

            ScheduleSessionEndTasks( pSessionMgr );
            return S_OK;
        }
    }

    // If this is a current Matchmaking session and we are the session
    // host, we need to modify the TrueSkill(TM) of the session
    if( pSessionMgr == GetMatchmakingSession() && 
        pSessionMgr->IsSessionHost() &&
        pSessionMgr->GetSessionState() < SessionStateDelete )
    {
        SetSessionTrueSkill( pSessionMgr );
    }

    SwitchToPostJoinLeaveAppState( pSessionMgr );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoSessionStatsWriteTask()
// Desc: Handler for writing stats for the session
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionStatsWriteTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped )
{
    const XNKID& sessionID = pSessionMgr->GetSessionID();

    // Write stats for ourselves
    WriteStats( pSessionMgr, &m_Local, 0xF );

    // Write stats for clients/peers
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        if( !i->IsInSession( sessionID ) )
        {
            continue;
        }
        
        WriteStats( pSessionMgr, &(*i), 0xF );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoSessionStatsWriteTask()
// Desc: Handler for writing stats for the session
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionStatsWriteTask( SessionManager* pSessionMgr, 
                                         ClientInfo* pClient, 
                                         const DWORD userMask,
                                         XOVERLAPPED* pXOverlapped )
{
    return WriteStats( pSessionMgr, pClient, 0xF, pXOverlapped );
}


//--------------------------------------------------------------------------------------
// Name: DoSessionStatsWrittenTask()
// Desc: Handler for after stats are written for the session
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionStatsWrittenTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped )
{
    // Stats currently written synchronously, so ignore checking pXOverlapped
    //if( SetLastXSessionError( pSessionMgr, pXOverlapped ) )
    //{
    //    ScheduleSessionMgrDeletionTasks( pSessionMgr );
    //    ScheduleChangeStateTask( APPSTATE_MAINMENU, pSessionMgr );

    //    return S_OK;
    //}

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoSessionConnectingTask()
// Desc: Handler for establishing a connection to a session peer
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionConnectingTask( SessionManager* pSessionMgr )
{
    // Make sure the host hasn't forgotten about us
    HandleHeartbeat( pSessionMgr );
    ReceiveMessage();

    if( m_Local.IsInSession( pSessionMgr->GetSessionID() ) )
    {
        return S_OK; // Signals to the caller that this task is done
    }

    if( GetTickCount() - m_dwConnectionTimer > MAX_CONNECTIONWAIT )
    {
        if( pSessionMgr )
        {
            swprintf_s( m_wszLastSessionError, L"Connection attempt to Session %016I64X host timed out. Deleting the session\n", 
                        pSessionMgr->GetSessionIDAsInt() );

            ScheduleSessionMgrDeletionTasks( pSessionMgr );
            return S_OK; // Signals to caller that this task is done
        }
    }

    return E_FAIL; // Signals to the caller that this task is NOT done
}

//--------------------------------------------------------------------------------------
// Name: DoSessionMigrateHostTask()
// Desc: Handler for initiating host migration
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionMigrateHostTask( SessionManager* pSessionMgr )
{
    // Ony begin host migration if the session is not in a deleting state
    if( pSessionMgr->GetSessionState() < SessionStateDelete )
    {
        m_HostMigration.BeginHostMigration( this, pSessionMgr );
    }
    else
    {
        DebugSpew( "Sample::DoSessionMigrateHostTask(threadID 0x%x): Session 0x%p already deleted, so host migration cancelled.\n", 
		           GetCurrentThreadId(), pSessionMgr );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoSessionMigratingHostTask()
// Desc: Handler for updating host migration state
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionMigratingHostTask( SessionManager* pSessionMgr )
{
    // If the session is in a deleting state, don't bother with host migration and
    // return S_OK to indicate to the caller to end our host migration task
    if( pSessionMgr->GetSessionState() >= SessionStateDelete )
    {
        DebugSpew( "Sample::DoSessionMigratingHostTask(threadID 0x%x): Session 0x%p already deleted, so host migration cancelled.\n", 
		           GetCurrentThreadId(), pSessionMgr );

        return S_OK;
    }

    // Call our host migration helper class to update host migration state. If
    // the function returns a success HRESULT, then host migration has completed.
    // A failure HRESULT indicates that host migration is still underway.
    return m_HostMigration.UpdateHostMigration();
}

//--------------------------------------------------------------------------------------
// Name: DoSessionMigratedHostTask()
// Desc: Handler for cleaning up after host migration
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionMigratedHostTask( SessionManager* pSessionMgr )
{
    // Signal the end of host migration to the session manager
    // and our app
    if( pSessionMgr->GetSessionState() < SessionStateDelete )
    {
        pSessionMgr->SwitchToPreHostMigrationState();
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoSessionMigratedHostTask()
// Desc: Handler for cleaning up after host migration
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionRegisteredTask( SessionManager* pSessionMgr )
{
    // We're registered! If we're not the host, let them know
    if( !pSessionMgr->IsSessionHost() )
    {
        CMessage msg( MSG_REGISTERED );
        msg.SetSessionID( pSessionMgr->GetSessionID() );
        SendMessage( &msg, pSessionMgr->GetHostInAddr() );
    }
    else
    {
        // If we are the host, then process our registration list and remove
        // and cull clients who haven't registered
        ProcessRegistrationList();
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoSessionHostBeginRegisterTask()
// Desc: Handler for the host to kick off arbitration registration
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionHostBeginRegisterTask( SessionManager* pSessionMgr )
{
    // Start the timer
    m_dwRegistrationTimer = GetTickCount();

    const XNKID sessionID = pSessionMgr->GetSessionID();

    // Send the message to all peers in the session
    CMessage msg( MSG_REGISTER );
    msg.SetSessionID( sessionID );

    // Start off by flagging everybody who's in the session as unregistered and sen
    // those people a MSG_REGISTER message. For those not in the session,
    // fake that they've registered so that we don't boot them mistakingly after
    // we've registered ourselves
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        IN_ADDR inaddr = {0};
        if( i->GetInAddrForSession( &inaddr, sessionID ) )
        {
            i->bRegistered = FALSE;
            SendMessage( &msg, inaddr );
        }
        else
        {
            i->bRegistered = TRUE;
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DoSessionHostWaitRegisterTask()
// Desc: Handler for the host to wait for all connected peers to register themselves
//       for arbitration
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionHostWaitRegisterTask( SessionManager* pSessionMgr )
{
    // Ensure we're sending/receiving heartbeats while waiting for clients to register
    HandleHeartbeats();

    // If we've exceeded our registration timeout, then we're done waiting
    // for clients to register. Return S_OK to indicate to the caller that this task is done
    if( ( GetTickCount() - m_dwRegistrationTimer ) > REGISTRATION_TIME )
    {
        return S_OK;
    }

    const XNKID& sessionID = pSessionMgr->GetSessionID();

    //
    // Check if we're done by looping through all clients in the session
    // and seeing if any is not done
    //
    BOOL bAllRegistered = TRUE;
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {        
        if( i->IsInSession( sessionID ) )
        {
            if( !i->bRegistered )
            {
                bAllRegistered = FALSE;
                break;
            }
        }
    }

    // If all clients have registered, start registering ourselves
    if( bAllRegistered )
    {
        return S_OK; // Indicate to the caller that we're done waiting for clients to register
    }

    return E_FAIL; // Indicates to the caller that we're still waiting for clients to register
}


//--------------------------------------------------------------------------------------
// Name: DoSessionSearchTask()
// Desc: Handler for kicking off a session search
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionSearchTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped )
{
    // Figure out the size of the max search results
    DWORD cbResults = 0;
    DWORD ret;

    // Pass in 0 for cbResults to have the buffer size 
    //  calculated for us by XTL
    ret = XSessionSearch(
        SESSION_MATCH_QUERY_FIND_MATCHES, // Procedure index
        pSessionMgr->GetSessionOwner(),   // User index
        MAX_SEARCHRESULTS,                // Maximum results
        0,                                // Number of properties   (ignored)
        0,                                // Number of contexts     (ignored)
        NULL,                             // Properties             (ignored)
        NULL,                             // Contexts               (ignored)
        &cbResults,                       // Size of result buffer
        NULL,                             // Pointer to results     (ignored)
        NULL                              // This call is always synchronous
        );

    if(( ret != ERROR_INSUFFICIENT_BUFFER ) || ( cbResults == 0 ))
    {
        FatalError( "XSessionSearch failed to get results buffer size with error %d\n", ret );
    }

    m_pSearchResults = (XSESSION_SEARCHRESULT_HEADER *)new BYTE[ cbResults ];

    if( !m_pSearchResults )
    {
        FatalError( "Failed to allocate memory for search results." );
    }

    // Initialize the property/context arrays. These must live on beyond the
    // call to XSessionSearch, so they are declared static
    static XUSER_PROPERTY aProperties[4];
    aProperties[0].dwPropertyId = PROPERTY_MIN_VICTORY_POINTS;
    aProperties[0].value.nData  = m_nMinVictoryPoints;
    aProperties[0].value.type   = XUSER_DATA_TYPE_INT32;
    aProperties[1].dwPropertyId = PROPERTY_MAX_VICTORY_POINTS;
    aProperties[1].value.nData  = m_nMaxVictoryPoints;
    aProperties[1].value.type   = XUSER_DATA_TYPE_INT32;

    WORD cProperties = 2;

    static XUSER_CONTEXT aContexts[3];
    aContexts[0].dwContextId = X_CONTEXT_GAME_TYPE;
    aContexts[0].dwValue     = m_nGameType;
    aContexts[1].dwContextId = X_CONTEXT_GAME_MODE;
    aContexts[1].dwValue     = m_nGameMode;
    aContexts[2].dwContextId = CONTEXT_MAP;
    aContexts[2].dwValue     = m_nMap;

    WORD cContexts = m_nMap == m_cMaps? 2 : 3;

    // If we are in a Presence session and are the session
    // host, calculate the TrueSkill for the team
    // and set our search parameters accordingly
    SessionManager* pPresenceSessionMgr = GetPresenceSession(); 
    if( pPresenceSessionMgr != NULL && pPresenceSessionMgr->IsSessionHost() )
    {
        DOUBLE mu, sigma;
        CalculateTeamTrueSkill( mu, sigma );

        aProperties[2].dwPropertyId = X_PROPERTY_GAMER_MU;
        aProperties[2].value.type = XUSER_DATA_TYPE_DOUBLE;
        aProperties[2].value.dblData = mu;
		
        aProperties[3].dwPropertyId = X_PROPERTY_GAMER_SIGMA;
        aProperties[3].value.type = XUSER_DATA_TYPE_DOUBLE;
        aProperties[3].value.dblData = sigma;

        cProperties = 4;
    }

    // Fire off the query
    ret = XSessionSearch(
        SESSION_MATCH_QUERY_FIND_MATCHES, // Procedure index
        pSessionMgr->GetSessionOwner(),   // User index
        MAX_SEARCHRESULTS,                // Maximum results
        cProperties,                      // Number of properties
        cContexts,                        // Number of contexts
        aProperties,                      // Properties
        aContexts,                        // Contexts
        &cbResults,                       // Size of result buffer
        m_pSearchResults,                 // Pointer to results
        pXOverlapped                      // Overlapped data structure
        );

    if( ret != ERROR_IO_PENDING && ret != ERROR_SUCCESS )
    {
        FatalError( "XOnlineMatchSearch failed with error %d\n", ret );
    }

    return HRESULT_FROM_WIN32( ret );
}

//--------------------------------------------------------------------------------------
// Name: DoSessionSearchDoneTask()
// Desc: Handler for processing session search results
//--------------------------------------------------------------------------------------
HRESULT Sample::DoSessionSearchDoneTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped )
{
    m_pSessionMgrCtx = NULL;

    if( SetLastXSessionError( pSessionMgr, pXOverlapped ) )
    {
        ScheduleSessionMgrDeletionTasks( pSessionMgr );
        ScheduleChangeStateTask( APPSTATE_MAINMENU, pSessionMgr );

        return S_OK;
    }


    if( m_pSearchResults->dwSearchResults )
    {
        if( !m_bQoSTesting && m_pSearchResults )
        {
            assert( m_pSearchResults->dwSearchResults <= MAX_SEARCHRESULTS );

            for( DWORD nSessions = 0; nSessions < m_pSearchResults->dwSearchResults; ++nSessions )
            {
                m_QoSxnaddr[nSessions] = &(m_pSearchResults->pResults[nSessions].info.hostAddress);
                m_QoSxnkid[nSessions] = &(m_pSearchResults->pResults[nSessions].info.sessionID);
                m_QoSxnkey[nSessions] = &(m_pSearchResults->pResults[nSessions].info.keyExchangeKey);
            }
            // Create an event object that is autoreset with an initial state of "not signaled".
            // Pass this event handle to the QoSLookup to recieve notification of each QoS lookup.
            HANDLE hQoSLookup = CreateEvent( NULL, false, false, NULL );
            assert( hQoSLookup != NULL );

            INT iRet = XNetQosLookup( m_pSearchResults->dwSearchResults,    // Number of remote Xbox 360 consoles to probe.
                                      m_QoSxnaddr,                          // Array of pointers to XNADDR structures  
                                      m_QoSxnkid,                           // Array of pointers to XNKID structures that contain session IDs for the remote Xbox 360 consoles
                                      m_QoSxnkey,                           // Array of pointers to XNKEY structures that contain key-exchange keys for the remote Xbox 360 consoles. 
                                      0,                                    // Number of security gateways to probe. 
                                      NULL,                                 // Pointer to an array of IN_ADDR structures that contain the IP addresses of the security gateways. 
                                      NULL,                                 // Pointer to an array of service IDs for the security gateways.
                                      8,                                    // Number of desired probe replies to receive.
                                      0,                                    // Maximum upstream bandwidth that the outgoing QoS probe packets can consume. 
                                      0,                                    // Flags
                                      hQoSLookup,                           // Event handle
                                      &m_pQoSResult );                      // Pointer to a pointer to an XNQOS structure that receives the results from the QoS probes.

            if( 0 != iRet )
            {
                FatalError( "XNetQosLookup failed with error 0x%08x", iRet);
            }

            m_bQoSTesting = TRUE;

            // Wait for results to all complete.  cxnqosPending will eventually hit zero.
            // Pause thread waiting for QosLookup events to be triggered.
            while( m_pQoSResult->cxnqosPending != 0 )
            {
                if( hQoSLookup )
                {
                    WaitForSingleObject( hQoSLookup, INFINITE );
                }
            }

            // Close handle
            XCloseHandle( hQoSLookup );

            // Sort search results for ascending median roundtrip times
            // NOTE: There are more efficient sorting algorithms
            for( DWORD nSessions1 = 0; nSessions1 < m_pSearchResults->dwSearchResults; ++nSessions1 )
            {
                for( DWORD nSessions2 = nSessions1 + 1; nSessions2 < m_pSearchResults->dwSearchResults; ++nSessions2 )
                {
                    if( nSessions1 == nSessions2 )
                        continue;

                    // Swap results if required
                    if( m_pQoSResult->axnqosinfo[nSessions1].wRttMedInMsecs > m_pQoSResult->axnqosinfo[nSessions2].wRttMedInMsecs )
                    {
                        XSESSION_SEARCHRESULT temp_result = m_pSearchResults->pResults[nSessions1];

                        m_pSearchResults->pResults[nSessions1] = m_pSearchResults->pResults[nSessions2];
                        m_pSearchResults->pResults[nSessions2] = temp_result;

                        XNQOSINFO temp_info = m_pQoSResult->axnqosinfo[nSessions1];
                        m_pQoSResult->axnqosinfo[nSessions1] = m_pQoSResult->axnqosinfo[nSessions2];
                        m_pQoSResult->axnqosinfo[nSessions2] = temp_info;
                    }
                }
            }
        }

        m_pSessionMgrCtx = pSessionMgr;
    }
    else
    {
        // No search results
        swprintf_s( m_wszLastSessionError, L"Search did not return any results\n" );
    }

    return S_OK;
}

