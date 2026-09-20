//--------------------------------------------------------------------------------------
// SessionsUpdate.cpp
//
// Contains methods to update the Sessions UI
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
#include <vector>


//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
const DWORD MAX_CONNECTIONWAIT = 10000;    // wait ten seconds for response from host


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    HRESULT hr = S_OK;

    // Update the signin
    DWORD dwUpdateFlags = ATG::SignIn::Update();

    // Cache local users
    if( dwUpdateFlags & ATG::SignIn::SIGNIN_USERS_CHANGED )
    {
        CHAR szGamerTag[XUSER_NAME_SIZE];
        m_Local.cPlayers = 0;

        // Get the local player info
        for( int i = 0; i < XUSER_MAX_COUNT; ++i )
        {
            if( ATG::SignIn::IsUserSignedIn( i ) &&
                ATG::SignIn::CheckPrivilege( i, XPRIVILEGE_MULTIPLAYER_SESSIONS ) )
            {
                // Retrieve XUID and name for each logged-in user
                m_Local.nController[ m_Local.cPlayers ] = i;
                XUserGetXUID( i, &m_Local.xuids[ m_Local.cPlayers ] );
                XUserGetName( i, szGamerTag, XUSER_NAME_SIZE );
                szGamerTag[ XUSER_NAME_SIZE - 1 ] = '\0';
                MultiByteToWideChar( CP_ACP, 0, szGamerTag, -1, m_Local.strGamertags[ m_Local.cPlayers ],
                                     XUSER_NAME_SIZE );
                m_Local.cPlayers++;
            }
        }

        // Notify the voice manager that we've got new users
        m_Voice.LocalUsersChanged();

        if( m_AppState != APPSTATE_MAINMENU )
        {
            m_Session.SetSessionError( L"Signed in user changed, back to Main Menu" );
            SwitchToState( APPSTATE_DELETING );
            return S_OK;
        }

    }

    // If signin state changed, update our machine ID
    if( dwUpdateFlags & ATG::SignIn::CONNECTION_CHANGED )
    {
        XNADDR xnaddr;

        while( XNetGetTitleXnAddr( &xnaddr ) == XNET_GET_XNADDR_PENDING );

        XNetXnAddrToMachineId( &xnaddr, &m_Local.id );
    }

    // NOP if we're not signed in
    if( !ATG::SignIn::AreUsersSignedIn() )
    {
        return S_OK;
    }

    // Check for accepted invitations
    if( CheckForAcceptedInvitation() )
    {
        return hr;
    }

    // Look for any mutelist changed notifications
    DWORD dwNotificationID;
    ULONG_PTR ulParam;

    if( XNotifyGetNext( m_hSysListener, 0, &dwNotificationID, &ulParam ) &&
        dwNotificationID == XN_SYS_MUTELISTCHANGED )
    {
        UpdateMuteLists();
    }

    // Process available voice data
    if( m_Voice.ProcessVoice() )
    {
        SendVoiceMessage();
    }

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput( ATG::SignIn::GetSignedInUserMask() );

    if( APPSTATE_VIEWSTATS == m_AppState || m_bDrawHelp )
    {
        // Toggle help
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
            m_bDrawHelp = !m_bDrawHelp;
    }
    else
    {
        // Change position for notification popup
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
        {
            ++m_nNotificationPosition;
            m_nNotificationPosition %= m_cNotificationPosition;

            ChangeNotificationPosition();
        }

        // Invoke friends list
        for( UINT nController = 0; nController < XUSER_MAX_COUNT; ++nController )
        {
            if( !ATG::SignIn::IsUserSignedIn( nController ) )
            {
                continue;
            }

            if( ( ATG::Input::m_Gamepads[ nController ].wPressedButtons & XINPUT_GAMEPAD_Y ) &&
                !ATG::SignIn::IsSystemUIShowing() )
            {
                ShowCommunitySessionsUI( nController );
            }
        }
    }

    m_Session.Update();

    switch( m_AppState )
    {
        case APPSTATE_MAINMENU:
            hr = UpdateMainMenu();
            break;

        case APPSTATE_SEARCHUI:
            hr = UpdateSearchUI();
            break;

        case APPSTATE_SEARCH:
            hr = UpdateSearch();
            break;

        case APPSTATE_CREATEUI:
            hr = UpdateCreateUI();
            break;

        case APPSTATE_CONNECTING:
            hr = UpdateConnecting();
            break;

        case APPSTATE_REGISTERING:
            hr = UpdateRegistering();
            break;

        case APPSTATE_WAITINGFORREGISTRATION:
            hr = UpdateWaitingForRegistration();
            break;

        case APPSTATE_STARTING:
            hr = UpdateStarting();
            break;

        case APPSTATE_ENDING:
            hr = UpdateEnding();
            break;

        case APPSTATE_VIEWSTATS:
            if( !m_bDrawHelp )
            {
                hr = UpdateViewStats();
            }
            break;

        case APPSTATE_DELETING:
            hr = UpdateDeleting();
            break;

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

    return hr;
}

//--------------------------------------------------------------------------------------
// Name: UpdateMainMenu()
// Desc: Handle the main menu
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateMainMenu()
{
    // Get the current gamepad status
    ATG::GAMEPAD& gamepad = ATG::Input::m_DefaultGamepad;

    // B: return to signin
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_B &&
        !ATG::SignIn::IsSystemUIShowing() )
    {
        ATG::SignIn::ShowSignInUI();
    }

    // Down/up: change current menu item
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        ++m_nMenuItem;
        m_nMenuItem %= MAINMENU_MAX;
    }

    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        --m_nMenuItem;
        m_nMenuItem += MAINMENU_MAX;
        m_nMenuItem %= MAINMENU_MAX;
    }

    // A/Start: select current menu item
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_A ||
        gamepad.wPressedButtons & XINPUT_GAMEPAD_START )
    {
        switch( m_nMenuItem )
        {
            case MAINMENU_CREATE:
                SwitchToState( APPSTATE_CREATEUI );
                break;

            case MAINMENU_SEARCH:
                SwitchToState( APPSTATE_SEARCHUI );
                break;

            case MAINMENU_VIEWSTATS:
                SwitchToState( APPSTATE_VIEWSTATS );
                break;

            case MAINMENU_LOGIN:
                ATG::SignIn::ShowSignInUI();
                break;
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
    // Get the current gamepad status
    ATG::GAMEPAD& gamepad = ATG::Input::m_DefaultGamepad;

    // B: return to main menu
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_B &&
        !ATG::SignIn::IsSystemUIShowing() )
    {
        SwitchToState( APPSTATE_MAINMENU );
    }

    // Down/up: change current menu item
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        ++m_nMenuItem;
        m_nMenuItem %= SEARCHMENU_MAX;
    }

    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        --m_nMenuItem;
        m_nMenuItem += SEARCHMENU_MAX;
        m_nMenuItem %= SEARCHMENU_MAX;
    }

    // Left/right: change current menu item value
    if( gamepad.wPressedButtons & ( XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT ) )
    {
        bool bIncrement = ( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT ) != 0;

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

    // A/Start: select current menu item
    if( m_nMenuItem == SEARCHMENU_SEARCH )
    {
        for( DWORD nController = 0; nController < XUSER_MAX_COUNT; ++nController )
        {
            if( !ATG::SignIn::IsUserSignedIn( nController ) )
            {
                continue;
            }

            if( ATG::Input::m_Gamepads[ nController ].wPressedButtons &
                ( XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_START ) )
            {
                m_Session.SetSessionOwner( nController );
                SwitchToState( APPSTATE_SEARCH );
            }
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateSearch()
// Desc: Handle the searching screen
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateSearch()
{
    // If we're in the middle of an overlapped operation, wait for it to finish
    // before we honor user input.
    if( !XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        return S_OK;
    }

    // Get the current gamepad status
    ATG::GAMEPAD& gamepad = ATG::Input::m_DefaultGamepad;

    // We have results if the overlapped operation succeeded and if there are
    // more than zero results.
    BOOL bHasResults =
        SUCCEEDED( XGetOverlappedExtendedError( &m_Overlapped ) ) &&
        m_pSearchResults->dwSearchResults > 0;

    // B: return to main menu
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_B )
    {
        SwitchToState( APPSTATE_MAINMENU );
    }

    if( bHasResults && m_pSearchResults )
    {
        if( !m_bQoSTesting )
        {
            for( DWORD nSessions = 0; nSessions < m_pSearchResults->dwSearchResults; ++nSessions )
            {
                m_QoSxnaddr[nSessions] = &( m_pSearchResults->pResults[nSessions].info.hostAddress );
                m_QoSxnkid[nSessions] = &( m_pSearchResults->pResults[nSessions].info.sessionID );
                m_QoSxnkey[nSessions] = &( m_pSearchResults->pResults[nSessions].info.keyExchangeKey );
            }

            // Create an event object that is autoreset with an initial state of "not signaled".
            // Pass this event handle to the QoSLookup to recieve notification of each QoS lookup.
            HANDLE QoSLookupHandle = CreateEvent(NULL, false, false, NULL);

            if( QoSLookupHandle == NULL )
            {
                ATG::FatalError( "CreateEvent failed" );
            }

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
                                      QoSLookupHandle,                      // Event handle
                                      &m_pQoSResult );                      // Pointer to a pointer to an XNQOS structure that receives the results from the QoS probes.

            if( 0 != iRet )
            {
                ATG::FatalError( "XNetQosLookup failed with error 0x%08x", iRet );
            }

            m_bQoSTesting = TRUE;
            
            // Wait for results to all complete.  cxnqosPending will eventually hit zero.
            // Pause thread waiting for QosLookup events to be triggered.
            while (m_pQoSResult->cxnqosPending != 0)
            {
                WaitForSingleObject(QoSLookupHandle, INFINITE);
            }

            // Close the event handle
            CloseHandle(QoSLookupHandle);

            // Sort search results for ascending median roundtrip times
            // NOTE: There are more efficient sorting algorithms
            for( DWORD nSessions1 = 0; nSessions1 < m_pSearchResults->dwSearchResults; ++nSessions1 )
            {
                for( DWORD nSessions2 = nSessions1 + 1; nSessions2 < m_pSearchResults->dwSearchResults; ++nSessions2 )
                {
                    if (nSessions1 == nSessions2)
                        continue;

                    // Swap results if required
                    if (m_pQoSResult->axnqosinfo[nSessions1].wRttMedInMsecs > m_pQoSResult->axnqosinfo[nSessions2].wRttMedInMsecs)
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

        // If up or down is pressed, move the menu
        if( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        {
            ++m_nMenuItem;
            m_nMenuItem %= m_pSearchResults->dwSearchResults;
        }

        if( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        {
            --m_nMenuItem;
            m_nMenuItem += m_pSearchResults->dwSearchResults;
            m_nMenuItem %= m_pSearchResults->dwSearchResults;
        }

        // If A or Start is pressed, join the selected session
        for( DWORD nController = 0; nController < XUSER_MAX_COUNT; ++nController )
        {
            if( !ATG::SignIn::IsUserSignedIn( nController ) )
            {
                continue;
            }

            if( ATG::Input::m_Gamepads[ nController ].wPressedButtons &
                ( XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_START ) )
            {
                m_Session.SetSessionOwner( nController );
                m_Session.SetSessionInfo( m_pSearchResults->pResults[ m_nMenuItem ].info );
                JoinSession( FALSE );
            }
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateCreateUI()
// Desc: Handle the session creation UI screen
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateCreateUI()
{
    // Get the current gamepad status
    ATG::GAMEPAD& gamepad = ATG::Input::m_DefaultGamepad;

    // B: return to main menu
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_B &&
        !ATG::SignIn::IsSystemUIShowing() )
    {
        SwitchToState( APPSTATE_MAINMENU );
    }

    // Down/up: change current menu item
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        ++m_nMenuItem;
        m_nMenuItem %= CREATEMENU_MAX;
    }

    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        --m_nMenuItem;
        m_nMenuItem += CREATEMENU_MAX;
        m_nMenuItem %= CREATEMENU_MAX;
    }

    // Left/right: change current menu item value
    if( gamepad.wPressedButtons & ( XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT ) )
    {
        bool bIncrement = ( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT ) != 0;

        switch( m_nMenuItem )
        {
            case CREATEMENU_MAP:
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

            case CREATEMENU_GAMETYPE:
                if( m_nGameType == X_CONTEXT_GAME_TYPE_STANDARD )
                {
                    m_nGameType = X_CONTEXT_GAME_TYPE_RANKED;
                    m_Session.SetSessionFlags( XSESSION_CREATE_USES_ARBITRATION );
                }
                else
                {
                    m_nGameType = X_CONTEXT_GAME_TYPE_STANDARD;
                    m_Session.ClearSessionFlags( XSESSION_CREATE_USES_ARBITRATION );
                }

                break;

            case CREATEMENU_GAMEMODE:
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

            case CREATEMENU_VICTORYPOINTS:
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

            case CREATEMENU_INVITES:
                m_Session.FlipSessionFlags( XSESSION_CREATE_INVITES_DISABLED );
                break;

            case CREATEMENU_JOINVIAPRESENCE:
                m_Session.FlipSessionFlags( XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED );
                break;

            case CREATEMENU_JOININPROGRESS:
                m_Session.FlipSessionFlags( XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED );
                break;
        }
    }

    // A/Start: select current menu item
    if( m_nMenuItem == CREATEMENU_CREATE )
    {
        for( DWORD nController = 0; nController < XUSER_MAX_COUNT; ++nController )
        {
            if( !ATG::SignIn::IsUserSignedIn( nController ) )
            {
                continue;
            }

            if( ATG::Input::m_Gamepads[ nController ].wPressedButtons &
                ( XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_START ) )
            {
                m_Session.SetSessionOwner( nController );
                SwitchToState( APPSTATE_CREATE );
            }
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateConnecting()
// Desc: Wait for a response to a connection
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateConnecting()
{
    ReceiveMessage();

    if( GetTickCount() - m_dwConnectionTimer > MAX_CONNECTIONWAIT )
    {
        m_Session.SetSessionError( L"Connection timed out" );
        SwitchToState( APPSTATE_PREGAME );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateInSession()
// Desc: Handle the in-session case
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateInSession()
{
    // If we have an overlapped operation in progress, ignore inputs
    if( !XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        return S_OK;
    }

    // Get the current gamepad status
    ATG::GAMEPAD& gamepad = ATG::Input::m_DefaultGamepad;

    // If the B button is pressed, leave the session
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_B )
    {
        // Send a goodbye message to everyone
        if( !m_Session.GetSessionError() )
        {
            CMessage msgGoodbye( MSG_GOODBYE );

            if( !m_Session.IsHost() )
            {
                SendMessage( &msgGoodbye, m_pHost->addr );
            }
            else
            {
                for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
                {
                    SendMessage( &msgGoodbye, i->addr );
                }
            }
        }

        m_Session.SetSessionError( NULL );
        SwitchToState( APPSTATE_DELETING );

        return S_OK;
    }

    // If we're not in an error state, do network tasks
    if( !m_Session.GetSessionError() )
    {
        ReceiveMessage();

        HandleHeartbeat();

        // Check for controller input

        UINT iIndex, nController;
        for( nController = 0, iIndex = 0; nController < XUSER_MAX_COUNT; ++nController )
        {
            if( !ATG::SignIn::IsUserSignedIn( nController ) )
            {
                continue;
            }

            // X button: wave
            if( ATG::Input::m_Gamepads[ nController ].wPressedButtons & XINPUT_GAMEPAD_X )
            {
                // Send a wave message to the host
                if( nController < XUSER_MAX_COUNT )
                {
                    CMessage msgWave( MSG_WAVE );
                    msgWave.GetWave().id = m_Local.id;
                    msgWave.GetWave().xuid = m_Local.xuids[ iIndex ];

                    SendMessage( &msgWave, m_pHost->addr );
                }
            }

            // A button: score point
            if( ATG::Input::m_Gamepads[ nController ].wPressedButtons & XINPUT_GAMEPAD_A )
            {
                // Can only score points when in-game
                if( m_AppState == APPSTATE_INGAME )
                {
                    CMessage msgScorePoint( MSG_SCORE_POINT );
                    msgScorePoint.GetScorePoint().id = m_Local.id;
                    msgScorePoint.GetScorePoint().xuid = m_Local.xuids[ iIndex ];

                    SendMessage( &msgScorePoint, m_pHost->addr );
                }
            }

            // START button: start game
            if( ATG::Input::m_Gamepads[ nController ].wPressedButtons & XINPUT_GAMEPAD_START )
            {
                // Make sure we're the host and we haven't started yet and if this is a 
                // ranked session, we have at least one other player
                if( m_AppState == APPSTATE_PREGAME && m_Session.IsHost() &&
                    nController == m_Session.GetSessionOwner() &&
                    ( m_nGameType == X_CONTEXT_GAME_TYPE_STANDARD || !m_vecRemote.empty() ) )
                {
                    // if we're ranked, just start the game
                    if( m_nGameType == X_CONTEXT_GAME_TYPE_STANDARD )
                    {
                        StartGame();
                    }
                    else
                    {
                        // Order everyone else to register
                        StartRegistration();
                    }
                }
            }

            // Right trigger: toggle loopback mode
            if( ATG::Input::m_Gamepads[ nController ].wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
            {
                m_Voice.ToggleLoopbackMode( nController );
            }

            ++iIndex;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateRegistering()
// Desc: Wait for arbitration registration to complete
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateRegistering()
{
    if( !m_Session.GetSessionError() )
    {
        ReceiveMessage();

        HandleHeartbeat();
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateWaitingForRegistration()
// Desc: Wait for clients to finish registering
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateWaitingForRegistration()
{
    if( !m_Session.GetSessionError() )
    {
        ReceiveMessage();

        HandleHeartbeat();
    }

    if( ( GetTickCount() - m_dwRegistrationTimer ) > REGISTRATION_TIME )
    {
        // We've timed out. Register ourselves
        m_Session.RegisterForArbitration();
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateStarting()
// Desc: Start the session
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateStarting()
{
    if( !m_Session.GetSessionError() )
    {
        ReceiveMessage();

        HandleHeartbeat();
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateEnding()
// Desc: Finish the session
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateEnding()
{
    if( !m_Session.GetSessionError() )
    {
        ReceiveMessage();

        HandleHeartbeat();
    }

    FLOAT fCenterX = ( m_Font16.m_rcWindow.x2 - m_Font16.m_rcWindow.x1 ) / 2.0f;
    FLOAT fCenterY = ( m_Font16.m_rcWindow.y2 - m_Font16.m_rcWindow.y1 ) / 2.0f;
    m_Font16.DrawText( fCenterX, fCenterY, COLOR_HIGHLIGHT,
                       L"Writing Stats...", ATGFONT_CENTER_X );

    return S_OK;
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
    if( m_bReadingLeaderboard )
    {
        HRESULT hr = XGetOverlappedExtendedError( &m_Overlapped );

        if( FAILED( hr ) )
        {
            ATG::FatalError(
                "XEnumerate overlapped task failed with error 0x%08x",
                hr );
        }
        else
        {
            // Try to select the appropriate entry
            m_nMenuItem = 0;

            for( UINT i = 0; i < m_pStats->pViews[ 0 ].dwNumRows; ++i )
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

        m_bReadingLeaderboard = FALSE;

        return S_OK;
    }

    // Get the current gamepad status
    ATG::GAMEPAD& gamepad = ATG::Input::m_DefaultGamepad;

    // B: return to main menu
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_B )
    {
        SwitchToState( APPSTATE_MAINMENU );
    }

    // A: toggle leaderboard
    else if( gamepad.wPressedButtons & XINPUT_GAMEPAD_A )
    {
        ++m_nLeaderboard;
        m_nLeaderboard %= m_cLeaderboards;
        m_nNextSelectedRank = 1;
        ReadLeaderboard( 1, 0 );
    }

    // Right shoulder: toggle game type
    else if( gamepad.wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        m_nGameType = !m_nGameType;
        m_nNextSelectedRank = 1;
        ReadLeaderboard( 1, 0 );
    }

    // Right joystick click: switch between alpha blending for the TrueSkill(TM) skill bars 
    else if( gamepad.wPressedButtons & XINPUT_GAMEPAD_RIGHT_THUMB )
    {
        m_bUseAlphaBlending = !m_bUseAlphaBlending;
    }

    // X: zoom to top
    else if( gamepad.wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_nNextSelectedRank = 1;
        ReadLeaderboard( 1, 0 );
    }

    // Y: zoom to bottom
    else if( gamepad.wPressedButtons & XINPUT_GAMEPAD_Y )
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

    // Left shoulder: zoom to user
    else
    {
        for( UINT nController = 0; nController < XUSER_MAX_COUNT; ++nController )
        {
            if( !ATG::SignIn::IsUserSignedIn( nController ) ) continue;

            if( ATG::Input::m_Gamepads[ nController ].wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
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

    // Up: move selection up by one
    if( gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
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


//--------------------------------------------------------------------------------------
// Name: UpdateDeleting()
// Desc: Delete the session
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateDeleting()
{
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateMuteLists()
// Desc: Update the Mute lists for all players
//--------------------------------------------------------------------------------------
VOID Sample::UpdateMuteLists()
{
    CMessage msgMute( MSG_MUTE );
    MsgMute& Mute = msgMute.GetMute();

    m_Voice.ProcessMutelists( &Mute );

    // Send the mute update to everyone, except ourselves
    if( !m_Session.IsHost() )
    {
        SendMessage( &msgMute, m_pHost->addr );
    }
    else
    {
        for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
        {
            SendMessage( &msgMute, i->addr );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: CSession::Update()
// Desc: Called by sample update, processes overlapped call checks
//--------------------------------------------------------------------------------------
VOID CSession::Update()
{
    switch( m_SessionState )
    {
        case SESSION_STATE_CREATING:
            UpdateCreating();
            break;
        case SESSION_STATE_REGISTERING:
            UpdateRegistering();
            break;
        case SESSION_STATE_STARTING:
            UpdateStarting();
            break;
        case SESSION_STATE_ENDING:
            UpdateEnding();
            break;
        case SESSION_STATE_DELETING:
            UpdateDeleting();
            break;
    }
}


//--------------------------------------------------------------------------------------
// Name: CSession::UpdateCreating()
// Desc: Check for completion of XSessionCreate
//--------------------------------------------------------------------------------------
VOID CSession::UpdateCreating()
{
    // If the overlapped IO is still going, wait
    if( XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        // If there was no error, we're in
        HRESULT hr = XGetOverlappedExtendedError( &m_Overlapped );

        if( FAILED( hr ) ) // tbd -- drop back in state, and display error
        {
            ATG::FatalError(
                "XSessionCreate overlapped task failed with error 0x%08x",
                hr );
        }
        else
        {
            SwitchToState( SESSION_STATE_IDLE );
            m_Parent->SessionNotification( SESSION_NOTIFY_CREATED );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: CSession::UpdateRegistering()
// Desc: Check for completion of XSessionArbitrationRegister
//--------------------------------------------------------------------------------------
VOID CSession::UpdateRegistering()
{
    if( XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        if( SUCCEEDED( XGetOverlappedExtendedError( &m_Overlapped ) ) )
        {
            SwitchToState( SESSION_STATE_REGISTERED );
            m_Parent->SessionNotification( SESSION_NOTIFY_REGISTERED );
        }
        else
        {
            SwitchToState( SESSION_STATE_IDLE );
            m_Parent->SessionNotification( SESSION_NOTIFY_FAIL_REGISTER );
            m_strSessionError = L"Arbitration registration failed.";
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: CSession::UpdateStarting()
// Desc: Check for completion of XSessionStart
//--------------------------------------------------------------------------------------
VOID CSession::UpdateStarting()
{
    if( XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        if( SUCCEEDED( XGetOverlappedExtendedError( &m_Overlapped ) ) )
        {
            // The session is started; start playing
            SwitchToState( SESSION_STATE_IN_GAME );
            m_Parent->SessionNotification( SESSION_NOTIFY_STARTED );
        }
        else
        {
            SwitchToState( SESSION_STATE_IDLE );
            m_Parent->SessionNotification( SESSION_NOTIFY_FAIL_START );
            m_strSessionError = L"XSessionStart() failed.";
        }
    }

}


//--------------------------------------------------------------------------------------
// Name: CSession::UpdateEnding()
// Desc: Check for completion of XSessionEnd
//--------------------------------------------------------------------------------------
VOID CSession::UpdateEnding()
{
    if( XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        if( SUCCEEDED( XGetOverlappedExtendedError( &m_Overlapped ) ) )
        {
            // We're done, move to post-game
            SwitchToState( SESSION_STATE_FINISHED );
            m_Parent->SessionNotification( SESSION_NOTIFY_ENDED );
        }
        else
        {
            SwitchToState( SESSION_STATE_IN_GAME );
            m_Parent->SessionNotification( SESSION_NOTIFY_FAIL_END );
            m_strSessionError = L"XSessionEnd() failed.";
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: CSession::UpdateDeleting()
// Desc: Check for completion of XSessionDelete
//--------------------------------------------------------------------------------------
VOID CSession::UpdateDeleting()
{
    if( XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        CloseHandle( m_hSession );
        m_hSession = INVALID_HANDLE_VALUE;
        SwitchToState( SESSION_STATE_NONE );
        m_Parent->SessionNotification( SESSION_NOTIFY_DELETED );
    }
}
