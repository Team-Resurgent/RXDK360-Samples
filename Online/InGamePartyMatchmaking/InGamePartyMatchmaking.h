//--------------------------------------------------------------------------------------
// InGamePartyMatchmaking.h
//
// Definition of Sample for InGamePartyMatchmaking sample
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "Xplat.h"

// forward declarations
class CHostMigrationHelper;

//--------------------------------------------------------------------------------------
// Color definitions
//--------------------------------------------------------------------------------------
#define LIGHTGREY       D3DCOLOR_ARGB( 0xD3, 0xD3, 0xD3, 0xD3 )
#define GREY            D3DCOLOR_ARGB( 0xFF, 0x54, 0x54, 0x54 )
#define GREEN           D3DCOLOR_ARGB( 0xFF, 0x00, 0xFF, 0x00 )

//--------------------------------------------------------------------------------------
// Global constants
//--------------------------------------------------------------------------------------
const DWORD HEARTBEAT_TIME          = 1000;  // send a heartbeat every second
const DWORD HEARTBEAT_TIMEOUT       = 10000; // timeout in milliseconds if a heartbeat not received
const DWORD IDLE_TIMEOUT            = 90000; // timeout in milliseconds if no user activity
const DWORD WAVE_TIME               = 5000;  // wave for five seconds
const DWORD WAVE_BLINKS             = 10;    // blink five times when waving
const DWORD REGISTRATION_TIME       = 10000; // wait ten seconds for everybody to register
const DWORD MAX_SEARCHRESULTS       = 10;    // Maximum search results
const IN_ADDR  NULLADDR             = { 0 }; // Null IN_ADDR
const DWORD MAX_CHAR_SESSION_ERROR  = 256;   // Maximum chars in session error string
const DWORD MAX_OVERLAPPED_TIME     = 30000; // wait these many milliseconds for overlapped operation

//--------------------------------------------------------------------------------------
// Session constants
//--------------------------------------------------------------------------------------
const DWORD PRESENCE_SESSION_FLAGS           = XSESSION_CREATE_GROUP_LOBBY;
const DWORD MATCHMAKING_SESSION_FLAGS        = XSESSION_CREATE_GROUP_GAME;
const DWORD PRESENCE_SESSION_PUBLICSLOTS     = 2;     // Default number of public slots
const DWORD PRESENCE_SESSION_PRIVATESLOTS    = 2;     // Default number of private slots
const DWORD MATCHMAKING_SESSION_PUBLICSLOTS  = 4;     // Default number of public slots
const DWORD MATCHMAKING_SESSION_PRIVATESLOTS = 4;     // Default number of private slots

//--------------------------------------------------------------------------------------
// Custom actions
//--------------------------------------------------------------------------------------
const DWORD CUSTOM_ACTION_SUBSCRIBE_PRESENCE = 0;
const DWORD CUSTOM_ACTION_UNSUBSCRIBE_PRESENCE = 1;
const DWORD MAX_NON_FRIENDS_SUBSCRIPTIONS    = 10;

//--------------------------------------------------------------------------------------
// Typedefs
//--------------------------------------------------------------------------------------
typedef std::pair<ULONGLONG, SessionManager*> NONCE_SESSION_PAIR;

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
#ifdef _XBOX
class Sample : public ATG::Application
#else if LIVE_ON_WINDOWS
class Sample : public CSample
#endif
{
    friend class CXPlat_Draw;
    friend class CXPlat_UserInput;
    friend class CXPlat_Signin;
    friend class CXPlat_XOnline;

    // Valid app states
    enum APPSTATE
    {
        APPSTATE_MAINMENU,                       // Main Menu
        APPSTATE_ENUMERATING_PRESENCE,           // Enumerating presence of friends or non-friends
        APPSTATE_UPDATING_RICHPRESENCE,          // Updating rich presence
        APPSTATE_CREATE_PRESENCE_UI,             // Display the create presence session UI screen 
        APPSTATE_CREATE_MATCHMAKING_UI,          // Display the create matchmaking session
                                                 // UI screen
        APPSTATE_CREATE_PRESENCE,                // Create a new presence session
        APPSTATE_CREATE_MATCHMAKING,             // Create a new matchmaking session
        APPSTATE_CREATE_SESSION_UNHOSTED,        // Create a new session but hosted elsewhere
        APPSTATE_CREATING_SESSION,               // Creating the XSession
        APPSTATE_ADDING_LOCAL_PLAYERS,           // Adding local players to session
        APPSTATE_ADDED_LOCAL_PLAYERS,            // Added local players to session
        APPSTATE_ADDING_REMOTE_PLAYERS,          // Adding local players to session
        APPSTATE_ADDED_REMOTE_PLAYERS,           // Added local players to session
        APPSTATE_REMOVING_LOCAL_PLAYERS,         // Removing local players from session
        APPSTATE_REMOVED_LOCAL_PLAYERS,          // Removed local players from session
        APPSTATE_REMOVING_REMOTE_PLAYERS,        // Removing remote players from session
        APPSTATE_REMOVED_REMOTE_PLAYERS,         // Removing remote players from session
        APPSTATE_SEARCHUI_MATCHMAKING,           // Display matchmaking search UI screen
        APPSTATE_SEARCH_MATCHMAKING,             // Search for a matchmaking session
        APPSTATE_SEARCH_MATCHMAKING_DONE,        // Search results for a matchmaking session
        APPSTATE_CONNECTING_SESSION,             // Connecting to a session
        APPSTATE_VIEWSTATS,                      // Viewing stats
        APPSTATE_DELETING_SESSION,               // Deleting the session

        // The below appstates are all "in session"
        APPSTATE_PREGAME,                       // In a session
        APPSTATE_WAITINGFORREGISTRATION,        // Waiting for players to register
        APPSTATE_REGISTERING,                   // Registering for arbitration
        APPSTATE_REGISTERED,                    // Waiting for the host to start the game
        APPSTATE_STARTING,                      // Starting the session
        APPSTATE_INGAME,                        // Game in progress
        APPSTATE_WRITINGSTATS,                  // Writing stats
        APPSTATE_ENDING,                        // Ending the session
        APPSTATE_POSTGAME,                      // Game over
        APPSTATE_HOSTMIGRATION,                 // Begin migrating a session

        APPSTATE_CANCELLING_OVERLAPPED,          // Cancelling an overlapped operation
        APPSTATE_COUNT                          // Count of APPSTATE enumeration values
    };

    // main menu
    enum MAINMENU
    {
        MAINMENU_CREATE_PRESENCE,          // Create a presence session
        MAINMENU_CREATE_MATCHMAKING,       // Create a matchmaking session
        MAINMENU_SEARCH_MATCHMAKING,       // Search for a matchmaking session
        #ifdef _XBOX
        MAINMENU_VIEWSTATS,                // View stats
        #endif
        MAINMENU_LOGIN,                    // Change logged-in users
        MAINMENU_DELETE_PRESENCE,          // Delete the presence session
        #ifdef _XBOX
        MAINMENU_FIND_LIVEPARTY_SESSIONS,  // Invoke Xbox LIVE Party Community Sessions UI
        #endif
        MAINMENU_MAX
    };

    // create matchmaking menu
    enum CREATE_MATCHMAKING_MENU
    {
        CREATE_MATCHMAKING_MENU_GAMETYPE,               // Game type
        CREATE_MATCHMAKING_MENU_GAMEMODE,               // Game mode
        CREATE_MATCHMAKING_MENU_MAP,                    // Map
        CREATE_MATCHMAKING_MENU_VICTORYPOINTS,          // Victory points
        CREATE_MATCHMAKING_MENU_INVITES,                // Allow invitations
        CREATE_MATCHMAKING_MENU_JOINVIAPRESENCE,        // Allow join-via-presence
        CREATE_MATCHMAKING_MENU_JOININPROGRESS,         // Allow join-in-progress
        CREATE_MATCHMAKING_MENU_CREATE,                 // Create a session
        CREATE_MATCHMAKING_MENU_MAX
    };

    // create presence menu
    enum CREATE_PRESENCE_MENU
    {
        CREATE_PRESENCE_MENU_INVITES,                // Allow invitations
        CREATE_PRESENCE_MENU_JOINVIAPRESENCE,        // Allow join-via-presence
        CREATE_PRESENCE_MENU_JOININPROGRESS,         // Allow join-in-progress
        CREATE_PRESENCE_MENU_CREATE,                 // Create a session
        CREATE_PRESENCE_MENU_MAX
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
    static const D3DCOLOR COLOR_BACKGROUND1   = D3DCOLOR_ARGB( 0xFF, 0x00, 0x00, 0xFF);
    static const D3DCOLOR COLOR_BACKGROUND2   = D3DCOLOR_ARGB( 0xFF, 0x00, 0x00, 0x00);
    static const D3DCOLOR COLOR_TEXT          = D3DCOLOR_ARGB( 0xFF, 0xFF, 0xFF, 0xFF);
    static const D3DCOLOR COLOR_GRAY          = D3DCOLOR_ARGB( 0xC8, 0xB0, 0xB0, 0xB0);
    static const D3DCOLOR COLOR_HILIGHT       = D3DCOLOR_ARGB( 0xFF, 0xFF, 0xFF, 0x00);
    static const D3DCOLOR COLOR_HEADER        = D3DCOLOR_ARGB( 0xFF, 0x00, 0xFF, 0xFF);
    static const D3DCOLOR COLOR_LOOPBACK      = D3DCOLOR_ARGB( 0xFF, 0xFF, 0x00, 0x00 );
    static const D3DCOLOR COLOR_TALKING       = D3DCOLOR_ARGB( 0xFF, 0x00, 0xFF, 0x00 );

    // Constants
    static const DWORD  m_anNotificationPosition[];   // notification placement
    static const size_t m_cNotificationPosition;      // number of placements
    static const WCHAR* m_astrNotificationPosition[]; // notification placement labels
    static const char*  m_astrAppStates[APPSTATE_COUNT];

    static const WCHAR* m_astrGameTypes[];
    static const DWORD  m_cGameTypes;
    static const WCHAR* m_astrGameModes[];
    static const DWORD  m_cGameModes;
    static const DWORD  m_cPartySessionGameMode;
    static const WCHAR* m_astrMaps[];
    static const DWORD  m_cMaps;
    static const DWORD  m_dwSkillViews[];
    static const WCHAR* m_QoSString;

    static const DWORD  VICTORY_POINTS_MIN = 1;
    static const DWORD  VICTORY_POINTS_MAX = 20;

    static const struct _LEADERBOARD
    {
        WCHAR* m_wchName;
        DWORD  m_dwIndex;
        DWORD  m_dwSkillIndex;            // index of the associated TrueSkill(TM)
                                          // skill leaderboard -- if 0, then no skill
                                          // leaderboard is taken
    } m_aLeaderboards[2][4];
    static const DWORD  m_cLeaderboards;
    static const DWORD  m_nMaxLeaderboardRows;

    CXPlat_Signin       m_CXPlat_Signin;
    CXPlat_Draw         m_CXPlat_Draw;
    CXPlat_UserInput    m_CXPlat_UserInput;
    CXPlat_XOnline      m_CXPlat_XOnline;
    
    #ifdef _XBOX
    BOOL                m_bDrawHelp;            // Draw the help screen
    ATG::Help           m_Help;

    BOOL                m_bUseAlphaBlending;    // use alpha blending in the
                                                // TrueSkill(TM) skill bar display?
    #endif

    HANDLE             m_hLiveListener;         // Live notification listener
    HANDLE             m_hFriendsListener;      // Friends notification listener
    HANDLE             m_hCustomListener;       // Custom notification listener
    APPSTATE           m_AppState;                  // Current application state
    APPSTATE           m_AppStateBeforeMigration;   // Last application state before migration
    UINT               m_nNotificationPosition; // Current notification placement
    UINT               m_nMenuItem;             // Current menu item
    XOVERLAPPED        m_Overlapped;            // Overlapped task data
    XOVERLAPPED        m_XSessionOverlapped;    // XOVERLAPPED structure for all XSession calls

    // Networking-related member variables
    SOCKET             m_Socket;                        // Socket for communication
    DWORD              m_dwConnectionTimer;             // tick count of connection attempt
    DWORD              m_dwRegistrationTimer;           // time for other clients to register
    LocalClientInfo    m_Local;                         // information about the local client
    ClientInfoVec      m_vecRemote;                     // information about the remote clients
    XUSER_SIGNIN_INFO  m_SignInInfo[MAX_USER_COUNT];    // sign-in info for the local client

    // Game parameters
    DWORD              m_nGameType;
    DWORD              m_nGameMode;
    DWORD              m_nMap;
    DWORD              m_nVictoryPoints;
    DWORD              m_nMinVictoryPoints;
    DWORD              m_nMaxVictoryPoints;

    // QoS Data
    BOOL               m_bQoSTesting;
    XNQOS              m_QoSResult;
    XNQOS*             m_pQoSResult;
    const XNADDR*      m_QoSxnaddr[MAX_SEARCHRESULTS];
    const XNKID*       m_QoSxnkid[MAX_SEARCHRESULTS];
    const XNKEY*       m_QoSxnkey[MAX_SEARCHRESULTS];

    // Stats
    PXUSER_STATS_READ_RESULTS m_pStats;       // Leaderboard data
    DWORD                     m_nLeaderboard; // Current leaderboard
    XUSER_STATS_SPEC          m_Spec;         // Stats specification
    BOOL                      m_bReadingLeaderboard;
    UINT                      m_nNextSelectedRank;    // Rank of user to attempt to select
    XUID                      m_xuidNextSelectedXuid; // XUID of user to attempt to select

    PXSESSION_SEARCHRESULT_HEADER      m_pSearchResults; // Match search results

    // Host Migration
    CHostMigrationHelper m_HostMigration;

    // STL map of session IDs and their associated SessionManager*
    std::map< ULONGLONG /*session nonce*/, SessionManager* /*instance*/ > m_mapSessions;

    // Our task scheduler instance
    TaskScheduler   m_TaskScheduler;

    // Linked list for serializing XSession calls
    std::deque<TASKHANDLE> m_listSessionTaskGroupHandles;

    // Vector of matchmaking sessions that have In-Game party members in them
    std::vector<XNKID> m_vMatchmakingSessionsWithPartyMembers;

    // Session manager context used in UI navigation
    SessionManager* m_pSessionMgrCtx;

    // Last session error
    WCHAR                       m_wszLastSessionError[ MAX_CHAR_SESSION_ERROR ];

    // XBox LIVE Party user list
    #ifdef _XBOX
    XPARTY_USER_LIST m_LivePartyUserList;
    #endif

    //
    // Task-releated data structures
    struct JoinLeaveData
    {
       ClientInfo* m_pClient; 
       DWORD       m_dwUserMask;
       DWORD       m_aIndices [ MAX_USER_COUNT ];
       BOOL        m_abPrivate[ MAX_USER_COUNT ];
       XUID        m_aXuids   [ MAX_USER_COUNT ];
    };

    struct ModifyFlagsData
    {
        DWORD   m_dwFlags;
        BOOL    m_bClearFlags;
    };

    struct NetMsgData
    {
        CMessage*   m_pMsg;
        IN_ADDR     m_inaddrFrom;
        XNADDR      m_addrFrom;
    };

    struct EnumPresenceData
    {
       DWORD       m_dwUserIndex;
       HANDLE      m_hEnum;
    };

    struct UserContextData
    {
       DWORD       m_dwUserIndex;
       DWORD       m_dwContextID;
       DWORD       m_dwContextValue;
    };

    // Task Scheduler task data
    #pragma pack( push )
    #pragma pack( 1 )
    struct TaskSchedulerTaskData
    {
        int                 m_index;
        WORD                m_wTaskID;
        Sample*             m_pSample;
        XOVERLAPPED*        m_pXOverlapped;
        APPSTATE            m_newAppState;
        SessionManager*     m_pSessionMgr;
        
        TaskSchedulerTaskData()
        {
            m_index         = -1;
            m_newAppState   = (APPSTATE)-1;
            m_cRef          = 0;
            m_wTaskID       = 0;
            m_pSample       = NULL;
            m_pSessionMgr   = NULL;
            m_pXOverlapped  = NULL;
        }
        union
        {
            JoinLeaveData       m_JoinLeaveData;
            ModifyFlagsData     m_ModifyFlagsData;
            NetMsgData          m_NetMsgData;
            EnumPresenceData    m_EnumPresenceData;
            UserContextData     m_UserContextData;
        };
        
        unsigned int        m_cRef;
        
        VOID AddRef()
        {
            ++m_cRef;

            if( ( TASK_AREA( m_wTaskID ) == TASKAREA_NETWORKMESSAGE ) && m_NetMsgData.m_pMsg )
            {
                m_NetMsgData.m_pMsg->AddRef();
            }
        }

        VOID Release()
        {
            assert( m_cRef != 0 );
            if( m_cRef == 0 )
            {
                // Should never come here
                DebugBreak();
            }

            if( ( TASK_AREA( m_wTaskID ) == TASKAREA_NETWORKMESSAGE ) && m_NetMsgData.m_pMsg )
            {
                m_NetMsgData.m_pMsg->Release();
            }

            if( --m_cRef == 0 )
            {
                m_pSample->ReleaseTaskSchedulerTaskData( this );
            }
        }        
    };
    #pragma pack( pop )

	TaskSchedulerTaskData*  m_pTaskSchedulerTaskDataStructs;
	std::stack<UINT>		m_stackFreeTaskSchedulerTaskDataIndices;

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();

    #ifdef _XBOX
    virtual HRESULT Render();
    #else if LIVE_ON_WINDOWS
    virtual VOID Render();
    virtual HRESULT DeviceCreated();
    virtual VOID DeviceDestroyed();
    #endif

    // Set last error from an XSession call
    BOOL SetLastXSessionError( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped = NULL );

    // Display the friends list
    VOID ShowFriendsList( UINT nController );

    // Change notification position
    VOID ChangeNotificationPosition();

    // Handle signin changes
    VOID HandleSigninChanges( const BOOL bConsiderPreviousSigninState = TRUE );

    // Returns a pointer to a free TaskSchedulerTaskData struct in our m_pTaskSchedulerTaskDataStructs array
    // that we can use for task scheduling
    TaskSchedulerTaskData* GetFreeTaskSchedulerTaskData();

    // Release a TaskSchedulerTaskData struct for use in a future scheduled task
    VOID ReleaseTaskSchedulerTaskData( const TaskSchedulerTaskData* pData );

    // Cancel all pending XSession tasks for the specified session
    VOID CancelAllPendingXSessionTasksForSession( SessionManager* pSessionMgr );

    // Helper functions to facilitate session task scheduling
    VOID ScheduleTasks( const WORD* pTaskIDs, 
                        const APPSTATE* pAppStates, 
                        const UINT cTasks, 
                        SessionManager* pSessionMgr = NULL,
                        void* pContext = NULL );

    VOID RescheduleTask( const WORD taskID, const TASKHANDLE* pHandle );

    VOID ScheduleCancelOverlappedTask();

    VOID ScheduleSessionSearchTasks( SessionManager* pSessionMgr );

    VOID SchedulePresenceEnumTasks( const BOOL bEnumFriends, const DWORD dwUserIndex );

    VOID ScheduleUserContextUpdateTasks( const DWORD dwUserIndex, const DWORD dwContextID, const DWORD dwContextValue );

    VOID ScheduleNetMsgTask( const WORD taskID, const CMessage* pMsg, IN_ADDR& inAddrFrom );
    VOID ScheduleChangeStateTask( const APPSTATE newState, SessionManager* pSessionMgr = NULL );
    VOID ScheduleSessionCreationTasks( SessionManager* pSessionMgr );
    VOID ScheduleSessionStartTasks( SessionManager* pSessionMgr );
    VOID ScheduleSessionEndTasks( SessionManager* pSessionMgr );
    VOID ScheduleSessionDeletionTasks( SessionManager* pSessionMgr );
    VOID ScheduleSessionMgrDeletionTasks( SessionManager* pSessionMgr );
    VOID ScheduleSessionJoinTasks( SessionManager* pSessionMgr, 
                                   ClientInfo* pClient, 
                                   const DWORD dwUserMask = 0xF );

    VOID ScheduleSessionConnectionTasks( SessionManager* pSessionMgr );

    VOID ScheduleSessionModifyTasks( SessionManager* pSessionMgr, const DWORD dwFlags, const BOOL bClearFlags = FALSE ); 
    
    VOID ScheduleSessionLeaveTasks( SessionManager* pSessionMgr, 
                                    ClientInfo* pClient, 
                                    const DWORD userMask = 0xF  );

    VOID ScheduleSessionWriteTasks( SessionManager* pSessionMgr );
    VOID ScheduleSessionWriteTasks( SessionManager* pSessionMgr, 
                                    ClientInfo* pClient, 
                                    const DWORD dwUserMask );

    VOID ScheduleSessionMigrationTasks( SessionManager* pSessionMgr );

    VOID ScheduleSessionRegisterTasks( SessionManager* pSessionMgr );
    VOID ScheduleSessionHostRegisterTasks( SessionManager* pSessionMgr );

    // Task Scheduler task handlers
	static HRESULT DoTaskCallback( const WORD taskID, const TASKHANDLE* pHandle, void* pTaskData );
    static BOOL ReleaseTaskCallback( const TASKHANDLE* phTask, const void* pTaskData, void* pUserContext );

    // App-related task handler
    HRESULT DoSwitchAppStateTask( const APPSTATE NewState, SessionManager* pSessionMgr ); 
    VOID SwitchToPostJoinLeaveAppState( SessionManager* pSessionMgr );

    // Rich-presence mapping helper
    DWORD GetRichPresenceIDForSession( const SessionManager* pSessionMgr );

    // Session-related task handler
    HRESULT DoSessionTask( const WORD taskID, 
                           const TASKHANDLE* pHandleTask,
                           TaskSchedulerTaskData* pTSTaskData,
                           SessionManager* pSessionMgr,
                           XOVERLAPPED* pXOverlapped ); 


    // Network-message-related task handler
    HRESULT DoNetMsgTask( const WORD taskID, 
                          const NetMsgData& netMsgData ); 

    // XOverlapped-related task handler
    HRESULT DoOverlappedTask( const WORD taskID, 
                              const TASKHANDLE* pHandle,
                              TaskSchedulerTaskData* pTSTaskData,
                              SessionManager* pSessionMgr, 
                              XOVERLAPPED* pXOverlapped ); 

    // Presence-related task handlers
    HRESULT DoPresenceTask( const WORD taskID,
                            const DWORD dwUserIndex,
                            HANDLE* phEnum,
                            XOVERLAPPED* pXOverlapped ); 

    // UserContext-related task handlers
    HRESULT DoUpdateUserContextTask( const WORD taskID,
                                     const DWORD dwUserIndex, 
                                     const DWORD dwContextID, 
                                     const DWORD dwContextValue,
                                     XOVERLAPPED* pXOverlapped ); 

    //
    // Session task handlers
    HRESULT DoSessionCreatedTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped );
    HRESULT DoSessionStartedTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped );
    HRESULT DoSessionEndedTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped );
    HRESULT DoSessionDeletedTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped );
    HRESULT DoSessionMgrDeleteTask( SessionManager* pSessionMgr );
    HRESULT DoSessionStatsWriteTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped = NULL  );
    HRESULT DoSessionStatsWriteTask( SessionManager* pSessionMgr, 
                                     ClientInfo* pClient, 
                                     const DWORD userMask,
                                     XOVERLAPPED* pXOverlapped = NULL );    
    HRESULT DoSessionStatsWrittenTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped );

    HRESULT DoSessionSearchTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped = NULL  );
    HRESULT DoSessionSearchDoneTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped );

    VOID DoSessionAddUsersTask ( ClientInfo* pClient, 
                                 SessionManager* pSessionMgr,
                                 __inout_ecount(dwCountPlayers) DWORD* paIndices,
                                 __inout_ecount(dwCountPlayers) XUID* paXuids,
                                 __inout_ecount(dwCountPlayers) BOOL* pabPrivate,
                                 const DWORD dwCountPlayers,
                                 XOVERLAPPED* pXOverlapped = NULL,
                                 const DWORD userMask = 0xF );

    VOID DoSessionRemoveUsersTask (  ClientInfo* pClient, 
                                     SessionManager* pSessionMgr, 
                                     __inout_ecount(dwCountPlayers) DWORD* paIndices,
                                     __inout_ecount(dwCountPlayers) XUID* paXuids,
                                     __inout_ecount(dwCountPlayers) BOOL* pabPrivate,
                                     const DWORD dwCountPlayers,
                                     XOVERLAPPED* pXOverlapped = NULL,
                                     const DWORD userMask = 0xF );


    // Join/Leave handlers
    HRESULT DoSessionLocalPlayersAddedTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped );
    HRESULT DoSessionLocalPlayersRemovedTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped );
    HRESULT DoSessionRemotePlayersAddedTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped );
    HRESULT DoSessionRemotePlayersRemovedTask( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped );

    // Connection handler
    HRESULT DoSessionConnectingTask( SessionManager* pSessionMgr );

    // Host migration handlers
    HRESULT DoSessionMigrateHostTask( SessionManager* pSessionMgr );
    HRESULT DoSessionMigratingHostTask( SessionManager* pSessionMgr );
    HRESULT DoSessionMigratedHostTask( SessionManager* pSessionMgr );

    // Arbitration registration handlers
    HRESULT DoSessionHostBeginRegisterTask( SessionManager* pSessionMgr );
    HRESULT DoSessionHostWaitRegisterTask( SessionManager* pSessionMgr );
    HRESULT DoSessionRegisteredTask( SessionManager* pSessionMgr );

    //
    // Network message task handlers
    VOID DoNetMsgStartGame( const NetMsgData& netMsgData );
    VOID DoNetMsgQuerySession( const NetMsgData& netMsgData );
    VOID DoNetMsgRespSession( const NetMsgData& netMsgData );
    VOID DoNetMsgJoinSession( const NetMsgData& netMsgData );
    VOID DoNetMsgJoinSessionParty( const NetMsgData& netMsgData );
    VOID DoNetMsgFoundSession( const NetMsgData& netMsgData );
    VOID DoNetMsgLeftSession( const NetMsgData& netMsgData );
    VOID DoNetMsgJoinResponse( const NetMsgData& netMsgData );
    VOID DoNetMsgJoinResponseParty( const NetMsgData& netMsgData );
    VOID DoNetMsgPlayerInfo( const NetMsgData& netMsgData );
    VOID DoNetMsgWave( const NetMsgData& netMsgData );
    VOID DoNetMsgHeartbeat( const NetMsgData& netMsgData );
    VOID DoNetMsgGoodbye( const NetMsgData& netMsgData );
    VOID DoNetMsgEndGame( const NetMsgData& netMsgData );
    VOID DoNetMsgRegister( const NetMsgData& netMsgData );
    VOID DoNetMsgRegistered( const NetMsgData& netMsgData );
    VOID DoNetMsgScorePoint( const NetMsgData& netMsgData );
    VOID DoNetMsgPointTotal( const NetMsgData& netMsgData );

    //
    // Presence task handlers
    HRESULT DoPresenceEnumFriends( const DWORD dwUserIndex, HANDLE* phEnum, XOVERLAPPED* pXOverlapped ); 
    HRESULT DoPresenceEnumNonFriends( const DWORD dwUserIndex, HANDLE* phEnum, XOVERLAPPED* pXOverlapped ); 
    HRESULT DoPresenceEnumFriendsDone( const DWORD dwUserIndex, const HANDLE* phEnum, XOVERLAPPED* pXOverlapped ); 
    HRESULT DoPresenceEnumNonFriendsDone( const DWORD dwUserIndex, const HANDLE* phEnum, XOVERLAPPED* pXOverlapped ); 

    // Functions to manage membership in Matchmaking sessions that our
    // party is in
    BOOL IsMatchmakingSessionWithPartyMembers( const XNKID& sessionID );
    VOID AddSessionIDToPartyMatchmakingSessionsList( const XNKID& sessionID );
    VOID RemoveSessionIDFromPartyMatchmakingSessionsList( const XNKID& sessionID );

    // Retrieves the right SessionManager* which is our
    // Matchmaking session
    SessionManager* GetMatchmakingSession();

    // Retrieves the right SessionManager* which is our
    // Presence session
    SessionManager* GetPresenceSession();

    // Gracefully delete a session
    VOID DeleteSessionGracefully( SessionManager* pSessionMgr );

    // Retrieves the right SessionManager* given its session nonce
    const SessionManager* SessionManagerFromNonce ( ULONGLONG qwSessionNonce );

    // Retrieves the right SessionManager* given its session ID
    const SessionManager* SessionManagerFromSessionID ( const XNKID& sessionID );

    // Retrieves the right ClientInfo* given its session ID
    ClientInfo* SessionHostFromSessionID( const XNKID& sessionID );

    // Retrieves the right SessionManager* given a set of expected and 
    // unexpected flags
    const SessionManager* FromFlags ( const DWORD dwExpectedFlags, 
                                         const DWORD dwUnexpectedFlags = 0 );

    // Look for a session
    VOID SearchForSession();

    // Join party members to a found matchmaking session
    VOID JoinPartyMembersToSession( const XSESSION_INFO& sessionInfo, 
                                    const BOOL bInvited );

    // Join party member to a found matchmaking session
    VOID JoinPartyMemberToSession( const XSESSION_INFO& sessionInfo, 
                                   const ClientInfo* pClient,
                                   const BOOL bInvited );

    // Clears local client info about presence session affiliation
    VOID ClearPresenceSessionAffiliation();

    // Join a session from an XN_LIVE_INVITE_ACCEPTED notification
    VOID JoinSessionFromInviteInfoPreamble( XINVITE_INFO InviteInfo );

    // Determines if game invites should be sent to the XBox LIVE Party
    #ifdef _XBOX
    BOOL ShouldSendGameInvitesToLiveParty();
    #endif

    // Determines if game invites should be sent to friends
    BOOL ShouldSendGameInvitesToFriends( const DWORD dwUserIndex = 0 );

    // Send a message to a remote player
    VOID SendMessage( CMessage* pMessage, IN_ADDR addr );

    // Is IN_ADDR from Presence session host?
    BOOL IsPresenceSessionHostInAddr( const IN_ADDR& addr );

    // Send a voice message
    VOID SendVoiceMessage();

    // Receive a message
    VOID ReceiveMessage();

    // Handle a client leaving the session
    VOID ClientDropped( IN_ADDR addr, const XNKID& sessionID, DWORD userMask = 0xF );

    // Do heartbeat housework
    VOID HandleHeartbeats();
    VOID HandleHeartbeat( SessionManager* pSessionMgr );
    HRESULT HandleHeartbeat( ClientInfo* pClient, const XNKID& sessionID );

    // Do idle timeout housework
    VOID HandleIdle();

    // Delete local XSession instances
    VOID DeleteLocalSession( SessionManager* pSessionMgr );

    // Delete all local XSession instances
    VOID DeleteLocalSessions();

    // Retrieve the TrueSkill(TM) skill values for users
    VOID ReadTrueSkills( ClientInfo* pClient );

    // Calculate the aggregate TrueSkill(TM) of party members in the same
    // Presence session
    VOID CalculateTeamTrueSkill( DOUBLE& teamMu, DOUBLE& teamSigma );

    // Set the TrueSkill(TM) of a session
    VOID SetSessionTrueSkill( SessionManager* pSessionMgr );

    // Helper function to dump players joined to a session
    VOID DebugDumpSessionMembers( SessionManager* pSessionMgr );

    // Handle an accepted invitation notification
    BOOL HandleAcceptedInvitation( const XINVITE_INFO& InviteInfo );

    // Check to see if any custom action was invoked
    BOOL CheckForCustomAction();

    // Handle any LIVE notifications
    BOOL CheckForLiveNotifications();

    // Start the game
    VOID StartGame( const XNKID& sessionID );

    // End the game
    VOID EndGame( const XNKID& sessionID );

    // Helpers for joining/leaving sessions
    HRESULT LeaveSession( SessionManager* pSessionMgr );

    HRESULT JoinSession( const XNKID& sessionID, const IN_ADDR& hostInAddr, const BOOL bInvited );

    HRESULT JoinSessionPartyHost( SessionManager* pPresenceSessionMgr,
                                  const XNKID& sessionID, 
                                  const IN_ADDR& hostInAddr, 
                                  const BOOL bInvited );

    HRESULT SayGoodbye( SessionManager* pSessionMgr,
                        const DWORD dwXuidCount,
                        const XUID* pXuids,
                        const IN_ADDR& inaddr );

    // Arbitration helper
    VOID ProcessRegistrationList();

    // Stats
    HRESULT WriteStats( SessionManager* pSessionMgr, XOVERLAPPED* pXOverlapped = NULL );

    HRESULT WriteStats( SessionManager* pSessionMgr,
                     const ClientInfo* pClient,
                     const DWORD userMask,
                     XOVERLAPPED* pXOverlapped = NULL ); 

    VOID ReadLeaderboard( INT idx, XUID xuid );

    // Check to see if the presence state of any friends and subscribed non-friends has changed
    BOOL CheckForFriendsPresenceChanges();
    BOOL CheckForNonFriendsPresenceChanges();

    // Friends presence
    HRESULT RetrieveFriendsPresenceData( const DWORD dwUserIndex, 
                                         XONLINE_FRIEND* pArrayFriends, 
                                         const UINT cbFriendsArraySize, 
                                         DWORD* pcFriendsRetrieved,
                                         XOVERLAPPED* pOverlapped = NULL,
                                         HANDLE* phFriendsEnum = NULL );

    // Non-friends presence
    HRESULT RetrievePresenceData( const DWORD dwUserIndex, 
                                  const UINT cPeers,
                                  const XUID* pXuids,
                                  XONLINE_PRESENCE* pArrayPresence,
                                  const UINT cbPresenceArraySize, 
                                  DWORD* pcPresenceItemsRetrieved,
                                  XOVERLAPPED* pOverlapped = NULL,
                                  HANDLE* phEnumerator = NULL );

    // Update methods
    HRESULT UpdateMainMenu();
    HRESULT UpdateSearchUI();
    HRESULT UpdateSearchDone();
    HRESULT UpdateCreatePresenceUI();
    HRESULT UpdateCreateMatchmakingUI();
    HRESULT UpdateDeletingSessionFromContext();
    HRESULT UpdateInSession();

    #ifdef _XBOX
    HRESULT UpdateViewStats();
    #endif

    // Render methods
    HRESULT RenderUserStatus();
    HRESULT RenderMainMenu();
    HRESULT RenderEnumeratingPresence();
    HRESULT RenderUpdatingRichPresence();
    HRESULT RenderSearchUI();
    HRESULT RenderSearch();
    HRESULT RenderSearchDone();
    HRESULT RenderCreatePresenceSessionUI();
    HRESULT RenderCenteredText( const WCHAR* pwszText );
    HRESULT RenderAddPlayers();
    HRESULT RenderRemovePlayers();
    HRESULT RenderCreateMatchmakingSessionUI();
    HRESULT RenderCreatingSession();
    HRESULT RenderDeletingSession();
    HRESULT RenderCancellingOverlapped();
    HRESULT RenderConnecting();
    HRESULT RenderInSession();
    #ifdef _XBOX
    HRESULT RenderViewStats();
    #endif

    // Draw the players on a given client
    FLOAT RenderClient( WCHAR* strLabel, FLOAT x, FLOAT y, ClientInfo* pClient, ClientInfo* pHost );
public:

    Sample();
    ~Sample();

    const LocalClientInfo*      GetLocalClientInfo();
    const ClientInfoVec*        GetRemoteClientInfoVec();

    friend class CHostMigrationHelper;

    DWORD GetGameType();
    DWORD GetGameMode();
    DWORD GetMap();
    DWORD GetNumVictoryPoints();
    DWORD GetNumMinVictoryPoints();
    DWORD GetNumMaxVictoryPoints();
};

//--------------------------------------------------------------------------------------
// Name: operator== (IN_ADDR)
// Desc: Little helper operator that will prove useful
//--------------------------------------------------------------------------------------
BOOL operator==( const IN_ADDR& i1, const IN_ADDR& i2 );

