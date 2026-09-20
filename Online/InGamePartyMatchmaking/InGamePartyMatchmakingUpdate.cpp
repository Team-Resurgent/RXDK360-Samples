//--------------------------------------------------------------------------------------
// InGamePartyMatchmakingUpdate.cpp
//
// Contains methods to update the InGamePartyMatchmakingUpdate sample state and UI
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "XPlat.h"

//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    HRESULT hr = S_OK;

    // Update the signin
    m_CXPlat_Signin.Update();

    // Handle signin changes
    if( m_CXPlat_Signin.HaveSigninUsersChanged() )
    {
        HandleSigninChanges();
    }

    // If connection state changed, update our machine ID    
    if( m_CXPlat_Signin.HasConnectionChanged() )
    {
        DebugSpew( "Connection status changed. Getting new machine id..." );
        
        while( XNetGetTitleXnAddr( &m_Local.xnaddr ) == XNET_GET_XNADDR_PENDING )
            ;

        XNetXnAddrToMachineId( &m_Local.xnaddr, &m_Local.id );

        DebugSpew( "%I64u\n", m_Local.id );
    }

    // If any local user is signed into Live, make sure we have an online xnaddr and
    // update our machine ID
    if( m_CXPlat_Signin.AreUsersSignedInToLive() && m_Local.xnaddr.inaOnline.S_un.S_addr == 0 )
    {
        while( XNetGetTitleXnAddr( &m_Local.xnaddr ) == XNET_GET_XNADDR_PENDING || 
               XNetGetTitleXnAddr( &m_Local.xnaddr ) != XNET_GET_XNADDR_ONLINE )
            ;

        XNetXnAddrToMachineId( &m_Local.xnaddr, &m_Local.id );
    }

    // Handle any LIVE notifications not handled by framework code
    if( CheckForLiveNotifications() )
    {
        return hr;
    }
    
    // Make sure we're sending out hearbeats to all connected peers
    HandleHeartbeats();

    // Give task scheduler its timeslice
    if( m_TaskScheduler.DoesWorkExist() )
    {
        m_TaskScheduler.DoWork();
    }

    // Check for presence changes
    if( CheckForFriendsPresenceChanges() )
    {
        return hr;
    }

    if( CheckForNonFriendsPresenceChanges() )
    {
        return hr;
    }

    // Check for custom action invocations
    if( CheckForCustomAction() )
    {
        return hr;
    }

    // Get the current gamepad state
    DWORD dwActiveGamePadsMask = 0;
    m_CXPlat_UserInput.MergeInputs( m_CXPlat_Signin.GetSignedInUserMask(), &dwActiveGamePadsMask );

    // Whichever gamepad was active, we'll update the corresponding user's activity timestamp.
    // Also, if we're in the middle of host migration or also in a Matchmaking session,
    // keep activity current for everyone we're not removing
    for( DWORD i = 0; i < MAX_USER_COUNT; ++i )
    {
        if( dwActiveGamePadsMask & 1 << m_Local.nController[ i ] )
        {
            m_Local.lastActivity[ i ] = GetTickCount();
        }
        else if( m_AppState == APPSTATE_HOSTMIGRATION && !m_Local.bToRemove[ i ] )
        {
            m_Local.lastActivity[ i ] = GetTickCount();
        }
        else if( GetMatchmakingSession() )
        {
            m_Local.lastActivity[ i ] = GetTickCount();
        }
    }

    // Get the current userinput status and check if system UI is showing
    if( !m_CXPlat_UserInput.Update() )
    {
        return S_OK;
    }

    // Change position for notification popup
    if( m_CXPlat_UserInput.WasUserInputSelected( USER_INPUT_SELECTION_CHANGE_NOTIF_POSITION ) )
    {
        ChangeNotificationPosition();
    }

    // Cancel pending overlapped operation?
    if( m_CXPlat_UserInput.WasUserInputSelected( USER_INPUT_SELECTION_CANCEL_OVERLAPPED_OPERATION ) )
    {
        // For now, only session searches can be cancelled
        if( m_AppState == APPSTATE_SEARCH_MATCHMAKING )
        {
            DebugSpew( "Cancelling overlapped task based on user input..\n" );
            ScheduleCancelOverlappedTask();
        }
    }

    // Invoke friends list or send invites to XBox LIVE Party
    for( UINT nController = 0; nController < MAX_USER_COUNT; ++nController )
    {
        const XUSER_SIGNIN_INFO& info = m_SignInInfo[ nController ];
        if( info.UserSigninState == eXUserSigninState_NotSignedIn )
        {
            continue;
        }

        if( m_CXPlat_UserInput.WasUserInputSelected( USER_INPUT_SELECTION_SHOW_FRIENDS_LIST, nController ) && m_AppState != APPSTATE_VIEWSTATS )
        {
            // Are we in an Xbox LIVE Party with at least one other member? If so, invite
            // players into our Matchmaking session with XPartySendGameInvites. 
            // Otherwise, invite players into our Matchmaking session with XInviteSend   
            DWORD dwRet = ERROR_SUCCESS;

            #ifdef _XBOX
            if( ShouldSendGameInvitesToLiveParty() )
            {
                dwRet = XPartySendGameInvites( nController, NULL );
            }
            else 
            #endif
            if( ShouldSendGameInvitesToFriends( nController ) )
            {
                dwRet = XInviteSend( nController, m_Local.cFriends[nController], m_Local.friendXuids[nController], L"InGamePartyMatchmaking invite via XInviteSend", NULL );
            }
            else
            {
                ShowFriendsList( nController );
            }

            if( dwRet != ERROR_SUCCESS )
            {
                FatalError( "Failed to send invites! dwRet = %d\n", dwRet );
            }
        }
        else if( m_CXPlat_UserInput.WasUserInputSelected( USER_INPUT_SELECTION_UPDATE_PRESENCE ) )
        {
            SchedulePresenceEnumTasks( TRUE, nController );
            SchedulePresenceEnumTasks( FALSE, nController );
        }
    }

    // Process current app state
    switch( m_AppState )
    {
        case APPSTATE_MAINMENU:
            hr = UpdateMainMenu();
            break;

        case APPSTATE_SEARCHUI_MATCHMAKING:
            hr = UpdateSearchUI();
            break;

        case APPSTATE_SEARCH_MATCHMAKING_DONE:
            hr = UpdateSearchDone();
            break;

        case APPSTATE_CREATE_PRESENCE_UI:
            hr = UpdateCreatePresenceUI();
            break;

        case APPSTATE_CREATE_MATCHMAKING_UI:
            hr = UpdateCreateMatchmakingUI();
            break;

        #ifdef _XBOX
        case APPSTATE_VIEWSTATS:
            if( !m_bDrawHelp )
            {
                hr = UpdateViewStats();
            }
            break;
        #endif

        case APPSTATE_REGISTERED:
        case APPSTATE_PREGAME:
        case APPSTATE_INGAME:
        case APPSTATE_POSTGAME:
            hr = UpdateInSession();
            break;

        case APPSTATE_HOSTMIGRATION:
            m_HostMigration.UpdateHostMigration();
            break;
    }

    // Check for messages from remote players
    ReceiveMessage();

    // Handle heartbeat of remote players in our Presence session
    HandleHeartbeat( GetPresenceSession() );

    return hr;
}

//--------------------------------------------------------------------------------------
// Name: UpdateMainMenu()
// Desc: Handle the main menu
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateMainMenu()
{
    // Get the current userinput status and check if system UI is showing
    if( !m_CXPlat_UserInput.Update() )
    {
        return S_OK;
    }
    
    // Change current menu item
    if( m_CXPlat_UserInput.GotoNextMenuItem() )
    {
        ++m_nMenuItem;
        m_nMenuItem %= MAINMENU_MAX;
    }

    if( m_CXPlat_UserInput.GotoPreviousMenuItem() )
    {
        --m_nMenuItem;
        m_nMenuItem += MAINMENU_MAX;
        m_nMenuItem %= MAINMENU_MAX;
    }

    // Select current menu item
    for( UINT nController = 0; nController < MAX_USER_COUNT; ++nController )
    {
        if( m_CXPlat_UserInput.SelectCurrentMenuItem( nController ) )
        {
            switch( m_nMenuItem )
            {
                case MAINMENU_CREATE_PRESENCE:
                    if( !GetPresenceSession() )
                    {
                        ScheduleChangeStateTask( APPSTATE_CREATE_PRESENCE_UI );
                    }
                    break;
            
                case MAINMENU_CREATE_MATCHMAKING:
                    ScheduleChangeStateTask( APPSTATE_CREATE_MATCHMAKING_UI );
                    break;

                case MAINMENU_SEARCH_MATCHMAKING:
                    ScheduleChangeStateTask( APPSTATE_SEARCHUI_MATCHMAKING );
                    break;

                #ifdef _XBOX // Currently only supported in Xbox 360 version of this sample
                case MAINMENU_VIEWSTATS:
                    ScheduleChangeStateTask( APPSTATE_VIEWSTATS );
                    break;
                #endif

                case MAINMENU_LOGIN:
                    m_CXPlat_Signin.ShowSigninUI();
                    break;

                case MAINMENU_DELETE_PRESENCE:
                    DeleteSessionGracefully( GetPresenceSession() );
                    break;

                #ifdef _XBOX
                case MAINMENU_FIND_LIVEPARTY_SESSIONS:
                    XShowCommunitySessionsUI( nController, XSHOWCOMMUNITYSESSION_SHOWPARTY );
                    break;
                #endif
            }            
            break; //exit for-loop
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateSeachUI()
// Desc: Handle the session searching UI screen
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateSearchUI()
{
    // Get the current userinput status and check if system UI is showing
    if( !m_CXPlat_UserInput.Update() )
    {
        return S_OK;
    }
    
    // Return to main menu
    if( m_CXPlat_UserInput.ReturnToPreviousUIScreen() )
    {
        ScheduleChangeStateTask( APPSTATE_MAINMENU );
    }

    // Change current menu item
    if( m_CXPlat_UserInput.GotoNextMenuItem() )
    {
        ++m_nMenuItem;
        m_nMenuItem %= SEARCHMENU_MAX;
    }

    if( m_CXPlat_UserInput.GotoPreviousMenuItem() )
    {
        --m_nMenuItem;
        m_nMenuItem += SEARCHMENU_MAX;
        m_nMenuItem %= SEARCHMENU_MAX;
    }

    // Change current menu item value
    const BOOL bRight = m_CXPlat_UserInput.GotoNextMenuItemValue();
    const BOOL bLeft  = m_CXPlat_UserInput.GotoPreviousMenuItemValue();
    if( bLeft || bRight )
    {
        BOOL bIncrement = bRight;

        switch( m_nMenuItem )
        {
        case SEARCHMENU_MAP:
            // Game mode
            if( bIncrement )
            {
                ++m_nMap;
            }
            else
            {
                --m_nMap;
            }
            m_nMap += ( m_cMaps + 1 ); 
            m_nMap %= ( m_cMaps + 1 );
            break;

        case SEARCHMENU_GAMETYPE:
            if( m_nGameType == X_CONTEXT_GAME_TYPE_STANDARD )
            {
                m_nGameType = X_CONTEXT_GAME_TYPE_RANKED;
            }
            else
            {
                m_nGameType = X_CONTEXT_GAME_TYPE_STANDARD;
            }
            break;

        case SEARCHMENU_GAMEMODE:
            // Game mode
            if( bIncrement )
            {
                ++m_nGameMode;
            }
            else
            {
                --m_nGameMode;
            }
            m_nGameMode += m_cGameModes; 
            m_nGameMode %= m_cGameModes;
            break;

        case SEARCHMENU_MINVICTORYPOINTS:
            // Minimum victory points
            if( bIncrement && m_nMinVictoryPoints < m_nMaxVictoryPoints )
            {
                ++m_nMinVictoryPoints;
            }
            else if( !bIncrement && m_nMinVictoryPoints > VICTORY_POINTS_MIN )
            {
                --m_nMinVictoryPoints;
            }
            break;

        case SEARCHMENU_MAXVICTORYPOINTS:
            // Maximum victory points
            if( bIncrement && m_nMaxVictoryPoints < VICTORY_POINTS_MAX )
            {
                ++m_nMaxVictoryPoints;
            }
            else if( !bIncrement && m_nMaxVictoryPoints > m_nMinVictoryPoints )
            {
                --m_nMaxVictoryPoints;
            }
            break;

        }
    }

    // Select current menu item
    if( m_nMenuItem == SEARCHMENU_SEARCH )
    {
        for( DWORD nController = 0; nController < MAX_USER_COUNT; ++nController )
        {
            const XUSER_SIGNIN_INFO& info = m_SignInInfo[ nController ];
            if( info.UserSigninState == eXUserSigninState_NotSignedIn )
            {
                continue;
            }

            if( m_CXPlat_UserInput.SelectCurrentMenuItem( nController ) )
            {
                SessionManager* pSessionMgr = new SessionManager();
                pSessionMgr->SetSessionOwner( nController );
                m_pSessionMgrCtx = pSessionMgr;
                ScheduleSessionSearchTasks( pSessionMgr );
                break;
            }
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateSearchDone()
// Desc: Handle the search done screen
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateSearchDone()
{    
    // Get the current userinput status and check if system UI is showing
    if( !m_CXPlat_UserInput.Update() )
    {
        return S_OK;
    }

    SessionManager* pSessionMgr = m_pSessionMgrCtx;
    if( !pSessionMgr )
    {
        ScheduleChangeStateTask( APPSTATE_MAINMENU );
        return S_OK;
    }

    // Return to main menu
    if( m_CXPlat_UserInput.ReturnToPreviousUIScreen() )
    {
        // $TODO - necessary?
        if( SessionManagerFromNonce( pSessionMgr->GetSessionNonce() ) )
        {
            m_mapSessions.erase( pSessionMgr->GetSessionNonce() ); 
        }
        
        delete pSessionMgr;
        m_pSessionMgrCtx = NULL;
        ScheduleChangeStateTask( APPSTATE_MAINMENU );
    }

    if( m_pSearchResults && m_pSearchResults->dwSearchResults )
    {
        // Move the menu
        if( m_CXPlat_UserInput.GotoNextMenuItem() )
        {
            ++m_nMenuItem;
            m_nMenuItem %= m_pSearchResults->dwSearchResults;
        }

        if( m_CXPlat_UserInput.GotoPreviousMenuItem() )
        {
            --m_nMenuItem;
            m_nMenuItem += m_pSearchResults->dwSearchResults;
            m_nMenuItem %= m_pSearchResults->dwSearchResults;
        }

        // Join the selected session
        for( DWORD nController = 0; nController < MAX_USER_COUNT; ++nController )
        {
            const XUSER_SIGNIN_INFO& info = m_SignInInfo[ nController ];
            if( info.UserSigninState == eXUserSigninState_NotSignedIn )
            {
                continue;
            }

            if( m_CXPlat_UserInput.SelectCurrentMenuItem( nController ) )
            {
                // Initialize our session manager
                SessionManagerInitParams initParams;
                initParams.m_SessionCreationReason  = SessionCreationReasonJoinFromSearch;
                initParams.m_bIsHost                = FALSE;
                initParams.m_dwSessionFlags         = MATCHMAKING_SESSION_FLAGS;

                // If the gametype is ranked, but sure to add the 
                // XSESSION_CREATE_USES_ARBITRATION and XSESSION_CREATE_USES_STATS flags
                if( m_nGameType == X_CONTEXT_GAME_TYPE_RANKED )
                {
                    initParams.m_dwSessionFlags |= XSESSION_CREATE_USES_ARBITRATION
                                                |  XSESSION_CREATE_USES_STATS;
                }

                initParams.m_dwMaxPublicSlots   = MATCHMAKING_SESSION_PUBLICSLOTS;
                initParams.m_dwMaxPrivateSlots  = MATCHMAKING_SESSION_PRIVATESLOTS;

                pSessionMgr->Initialize( initParams );

                // Give it the XSESSION_INFO from the host
                pSessionMgr->SetSessionInfo( m_pSearchResults->pResults[ m_nMenuItem ].info );

                // Resolve the host's IP address
                XNetRegisterKey( &m_pSearchResults->pResults[ m_nMenuItem ].info.sessionID, 
                                 &m_pSearchResults->pResults[ m_nMenuItem ].info.keyExchangeKey );

                IN_ADDR in_addr;
                if( XNetXnAddrToInAddr( &m_pSearchResults->pResults[ m_nMenuItem ].info.hostAddress,
                                        &m_pSearchResults->pResults[ m_nMenuItem ].info.sessionID, 
                                        &in_addr ) != 0 )
                {
                    FatalError( "Could not resolve host's IP address." );
                }

                // Store the host's IN_ADDR
                pSessionMgr->SetHostInAddr( in_addr );

                // Set local owner of this session
                pSessionMgr->SetSessionOwner( nController );

                // Set up game type and game mode contexts
                XUserSetContext( nController, 
                                 X_CONTEXT_GAME_TYPE, 
                                 m_nGameType );

                XUserSetContext( nController, 
                                 X_CONTEXT_GAME_MODE, 
                                 m_nGameMode );

                // Create the session
                ScheduleChangeStateTask( APPSTATE_CREATE_SESSION_UNHOSTED, pSessionMgr );

                return S_OK;
            }
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: UpdateCreatePresenceUI()
// Desc: Handle the presence session creation UI screen
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateCreatePresenceUI()
{
    SessionManager* pSessionMgr = m_pSessionMgrCtx; 
    if( !pSessionMgr )
    {
        pSessionMgr = new SessionManager();
        m_pSessionMgrCtx = pSessionMgr;
    }

    // Get the current userinput status and check if system UI is showing
    if( !m_CXPlat_UserInput.Update() )
    {
        return S_OK;
    }

    // Return to main menu
    if( m_CXPlat_UserInput.ReturnToPreviousUIScreen() )
    {
        if( SessionManagerFromNonce( pSessionMgr->GetSessionNonce() ) )
        {
            delete pSessionMgr;
        }
        m_pSessionMgrCtx = NULL;
        ScheduleChangeStateTask( APPSTATE_MAINMENU );
    }

    // Change current menu item
    if( m_CXPlat_UserInput.GotoNextMenuItem() )
    {
        ++m_nMenuItem;
        m_nMenuItem %= CREATE_PRESENCE_MENU_MAX;
    }

    if( m_CXPlat_UserInput.GotoPreviousMenuItem() )
    {
        --m_nMenuItem;
        m_nMenuItem += CREATE_PRESENCE_MENU_MAX;
        m_nMenuItem %= CREATE_PRESENCE_MENU_MAX;
    }

    // Change current menu item value
    const BOOL bRight = m_CXPlat_UserInput.GotoNextMenuItemValue();
    const BOOL bLeft  = m_CXPlat_UserInput.GotoPreviousMenuItemValue();
    if( bLeft || bRight )
    {
        switch( m_nMenuItem )
        {
        case CREATE_PRESENCE_MENU_INVITES:
            pSessionMgr->FlipSessionFlags( XSESSION_CREATE_INVITES_DISABLED );
            break;

        case CREATE_PRESENCE_MENU_JOINVIAPRESENCE:
            pSessionMgr->FlipSessionFlags( XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED );
            break;

        case CREATE_PRESENCE_MENU_JOININPROGRESS:
            pSessionMgr->FlipSessionFlags( XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED );
            break;
        }
    }

    // Select current menu item
    if( m_nMenuItem == CREATE_PRESENCE_MENU_CREATE )
    {
        for( DWORD nController = 0; nController < MAX_USER_COUNT; ++nController )
        {
            const XUSER_SIGNIN_INFO& info = m_SignInInfo[ nController ];
            if( info.UserSigninState == eXUserSigninState_NotSignedIn )
            {
                continue;
            }

            if( m_CXPlat_UserInput.SelectCurrentMenuItem( nController ) )
            {
                m_pSessionMgrCtx = NULL;

                // Set owner of the presence session
                pSessionMgr->SetSessionOwner( nController );

                // Create session
                ScheduleChangeStateTask( APPSTATE_CREATE_PRESENCE, pSessionMgr );
                break;
            }
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: UpdateCreateMatchmakingUI()
// Desc: Handle the matchmaking session creation UI screen
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateCreateMatchmakingUI()
{
    SessionManager* pSessionMgr = m_pSessionMgrCtx; 
    if( !pSessionMgr )
    {
        pSessionMgr = new SessionManager();
        m_pSessionMgrCtx = pSessionMgr;
    }

    // Get the current userinput status and check if system UI is showing
    if( !m_CXPlat_UserInput.Update() )
    {
        return S_OK;
    }

    // Return to main menu
    if( m_CXPlat_UserInput.ReturnToPreviousUIScreen() )
    {
        if( SessionManagerFromNonce( pSessionMgr->GetSessionNonce() ) )
        {
            delete pSessionMgr;
        }
        m_pSessionMgrCtx = NULL;
        ScheduleChangeStateTask( APPSTATE_MAINMENU );
    }

    // Change current menu item
    if( m_CXPlat_UserInput.GotoNextMenuItem() )
    {
        ++m_nMenuItem;
        m_nMenuItem %= CREATE_MATCHMAKING_MENU_MAX;
    }

    if( m_CXPlat_UserInput.GotoPreviousMenuItem() )
    {
        --m_nMenuItem;
        m_nMenuItem += CREATE_MATCHMAKING_MENU_MAX;
        m_nMenuItem %= CREATE_MATCHMAKING_MENU_MAX;
    }

    // Change current menu item value
    const BOOL bRight = m_CXPlat_UserInput.GotoNextMenuItemValue();
    const BOOL bLeft  = m_CXPlat_UserInput.GotoPreviousMenuItemValue();
    if( bLeft || bRight )
    {
        BOOL bIncrement = bRight;

        const SessionManager* const pPresenceSessionMgr = GetPresenceSession();

        switch( m_nMenuItem )
        {
        case CREATE_MATCHMAKING_MENU_MAP:
            // Map
            if( bIncrement )
            {
                ++m_nMap;
            }
            else
            {
                --m_nMap;
            }
            m_nMap += m_cMaps;
            m_nMap %= m_cMaps;
            break;

        case CREATE_MATCHMAKING_MENU_GAMETYPE:
            if( m_nGameType == X_CONTEXT_GAME_TYPE_STANDARD )
            {
                m_nGameType = X_CONTEXT_GAME_TYPE_RANKED;
                pSessionMgr->SetSessionFlags( XSESSION_CREATE_USES_ARBITRATION, FALSE );
            }
            else
            {
                m_nGameType = X_CONTEXT_GAME_TYPE_STANDARD;
                pSessionMgr->ClearSessionFlags( XSESSION_CREATE_USES_ARBITRATION );
            }

            break;

        case CREATE_MATCHMAKING_MENU_GAMEMODE:
            // Game mode
            if( bIncrement )
            {
                ++m_nGameMode;
            }
            else
            {
                --m_nGameMode;
            }
            m_nGameMode += m_cGameModes;
            m_nGameMode %= m_cGameModes;
            break;

        case CREATE_MATCHMAKING_MENU_VICTORYPOINTS:
            // Victory points
            if( bIncrement && m_nVictoryPoints < VICTORY_POINTS_MAX )
            {
                ++m_nVictoryPoints;
            }
            else if( !bIncrement && m_nVictoryPoints > VICTORY_POINTS_MIN )
            {
                --m_nVictoryPoints;
            }
            break;

        case CREATE_MATCHMAKING_MENU_INVITES:
            if( !pPresenceSessionMgr )
            {
                pSessionMgr->FlipSessionFlags( XSESSION_CREATE_INVITES_DISABLED );
                break;
            }
        case CREATE_MATCHMAKING_MENU_JOINVIAPRESENCE:
            if( !pPresenceSessionMgr )
            {
                pSessionMgr->FlipSessionFlags( XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED );
            }
            break;

        case CREATE_MATCHMAKING_MENU_JOININPROGRESS:
            if( !pPresenceSessionMgr )
            {
                pSessionMgr->FlipSessionFlags( XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED );
            }
            break;
        }
    }

    // Select current menu item
    if( m_nMenuItem == CREATE_MATCHMAKING_MENU_CREATE )
    {
        for( DWORD nController = 0; nController < MAX_USER_COUNT; ++nController )
        {
            const XUSER_SIGNIN_INFO& info = m_SignInInfo[ nController ];
            if( info.UserSigninState == eXUserSigninState_NotSignedIn )
            {
                continue;
            }

            if( m_CXPlat_UserInput.SelectCurrentMenuItem( nController ) )
            {
                m_pSessionMgrCtx = NULL;

                // Set owner of the session
                pSessionMgr->SetSessionOwner( nController );

                // Create session
                ScheduleChangeStateTask( APPSTATE_CREATE_MATCHMAKING, pSessionMgr );
                break;
            }
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: UpdateInSession()
// Desc: Handle the in-session case
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateInSession()
{
    SessionManager* pSessionMgr = GetMatchmakingSession(); 
    if( !pSessionMgr || pSessionMgr->GetSessionState() > SessionStateDelete  )
    {
        // Session gone! Return to main menu
        ScheduleChangeStateTask( APPSTATE_MAINMENU );
        return S_OK;
    }

    const XNKID& sessionID  = pSessionMgr->GetSessionID();
    const BOOL bIsHost      = pSessionMgr->IsSessionHost();

    // Get the current userinput status and check if system UI is showing
    if( !m_CXPlat_UserInput.Update() )
    {
        // Do network tasks and then exit
        ReceiveMessage();

        HandleHeartbeat( pSessionMgr );
        HandleHeartbeat( GetPresenceSession() );

        return S_OK;
    }

    // Leave the session?
    if( m_CXPlat_UserInput.ReturnToPreviousUIScreen() )
    {
        // If we're the host of a Presence session, then instruct all party
        // members to also leave this Matchmaking session
        SessionManager* pPresenceSessionMgr = GetPresenceSession(); 
        if( pPresenceSessionMgr != NULL &&
            pPresenceSessionMgr->IsSessionHost() )
        {
            LeaveSession( pSessionMgr );
        }

        // Gracefully delete this Matchmaking session
        DeleteSessionGracefully( pSessionMgr );

        return S_OK;
    }

    // If we're not in an error state, do network tasks
    if( !pSessionMgr->GetSessionError() )
    {
        ReceiveMessage();

        HandleHeartbeat( pSessionMgr );
        HandleHeartbeat( GetPresenceSession() );

        // Check for user input

        UINT iIndex         = 0;
        UINT nController    = 0;
        for( nController = 0, iIndex = 0; nController < MAX_USER_COUNT; ++nController )
        {
            const XUSER_SIGNIN_INFO& info = m_SignInInfo[ nController ];
            if( info.UserSigninState == eXUserSigninState_NotSignedIn )
            {
                continue;
            }

            const DWORD dwLastUserInput                     = m_CXPlat_UserInput.GetLastUserInput( nController );
            const BOOL bWasUserInputWaveSelected            = ( dwLastUserInput == USER_INPUT_SELECTION_WAVE       );
            const BOOL bWasUserInputStartGameSelected       = ( dwLastUserInput == USER_INPUT_SELECTION_STARTGAME  );
            const BOOL bWasUserInputScorePointSelected      = ( dwLastUserInput == USER_INPUT_SELECTION_SCOREPOINT );
            const BOOL bWasUserInputToggleLoopbackSelected  = ( dwLastUserInput == USER_INPUT_SELECTION_TOGGLE     );

            if( bWasUserInputWaveSelected )
            {
                // Send a wave message to the host
                if( nController < MAX_USER_COUNT )
                {
                    CMessage msgWave( MSG_WAVE );
                    msgWave.SetSessionID( sessionID );

                    msgWave.GetWave().id   = m_Local.id;
                    msgWave.GetWave().xuid = m_Local.xuids[ iIndex ];

                    SendMessage( &msgWave, pSessionMgr->GetHostInAddr() );
                }
            }

            if( bWasUserInputScorePointSelected )
            {
                // Can only score points when in-game
                if( m_AppState == APPSTATE_INGAME )
                {
                    CMessage msgScorePoint( MSG_SCORE_POINT );
                    msgScorePoint.SetSessionID( sessionID );

                    msgScorePoint.GetScorePoint().id   = m_Local.id;
                    msgScorePoint.GetScorePoint().xuid = m_Local.xuids[ iIndex ];

                    SendMessage( &msgScorePoint, pSessionMgr->GetHostInAddr() );
                }
            }

            if( bWasUserInputStartGameSelected )
            {
                // Make sure we're the host and we haven't started yet and if this is a 
                // ranked game, we have at least one other player
                if( m_AppState == APPSTATE_PREGAME && bIsHost &&
                    nController == pSessionMgr->GetSessionOwner() &&
                    ( m_nGameType == X_CONTEXT_GAME_TYPE_STANDARD || !m_vecRemote.empty() ) )
                {
                    // if we're not ranked, just start the game
                    if( m_nGameType == X_CONTEXT_GAME_TYPE_STANDARD )
                    {
                        StartGame( sessionID );
                    }
                    else
                    {
                        // Order everyone else to register
                        ScheduleSessionHostRegisterTasks( pSessionMgr );
                    }
                }
            }

            if( bWasUserInputToggleLoopbackSelected )
            {
//                m_Voice.ToggleLoopbackMode( nController );
            }

            ++iIndex;
        }
    }

    return S_OK;
}

#ifdef _XBOX
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
    if( m_bReadingLeaderboard )
    {
        HRESULT hr = XGetOverlappedExtendedError( &m_Overlapped );

        if( FAILED( hr ) )
        {
            FatalError(
                "XEnumerate overlapped task failed with error 0x%08x", 
                hr);
        }
        else
        {
            // Try to select the appropriate entry
            m_nMenuItem = 0;

            for( UINT i = 0; i < m_pStats->pViews[ 0 ].dwNumRows; ++i )
            {
                if( ( m_nNextSelectedRank &&
                      m_nNextSelectedRank == m_pStats->pViews[ 0 ].pRows[ i ].dwRank ) ||
                    (!m_nNextSelectedRank &&
                      m_xuidNextSelectedXuid == m_pStats->pViews[ 0 ].pRows[ i ].xuid ) )
                {
                    m_nMenuItem = i;
                    break;
                }
            }
        }

        m_bReadingLeaderboard = FALSE;

        return S_OK;
    }

    // Get the current userinput status and check if system UI is showing
    if( !m_CXPlat_UserInput.Update() )
    {
        return S_OK;
    }

    // Return to main menu
    if( m_CXPlat_UserInput.ReturnToPreviousUIScreen() )
    {
        ScheduleChangeStateTask( APPSTATE_MAINMENU );
    }

    // Toggle leaderboard
    if( m_CXPlat_UserInput.WasUserInputSelected( USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_LEADERBOARD ) )
    {
        ++m_nLeaderboard;
        m_nLeaderboard %= m_cLeaderboards;
        m_nNextSelectedRank = 1;
        ReadLeaderboard( 1, 0 );
    }

    // Toggle game type
    if( m_CXPlat_UserInput.WasUserInputSelected( USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_GAMETYPE ) )
    {
        m_nGameType = !m_nGameType;
        m_nNextSelectedRank = 1;
        ReadLeaderboard( 1, 0 );
    }

    // Switch between alpha blending for the TrueSkill(TM) skill bars 
    if( m_CXPlat_UserInput.WasUserInputSelected( USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_ALPHABLENDING ) )
    {
        m_bUseAlphaBlending = !m_bUseAlphaBlending;
    }

    // Zoom to top
    if( m_CXPlat_UserInput.WasUserInputSelected( USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_ZOOM_TOP ) )
    {
        m_nNextSelectedRank = 1;
        ReadLeaderboard( 1, 0 );
    }

    // Zoom to bottom
    if( m_CXPlat_UserInput.WasUserInputSelected( USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_ZOOM_BOTTOM ) )
    {
        DWORD dwLast = m_pStats->pViews[ 0 ].dwTotalViewRows;
        m_nNextSelectedRank = dwLast;

        INT nTopIndex = 1;
        if( dwLast > m_nMaxLeaderboardRows )
        {
            nTopIndex = dwLast - m_nMaxLeaderboardRows + 1;
        }

        ReadLeaderboard( nTopIndex, 0 );
    }

    // Zoom to user
    for( UINT nController = 0; nController < MAX_USER_COUNT; ++nController )
    {
        const XUSER_SIGNIN_INFO& info = m_SignInInfo[ nController ];
        if( info.UserSigninState == eXUserSigninState_NotSignedIn ) continue;

        if( m_CXPlat_UserInput.WasUserInputSelected( USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_ZOOM_USER, nController ) )
        {
            m_nNextSelectedRank = 0;
            m_xuidNextSelectedXuid = info.xuid;
            ReadLeaderboard( 0, m_xuidNextSelectedXuid );
        }
    }

    // Move selection down by one
    if( m_CXPlat_UserInput.GotoNextMenuItem() && m_pStats )
    {
        // If we can move without scrolling, do it
        if( m_nMenuItem < m_pStats->pViews[ 0 ].dwNumRows - 1 )
        {
            ++m_nMenuItem;
        }
        else
        {
            // We need to scroll, but only if we're not at the bottom
            if( m_pStats->pViews[ 0 ].dwTotalViewRows >
                m_pStats->pViews[ 0 ].pRows[ m_nMenuItem ].dwRank )
            {
                m_nNextSelectedRank = m_pStats->pViews[ 0 ].pRows[ m_nMenuItem ].dwRank + 1;
                ReadLeaderboard( m_nNextSelectedRank, 0 );
            }
        }
    }

    // Move selection up by one
    if( m_CXPlat_UserInput.GotoPreviousMenuItem() && m_pStats )
    {
        // If we can move without scrolling, do it
        if( m_nMenuItem > 0 )
        {
            --m_nMenuItem;
        }
        else
        {
            // We need to scroll, but only if we're not at the top
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
    }

    return S_OK;
}
#endif
