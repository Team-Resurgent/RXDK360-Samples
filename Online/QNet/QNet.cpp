//-----------------------------------------------------------------------------
// qnet.cpp
//
// Automated sample demonstrating minimal QNet interface usage for creating or
// joining a session, starting and ending it, and sending/receiving messages.
// For simplicity, there is no user interface; the only user interaction is to
// sign a profile in if one is not found at startup. Thereafter this sample will
// use timers to automatically transition the QNet session state through
// various example phases.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <winsockx.h>
#include <xaudio2.h>
#include <xcam.h>
#include <qnet.h>
#include <AtgInput.h>
#include <AtgConsole.h>
#include <stdio.h>
#include "qnetp.h"
#include "QNet.spa.h"


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
CQNetSample     g_QNetSample;

// Console for output
ATG::Console g_Console;


//-----------------------------------------------------------------------------
// Name: CQNetSample::NotifyStateChanged()
// Desc: Callback invoked when QNet state changes
//-----------------------------------------------------------------------------
VOID
CQNetSample::NotifyStateChanged(
        IN QNET_STATE               OldState,
        IN QNET_STATE               NewState,
        IN HRESULT                  hrInfo
    )
{
    static const char * c_apszStateNames[] =
    {
        "QNET_STATE_IDLE",
        "QNET_STATE_SESSION_HOSTING",
        "QNET_STATE_SESSION_JOINING",
        "QNET_STATE_GAME_LOBBY",
        "QNET_STATE_SESSION_REGISTERING",
        "QNET_STATE_SESSION_STARTING",
        "QNET_STATE_GAME_PLAY",
        "QNET_STATE_SESSION_ENDING",
        "QNET_STATE_SESSION_LEAVING",
        "QNET_STATE_SESSION_DELETING",
    };

    g_Console.Format( "State: %s ==> %s, result 0x%08x.\n",
        c_apszStateNames[ OldState ],
        c_apszStateNames[ NewState ],
        hrInfo );
    m_dwStateTime = GetTickCount();

    //
    // If we just entered game play, consider this as a "game played".
    //
    if (NewState == QNET_STATE_GAME_PLAY)
    {
        m_dwNumGamesPlayed++;
    }
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::NotifyPlayerJoined()
// Desc: Callback invoked when a player joins the QNet session
//-----------------------------------------------------------------------------
VOID
CQNetSample::NotifyPlayerJoined(
    IN IQNetPlayer *            pPlayer
    )
{
    const char * pszDescription;
    CQNetSamplePlayer * pWrapper;

    if( pPlayer->IsLocal() )
    {
        if( pPlayer->IsHost() )
        {
            pszDescription = "local host";
        }
        else
        {
            pszDescription = "local";
        }
    }
    else
    {
        if( pPlayer->IsHost() )
        {
            pszDescription = "remote host";
        }
        else
        {
            pszDescription = "remote";
        }
    }

    g_Console.Format( "Player 0x%p \"%ls\" joined; %s; voice %i; camera %i.\n",
        pPlayer,
        pPlayer->GetGamertag(),
        pszDescription,
        (int) pPlayer->HasVoice(),
        (int) pPlayer->HasCamera() );

    // Create a wrapper object for the player.
    pWrapper = new CQNetSamplePlayer( pPlayer );
    if( pWrapper == NULL )
    {
        g_Console.Format( "Couldn't allocate memory for new player object!\n" );
    }
    else
    {
        // Have QNet associate our wrapper object with its player object so we
        // have a convenient shortcut to it during callbacks.
        pPlayer->SetCustomDataValue( (ULONG_PTR) pWrapper );
    }
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::NotifyPlayerLeaving()
// Desc: Callback invoked when a player leaves the QNet session
//-----------------------------------------------------------------------------
VOID
CQNetSample::NotifyPlayerLeaving(
    IN IQNetPlayer *            pPlayer
    )
{
    CQNetSamplePlayer * pWrapper;

    // Get our wrapper object associated with this player.
    pWrapper = (CQNetSamplePlayer *) pPlayer->GetCustomDataValue();
    if( pWrapper != NULL )
    {
        // Free the wrapper object memory.
        delete pWrapper;

        pPlayer->SetCustomDataValue( NULL );
    }

    g_Console.Format( "Player 0x%p \"%ls\" leaving.\n",
        pPlayer,
        pPlayer->GetGamertag() );
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::NotifyNewHost()
// Desc: Callback invoked when the QNet session host changes players
//-----------------------------------------------------------------------------
VOID
CQNetSample::NotifyNewHost(
    IN IQNetPlayer *            pPlayer
    )
{
    g_Console.Format( "Player 0x%p \"%ls\" (local %i) is new host.\n",
        pPlayer,
        pPlayer->GetGamertag(),
        (int) pPlayer->IsLocal() );
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::NotifyDataReceived()
// Desc: Callback invoked when a message has been received by one or more local
//       players
//-----------------------------------------------------------------------------
VOID
CQNetSample::NotifyDataReceived(
    IN IQNetPlayer *            pPlayerFrom,
    IN DWORD                    dwNumPlayersTo,
    IN IQNetPlayer **           apPlayersTo,
    IN const BYTE *             pbData,
    IN DWORD                    dwDataSize
    )
{
    const UNALIGNED QNETSAMPLEMSG * pMsg;
    CQNetSamplePlayer * pWrapperPlayerFrom;
    DWORD dwPlayer;
    char strDescription[256];

    // Make sure the message is large enough to hold our generic header for all
    // messages.
    if( dwDataSize < sizeof( QNETSAMPLEMSG ) )
    {
        g_Console.Format( "Data too small to hold valid message (%u < %u)!\n",
            dwDataSize, sizeof( QNETSAMPLEMSG ) );
        return;
    }

    // Cast the data to our message header structure.
    pMsg = (const UNALIGNED QNETSAMPLEMSG *) pbData;

    // Parse the individual message types.
    switch( pMsg->byMsgType )
    {
        case QNETSAMPLEMSGTYPE_DUMMY:
            const UNALIGNED QNETSAMPLEMSG_DUMMY * pMsgDummy;
            WORD wDummy1;
            DWORD dwDummy2;

            // Make sure the 'dummy' message is the correct size.
            if( dwDataSize != sizeof( QNETSAMPLEMSG_DUMMY ) )
            {
                g_Console.Format( "Unexpected dummy message size (%u != %u)!\n",
                    dwDataSize, sizeof( QNETSAMPLEMSG_DUMMY ) );
                return;
            }

            // Cast the message header to the 'dummy' message.
            pMsgDummy = (const UNALIGNED QNETSAMPLEMSG_DUMMY *) pMsg;

            // Extract the payload.  We use the ntohs() and ntohl() Winsock
            // macros to convert the 2 byte (short) & 4 byte (long) dummy
            // values from "network" to "host" byte order.  Standardizing on an
            // "endianness" for data on the wire is good practice.  Note that
            // network byte order happens to be big-endian, the same as Xbox
            // 360, so the macro is actually a no-op on that platform.
            // This is a mirror operation of the original send.
            wDummy1 = ntohs(pMsgDummy->wDummy1);
            dwDummy2 = ntohl(pMsgDummy->dwDummy2);

            // This sample doesn't actually do anything with the payload except
            // format a text description from it.
            _snprintf_s(
                strDescription,
                ARRAYSIZE(strDescription),
                _TRUNCATE,
                "[Dummy 0x%04X, 0x%08X]",
                wDummy1,
                dwDummy2 );

            g_Console.Format( "Message received: %s\n", strDescription );
            break;

        default:
            g_Console.Format( "Invalid message type 0x%02x!\n", pMsg->byMsgType );
            return;
    }

    // Get our associated wrapper object.
    pWrapperPlayerFrom = (CQNetSamplePlayer *) pPlayerFrom->GetCustomDataValue();

    // If the player is remote, save the 'last received' timestamp for
    // informational purposes.
    if( !pPlayerFrom->IsLocal() )
    {
        pWrapperPlayerFrom->m_dwLastMessageTime = GetTickCount();
    }

    // Loop through all the local players that were targeted and print info
    // regarding this message.
    for( dwPlayer = 0; dwPlayer < dwNumPlayersTo; dwPlayer++ )
    {
        g_Console.Format( "Received %u bytes of data from \"%ls\" to \"%ls\": %s\n",
            dwDataSize,
            pPlayerFrom->GetGamertag(),
            apPlayersTo[ dwPlayer ]->GetGamertag(),
            strDescription );
    }
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::NotifyWriteStats()
// Desc: Callback invoked when stats should written for the specified player
//-----------------------------------------------------------------------------
VOID
CQNetSample::NotifyWriteStats(
    IN IQNetPlayer *            pPlayer
    )
{
    HRESULT hr;
    CQNetSamplePlayer * pWrapperPlayer;
    QNET_SESSIONTYPE SessionType;
    DWORD dwSessionTypeSize;
    const XUSER_CONTEXT * pXUserContext;
    DWORD dwGameType;
    DWORD dwGameMode;
    DWORD dwLeaderboardTypeMode;
    DWORD dwLeaderboardType;
    DWORD dwNumViews;
    XSESSION_VIEW_PROPERTIES aViewProperties[ 3 ];
    XUSER_PROPERTY aUserPropertiesSkill[ 2 ];
    XUSER_PROPERTY aUserPropertiesStats[ 4 ];

    // Get our associated wrapper object.
    pWrapperPlayer = (CQNetSamplePlayer *) pPlayer->GetCustomDataValue();

    g_Console.Format( "Player \"%ls\" can now write stats (local = %i).\n",
        pPlayer->GetGamertag(), (int) pPlayer->IsLocal() );

    // Get the session type.
    dwSessionTypeSize = sizeof( SessionType );
    m_pIQNet->GetOpt(
        QNET_OPTION_TYPE_SESSIONTYPE,
        &SessionType,
        &dwSessionTypeSize );

    // Get the game type and mode context values.
    pXUserContext = m_pIQNet->GetGameContext( X_CONTEXT_GAME_TYPE );
    dwGameType = pXUserContext->dwValue;
    pXUserContext = m_pIQNet->GetGameContext( X_CONTEXT_GAME_MODE );
    dwGameMode = pXUserContext->dwValue;

    // Determine the leaderboard type.
    dwLeaderboardType = ( SessionType == QNET_SESSIONTYPE_XBOXLIVE_RANKED ) ? STATS_VIEW_RANKED_GAMES : STATS_VIEW_STANDARD_GAMES;

    // Determine the leaderboard type/mode.
    switch( ( dwGameType << 8 ) | ( dwGameMode ) )
    {
        case ( ( X_CONTEXT_GAME_TYPE_RANKED << 8 ) | CONTEXT_GAME_MODE_DEATHMATCH ):
            dwLeaderboardTypeMode = STATS_VIEW_RANKED_DEATHMATCH;
            break;

        case ( ( X_CONTEXT_GAME_TYPE_RANKED << 8 ) | CONTEXT_GAME_MODE_COOPERATIVE ):
            dwLeaderboardTypeMode = STATS_VIEW_RANKED_COOPERATIVE;
            break;

        case ( ( X_CONTEXT_GAME_TYPE_RANKED << 8 ) | CONTEXT_GAME_MODE_TEAM_BATTLE ):
            dwLeaderboardTypeMode = STATS_VIEW_RANKED_TEAM_PLAY;
            break;

        case ( ( X_CONTEXT_GAME_TYPE_STANDARD << 8 ) | CONTEXT_GAME_MODE_DEATHMATCH ):
            dwLeaderboardTypeMode = STATS_VIEW_STANDARD_DEATHMATCH;
            break;

        case ( ( X_CONTEXT_GAME_TYPE_STANDARD << 8 ) | CONTEXT_GAME_MODE_COOPERATIVE ):
            dwLeaderboardTypeMode = STATS_VIEW_STANDARD_COOPERATIVE;
            break;

        case ( ( X_CONTEXT_GAME_TYPE_STANDARD << 8 ) | CONTEXT_GAME_MODE_TEAM_BATTLE ):
            dwLeaderboardTypeMode = STATS_VIEW_STANDARD_TEAM_PLAY;
            break;

        default:
            g_Console.Format( "Invalid game type + mode!\n" );
            return;
    }

    dwNumViews = 0;

    // Build the skill stats view.
    aUserPropertiesSkill[ 0 ].dwPropertyId = X_PROPERTY_RELATIVE_SCORE;
    aUserPropertiesSkill[ 0 ].value.type   = XUSER_DATA_TYPE_INT32;
    //aUserPropertiesSkill[ 0 ].value.nData  = pWrapperPlayer->GetNumPoints() ];
    aUserPropertiesSkill[ 0 ].value.nData  = 10;

    aUserPropertiesSkill[ 1 ].dwPropertyId = X_PROPERTY_SESSION_TEAM;
    aUserPropertiesSkill[ 1 ].value.type   = XUSER_DATA_TYPE_INT32;
    aUserPropertiesSkill[ 1 ].value.nData  = (LONG) ( ( pPlayer->GetXuid() >> 32 ) ^ ( pPlayer->GetXuid() & MAXDWORD ) );

    aViewProperties[ dwNumViews ].dwNumProperties = ARRAYSIZE( aUserPropertiesSkill );
    aViewProperties[ dwNumViews ].dwViewId        = X_STATS_VIEW_SKILL;
    aViewProperties[ dwNumViews ].pProperties     = aUserPropertiesSkill;
    dwNumViews++;

    // Write stats to the non-skill leaderboards.  If this is a ranked game,
    // write stats to non-skill leaderboards for every user.  Otherwise, write
    // stats to non-skill leaderboards only for local users.
    if( ( SessionType == QNET_SESSIONTYPE_XBOXLIVE_RANKED ) ||
        ( pPlayer->IsLocal() ) )
    {
        aUserPropertiesStats[ 0 ].dwPropertyId  = PROPERTY_GAMES_PLAYED;
        aUserPropertiesStats[ 0 ].value.type    = XUSER_DATA_TYPE_INT32;
        aUserPropertiesStats[ 0 ].value.nData   = 1;

        aUserPropertiesStats[ 1 ].dwPropertyId  = PROPERTY_GAMES_WON;
        aUserPropertiesStats[ 1 ].value.type    = XUSER_DATA_TYPE_INT64;
        //aUserPropertiesStats[ 1 ].value.i64Data = pWrapperPlayer->GetNumPoints() == m_iVictoryPoints;
        aUserPropertiesStats[ 1 ].value.i64Data = 1;

        aUserPropertiesStats[ 2 ].dwPropertyId  = PROPERTY_POINTS_SCORED;
        aUserPropertiesStats[ 2 ].value.type    = XUSER_DATA_TYPE_INT32;
        //aUserPropertiesStats[ 2 ].value.nData   = pWrapperPlayer->GetNumPoints();
        aUserPropertiesStats[ 2 ].value.nData   = 10;

        aUserPropertiesStats[ 3 ].dwPropertyId  = CONTEXT_MAP;
        aUserPropertiesStats[ 3 ].value.type    = XUSER_DATA_TYPE_CONTEXT;
        pXUserContext = m_pIQNet->GetGameContext( CONTEXT_MAP );
        aUserPropertiesStats[ 3 ].value.nData   = pXUserContext->dwValue;

        aViewProperties[ dwNumViews ].dwNumProperties = ARRAYSIZE( aUserPropertiesStats );
        aViewProperties[ dwNumViews ].dwViewId        = dwLeaderboardTypeMode;
        aViewProperties[ dwNumViews ].pProperties     = aUserPropertiesStats;
        dwNumViews++;

        aViewProperties[ dwNumViews ].dwNumProperties = ARRAYSIZE( aUserPropertiesStats );
        aViewProperties[ dwNumViews ].dwViewId        = dwLeaderboardType;
        aViewProperties[ dwNumViews ].pProperties     = aUserPropertiesStats;
        dwNumViews++;
    }

    // Actually write the stats.
    g_Console.Format( "Writing player \"%ls\" stats.\n",
        pPlayer->GetGamertag() );
    hr = pPlayer->WriteStats( dwNumViews, aViewProperties );
    if( FAILED( hr ) )
    {
        g_Console.Format( "Failed writing stats (err = 0x%08x)!\n", hr );
    }
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::NotifyReadinessChanged()
// Desc: Callback invoked when a player's readiness state changes
//-----------------------------------------------------------------------------
VOID
CQNetSample::NotifyReadinessChanged(
    IN IQNetPlayer *            pPlayer,
    IN BOOL                     bReady
    )
{
    g_Console.Format( "Player 0x%p readiness is now %i.\n", pPlayer, (int) bReady );
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::NotifyCommSettingsChanged()
// Desc: Callback invoked when a player's voice or camera state changes
//-----------------------------------------------------------------------------
VOID
CQNetSample::NotifyCommSettingsChanged(
    IN IQNetPlayer *            pPlayer
    )
{
    g_Console.Format( "Player 0x%p comm settings changed, voice = %i, camera = %i.\n",
        pPlayer,
        (int) pPlayer->HasVoice(),
        (int) pPlayer->HasCamera() );
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::NotifyGameSearchComplete()
// Desc: Callback invoked when a game search finishes
//-----------------------------------------------------------------------------
VOID
CQNetSample::NotifyGameSearchComplete(
    IN IQNetGameSearch *        pGameSearch,
    IN HRESULT                  hrComplete,
    IN DWORD                    dwNumResults
    )
{
    HRESULT hr;
    BOOL bJoiningGame;
    DWORD dwResultToUse;
    DWORD dwResult;
    const XSESSION_SEARCHRESULT * pSearchResult;
    const XNQOSINFO * pxnqi;

    // Assume we didn't/won't find any valid hosts.
    bJoiningGame = FALSE;

    if( SUCCEEDED( hrComplete ) )
    {
        g_Console.Format( "Game search 0x%p completed successfully, %u results.\n",
            pGameSearch, dwNumResults );

        // Loop through all the results.
        dwResultToUse = (DWORD) -1;
        for( dwResult = 0; dwResult < pGameSearch->GetNumResults(); dwResult++ )
        {
            pSearchResult = pGameSearch->GetSearchResultAtIndex( dwResult );

            // Print some info about this result.
            g_Console.Format( "Search result %u:\n", dwResult );
            g_Console.Format( "    public slots open = %u, filled = %u\n",
                pSearchResult->dwOpenPublicSlots, pSearchResult->dwFilledPublicSlots );
            g_Console.Format( "    private slots open = %u, filled = %u\n",
                pSearchResult->dwOpenPrivateSlots, pSearchResult->dwFilledPrivateSlots );
            //PrintProperties( pSearchResult->pProperties, pSearchResult->cProperties );
            //PrintContexts( pSearchResult->pContexts, pSearchResult->cContexts );

            // See if this result was contacted successfully via QoS probes.
            pxnqi = pGameSearch->GetQosInfoAtIndex( dwResult );
            if( pxnqi->bFlags & XNET_XNQOSINFO_TARGET_CONTACTED )
            {
                // Print the round trip time and the rough estimation of
                // bandwidth.
                g_Console.Format( "    RTT min = %u, med = %u\n",
                    pxnqi->wRttMinInMsecs, pxnqi->wRttMedInMsecs );
                g_Console.Format( "    bps up = %u, down = %u\n",
                    pxnqi->dwUpBitsPerSec, pxnqi->dwDnBitsPerSec );

                // If this host wasn't disabled, and we don't already have a
                // result, use this one.
                if( ( !( pxnqi->bFlags & XNET_XNQOSINFO_TARGET_DISABLED ) ) &&
                    ( dwResultToUse == (DWORD) -1 ) )
                {
                    dwResultToUse = dwResult;
                }
            }
        }

        // See if we found a result to use.
        if( dwResultToUse != (DWORD) -1 )
        {
            // Re-retrieve the selected result and join it.
            g_Console.Format( "Joining search result index %u.\n", dwResultToUse );
            pSearchResult = pGameSearch->GetSearchResultAtIndex( dwResultToUse );
            hr = m_pIQNet->JoinGameFromSearchResult(
                m_dwPrimaryUserIndex, // dwUserIndex
                m_dwLocalUsersMask,   // dwUserMask
                pSearchResult );      // pSearchResult
            if( FAILED( hr ) )
            {
                g_Console.Format( "Failed joining game (err = 0x%08x)!\n", hr ); 
            }
            else
            {
                bJoiningGame = TRUE;
            }
        }
        else
        {
            g_Console.Format( "No acceptable search results found.\n" );
        }
    }
    else
    {
        g_Console.Format( "Game search 0x%p failed with result 0x%08x.\n",
            pGameSearch, hrComplete );
    }

    // Clear our copy of the game search interface pointer.
    if( pGameSearch == m_pIQNetGameSearch )
    {
        m_pIQNetGameSearch = NULL;
    }

    // Destroy the game search interface.
    pGameSearch->Destroy();
    pGameSearch = NULL;

    // If we're not already joining a session, create one.
    if( !bJoiningGame )
    {
        hr = CreateGame();
        if( FAILED( hr ) )
        {
            g_Console.Format( "Failed creating game (err = 0x%08x)!\n", hr );
        }
    }
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::NotifyGameInvite()
// Desc: Callback invoked when a game invite has been accepted, or the user has
//       requested to join a friend's session.
//-----------------------------------------------------------------------------
VOID
CQNetSample::NotifyGameInvite(
    IN DWORD                    dwUserIndex,
    IN const XINVITE_INFO *     pInviteInfo
    )
{
    HRESULT hr;

    g_Console.Format( "User index %u received game invite.\n", dwUserIndex );

    // Save the invite info and remember that we need to join it.
    memcpy( &m_XInviteInfo, pInviteInfo, sizeof( m_XInviteInfo ) );
    m_fJoinFromInvite = TRUE;

    // The user received a game invite.  If we were in the middle of a session
    // already, we should prompt the user about terminating the session and the
    // possible data loss, then leave the session.
    if( m_pIQNet->GetState() != QNET_STATE_IDLE )
    {
        // Start leaving the game.
        g_Console.Format( "Leaving game due to invite.\n" );
        hr = m_pIQNet->LeaveGame( TRUE );
        if( FAILED( hr ) )
        {
            g_Console.Format( "Failed leaving game (err = 0x%08x)!\n", hr );
            return;
        }

        // Once we've left the game (may have happened inside the LeaveGame
        // call), we will automatically join the specified game.  See the logic
        // in CQNetSample::RunStateIdle() and CQNetSample::JoinInvitedGame().
    }
    else
    {
        // We're not in a session, so we can join right now.
        hr = JoinInvitedGame();
        if( FAILED( hr ) )
        {
            g_Console.Format( "Failed joining invited game (err = 0x%08x)!\n", hr );
            return;
        }
    }
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::NotifyContextChanged()
// Desc: Callback invoked when the host changes a game context.
//-----------------------------------------------------------------------------
VOID
CQNetSample::NotifyContextChanged(
    IN const XUSER_CONTEXT *    pContext
    )
{
    g_Console.Format( "Context 0x%p changed.\n", pContext );
    //PrintContexts( pContext, 1 );
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::NotifyPropertyChanged()
// Desc: Callback invoked when the host changes a game property.
//-----------------------------------------------------------------------------
VOID
CQNetSample::NotifyPropertyChanged(
    IN const XUSER_PROPERTY *   pProperty
    )
{
    g_Console.Format( "Property 0x%p changed.\n", pProperty );
    //PrintProperties( pProperty, 1 );
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::CQNetSample()
// Desc: CQNetSample constructor
//-----------------------------------------------------------------------------
CQNetSample::CQNetSample()
{
    m_pIQNet = NULL;
    m_pXAudio2 = NULL;
    m_pXAudio2MasteringVoice = NULL;
    m_dwPrimaryUserIndex = XUSER_INDEX_NONE;
    m_dwLocalUsersMask = 0;
    m_dwStateTime = GetTickCount();
    m_dwNumGamesPlayed = 0;
    m_pIQNetGameSearch = NULL;
    ZeroMemory( &m_XInviteInfo, sizeof( m_XInviteInfo ) );
    m_fJoinFromInvite = FALSE;
    m_fQuitting = FALSE;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::~CQNetSample()
// Desc: CQNetSample destructor
//-----------------------------------------------------------------------------
CQNetSample::~CQNetSample()
{
    Cleanup();
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::Initialize()
// Desc: Initialization routine
//-----------------------------------------------------------------------------
HRESULT CQNetSample::Initialize()
{
    HRESULT hr;
    int iResult;
    DWORD dwResult;

    // Initialize the console window
    g_Console.Create( "game:\\Media\\Fonts\\Courier_New_11.xpr", 0xFF1F005F, 0xFFFFFFFF );

    // Start XAudio2
    hr = XAudio2Create( &m_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR );
    if( FAILED( hr ) )
    {
        g_Console.Format( "Initializing XAudio2 failed (err = 0x%08x)!\n", hr );
        return hr;
    }

    // Create an XAudio2 mastering voice (utilized by XHV2 when voice data is mixed to main speakers)
    hr = m_pXAudio2->CreateMasteringVoice(&m_pXAudio2MasteringVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, NULL);
    if ( FAILED( hr ) )
    {
        g_Console.Format( "Creating XAudio2 mastering voice failed (err = 0x%08x)!\n", hr );
        return hr;
    }

    // Start up XNet with default settings.
    iResult = XNetStartup( NULL );
    if( iResult != 0 )
    {
        g_Console.Format( "Starting up XNet failed (err = %i)!\n", iResult );
        return HRESULT_FROM_WIN32( iResult );
    }

    // Start up XOnline.
    dwResult = XOnlineStartup();
    if( dwResult != ERROR_SUCCESS )
    {
        g_Console.Format( "Starting up XOnline failed (err = %u)!\n", dwResult );
        XNetCleanup();
        return HRESULT_FROM_WIN32( dwResult );
    }

    // Create the QNet object.
    hr = QNetCreateUsingXAudio2( c_QNetSampleSessionType, this, NULL, m_pXAudio2, &m_pIQNet );
    if( FAILED( hr ) )
    {
        g_Console.Format( "Creating QNet object failed (err = 0x%08x)!\n", hr );
        XOnlineCleanup();
        XNetCleanup();
        return hr;
    }

    // Success!
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::Cleanup()
// Desc: Post-run cleanup routine
//-----------------------------------------------------------------------------
VOID CQNetSample::Cleanup()
{
    // Destroy any game search we still have at this point.
    if( m_pIQNetGameSearch != NULL )
    {
        m_pIQNetGameSearch->Destroy();
        m_pIQNetGameSearch = NULL;
    }

    // See if we have a QNet interface.
    if( m_pIQNet != NULL )
    {
        // Destroy it.
        m_pIQNet->Destroy();
        m_pIQNet = NULL;

        // Clean up XOnline.
        XOnlineCleanup();

        // Clean up XNet.
        XNetCleanup();

        // Release XAudio2 reference
        m_pXAudio2->Release();
    }
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::Run()
// Desc: Main sample loop
//-----------------------------------------------------------------------------
VOID CQNetSample::Run()
{
    HRESULT hr;

    // Block until at least one profile is signed in.  Real titles would likely
    // handle this more asynchronously.
    BlockUntilSignedIn();

    g_Console.Format( "Starting up.\n" );

    // Start searching for a game session.  When this completes, we'll either
    // join one or create our own.
    hr = StartSearchForGames();
    if( FAILED( hr ) )
    {
        g_Console.Format( "Failed starting search for games (err = 0x%08x)!\n", hr );
        return;
    }

    // Keep looping until it's time to quit.
    while( !m_fQuitting )
    {        
        ATG::Input::GetMergedInput(); // Detect reboot keypress

        switch( m_pIQNet->GetState() )
        {
            case QNET_STATE_IDLE:                   hr = RunStateIdle();                break;
            case QNET_STATE_SESSION_HOSTING:        hr = RunStateSessionHosting();      break;
            case QNET_STATE_SESSION_JOINING:        hr = RunStateSessionJoining();      break;
            case QNET_STATE_GAME_LOBBY:             hr = RunStateGameLobby();           break;
            case QNET_STATE_SESSION_REGISTERING:    hr = RunStateSessionRegistering();  break;
            case QNET_STATE_SESSION_STARTING:       hr = RunStateSessionStarting();     break;
            case QNET_STATE_GAME_PLAY:              hr = RunStateGamePlay();            break;
            case QNET_STATE_SESSION_ENDING:         hr = RunStateSessionEnding();       break;
            case QNET_STATE_SESSION_LEAVING:        hr = RunStateSessionLeaving();      break;
            case QNET_STATE_SESSION_DELETING:       hr = RunStateSessionDeleting();     break;
            default:                                hr = E_FAIL;                        break;
        }
        if( FAILED( hr ) )
        {
            g_Console.Format( "An error occurred while running (0x%08x)!\n", hr );
            break;
        }
    }

    g_Console.Format( "Shutting down.\n" );

    // We're quitting.
    return;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::BlockUntilSignedIn()
// Desc: A simple function that doesn't return until at least one profile is
//       signed in.  Real titles will probably wish to use the more robust
//       ATG::SignIn sample class for managing sign-in, including the various
//       state changes that happen during title startup.
//-----------------------------------------------------------------------------
VOID CQNetSample::BlockUntilSignedIn()
{
    BOOL bSigninPrompted = FALSE;
    DWORD dwStartTime = GetTickCount();
    DWORD dwResult;
    DWORD dwUserIndex;

    g_Console.Format( "Waiting until at least one profile is signed in...\n" );

    // Keep looping until we've found at least one profile to use.
    while( m_dwPrimaryUserIndex == XUSER_INDEX_NONE )
    {
        // See if we haven't prompted to sign in yet.
        if( !bSigninPrompted )
        {
            // Allow some time elapse before assuming no one is already signed
            // in. 
            if( ( GetTickCount() - dwStartTime ) >= c_dwQNetSampleInitialSigninWaitTime )
            {
                g_Console.Format( "No users found yet, showing sign-in UI.\n" );

                // Prompt for a single user.
                dwResult = XShowSigninUI( 1, 0 );
                if( dwResult != ERROR_SUCCESS )
                {
                    g_Console.Format( "Failed showing sign-in UI (err = %u)!\n", dwResult );
                }

                bSigninPrompted = TRUE;
            }
        }

        // Loop through all local user index slots.
        for( dwUserIndex = 0; dwUserIndex < XUSER_MAX_COUNT; dwUserIndex++ )
        {
            switch( XUserGetSigninState( dwUserIndex ) )
            {
                case eXUserSigninState_NotSignedIn:
                    //g_Console.Format( "    User %u: not signed in.\n", dwUserIndex );
                    break;

                case eXUserSigninState_SignedInLocally:
                    if( ( c_QNetSampleSessionType == QNET_SESSIONTYPE_LOCAL ) ||
                        ( c_QNetSampleSessionType == QNET_SESSIONTYPE_SYSTEMLINK ) )
                    {
                        g_Console.Format( "    User %u: signed in locally.\n", dwUserIndex );
                        if( m_dwPrimaryUserIndex == XUSER_INDEX_NONE )
                        {
                            m_dwPrimaryUserIndex = dwUserIndex;
                        }
                        m_dwLocalUsersMask |= 1 << dwUserIndex;
                    }
                    else
                    {
                        //g_Console.Format( "    User %u: signed in locally.\n", dwUserIndex );
                    }
                    break;

                case eXUserSigninState_SignedInToLive:
                    g_Console.Format( "    User %u: signed in to Xbox Live.\n", dwUserIndex );
                    if( m_dwPrimaryUserIndex == XUSER_INDEX_NONE )
                    {
                        m_dwPrimaryUserIndex = dwUserIndex;
                    }
                    m_dwLocalUsersMask |= 1 << dwUserIndex;
                    break;
            }
        }
    }

    g_Console.Format( "Done, primary user = %u, user mask = 0x%08x.\n",
        m_dwPrimaryUserIndex, m_dwLocalUsersMask );
    return;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::StartSearchForGames()
// Desc: Routine to begin searching for games
//-----------------------------------------------------------------------------
HRESULT CQNetSample::StartSearchForGames()
{
    HRESULT hr;
    DWORD dwMatchQueryIndex;

    // Specify the matchmaking query index to use.  For System Link play, this
    // value is ignored.
    dwMatchQueryIndex = SESSION_MATCH_QUERY_FIND_MATCHES;

    // Search for sessions with matching game types and game modes, on any map.
    XUSER_CONTEXT aXUserContexts[] = { { X_CONTEXT_GAME_TYPE, c_dwQNetSampleGameType },
                                       { X_CONTEXT_GAME_MODE, CONTEXT_GAME_MODE_DEATHMATCH } };

    // Create the game search object.  We will get a NotifyGameSearchComplete
    // callback when the search is finished.
    g_Console.Format( "Creating game search.\n" );
    hr = m_pIQNet->CreateGameSearch(
            m_dwPrimaryUserIndex,              // dwUserIndex
            m_dwLocalUsersMask,                // dwUserMask
            dwMatchQueryIndex,                 // dwMatchQueryIndex
            c_dwQNetSampleMaxNumSearchResults, // dwMaxResults
            0,                                 // cProperties
            NULL,                              // pProperties
            ARRAYSIZE( aXUserContexts ),       // cContexts
            aXUserContexts,                    // pContexts
            &m_pIQNetGameSearch );             // ppGameSearch
    if( FAILED( hr ) )
    {
        g_Console.Format( "Creating game search failed (err = 0x%08x)!\n", hr );
    }

    return hr;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::CreateGame()
// Desc: Routine to create a new game session
//-----------------------------------------------------------------------------
HRESULT CQNetSample::CreateGame()
{
    HRESULT hr;

    // Create a session using the default game type, in deatchmatch game mode,
    // and specifying "Stalingrad" as the map.
    XUSER_CONTEXT aXUserContexts[] = { { X_CONTEXT_GAME_TYPE, c_dwQNetSampleGameType },
                                       { X_CONTEXT_GAME_MODE, CONTEXT_GAME_MODE_DEATHMATCH },
                                       { CONTEXT_MAP, CONTEXT_MAP_STALINGRAD } };

    // Start hosting a new game.
    hr = m_pIQNet->HostGame(
        m_dwPrimaryUserIndex,          // dwUserIndex
        m_dwLocalUsersMask,            // dwUserMask
        c_dwQNetSampleNumPublicSlots,  // dwPublicSlots
        c_dwQNetSampleNumPrivateSlots, // dwPrivateSlots
        0,                             // cProperties
        NULL,                          // pProperties
        ARRAYSIZE( aXUserContexts ),   // cContexts
        aXUserContexts );              // pContexts
    if( FAILED( hr ) )
    {
        g_Console.Format( "Hosting a game failed (err = 0x%08x)!\n", hr );
        return hr;
    }

    return hr;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::JoinInvitedGame()
// Desc: Routine to join the session identified by a previously accepted game
//       invite
//-----------------------------------------------------------------------------
HRESULT CQNetSample::JoinInvitedGame()
{
    HRESULT hr;

    // If we haven't been invited, there's nothing to join.
    if( !m_fJoinFromInvite )
    {
        return S_FALSE;
    }

    // Clear the invite flag.
    m_fJoinFromInvite = FALSE;

    // Join the game.
    g_Console.Format( "Joining game from invite.\n" );
    hr = m_pIQNet->JoinGameFromInviteInfo(
        m_dwPrimaryUserIndex, // dwUserIndex
        m_dwLocalUsersMask,   // dwUserMask
        &m_XInviteInfo );     // pInviteInfo
    if( FAILED( hr ) )
    {
        g_Console.Format( "Failed joining game from invite (err = 0x%08x)!\n", hr );
        return hr;
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::DoSends()
// Desc: Routine to perform sends from the local players at periodic intervals
//-----------------------------------------------------------------------------
HRESULT CQNetSample::DoSends()
{
    HRESULT hr;
    DWORD dwCurrentTime;
    DWORD dwUserIndex;
    IQNetPlayer * pIQNetPlayer;
    CQNetSamplePlayer * pPlayer;

    // If there is only one player currently in the session, it must be local
    // and it's not terribly interesting to perform sends.  This is a common
    // case (a single profile signed in locally, hosting a session but waiting
    // for others to join), so rather than filling the debugger with send
    // output, we just won't bother sending.
    if (m_pIQNet->GetPlayerCount() == 1)
    {
        return S_OK;
    }

    dwCurrentTime = GetTickCount();

    // Loop through all local players.
    for( dwUserIndex = 0; dwUserIndex < XUSER_MAX_COUNT; dwUserIndex++ )
    {
        // Check our mask to see if we have a local player at this index.
        if( m_dwLocalUsersMask & ( 1 << dwUserIndex ) )
        {
            // Get the QNet player object at this index.
            pIQNetPlayer = m_pIQNet->GetLocalPlayerByUserIndex( dwUserIndex );

            // Get our associated wrapper object.
            pPlayer = (CQNetSamplePlayer *) pIQNetPlayer->GetCustomDataValue();

            // If it's been long enough since our last message, send again.
            if( ( dwCurrentTime - pPlayer->m_dwLastMessageTime ) >= c_dwQNetSampleSendInterval )
            {
                g_Console.Format( "Sending dummy message from \"%ls\" to everyone.\n",
                    pIQNetPlayer->GetGamertag() );

                // Create a dummy message.  We use the htons() and htonl()
                // Winsock macros to convert the 2 byte (short) & 4 byte
                // (long) dummy values from "host" to "network" byte order.
                // Standardizing on an "endianness" for data on the wire is
                // good practice.  Note that network byte order happens to be
                // big-endian, the same as Xbox 360, so the macro is actually
                // a no-op on that platform.
                // This is a mirror operation of the parsing in the receive
                // notification callback.
                QNETSAMPLEMSG_DUMMY MsgDummy;
                MsgDummy.byMsgType = QNETSAMPLEMSGTYPE_DUMMY;
                MsgDummy.wDummy1   = htons(0xABCD);
                MsgDummy.dwDummy2  = htonl(0x12345678);

                // Send using dwFlags == 0, which means it is sent unreliably
                // (if the packet is dropped by the network, it will not be
                // retried), non-sequentially (if the packet gets misordered by
                // the network, it will be delivered as soon as it arrives
                // rather than waiting for all messages preceding it), and
                // directly to all players instead of routed via the host.
                // Alternatively, the QNET_SENDDATA_RELIABLE,
                // QNET_SENDDATA_SEQUENTIAL, and/or QNET_SENDDATA_VIA_HOST
                // flags could be specified here.
                hr = pIQNetPlayer->SendData(
                    NULL,               // pPlayerTarget
                    &MsgDummy,          // pvData
                    sizeof( MsgDummy ), // dwDataSize
                    0 );                // dwFlags
                if( FAILED( hr ) )
                {
                    g_Console.Format( "Failed sending data to everyone (err = 0x%08x)!\n", hr );
                    return hr;
                }

                // Remember this last message send time.
                pPlayer->m_dwLastMessageTime = dwCurrentTime;
            }
        }
    }
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::RunStateIdle()
// Desc: Per-frame logic for the QNET_STATE_IDLE state
//-----------------------------------------------------------------------------
HRESULT CQNetSample::RunStateIdle()
{
    HRESULT hr;

    // If there's an active game search, let it run.
    if( m_pIQNetGameSearch != NULL )
    {
        // Let QNet do its processing.
        m_pIQNet->DoWork();
        return S_OK;
    }

    // If there's a invited game to join, do so.  Otherwise, quit.
    if( m_fJoinFromInvite )
    {
        hr = JoinInvitedGame();
        if( FAILED( hr ) )
        {
            g_Console.Format( "Failed joining invited game (err = 0x%08x)!\n", hr );
            return hr;
        }
    }
    else
    {
        g_Console.Format( "Sample is idle and nothing more to do, quitting.\n" );
        m_fQuitting = TRUE;
    }
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::RunStateSessionHosting()
// Desc: Per-frame logic for the QNET_STATE_SESSION_HOSTING state
//-----------------------------------------------------------------------------
HRESULT CQNetSample::RunStateSessionHosting()
{
    // Let QNet do its processing.
    m_pIQNet->DoWork();
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::RunStateSessionJoining()
// Desc: Per-frame logic for the QNET_STATE_SESSION_JOINING state
//-----------------------------------------------------------------------------
HRESULT CQNetSample::RunStateSessionJoining()
{
    // Let QNet do its processing.
    m_pIQNet->DoWork();
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::RunStateGameLobby()
// Desc: Per-frame logic for the QNET_STATE_GAME_LOBBY state
//-----------------------------------------------------------------------------
HRESULT CQNetSample::RunStateGameLobby()
{
    HRESULT hr;
    DWORD dwCurrentTime;
    DWORD dwUserIndex;
    IQNetPlayer * pIQNetPlayer;

    // Get the current time.
    dwCurrentTime = GetTickCount();

    // See if we're the host.
    if( m_pIQNet->IsHost() )
    {
        // If we have at least two players, and everyone is ready, and we've
        // been in the lobby long enough, then start the game automatically.
        if( ( m_pIQNet->GetPlayerCount() >= 2 ) &&
            ( m_pIQNet->IsEveryoneReady() ) &&
            ( ( dwCurrentTime - m_dwStateTime ) >= c_dwQNetSampleMinimumLobbyTime ) )
        {
            g_Console.Format( "Starting game.\n" );
            hr = m_pIQNet->StartGame();
            if( FAILED( hr ) )
            {
                g_Console.Format( "Couldn't start game (err = 0x%08x)!\n", hr );
                return hr;
            }

            // We're in a new state, so we're done here.
            return S_OK;
        }
    }

    // See if we've been in the lobby state long enough to be ready.
    if( ( dwCurrentTime - m_dwStateTime ) >= c_dwQNetSampleWaitUntilReadyTime )
    {
        // Loop through all local players, and if we haven't marked them as ready,
        // do so.
        for( dwUserIndex = 0; dwUserIndex < XUSER_MAX_COUNT; dwUserIndex++ )
        {
            // Check our mask to see if we have a local player at this index.
            if( m_dwLocalUsersMask & ( 1 << dwUserIndex ) )
            {
                pIQNetPlayer = m_pIQNet->GetLocalPlayerByUserIndex( dwUserIndex );

                // If the player isn't ready, mark it that way.
                if( !pIQNetPlayer->IsReady() )
                {
                    g_Console.Format( "Marking local player 0x%p (\"%ls\") as ready.\n",
                        pIQNetPlayer, pIQNetPlayer->GetGamertag() );
                    hr = pIQNetPlayer->SetReady( TRUE );
                    if( FAILED( hr ) )
                    {
                        g_Console.Format( "Couldn't mark local user index %u as ready (err = 0x%08x)!\n",
                            dwUserIndex, hr );
                        return hr;
                    }
                }
            }
        }
    }

    // Perform data sending.
    hr = DoSends();
    if( FAILED( hr ) )
    {
        g_Console.Format( "Failed performing data sends (err = 0x%08x)!\n", hr );
        return hr;
    }

    // Let QNet do its processing.
    m_pIQNet->DoWork();
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::RunStateSessionRegistering()
// Desc: Per-frame logic for the QNET_STATE_SESSION_REGISTERING state
//-----------------------------------------------------------------------------
HRESULT CQNetSample::RunStateSessionRegistering()
{
    // Let QNet do its processing.
    m_pIQNet->DoWork();
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::RunStateSessionStarting()
// Desc: Per-frame logic for the QNET_STATE_SESSION_STARTING state
//-----------------------------------------------------------------------------
HRESULT CQNetSample::RunStateSessionStarting()
{
    // Let QNet do its processing.
    m_pIQNet->DoWork();
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::RunStateGamePlay()
// Desc: Per-frame logic for the QNET_STATE_GAME_PLAY state
//-----------------------------------------------------------------------------
HRESULT CQNetSample::RunStateGamePlay()
{
    HRESULT hr;
    DWORD dwMinimumGameTime;

    // Pick a minimum game time.
    dwMinimumGameTime = m_pIQNet->IsHost() ? c_dwQNetSampleMinimumGameTimeHost : c_dwQNetSampleMinimumGameTimeNonhost;

    // See if we've been in the game for long enough.
    if( ( GetTickCount() - m_dwStateTime ) >= dwMinimumGameTime )
    {
        // See if this is the first game we've played.
        if( m_dwNumGamesPlayed <= 1 )
        {
            // If we're the host, end the game and go back to the lobby.
            // Otherwise, just continue waiting for the host to do that.
            if( m_pIQNet->IsHost() )
            {
                g_Console.Format( "Ending game #%u.\n", m_dwNumGamesPlayed );
                hr = m_pIQNet->EndGame();
                if( FAILED( hr ) )
                {
                    g_Console.Format( "Failed ending game (err = 0x%08x)!\n", hr );
                    return hr;
                }

                // We're in a new state, so we're done here.
                return S_OK;
            }
        }
        else
        {
            // Leave the game entirely instead of just ending the current
            // session.  Tell QNet to allow the host to migrate to others that
            // are still playing, in case we were the host.
            g_Console.Format( "Leaving game #%u.\n", m_dwNumGamesPlayed );
            hr = m_pIQNet->LeaveGame( TRUE );
            if( FAILED( hr ) )
            {
                g_Console.Format( "Failed leaving game (err = 0x%08x)!\n", hr );
                return hr;
            }

            // We're in a new state, so we're done here.
            return S_OK;
        }
    }

    // Perform data sending.
    hr = DoSends();
    if( FAILED( hr ) )
    {
        g_Console.Format( "Failed performing data sends (err = 0x%08x)!\n", hr );
        return hr;
    }

    // Let QNet do its processing.
    m_pIQNet->DoWork();
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::RunStateSessionEnding()
// Desc: Per-frame logic for the QNET_STATE_SESSION_ENDING state
//-----------------------------------------------------------------------------
HRESULT CQNetSample::RunStateSessionEnding()
{
    // Let QNet do its processing.
    m_pIQNet->DoWork();
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::RunStateSessionLeaving()
// Desc: Per-frame logic for the QNET_STATE_SESSION_LEAVING state
//-----------------------------------------------------------------------------
HRESULT CQNetSample::RunStateSessionLeaving()
{
    // Let QNet do its processing.
    m_pIQNet->DoWork();
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CQNetSample::RunStateSessionDeleting()
// Desc: Per-frame logic for the QNET_STATE_SESSION_DELETING state
//-----------------------------------------------------------------------------
HRESULT CQNetSample::RunStateSessionDeleting()
{
    // Let QNet do its processing.
    m_pIQNet->DoWork();
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: main()
// Desc: Main executable entry point
//-----------------------------------------------------------------------------
void _cdecl main( int argc, char * argv[] )
{
    HRESULT hr;

    // Initialize the sample.
    hr = g_QNetSample.Initialize();
    if( SUCCEEDED( hr ) )
    {
        // Run the sample loop.
        g_QNetSample.Run();

        // Clean up the sample prior to exiting.
        g_QNetSample.Cleanup();
    }

    g_Console.Format( "\nPress LT + RT + RB to exit.\n" );

    for( ;; )
    {
        ATG::Input::GetMergedInput(); // Detect reboot keypress
    }
}
