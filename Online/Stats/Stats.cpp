//--------------------------------------------------------------------------------------
// Stats.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgSignIn.h>
#include <AtgUtil.h>
#include <algorithm>

#include "Stats.spa.h"



//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
private:
    // Constants
    static const UINT m_nMaxLapTime          = 600;   // ten minutes
    static const UINT m_nMaxPoints           = 50;
    static const DWORD m_dwPublicSlots       = 8;
    static const DWORD m_dwPrivateSlots      = 8;
    static const UINT m_nMaxLeaderboardRows  = 8;

    static const D3DCOLOR COLOR_BACKGROUND1   = D3DCOLOR_ARGB( 0xFF, 0x00, 0x00, 0xFF );
    static const D3DCOLOR COLOR_BACKGROUND2   = D3DCOLOR_ARGB( 0xFF, 0x00, 0x00, 0x00 );
    static const D3DCOLOR COLOR_TEXT          = D3DCOLOR_ARGB( 0xFF, 0xFF, 0xFF, 0xFF );
    static const D3DCOLOR COLOR_HIGHLIGHT     = D3DCOLOR_ARGB( 0xFF, 0xFF, 0xFF, 0x00 );
    static const D3DCOLOR COLOR_HEADER        = D3DCOLOR_ARGB( 0xFF, 0x00, 0xFF, 0xFF );

    // Application states
    enum APPSTATE
    {
        APPSTATE_MAINMENU,
        APPSTATE_PLAYGAME,
        APPSTATE_VIEWSTATS,
        APPSTATE_MAX
    };

    // Substates
    enum SUBSTATE
    {
        PLAYGAMESTATE_FIRST,
        PLAYGAMESTATE_CREATESESSION = PLAYGAMESTATE_FIRST,
        PLAYGAMESTATE_JOINSESSION,
        PLAYGAMESTATE_STARTSESSION,
        PLAYGAMESTATE_RETRIEVESTATS,
        PLAYGAMESTATE_WRITESTATS,
        PLAYGAMESTATE_DONE,
        MAINMENUSTATE_FIRST,
        MAINMENUSTATE_ENDSESSION    = MAINMENUSTATE_FIRST,
        MAINMENUSTATE_LEAVESESSION,
        MAINMENUSTATE_DELETESESSION,
        MAINMENUSTATE_DONE,
        VIEWSTATSSTATE_LEADERBOARDS,
        VIEWSTATSSTATE_ENUMFRIENDS,
        VIEWSTATSSTATE_FRIENDSTATS
    };

    // Main menu items
    enum MAINMENU
    {
        MAINMENU_PLAYGAME,
        MAINMENU_VIEWSTATS,
        MAINMENU_SIGNIN,
        MAINMENU_MAX
    };

    // Play game menu items
    enum PLAYGAMEMENU
    {
        PLAYGAMEMENU_POINTS,
        PLAYGAMEMENU_MINUTES,
        PLAYGAMEMENU_SECONDS,
        PLAYGAMEMENU_PLAYGAME,
        PLAYGAMEMENU_MAX
    };

    // Leaderboard Stats Views
    enum STATSVIEW
    {
        STATSVIEW_POINTS = 1,
        STATSVIEW_TIME,
        STATSVIEW_POINTS_FRIENDS,
        STATSVIEW_TIME_FRIENDS,
    };

    XOVERLAPPED        m_Overlapped;       // Overlapped IO data structure
    APPSTATE           m_AppState;         // Current application state
    SUBSTATE           m_SubState;         // Current substate
    ATG::Font          m_Font16;           // Big font
    ATG::Font          m_Font12;           // Little font
    UINT               m_nMenuItem;        // Current menu item
    INT                m_nPoints;          // Number of points
    INT                m_nLapTime;         // Lap time (seconds)
    WCHAR              m_wstrError[ 256 ]; // Error message

    UINT                       m_nNumUsers;                   // Local users
    XUID                       m_Xuids   [ XUSER_MAX_COUNT ]; // Local XUIDs
    BOOL                       m_bPrivate[ XUSER_MAX_COUNT ]; // Users consuming private slots
    XUSER_STATS_SPEC           m_Spec;                        // Stats specification
    UINT                       m_nWritingStatsUser;           // User currently writing stats for
    XSESSION_VIEW_PROPERTIES   m_SessionViewProperties[ 2 ];  // Session properties when writing stats
    XUSER_PROPERTY             m_PointsLeaderboard[ 1 ];
    XUSER_PROPERTY             m_LapTimeLeaderboard[ 3 ];

    PXUSER_STATS_READ_RESULTS  m_pStats;                      // Leaderboard or local stats

    // Session data
    XSESSION_INFO      m_SessionInfo;      // Session info
    ULONGLONG          m_SessionNonce;     // Nonce
    HANDLE             m_hSession;         // Handle

    // Enumeration data
    BYTE*              m_pEnumMemory;

    // Friends data
    XUID               m_FriendsXuids[ MAX_FRIENDS + 1 ];   // friend's XUIDs
    DWORD              m_dwNumFriends;                      // number of friends in friend list
    XUID               m_Xuid;
    HANDLE             m_hEnum;
    int                m_topFriendsStatsViewRow;            // top row of stats displayed for friends

    // Leaderboard display
    UINT               m_nCurrentLeaderboard;     // Leaderboard being displayed (points or lap time)
    UINT               m_nNextSelectedRank;       // Rank of user to attempt to select
    XUID               m_xuidNextSelectedXuid;    // XUID of user to attempt to select

    // ATG::Application overrides
    virtual HRESULT             Initialize();
    virtual HRESULT             Update();
    virtual HRESULT             Render();

    // Change state helper
    VOID                        SwitchToState( APPSTATE appstate );

    // Helpers to process substates
    VOID                        ProcessMainMenuSubstate();
    VOID                        ProcessPlayGameSubstate();

    // Helpers to begin/join/start/end session
    VOID                        BeginSession();
    VOID                        JoinSession();
    VOID                        StartSession();
    VOID                        EndSession();
    VOID                        LeaveSession();
    VOID                        DeleteSession();

    // Read/write stats
    VOID                        RetrieveLocalUserStats();
    VOID                        WriteStats( BOOL bInit );
    VOID                        FriendsEnum( DWORD userIndex);
    VOID                        StatsEnumByFriend();
    VOID                        ReadLeaderboard( INT idx, XUID xuid );
    VOID                        SetStatsSpec( DWORD leaderboardID );
    VOID                        ProcessFriendsStatsView( PXUSER_STATS_VIEW pView );

    // Update methods
    HRESULT                     UpdateMainMenu();
    HRESULT                     UpdatePlayGame();
    HRESULT                     UpdateViewStats();

    // Render methods
    HRESULT                     RenderMainMenu();
    HRESULT                     RenderPlayGame();
    HRESULT                     RenderViewStats();
};

//--------------------------------------------------------------------------------------
// Name: CompareRows()
// Desc: Sorting based on rank
//--------------------------------------------------------------------------------------
static BOOL CompareRows( const XUSER_STATS_ROW& a, const XUSER_STATS_ROW& b )
{
    // Special case out unranked players
    if( a.dwRank == 0 && b.dwRank == 0 ) return FALSE; // If neither has played, they're equal
    else if( a.dwRank == 0 ) return FALSE;// If A hasn't played, return B first
    else if( b.dwRank == 0 ) return TRUE; // If B hasn't played, return A first

    // If A's rank is lower ( a better score ), this will be negative and A will be first in the list
    return ( a.dwRank < b.dwRank );
}


//--------------------------------------------------------------------------------------
// Name: isRankedRow()
// Desc: Check if row is ranked (ranking > 0)
//--------------------------------------------------------------------------------------
static BOOL isRankedRow( const XUSER_STATS_ROW& a )
{
    return ( a.dwRank > 0 );
}


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample app;
    ATG::GetVideoSettings( &app.m_d3dpp.BackBufferWidth, &app.m_d3dpp.BackBufferHeight );
    app.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize variables
    m_hSession = INVALID_HANDLE_VALUE;
    m_pStats = NULL;
    m_wstrError[0] = L'\0';
    m_pEnumMemory = NULL;
    m_hEnum = NULL;
    m_dwNumFriends = 0;
    m_topFriendsStatsViewRow = -1;
    m_Xuid = NULL;
    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );
    ZeroMemory( &m_Spec, sizeof( m_Spec ) );
    ZeroMemory( &m_FriendsXuids, sizeof( m_FriendsXuids ) );

    // Create the fonts
    if( FAILED( m_Font16.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( m_Font12.Create( "game:\\Media\\Fonts\\Arial_12.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font16.SetWindow( ATG::GetTitleSafeArea() );
    m_Font12.SetWindow( ATG::GetTitleSafeArea() );

    // Initialize autologin
    ATG::SignIn::Initialize( 1, 4, TRUE, 4 );

    // Begin in main menu state
    SwitchToState( APPSTATE_MAINMENU );
    m_SubState = MAINMENUSTATE_DONE;

    // Start XOnline
    DWORD dwResult = XOnlineStartup();
    if( FAILED( dwResult ) )
    {
        swprintf_s( m_wstrError,
                    L"XOnlineStartup failed with error %d", dwResult );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SwitchToState()
// Desc: Change to a new appstate
//--------------------------------------------------------------------------------------
VOID Sample::SwitchToState( APPSTATE appstate )
{
    // Perform post-state cleanup for old state
    switch( m_AppState )
    {
        case APPSTATE_PLAYGAME:
            EndSession();
            break;

        case APPSTATE_VIEWSTATS:
            if( m_pStats )
            {
                delete[] m_pStats;
                m_pStats = NULL;
            }
            break;
    }

    m_nMenuItem = 0;
    m_AppState = appstate;

    if( appstate != APPSTATE_MAINMENU )
    {
        m_wstrError[ 0 ] = L'\0';
    }

    // Do initialization for the new state
    switch( appstate )
    {
        case APPSTATE_PLAYGAME:
            m_nLapTime = m_nMaxLapTime / 2;
            m_nPoints = ( m_nMaxPoints + 1 ) / 2;

            BeginSession();
            break;

        case APPSTATE_VIEWSTATS:
            m_nCurrentLeaderboard = STATS_VIEW_POINTS_LEADERBOARD;
            ReadLeaderboard( 1, 0 );
            break;
    }
}


//--------------------------------------------------------------------------------------
// Name: SetStatsSpec()
// Desc: Set the StatsSpec based on leaderboard view
//--------------------------------------------------------------------------------------
VOID Sample::SetStatsSpec( DWORD leaderboardID )
{
    switch ( leaderboardID )
    {
        case STATS_VIEW_POINTS_LEADERBOARD:
            m_Spec.dwViewId = leaderboardID;

            m_Spec.dwNumColumnIds = 3;
            m_Spec.rgwColumnIds[ 0 ] = STATS_COLUMN_POINTS_LEADERBOARD_MAX_POINTS;
            m_Spec.rgwColumnIds[ 1 ] = STATS_COLUMN_POINTS_LEADERBOARD_LAST_POINTS;
            m_Spec.rgwColumnIds[ 2 ] = STATS_COLUMN_POINTS_LEADERBOARD_MIN_POINTS;
            break;

        case STATS_VIEW_LAP_TIME_LEADERBOARD:
            m_Spec.dwViewId = leaderboardID;

            m_Spec.dwNumColumnIds = 2;
            m_Spec.rgwColumnIds[ 0 ] = STATS_COLUMN_LAP_TIME_LEADERBOARD_BEST_LAP_TIME;
            m_Spec.rgwColumnIds[ 1 ] = STATS_COLUMN_LAP_TIME_LEADERBOARD_LAP_TIME_POINTS;
            break;

        default:
            break;
    }
}


//--------------------------------------------------------------------------------------
// Name: ProcessMainMenuSubstate()
// Desc: Handle substates when in the Main Menu state
//--------------------------------------------------------------------------------------
VOID Sample::ProcessMainMenuSubstate()
{
    if( XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        HRESULT hr = XGetOverlappedExtendedError( &m_Overlapped );

        if( SUCCEEDED( hr ) )
        {
            // Move on to the next substate
            switch( m_SubState )
            {
                case MAINMENUSTATE_ENDSESSION:
                    LeaveSession();                   break;
                case MAINMENUSTATE_LEAVESESSION:
                    DeleteSession();                  break;
                case MAINMENUSTATE_DELETESESSION:
                    CloseHandle( m_hSession );
                    m_hSession = INVALID_HANDLE_VALUE;
                    m_SubState = MAINMENUSTATE_DONE;  break;
            }
        }
        else
        {
            // Display an error message and quit
            WCHAR* strTask = NULL;
            switch( m_SubState )
            {
                case MAINMENUSTATE_ENDSESSION:
                    strTask = L"XSessionEnd";          break;
                case MAINMENUSTATE_LEAVESESSION:
                    strTask = L"XSessionLeaveRemote";  break;
                case MAINMENUSTATE_DELETESESSION:
                    strTask = L"XSessionDelete";       break;
            }

            swprintf_s( m_wstrError,
                        L"%s overlapped task failed with error 0x%08x",
                        strTask,
                        hr );

            // Always delete the session and close the handle
            if( INVALID_HANDLE_VALUE != m_hSession )
            {
                XSessionDelete( m_hSession, NULL );
                CloseHandle( m_hSession );
                m_hSession = INVALID_HANDLE_VALUE;
            }

            m_SubState = MAINMENUSTATE_DONE;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: ProcessPlayGameSubstate()
// Desc: Handle substates when in the Play Game state
//--------------------------------------------------------------------------------------
VOID Sample::ProcessPlayGameSubstate()
{
    if( XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        HRESULT hr = XGetOverlappedExtendedError( &m_Overlapped );

        if( SUCCEEDED( hr ) )
        {
            // Move on to the next substate
            switch( m_SubState )
            {
                case PLAYGAMESTATE_CREATESESSION:
                    JoinSession();                   break;
                case PLAYGAMESTATE_JOINSESSION:
                    StartSession();                  break;
                case PLAYGAMESTATE_STARTSESSION:
                    RetrieveLocalUserStats();        break;
                case PLAYGAMESTATE_RETRIEVESTATS:
                    m_SubState = PLAYGAMESTATE_DONE; break;
                case PLAYGAMESTATE_WRITESTATS:
                    WriteStats( FALSE );             break;
            }
        }
        else
        {
            // Display an error message and quit
            WCHAR* strTask = NULL;
            switch( m_SubState )
            {
                case PLAYGAMESTATE_CREATESESSION:
                    strTask = L"XSessionCreate";       break;
                case PLAYGAMESTATE_JOINSESSION:
                    strTask = L"XSessionJoinRemote";   break;
                case PLAYGAMESTATE_STARTSESSION:
                    strTask = L"XSessionStart";        break;
                case PLAYGAMESTATE_RETRIEVESTATS:
                    strTask = L"XUserReadStats";       break;
                case PLAYGAMESTATE_WRITESTATS:
                    strTask = L"XSessionWriteStats";   break;
            }

            swprintf_s( m_wstrError,
                        L"%s overlapped task failed with error 0x%08x",
                        strTask,
                        hr );

            // Always delete the session and close the handle
            if( INVALID_HANDLE_VALUE != m_hSession )
            {
                XSessionDelete( m_hSession, NULL );
                CloseHandle( m_hSession );
                m_hSession = INVALID_HANDLE_VALUE;
            }

            SwitchToState( APPSTATE_MAINMENU );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: BeginSession()
// Desc: Create a new session for writing stats
//--------------------------------------------------------------------------------------
VOID Sample::BeginSession()
{
    DWORD dwUser = ATG::SignIn::GetSignedInUser();

    // Zero out the overlapped structure to prepare for new task
    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

    // Set the context to standard (not ranked)
    XUserSetContext( dwUser, X_CONTEXT_GAME_TYPE, X_CONTEXT_GAME_TYPE_STANDARD );

    // Create a (non-matchmaking) session
    DWORD ret = XSessionCreate(
        XSESSION_CREATE_USES_STATS,
        dwUser,
        m_dwPublicSlots,
        m_dwPrivateSlots,
        &m_SessionNonce,
        &m_SessionInfo,
        &m_Overlapped,
        &m_hSession );

    if( ret != ERROR_IO_PENDING )
    {
        swprintf_s( m_wstrError,
                    L"XSessionCreate failed with error %d", ret );
        SwitchToState( APPSTATE_MAINMENU );
    }

    m_SubState = PLAYGAMESTATE_CREATESESSION;
}


//--------------------------------------------------------------------------------------
// Name: JoinSession()
// Desc: Join the session
//--------------------------------------------------------------------------------------
VOID Sample::JoinSession()
{
    // Retrieve local users
    m_nNumUsers = 0;

    for( UINT i = 0; i < XUSER_MAX_COUNT; i++ )
    {
        if( ATG::SignIn::IsUserSignedIn( i ) )
        {
            XUserGetXUID( i, &m_Xuids[ m_nNumUsers ] );
            m_bPrivate[ m_nNumUsers ] = FALSE;

            m_nNumUsers++;
        }
    }

    // Zero out the overlapped structure to prepare for new task
    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

    // Join the session
    DWORD ret = XSessionJoinRemote(
        m_hSession,
        m_nNumUsers,
        m_Xuids,
        m_bPrivate,
        &m_Overlapped );

    if( ret != ERROR_IO_PENDING )
    {
        swprintf_s( m_wstrError,
                    L"XSessionJoinRemote failed with error %d", ret );
        SwitchToState( APPSTATE_MAINMENU );
    }

    m_SubState = PLAYGAMESTATE_JOINSESSION;
}


//--------------------------------------------------------------------------------------
// Name: StartSession()
// Desc: Start the session
//--------------------------------------------------------------------------------------
VOID Sample::StartSession()
{
    // Zero out the overlapped structure to prepare for new task
    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

    // Join the session
    DWORD ret = XSessionStart(
        m_hSession,
        0,
        &m_Overlapped );

    if( ret != ERROR_IO_PENDING )
    {
        swprintf_s( m_wstrError,
                    L"XSessionStart failed with error %d", ret );
        SwitchToState( APPSTATE_MAINMENU );
    }

    m_SubState = PLAYGAMESTATE_STARTSESSION;
}


//--------------------------------------------------------------------------------------
// Name: EndSession()
// Desc: End the current session (and implicitly write stats) 
//--------------------------------------------------------------------------------------
VOID Sample::EndSession()
{
    // Free up memory from the stats
    if( m_pStats )
    {
        delete[] m_pStats;
        m_pStats = NULL;
    }

    // Don't end the session if it was never created
    if( m_hSession == INVALID_HANDLE_VALUE )
    {
        m_SubState = MAINMENUSTATE_DONE;
    }
    else
    {
        m_SubState = MAINMENUSTATE_ENDSESSION;

        // Zero out the overlapped structure to prepare for new task
        ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

        // End the session
        DWORD ret = XSessionEnd(
            m_hSession,
            &m_Overlapped );

        if( ret != ERROR_IO_PENDING )
        {
            swprintf_s( m_wstrError,
                        L"XSessionEnd failed with error %d", ret );
            m_SubState = MAINMENUSTATE_DONE;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: LeaveSession()
// Desc: Remove local users from the session preparatory to deleting it
//--------------------------------------------------------------------------------------
VOID Sample::LeaveSession()
{
    // Don't leave the session if it was never created
    if( m_hSession == INVALID_HANDLE_VALUE )
    {
        m_SubState = MAINMENUSTATE_DONE;
    }
    else
    {
        m_SubState = MAINMENUSTATE_LEAVESESSION;

        // Zero out the overlapped structure to prepare for new task
        ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

        // End the session
        DWORD ret = XSessionLeaveRemote(
            m_hSession,
            m_nNumUsers,
            m_Xuids,
            &m_Overlapped );

        if( ret != ERROR_IO_PENDING )
        {
            swprintf_s( m_wstrError,
                        L"XSessionLeaveRemote failed with error %d", ret );
            m_SubState = MAINMENUSTATE_DONE;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: DeleteSession()
// Desc: Remove the session
//--------------------------------------------------------------------------------------
VOID Sample::DeleteSession()
{
    // Don't delete the session if it was never created
    if( m_hSession == INVALID_HANDLE_VALUE )
    {
        m_SubState = MAINMENUSTATE_DONE;
    }
    else
    {
        m_SubState = MAINMENUSTATE_DELETESESSION;

        // Zero out the overlapped structure to prepare for new task
        ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

        // End the session
        DWORD ret = XSessionDelete(
            m_hSession,
            &m_Overlapped );

        if( ret != ERROR_IO_PENDING )
        {
            swprintf_s( m_wstrError,
                        L"XSessionDelete failed with error %d", ret );
            m_SubState = MAINMENUSTATE_DONE;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: RetrieveLocalUserStats()
// Desc: Get stats for the local users so we can do dependent updates later
//--------------------------------------------------------------------------------------
VOID Sample::RetrieveLocalUserStats()
{
    // Retrieve the current best time and score from the lap time leaderboard
    SetStatsSpec( STATS_VIEW_LAP_TIME_LEADERBOARD );

    // Retrieve the necessary buffer size
    DWORD cbResults = 0;

    DWORD ret = XUserReadStats(
        0,                   // Current title ID
        m_nNumUsers,         // Number of users
        m_Xuids,             // XUIDs of users
        1,                   // Number of stats specs
        &m_Spec,             // Stats spec(s)
        &cbResults,          // Size of buffer
        NULL,                // Buffer
        NULL );              // Perform synchronously

    if( ret != ERROR_INSUFFICIENT_BUFFER )
    {
        swprintf_s( m_wstrError,
                    L"Failed to retrieve the stats buffer size with error %d", ret );
        SwitchToState( APPSTATE_MAINMENU );

        return;
    }

    // Allocate the buffer
    m_pStats = ( PXUSER_STATS_READ_RESULTS )new BYTE[ cbResults ];

    // Zero out the overlapped structure to prepare for new task
    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

    // Retrieve the stats
    ret = XUserReadStats(
        0,                   // Current title ID
        m_nNumUsers,         // Number of users
        m_Xuids,             // XUIDs of users
        1,                   // Number of stats specs
        &m_Spec,             // Stats spec(s)
        &cbResults,          // Size of buffer
        m_pStats,            // Buffer
        &m_Overlapped );     // Overlapped

    if( ret != ERROR_IO_PENDING )
    {
        swprintf_s( m_wstrError,
                    L"XUserReadStats() failed with error %d", ret );
        SwitchToState( APPSTATE_MAINMENU );
    }

    m_SubState = PLAYGAMESTATE_RETRIEVESTATS;
}


//--------------------------------------------------------------------------------------
// Name: WriteStats()
// Desc: Write stats for the local users 
//--------------------------------------------------------------------------------------
VOID Sample::WriteStats( BOOL bInit )
{
    if( bInit )
    {
        // First time calling; set up initial state
        ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );
        m_nWritingStatsUser = 0;

        // The "sort time" property is used to produce a descending value for a 
        // column which we want sorted ascending. We'll multiply it by a negative factor 
        // and add in the number of points for the lap, so points will be a tiebreaker.
        // This technique is called "inversion."
        LONGLONG llRating = -100i64 * m_nLapTime + m_nPoints;

        // Populate the stats view properties
        m_SessionViewProperties[ 0 ].dwViewId = STATS_VIEW_POINTS_LEADERBOARD;
        m_SessionViewProperties[ 0 ].dwNumProperties = 1;
        m_SessionViewProperties[ 0 ].pProperties = m_PointsLeaderboard;

        m_PointsLeaderboard[ 0 ].dwPropertyId = PROPERTY_POINTS;
        m_PointsLeaderboard[ 0 ].value.type = XUSER_DATA_TYPE_INT64;
        m_PointsLeaderboard[ 0 ].value.i64Data = m_nPoints;

        m_SessionViewProperties[ 1 ].dwViewId = STATS_VIEW_LAP_TIME_LEADERBOARD;
        m_SessionViewProperties[ 1 ].dwNumProperties = 3;
        m_SessionViewProperties[ 1 ].pProperties = m_LapTimeLeaderboard;

        m_LapTimeLeaderboard[ 0 ].dwPropertyId = PROPERTY_LAPTIME;
        m_LapTimeLeaderboard[ 0 ].value.type = XUSER_DATA_TYPE_INT32;
        m_LapTimeLeaderboard[ 0 ].value.nData = m_nLapTime;
        m_LapTimeLeaderboard[ 1 ].dwPropertyId = PROPERTY_POINTS;
        m_LapTimeLeaderboard[ 1 ].value.type = XUSER_DATA_TYPE_INT64;
        m_LapTimeLeaderboard[ 1 ].value.i64Data = m_nPoints;
        m_LapTimeLeaderboard[ 2 ].dwPropertyId = PROPERTY_SORTLAPTIME;
        m_LapTimeLeaderboard[ 2 ].value.type = XUSER_DATA_TYPE_INT64;
        m_LapTimeLeaderboard[ 2 ].value.i64Data = llRating;

        m_SubState = PLAYGAMESTATE_WRITESTATS;
    }
    else
    {
        if( ( m_nWritingStatsUser < m_nNumUsers ) && ( m_nWritingStatsUser < XUSER_MAX_COUNT ) )
        {
            // Write the next user's stats

            // We only want to update the lap time leaderboard conditionally, if the new
            // lap time is better than the old lap time
            LONGLONG llRating = m_LapTimeLeaderboard[ 2 ].value.i64Data;
            LONGLONG llOldRating = m_pStats->pViews[ 0 ].pRows[ m_nWritingStatsUser ].i64Rating;

            BOOL bUpdateLapTimeLeaderboard =
                !llOldRating ||
                ( llOldRating < llRating );

            ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );
            DWORD ret = XSessionWriteStats(
                m_hSession,
                m_Xuids[ m_nWritingStatsUser ],
                bUpdateLapTimeLeaderboard ? 2 : 1,
                m_SessionViewProperties,
                &m_Overlapped );

            if( ret != ERROR_IO_PENDING )
            {
                swprintf_s( m_wstrError,
                            L"XSessionWriteStats() failed with error %d", ret );
                SwitchToState( APPSTATE_MAINMENU );
            }

            // Move on to the next users
            m_nWritingStatsUser++;
        }
        else
        {
            // We're done writing stats! Switch back to the main menu
            SwitchToState( APPSTATE_MAINMENU );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: FriendsEnum()
// Desc: Enumerate friends
//--------------------------------------------------------------------------------------
VOID Sample::FriendsEnum( DWORD userIndex )
{
    // Throw away old results
    if( m_pEnumMemory )
    {
        delete[] m_pEnumMemory;
        m_pStats = NULL;
    }

    // Retrieve the necessary buffer size
    DWORD cbResults = 0;

    // Get own XUID, will need it later on
    DWORD dwResult = XUserGetXUID( userIndex, &m_Xuid );
    if( dwResult != ERROR_SUCCESS )
    {
        swprintf_s( m_wstrError,
                    L"XUserGetXUID failed with error 0x%08x", dwResult );
        SwitchToState( APPSTATE_MAINMENU );
        return;
    }

    // Create the enumerator
    dwResult = XFriendsCreateEnumerator(
        userIndex,
        0, MAX_FRIENDS,
        &cbResults,
        &m_hEnum
        );

    if( dwResult != ERROR_SUCCESS )
    {
        DWORD err = HRESULT_FROM_WIN32( dwResult );
        swprintf_s( m_wstrError,
                    L"XFriendsCreateEnumerator failed with error 0x%08x", err );
        SwitchToState( APPSTATE_MAINMENU );
        return;
    }

    m_pEnumMemory = new BYTE[ cbResults ];

    // Zero out the overlapped structure to prepare for new task
    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

    dwResult = XEnumerate(
        m_hEnum,
        m_pEnumMemory,
        cbResults,
        NULL,
        &m_Overlapped
        );

    if( dwResult != ERROR_IO_PENDING )
    {
        dwResult = HRESULT_FROM_WIN32( dwResult );
    }

    if( !SUCCEEDED( dwResult ) )
    {
        if( m_hEnum != NULL )
        {
            CloseHandle( m_hEnum );
            m_hEnum = NULL;
        }
    }

    m_SubState = VIEWSTATSSTATE_ENUMFRIENDS;
}


//--------------------------------------------------------------------------------------
// Name: StatsEnumByFriend()
// Desc: Enumerate stats by friends
//--------------------------------------------------------------------------------------
VOID Sample::StatsEnumByFriend()
{
    DWORD err;

    // Throw away old results
    if( m_pStats )
    {
        delete[] m_pStats;
        m_pStats = NULL;
    }

    // Populate the stats spec
    SetStatsSpec( m_nCurrentLeaderboard - 2 );

    // Retrieve the necessary buffer size
    DWORD cbResults = 0;

    // Get the buffer size
    DWORD dwResult = XUserReadStats(
        0,
        m_dwNumFriends,
        m_FriendsXuids,
        1,
        &m_Spec,
        &cbResults,
        NULL,
        NULL
        );

    if( dwResult != ERROR_INSUFFICIENT_BUFFER )
    {
        err = HRESULT_FROM_WIN32( dwResult );
        swprintf_s( m_wstrError,
                    L"StatsEnumByFriend failed with error 0x%08x", err );
        SwitchToState( APPSTATE_MAINMENU );
        return;
    }

    // Allocate the buffer
    m_pStats = ( PXUSER_STATS_READ_RESULTS )new BYTE[ cbResults ];

    // Start the enumeration
    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

    dwResult = XUserReadStats(
        0,
        m_dwNumFriends,
        m_FriendsXuids,
        1,
        &m_Spec,
        &cbResults,
        m_pStats,
        &m_Overlapped
        );

    if( dwResult != ERROR_IO_PENDING )
    {
        err = HRESULT_FROM_WIN32( dwResult );
        swprintf_s( m_wstrError,
                    L"XUserReadStats failed with error 0x%08x", err );
        SwitchToState( APPSTATE_MAINMENU );
        return;
    }

    m_SubState = VIEWSTATSSTATE_FRIENDSTATS;
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

    // Populate the stats spec
    SetStatsSpec( m_nCurrentLeaderboard );

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
        swprintf_s( m_wstrError,
                    L"XUserCreateStatsEnumerator...() failed with error %d", ret );
        SwitchToState( APPSTATE_MAINMENU );
        return;
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
        swprintf_s( m_wstrError,
                    L"XEnumerate() failed with error %d", ret );
        SwitchToState( APPSTATE_MAINMENU );
        return;
    }

    m_SubState = VIEWSTATSSTATE_LEADERBOARDS;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    HRESULT hr = S_OK;

    // Update autosignin
    DWORD dwUpdateFlags = ATG::SignIn::Update();

    // Sign-in changed, bring back to main menu
    if( dwUpdateFlags & ATG::SignIn::SIGNIN_USERS_CHANGED )
    {
        if( INVALID_HANDLE_VALUE != m_hSession )
        {
            XSessionDelete( m_hSession, NULL );
            CloseHandle( m_hSession );
            m_hSession = INVALID_HANDLE_VALUE;
        }

        if( m_AppState != APPSTATE_MAINMENU )
        {
            swprintf_s( m_wstrError,
                        L"Signed in user changed, back to Main Menu" );

            SwitchToState( APPSTATE_MAINMENU );
        }
        return hr;
    }

    // If we're not signed in, wait until we are
    if( !ATG::SignIn::AreUsersSignedIn() )
    {
        return S_OK;
    }

    // Get the current gamepad state
    ATG::Input::GetMergedInput( ATG::SignIn::GetSignedInUserMask() );

    // Perform state-specific updates
    switch( m_AppState )
    {
        case APPSTATE_MAINMENU:
            hr = UpdateMainMenu();
            break;

        case APPSTATE_PLAYGAME:
            hr = UpdatePlayGame();
            break;

        case APPSTATE_VIEWSTATS:
            hr = UpdateViewStats();
            break;
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    HRESULT hr = S_OK;

    // Draw a gradient filled background
    ATG::RenderBackground( COLOR_BACKGROUND1, COLOR_BACKGROUND2 );

    // Draw the header
    m_Font16.DrawText( 0, 0, COLOR_TEXT, L"Stats" );

    // Draw the labels if an overlapped task is not in progress
    if( XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        if( m_AppState == APPSTATE_MAINMENU )
        {
            m_Font16.DrawText(
                0,
                m_Font16.m_rcWindow.y2 - m_Font16.m_rcWindow.y1 - m_Font16.GetFontHeight() * 2,
                COLOR_TEXT,
                GLYPH_A_BUTTON L": Select\n" GLYPH_B_BUTTON L": Back" );

        }
        else
        {
            m_Font16.DrawText(
                0,
                m_Font16.m_rcWindow.y2 - m_Font16.m_rcWindow.y1 - m_Font16.GetFontHeight(),
                COLOR_TEXT,
                GLYPH_B_BUTTON L": Back" );
        }
    }

    // Perform state-specific rendering
    switch( m_AppState )
    {
        case APPSTATE_MAINMENU:
            hr = RenderMainMenu();
            break;

        case APPSTATE_PLAYGAME:
            hr = RenderPlayGame();
            break;

        case APPSTATE_VIEWSTATS:
            hr = RenderViewStats();
            break;
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: UpdateMainMenu()
// Desc: Perform necessary updates for the main menu state
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateMainMenu()
{
    // If we're in a substate, check to see if we're done
    if( m_SubState != MAINMENUSTATE_DONE )
    {
        ProcessMainMenuSubstate();
        return S_OK;
    }

    // Get the current gamepad status
    ATG::GAMEPAD& gamepad = ATG::Input::m_DefaultGamepad;

    // B/Back: return to signin
    if( ( gamepad.wPressedButtons & XINPUT_GAMEPAD_B ||
          gamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) &&
        !ATG::SignIn::IsSystemUIShowing() )
    {
        ATG::SignIn::ShowSignInUI();
    }

    // Down/up: change current menu item
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_nMenuItem++;
        m_nMenuItem %= MAINMENU_MAX;
    }

    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        m_nMenuItem--;
        m_nMenuItem += MAINMENU_MAX;
        m_nMenuItem %= MAINMENU_MAX;
    }

    // A/Start: select current menu item
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_A ||
        gamepad.wPressedButtons & XINPUT_GAMEPAD_START )
    {
        switch( m_nMenuItem )
        {
            case MAINMENU_PLAYGAME:
                SwitchToState( APPSTATE_PLAYGAME );
                break;

            case MAINMENU_VIEWSTATS:
                SwitchToState( APPSTATE_VIEWSTATS );
                break;

            case MAINMENU_SIGNIN:
                ATG::SignIn::ShowSignInUI();
                break;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderMainMenu()
// Desc: Render the main menu
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderMainMenu()
{
    static const WCHAR* astrSubState[] =
    {
        L"Ending the session",
        L"Leaving the session",
        L"Deleting the session",
    };

    static const WCHAR* astrItems[] =
    {
        L"Play a game",
        L"View stats",
        L"Changed logged-in users"
    };


    FLOAT fCenterX = ( m_Font16.m_rcWindow.x2 - m_Font16.m_rcWindow.x1 ) / 2.0f;
    FLOAT fCenterY = ( m_Font16.m_rcWindow.y2 - m_Font16.m_rcWindow.y1 ) / 2.0f;

    if( m_SubState != MAINMENUSTATE_DONE )
    {
        if( ( m_SubState - MAINMENUSTATE_FIRST ) < ARRAYSIZE( astrSubState ) )
            m_Font16.DrawText( fCenterX,
                               fCenterY,
                               COLOR_TEXT,
                               astrSubState[ m_SubState - MAINMENUSTATE_FIRST ],
                               ATGFONT_CENTER_X );
    }
    else
    {
        FLOAT x = fCenterX;
        FLOAT y = fCenterY - ( MAINMENU_MAX * 2 - 1 ) * m_Font16.GetFontHeight() / 2;

        for( UINT i = 0; i < MAINMENU_MAX; i++ )
        {
            D3DCOLOR col = ( m_nMenuItem == i ) ? COLOR_HIGHLIGHT : COLOR_TEXT;

            m_Font16.DrawText( x, y, col, astrItems[ i ], ATGFONT_CENTER_X );
            y += m_Font16.GetFontHeight() * 2;
        }

        // Draw the error message, if any
        y += m_Font16.GetFontHeight() * 2;
        if( m_wstrError[ 0 ] != L'\0' )
        {
            m_Font12.DrawText( x, y, COLOR_HIGHLIGHT, m_wstrError, ATGFONT_CENTER_X );
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdatePlayGame()
// Desc: Perform necessary updates for the play game state
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdatePlayGame()
{
    // If we're in a substate, check to see if we're done
    if( m_SubState != PLAYGAMESTATE_DONE )
    {
        ProcessPlayGameSubstate();
        return S_OK;
    }

    // If we fall through here, we're done with our substates and are ready to display
    // the menu and accept user input

    // Get the current gamepad status
    ATG::GAMEPAD& gamepad = ATG::Input::m_DefaultGamepad;

    // B/Back: return to main menu
    if( gamepad.wPressedButtons & ( XINPUT_GAMEPAD_B | XINPUT_GAMEPAD_BACK ) )
    {
        SwitchToState( APPSTATE_MAINMENU );
    }

    // Down/up: change current menu item
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_nMenuItem++;
        m_nMenuItem %= PLAYGAMEMENU_MAX;
    }

    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        m_nMenuItem--;
        m_nMenuItem += PLAYGAMEMENU_MAX;
        m_nMenuItem %= PLAYGAMEMENU_MAX;
    }

    // Left/right: change current menu item value 
    if( gamepad.wPressedButtons &
        ( XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT ) )
    {
        BOOL bIncrement = ( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT ) != 0;

        switch( m_nMenuItem )
        {
            case PLAYGAMEMENU_POINTS:
                // Change points value, capping to [1..max]
                if( bIncrement && m_nPoints < m_nMaxPoints )
                {
                    m_nPoints++;
                }
                if( !bIncrement && m_nPoints > 1 )
                {
                    m_nPoints--;
                }
                break;

            case PLAYGAMEMENU_MINUTES:
            case PLAYGAMEMENU_SECONDS:
                {
                    INT nDelta =
                        m_nMenuItem == PLAYGAMEMENU_MINUTES ? 60 : 5;

                    nDelta *= bIncrement ? 1 : -1;

                    m_nLapTime += nDelta;

                    if( m_nLapTime < 5 ) m_nLapTime = 5;
                    if( m_nLapTime > m_nMaxLapTime ) m_nLapTime = m_nMaxLapTime;
                }
                break;
        }
    }

    // A/Start: select current menu item
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_A ||
        gamepad.wPressedButtons & XINPUT_GAMEPAD_START )
    {
        switch( m_nMenuItem )
        {
            case PLAYGAMEMENU_PLAYGAME:
                WriteStats( TRUE );
                break;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderPlayGame()
// Desc: Render the play game menu
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderPlayGame()
{
    static const WCHAR* astrSubState[] =
    {
        L"Creating a session",
        L"Joining the session",
        L"Starting the session",
        L"Retrieving stats",
        L"Writing stats"
    };

    static const WCHAR* astrItems[] =
    {
        L"Points scored: ",
        L"Lap time: ",
        L"Play game"
    };

    FLOAT fCenterX = ( m_Font16.m_rcWindow.x2 - m_Font16.m_rcWindow.x1 ) / 2.0f;
    FLOAT fCenterY = ( m_Font16.m_rcWindow.y2 - m_Font16.m_rcWindow.y1 ) / 2.0f;

    if( m_SubState != PLAYGAMESTATE_DONE )
    {
        if( ( m_SubState - PLAYGAMESTATE_FIRST ) < ARRAYSIZE( astrSubState ) )
            m_Font16.DrawText( fCenterX,
                               fCenterY,
                               COLOR_TEXT,
                               astrSubState[ m_SubState - PLAYGAMESTATE_FIRST ],
                               ATGFONT_CENTER_X );
    }
    else
    {
        FLOAT x = fCenterX;
        FLOAT y = fCenterY - ( ARRAYSIZE( astrItems ) - 1 ) * m_Font16.GetFontHeight();

        for( UINT i = 0; i < ARRAYSIZE( astrItems ); i++ )
        {
            WCHAR wchRenderString[ 256 ];

            BOOL bSelected = FALSE;

            switch( i )
            {
                case 0: // Points
                    bSelected = m_nMenuItem == PLAYGAMEMENU_POINTS;
                    break;

                case 1: // Lap time
                    bSelected = m_nMenuItem == PLAYGAMEMENU_MINUTES ||
                        m_nMenuItem == PLAYGAMEMENU_SECONDS;
                    break;

                case 2: // Play game
                    bSelected = m_nMenuItem == PLAYGAMEMENU_PLAYGAME;
                    break;
            }

            D3DCOLOR col = bSelected ? COLOR_HIGHLIGHT : COLOR_TEXT;

            switch( i )
            {
                case 0: // Points
                    swprintf_s( wchRenderString,
                                L"%s:  %s %d %s",
                                astrItems[ i ],
                                ( m_nMenuItem == PLAYGAMEMENU_POINTS ) ? GLYPH_LEFT_ARROW  : L" ",
                                m_nPoints,
                                ( m_nMenuItem == PLAYGAMEMENU_POINTS ) ? GLYPH_RIGHT_ARROW : L" " );
                    break;

                case 1: // Lap time
                    swprintf_s( wchRenderString,
                                L"%s:  %s%d%s:%s%02d%s",
                                astrItems[ i ],
                                ( m_nMenuItem == PLAYGAMEMENU_MINUTES ) ? GLYPH_LEFT_ARROW  : L"",
                                m_nLapTime / 60,
                                ( m_nMenuItem == PLAYGAMEMENU_MINUTES ) ? GLYPH_RIGHT_ARROW : L"",
                                ( m_nMenuItem == PLAYGAMEMENU_SECONDS ) ? GLYPH_LEFT_ARROW  : L"",
                                m_nLapTime % 60,
                                ( m_nMenuItem == PLAYGAMEMENU_SECONDS ) ? GLYPH_RIGHT_ARROW : L"" );
                    break;

                case 2: // Play game
                    wcscpy_s( wchRenderString, astrItems[ i ] );
                    break;
            }

            m_Font16.DrawText( x, y, col, wchRenderString, ATGFONT_CENTER_X );
            y += m_Font16.GetFontHeight() * 2;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ProcessFriendsStatsView()
// Desc: Perform necessary filtering and pruning (non-ranked) of friends stats
//--------------------------------------------------------------------------------------
VOID Sample::ProcessFriendsStatsView( PXUSER_STATS_VIEW pView )
{
    // sort the stats view
    std::sort( pView->pRows, &pView->pRows[ pView->dwNumRows ], &CompareRows );

    // prune non-ranked (rank = 0) friends from the end
    DWORD rows = std::count_if( pView->pRows, &pView->pRows[ pView->dwNumRows ], &isRankedRow );

    pView->dwTotalViewRows = rows;
    pView->dwNumRows = rows;
}


//--------------------------------------------------------------------------------------
// Name: UpdateViewStats()
// Desc: Perform necessary updates for the view stats state
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateViewStats()
{
    // If we're currently retrieving a leaderboard, check to see if we're done
    if( !XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        return S_OK;
    }

    // If we were retrieving stats, clean up
    if( m_SubState == VIEWSTATSSTATE_LEADERBOARDS )
    {
        HRESULT hr = XGetOverlappedExtendedError( &m_Overlapped );

        if( FAILED( hr ) )
        {
            swprintf_s( m_wstrError,
                        L"XEnumerate overlapped task failed with error 0x%08x",
                        hr );
            SwitchToState( APPSTATE_MAINMENU );
        }
        else
        {
            // Try to select the appropriate entry
            m_nMenuItem = 0;

            assert( m_pStats != 0 && m_pStats->pViews != 0 );
            for( UINT i = 0; i < m_pStats->pViews[ 0 ].dwNumRows; i++ )
            {
                if( ( m_nNextSelectedRank &&
                      m_nNextSelectedRank == m_pStats->pViews[ 0 ].pRows[ i ].dwRank ) ||
                    ( !m_nNextSelectedRank &&
                      m_xuidNextSelectedXuid == m_pStats->pViews[ 0 ].pRows[ i ].xuid ) )
                {
                    m_nMenuItem = i;
                    break;
                }
            }
        }

        m_SubState = MAINMENUSTATE_DONE;

        return S_OK;
    }

    // If we were enumerating friends, clean up and start retrieving data
    if( m_SubState == VIEWSTATSSTATE_ENUMFRIENDS )
    {
        DWORD dwCount = 0;
        DWORD dwResult = XGetOverlappedResult(
            &m_Overlapped,
            &dwCount,
            TRUE
            );

        if( FAILED( dwResult ) )
        {
            swprintf_s( m_wstrError,
                        L"XEnumerate overlapped task failed with error 0x%08x",
                        dwResult );
            SwitchToState( APPSTATE_MAINMENU );
        }
        else
        {
            // Get a list of friends XUIDs that includes ourselves and
            // exludes friend invitations that are not accepted yet
            if( ( dwResult == ERROR_SUCCESS ) ||
                ( dwResult == ERROR_NO_MORE_FILES ) ||
                ( dwResult == ERROR_FUNCTION_FAILED ) )
            {
                // Note: the XDK says that ERROR_NO_MORE_FILES means that you
                // have no friends. In practice it seems to return
                // ERROR_FUNCTION_FAILED in this case.
                m_dwNumFriends = 0;
                m_FriendsXuids[ m_dwNumFriends++] = m_Xuid;
            }

            if( dwResult == ERROR_SUCCESS )
            {
                PXONLINE_FRIEND pFriendsEnum = ( PXONLINE_FRIEND )m_pEnumMemory;
                for( DWORD dw = 0; dw < dwCount; ++dw )
                {
                    if( ( ( pFriendsEnum[ dw ].dwFriendState & XONLINE_FRIENDSTATE_FLAG_RECEIVEDREQUEST ) == 0 ) &&
                        ( ( pFriendsEnum[ dw ].dwFriendState & XONLINE_FRIENDSTATE_FLAG_SENTREQUEST ) == 0 ) )
                    {
                        m_FriendsXuids[ m_dwNumFriends++] = pFriendsEnum[ dw ].xuid;
                    }
                }
            }


            if( ( m_dwNumFriends > 0 ) )
            {
                StatsEnumByFriend();
                m_SubState = VIEWSTATSSTATE_FRIENDSTATS;
            }
            else
            {
                m_SubState = MAINMENUSTATE_DONE;
            }
        }

        return S_OK;
    }

    // If we were retrieving friend stats, clean up and start retrieving data
    if( m_SubState == VIEWSTATSSTATE_FRIENDSTATS )
    {
        DWORD dwCount = 0;
        DWORD dwResult = XGetOverlappedResult(
            &m_Overlapped,
            &dwCount,
            TRUE
            );

        if( dwResult == ERROR_SUCCESS )
        {
            ProcessFriendsStatsView(m_pStats->pViews);

            // Set new top Row
            m_topFriendsStatsViewRow = m_pStats->pViews->dwNumRows - 1;

            // Try to select the appropriate entry
            m_nMenuItem = 0;

            assert( m_pStats != 0 && m_pStats->pViews != 0 );
            for( UINT i = 0; i < m_pStats->pViews[ 0 ].dwNumRows; i++ )
            {
                if( ( m_nNextSelectedRank &&
                      m_nNextSelectedRank == m_pStats->pViews[ 0 ].pRows[ i ].dwRank ) ||
                    ( !m_nNextSelectedRank &&
                      m_xuidNextSelectedXuid == m_pStats->pViews[ 0 ].pRows[ i ].xuid ) )
                {
                    m_nMenuItem = i;
                    break;
                }
            }
        }

        m_SubState = MAINMENUSTATE_DONE;

        return S_OK;
    }

    // Get the current gamepad status
    ATG::GAMEPAD& gamepad = ATG::Input::m_DefaultGamepad;

    // B/Back: return to main menu
    if( gamepad.wPressedButtons & ( XINPUT_GAMEPAD_B | XINPUT_GAMEPAD_BACK ) )
    {
        SwitchToState( APPSTATE_MAINMENU );
    }

    // Right shoulder: toggle leaderboard
    else if( gamepad.wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        m_nNextSelectedRank = 1;

        // Handle friends leaderboard toggles differently
        if ( m_nCurrentLeaderboard > STATSVIEW_TIME )
        {
            m_nCurrentLeaderboard = 7 - m_nCurrentLeaderboard;
            FriendsEnum( gamepad.dwUserIndex );
        }
        else
        {
            m_nCurrentLeaderboard = 3 - m_nCurrentLeaderboard;
            ReadLeaderboard( 1, 0 );
        }
    }

    // A: toggle friends leaderboard
    else if( gamepad.wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_nNextSelectedRank = 1;

        // toggle to correct leaderboard ID
        if ( m_nCurrentLeaderboard > STATSVIEW_TIME )
        {
            m_nCurrentLeaderboard = m_nCurrentLeaderboard - STATSVIEW_TIME;
            ReadLeaderboard( 1, 0 );
        }
        else
        {
            m_nCurrentLeaderboard = m_nCurrentLeaderboard + STATSVIEW_TIME;
            FriendsEnum( gamepad.dwUserIndex );
        }
    }

    // X: zoom to top (only valid for non-friend leaderboards)
    else if( gamepad.wPressedButtons & XINPUT_GAMEPAD_X && ( m_nCurrentLeaderboard < STATSVIEW_POINTS_FRIENDS ) )
    {
        m_nNextSelectedRank = 1;
        ReadLeaderboard( 1, 0 );
    }

    // Y: zoom to bottom (only valid for non-friend leaderboards)
    else if( gamepad.wPressedButtons & XINPUT_GAMEPAD_Y && ( m_nCurrentLeaderboard < STATSVIEW_POINTS_FRIENDS ) )
    {
        assert( m_pStats != 0 && m_pStats->pViews != 0 );
        DWORD dwLast = m_pStats->pViews[ 0 ].dwTotalViewRows;
        m_nNextSelectedRank = dwLast;

        INT nTopIndex = 1;
        if( dwLast > m_nMaxLeaderboardRows )
        {
            nTopIndex = dwLast - m_nMaxLeaderboardRows + 1;
        }

        ReadLeaderboard( nTopIndex, 0 );
    }

    else
    {
        // Left shoulder: zoom to user
        for( UINT nController = 0; nController < XUSER_MAX_COUNT; nController++ )
        {
            if( !ATG::SignIn::IsUserSignedIn( nController ) ) continue;

            if( ATG::Input::m_Gamepads[ nController ].wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER && ( m_nCurrentLeaderboard < STATSVIEW_POINTS_FRIENDS ) )
            {
                m_nNextSelectedRank = 0;
                XUserGetXUID( nController, &m_xuidNextSelectedXuid );
                ReadLeaderboard( 0, m_xuidNextSelectedXuid );
            }
        }
    }

    // Down: move selection down by one
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        // If we can move without scrolling, do it
        // Don't scroll into unlisted section of friend's list
        assert( m_pStats != 0 && m_pStats->pViews != 0 );
        if( m_nMenuItem < m_pStats->pViews[ 0 ].dwNumRows - 1 )
        {
            m_nMenuItem++;
        }
        else
        {
            if ( m_nCurrentLeaderboard < STATSVIEW_POINTS_FRIENDS )
            {
                // We need to scroll, but only if we're not at the bottom
                if( m_pStats->pViews[ 0 ].dwTotalViewRows >
                    m_pStats->pViews[ 0 ].pRows[ m_nMenuItem ].dwRank )
                {
                    m_nNextSelectedRank = m_pStats->pViews[ 0 ].pRows[ m_nMenuItem ].dwRank + 1;
                    ReadLeaderboard( m_nNextSelectedRank, 0 );
                }
            }
            else
            {
                m_topFriendsStatsViewRow = m_pStats->pViews[ 0 ].pRows[ m_nMenuItem ].dwRank + 1;
            }
        }
    }

    // Up: move selection up by one
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        // If we can move without scrolling, do it
        if( m_nMenuItem > 0 )
        {
            m_nMenuItem--;
        }
        else
        {
            if ( m_nCurrentLeaderboard < STATSVIEW_POINTS_FRIENDS )
            {
                // We need to scroll, but only if we're not at the top
                assert( m_pStats != 0 && m_pStats->pViews != 0 && m_pStats->pViews != 0 );
                if( m_pStats->pViews[ 0 ].pRows[ m_nMenuItem ].dwRank > 1 )
                {
                    m_nNextSelectedRank = m_pStats->pViews[ 0 ].pRows[ m_nMenuItem ].dwRank - 1;

                    INT nTopIndex = 1;
                    if( m_nNextSelectedRank > m_nMaxLeaderboardRows )
                    {
                        nTopIndex = m_nNextSelectedRank - m_nMaxLeaderboardRows + 1;
                    }

                    ReadLeaderboard( nTopIndex, 0 );
                }
            }
            else
            {
                assert( m_pStats != 0 && m_pStats->pViews != 0 && m_pStats->pViews != 0 );
                if ( m_pStats->pViews[ 0 ].pRows[ m_nMenuItem ].dwRank >  m_pStats->pViews->pRows[0].dwRank )
                {
                    m_topFriendsStatsViewRow = m_pStats->pViews[ 0 ].pRows[ m_nMenuItem ].dwRank - 1;
                }
            }
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderViewStats()
// Desc: Render the stats page
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderViewStats()
{
    FLOAT fCenterX = ( m_Font16.m_rcWindow.x2 - m_Font16.m_rcWindow.x1 ) / 2.0f;
    FLOAT fCenterY = ( m_Font16.m_rcWindow.y2 - m_Font16.m_rcWindow.y1 ) / 2.0f;

    FLOAT x, y;

    // If we're currently retrieving stats, say so
    if( !XHasOverlappedIoCompleted( &m_Overlapped ) || m_SubState == VIEWSTATSSTATE_ENUMFRIENDS )
    {
        m_Font16.DrawText( fCenterX,
                           fCenterY,
                           COLOR_TEXT,
                           L"Retrieving leaderboard",
                           ATGFONT_CENTER_X );
    }
    else
    {
        // String to display at the bottom right for leaderboards
        static const WCHAR* wstrFullHelpText =
            GLYPH_RIGHT_BUTTON L": Toggle leaderboard "
            GLYPH_A_BUTTON     L": Toggle Friends-only\n"
            GLYPH_X_BUTTON     L": Top  "
            GLYPH_Y_BUTTON     L": Bottom  "
            GLYPH_LEFT_BUTTON  L": Current ";

        // String to display at the bottom right for friends leaderboards
        static const WCHAR* wstrFriendsHelpText =
            GLYPH_RIGHT_BUTTON L": Toggle leaderboard "
            GLYPH_A_BUTTON     L": Toggle Friends-only\n";

        // Leaderboard names
        static const WCHAR* awstrLeaderboards[] =
        {
            L"Points leaderboard",
            L"Lap time leaderboard",
            L"Points leaderboard (friends)",
            L"Lap time leaderboard (friends)",
        };

        // Headers for the points leaderboard
        static const WCHAR* awstrPointsHeaders[] =
        {
            L"Rank   ",
            L"Gamertag                      ",
            L"Total\nPoints ",
            L"Max\nPoints ",
            L"Last\nPoints ",
            L"Min\nPoints"
        };

        // Headers for the lap time leaderboard
        static const WCHAR* awstrLapTimeHeaders[] =
        {
            L"Rank   ",
            L"Gamertag                      ",
            L"Best Lap    ",
            L"Points Scored"
        };

        // Array of both header arrays
        static const WCHAR** aawstrHeaders[] =
        {
            awstrPointsHeaders,
            awstrLapTimeHeaders,
            awstrPointsHeaders,
            awstrLapTimeHeaders
        };

        // Number of columns in each leaderboard
        static const UINT anNumColumns[] =
        {
            ARRAYSIZE( awstrPointsHeaders ),
            ARRAYSIZE( awstrLapTimeHeaders ),
            ARRAYSIZE( awstrPointsHeaders ),
            ARRAYSIZE( awstrLapTimeHeaders )
        };

        // Special column values
        enum COLUMNS
        {
            COLUMN_RANK     = -1,       // Rank
            COLUMN_GAMERTAG = -2,       // Gamertag
            COLUMN_RATING   = -3,       // Rating
            COLUMN_LAPTIME  = -4,       // Lap time
        };

        // Column indices for points leaderboard
        static const INT anPointsColumns[] =
        {
            COLUMN_RANK,                // Rank
            COLUMN_GAMERTAG,            // Gamertag
            COLUMN_RATING,              // Total points
            0,                          // Max points
            1,                          // Last points
            2                           // Min points
        };

        // Column indices for lap time leaderboard
        static const INT anLapTimeColumns[] =
        {
            COLUMN_RANK,                // Rank
            COLUMN_GAMERTAG,            // Gamertag
            COLUMN_LAPTIME,             // Best lap time
            1                           // Points scored
        };

        // Array of both column arrays
        static const INT* aanColumns[] =
        {
            anPointsColumns,
            anLapTimeColumns,
            anPointsColumns,
            anLapTimeColumns
        };

        // Calculate zero-based leaderboard index by subtracting first leaderboard
        UINT nLeaderboard = m_nCurrentLeaderboard - STATS_VIEW_POINTS_LEADERBOARD;

        // Render the leaderboard label
        x = fCenterX;
        y = m_Font16.m_rcWindow.y1 + m_Font16.GetFontHeight();
        m_Font16.DrawText( x, y, COLOR_TEXT, awstrLeaderboards[ nLeaderboard ], ATGFONT_CENTER_X );

        // Render the help text
        FLOAT fxExtent, fyExtent;
        m_Font16.GetTextExtent( wstrFullHelpText, &fxExtent, &fyExtent );

        x = m_Font16.m_rcWindow.x2 - m_Font16.m_rcWindow.x1 - fxExtent;
        y = m_Font16.m_rcWindow.y2 - m_Font16.m_rcWindow.y1 - fyExtent;

        if ( m_nCurrentLeaderboard < STATSVIEW_POINTS_FRIENDS )
        {
            m_Font16.DrawText(
                x,
                y,
                COLOR_TEXT,
                wstrFullHelpText );
        }
        else
        {
            m_Font16.DrawText(
                x,
                y,
                COLOR_TEXT,
                wstrFriendsHelpText );
        }

        // Render the leaderboard
        INT nNumColumns = anNumColumns [ nLeaderboard ];
        const INT* anColumns = aanColumns   [ nLeaderboard ];
        const WCHAR** awstrHeaders = aawstrHeaders[ nLeaderboard ];

        FLOAT fLeaderboardWidth = 0;

        for( int i = 0; i < nNumColumns; i++ )
        {
            fLeaderboardWidth += m_Font12.GetTextWidth( awstrHeaders[ i ] );
        }

        // Center the leaderboard horizontally
        FLOAT fLeftEdge =
            ( m_Font12.m_rcWindow.x2 - m_Font12.m_rcWindow.x1 - fLeaderboardWidth ) / 2;

        // Position the leaderboard below the header
        y = m_Font16.m_rcWindow.y1 + m_Font16.GetFontHeight() * 3;

        WCHAR wchRender[ 256 ];

        // Special case: if there are no rows, say so
        if( m_pStats->pViews[ 0 ].dwNumRows == 0 )
        {
            m_Font12.DrawText( fCenterX, y, COLOR_TEXT, L"No rows in leaderboard", ATGFONT_CENTER_X );

            return S_OK;
        }

        int startRow = -1;
        int endRow = ( INT )m_pStats->pViews[ 0 ].dwNumRows;

        if ( m_nCurrentLeaderboard > STATSVIEW_TIME )
        {
            startRow = m_topFriendsStatsViewRow - 1;
            endRow = min( endRow, (m_topFriendsStatsViewRow + ( INT )m_nMaxLeaderboardRows) );
        }

        // Render the rows and columns
        for( int nRow = -1; nRow < ( INT )m_pStats->pViews[ 0 ].dwNumRows; nRow++ )
        {
            x = fLeftEdge;

            // Special case: if this is the first row and we're not at the top of
            // the leaderboard, render an up arrow
            if( ( nRow == 0 ) &&
                ( ( ( m_pStats->pViews[ 0 ].pRows[ nRow ].dwRank != 1 ) &&
                    ( m_nCurrentLeaderboard < STATSVIEW_POINTS_FRIENDS ) ) ||
                  ( ( m_pStats->pViews[ 0 ].pRows[ nRow ].dwRank != m_pStats->pViews[ 0 ].pRows[ 0 ].dwRank ) &&
                    ( m_nCurrentLeaderboard > STATSVIEW_TIME ) ) ) )
            {
                m_Font16.DrawText(
                    fLeftEdge + fLeaderboardWidth,
                    y,
                    COLOR_TEXT,
                    GLYPH_UP_ARROW );
            }

            // Special case: if this is the last row and we're not at the bottom of
            // the leaderboard, render a down arrow
            if( ( nRow == ( INT )m_pStats->pViews[ 0 ].dwNumRows - 1 ) &&
                ( ( ( m_pStats->pViews[ 0 ].pRows[ nRow ].dwRank != m_pStats->pViews[ 0 ].dwTotalViewRows ) &&
                    ( m_nCurrentLeaderboard < STATSVIEW_POINTS_FRIENDS ) ) ||
                  ( ( m_pStats->pViews[ 0 ].pRows[ nRow ].dwRank != m_pStats->pViews[ 0 ].pRows[ m_pStats->pViews[ 0 ].dwNumRows - 1 ].dwRank ) &&
                    ( m_nCurrentLeaderboard > STATSVIEW_TIME ) ) ) )
            {
                m_Font16.DrawText(
                    fLeftEdge + fLeaderboardWidth,
                    y,
                    COLOR_TEXT,
                    GLYPH_DOWN_ARROW );
            }

            for( int nCol = 0; nCol < nNumColumns; nCol++ )
            {
                // If we're rendering the header, draw it!
                if( nRow == -1 )
                {
                    wcscpy_s( wchRender,
                              awstrHeaders[ nCol ] );
                }
                else
                {
                    // Render the appropriate text
                    switch( anColumns[ nCol ] )
                    {
                        case COLUMN_RANK:
                            swprintf_s( wchRender,
                                        L"%d", m_pStats->pViews[ 0 ].pRows[ nRow ].dwRank );
                            break;

                        case COLUMN_GAMERTAG:
                            // convert the ANSI gamertag to WCHAR
                            swprintf_s( wchRender,
                                        L"%S", m_pStats->pViews[ 0 ].pRows[ nRow ].szGamertag );
                            break;

                        case COLUMN_RATING:
                            // Output the rating as a 64-bit integer
                            swprintf_s( wchRender,
                                        L"%I64d", m_pStats->pViews[ 0 ].pRows[ nRow ].i64Rating );
                            if( wcslen( wchRender ) > 6 )
                                wcscpy_s( wchRender, L"######" );
                            break;

                        case COLUMN_LAPTIME:
                            // Output the time (in column 0) formatted properly
                        {
                            UINT nLapTime =
                                m_pStats->pViews[ 0 ].pRows[ nRow ].pColumns[ 0 ].Value.nData;

                            swprintf_s( wchRender,
                                        L"%d:%02d", nLapTime / 60, nLapTime % 60 );
                        }
                            break;

                        default:
                            // Display the value in the appropriate column
                            swprintf_s( wchRender,
                                        L"%I64d",
                                        m_pStats->pViews[ 0 ].pRows[ nRow ].pColumns[ anColumns[ nCol ] ].Value.i64Data
                                        );
                            if( wcslen( wchRender ) > 6 )
                                wcscpy_s( wchRender, L"######" );
                            break;
                    }
                }

                // Render the value

                DWORD dwColor;
                if( nRow == -1 )
                {
                    dwColor = COLOR_HEADER;  // Header
                }
                else if( nRow == ( INT )m_nMenuItem )
                {
                    dwColor = COLOR_HIGHLIGHT;  // Currently-selected entry
                }
                else
                {
                    dwColor = COLOR_TEXT;  // Generic menu item
                }

                m_Font12.DrawText( x, y, dwColor, wchRender );

                x += m_Font12.GetTextWidth( awstrHeaders[ nCol ] );
            }

            // Go to the next row
            y += m_Font12.GetFontHeight();

            // Add a row for the header
            if( nRow == -1 )
            {
                y += m_Font12.GetFontHeight();
            }
        }
    }

    return S_OK;
}
