//--------------------------------------------------------------------------------------
// InGamePartyMatchmakingRender.cpp
//
// Contains methods to update the Sessions UI
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "Xplat.h"


// Disable warning C6211: Leaking memory <pointer> due to an exception.
// We "know" we won't run out of memory in this sample
#pragma warning ( disable : 6211 )


#ifdef _XBOX
//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Show/Hide\nhelp" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle\nleaderboard" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_1, L"Back" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_1, L"Top" },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_1, L"Bottom" },
    { ATG::HELP_LEFT_SHOULDER,  ATG::HELP_PLACEMENT_1, L"Current" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Toggle\ngame mode" },
    { ATG::HELP_RIGHT_STICK,    ATG::HELP_PLACEMENT_2, L"Change Bar\nDisplay" },
};
static const DWORD NUM_HELP_CALLOUTS = sizeof(g_HelpCallouts)/sizeof(g_HelpCallouts[0]);
#endif

//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
#ifdef _XBOX
HRESULT Sample::Render()
#else if LIVE_ON_WINDOWS
VOID Sample::Render()
#endif
{
    HRESULT hr = S_OK;

    m_CXPlat_Draw.BeginDraw();

    #ifdef _XBOX
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_CXPlat_Draw.GetLargeFont(), g_HelpCallouts, NUM_HELP_CALLOUTS );
        return hr;
    }
    #endif

    m_CXPlat_Draw.BeginFont( TRUE );
    m_CXPlat_Draw.SetScaleFactor( 1.2f, 1.2f );
    m_CXPlat_Draw.DrawText( 0, 0, COLOR_TEXT, L"InGamePartyMatchmaking" );
    m_CXPlat_Draw.SetScaleFactor( 1, 1 );
    m_CXPlat_Draw.EndFont( TRUE );

    m_CXPlat_Draw.SetScaleFactor( 1.0f, 1.0f );

    m_CXPlat_Draw.SetXPosition( m_CXPlat_Draw.GetTextWidth( UI_ELEMENT_NOTIFY ) * 2 );
    m_CXPlat_Draw.SetYPosition( m_CXPlat_Draw.GetWindowHeight() - 4 * m_CXPlat_Draw.GetFontHeight() );

    if( m_AppState != APPSTATE_VIEWSTATS )
    {
        const WCHAR* strNotification = L"Notify ";

        //
        // Draw the help text
        //
        // Back
        m_CXPlat_Draw.DrawText( 0, m_CXPlat_Draw.GetYPosition(), COLOR_TEXT, UI_ELEMENT_BACK );
        m_CXPlat_Draw.DrawText( m_CXPlat_Draw.GetXPosition(), m_CXPlat_Draw.GetYPosition(), COLOR_TEXT, L"Back" );

        // Report session errors
        m_CXPlat_Draw.DrawText( m_CXPlat_Draw.GetCenterXPosition() - m_CXPlat_Draw.GetTextWidth( m_wszLastSessionError ) / 2,
                                m_CXPlat_Draw.GetYPosition(),
                                COLOR_HILIGHT,
                                m_wszLastSessionError );

        m_CXPlat_Draw.DecrementYPosition( m_CXPlat_Draw.GetFontHeight() );

        // Notify
        m_CXPlat_Draw.DrawText( 0, m_CXPlat_Draw.GetYPosition(), COLOR_TEXT, UI_ELEMENT_NOTIFY );
        m_CXPlat_Draw.DrawText( m_CXPlat_Draw.GetXPosition(), m_CXPlat_Draw.GetYPosition(), COLOR_TEXT, strNotification );


        m_CXPlat_Draw.DrawText( m_CXPlat_Draw.GetXPosition() + m_CXPlat_Draw.GetTextWidth( strNotification ),
                                m_CXPlat_Draw.GetYPosition(),
                                COLOR_TEXT,
                                m_astrNotificationPosition[ m_nNotificationPosition ] );

        m_CXPlat_Draw.DecrementYPosition( m_CXPlat_Draw.GetFontHeight() );

        // Are we in an Xbox LIVE Party with at least one other member? If so, indicate
        // that they can send game invites to the Xbox LIVE Party. Otherwise, Friends UI
        m_CXPlat_Draw.DrawText( 0, m_CXPlat_Draw.GetYPosition(), COLOR_TEXT, UI_ELEMENT_FRIENDS );
        #ifdef _XBOX
        if( ShouldSendGameInvitesToLiveParty() )
        {
            m_CXPlat_Draw.DrawText( m_CXPlat_Draw.GetXPosition(), m_CXPlat_Draw.GetYPosition(), COLOR_TEXT, L"Invite Xbox LIVE Party" );
        }
        else
        #endif
        if( ShouldSendGameInvitesToFriends() )
        {
            m_CXPlat_Draw.DrawText( m_CXPlat_Draw.GetXPosition(), m_CXPlat_Draw.GetYPosition(), COLOR_TEXT, L"Invite Friends (XInviteSend)" );
        }
        else
        {
            m_CXPlat_Draw.DrawText( m_CXPlat_Draw.GetXPosition(), m_CXPlat_Draw.GetYPosition(), COLOR_TEXT, L"Show Friends List" );
        }

        m_CXPlat_Draw.DecrementYPosition( m_CXPlat_Draw.GetFontHeight() );

        // Give user the option to cancel an overlapped operation if there is one underway
        if( ERROR_IO_INCOMPLETE == XGetOverlappedResult( &m_XSessionOverlapped, NULL, FALSE ) )
        {
            // For now, only session searches can be cancelled
            if( m_AppState == APPSTATE_SEARCH_MATCHMAKING )
            {
                m_CXPlat_Draw.DrawText( 0, m_CXPlat_Draw.GetYPosition(), COLOR_TEXT, UI_ELEMENT_CANCELXOVERLAPPED );
                m_CXPlat_Draw.DrawText( m_CXPlat_Draw.GetXPosition(), m_CXPlat_Draw.GetYPosition(), COLOR_TEXT, L"Cancel overlapped operation" );
                m_CXPlat_Draw.DecrementYPosition( m_CXPlat_Draw.GetFontHeight() );
            }
        }
    }

    if( m_AppState >= APPSTATE_PREGAME )
    {
        // Wave
        m_CXPlat_Draw.DrawText( 0, m_CXPlat_Draw.GetYPosition(), COLOR_TEXT, UI_ELEMENT_WAVE );
        m_CXPlat_Draw.DrawText( m_CXPlat_Draw.GetXPosition(), m_CXPlat_Draw.GetYPosition(), COLOR_TEXT, L"Wave" );

    }

    RenderUserStatus();

    m_CXPlat_Draw.SetScaleFactor( 1, 1 );

    switch( m_AppState )
    {
        case APPSTATE_MAINMENU:
            hr = RenderMainMenu();
            break;

        case APPSTATE_ENUMERATING_PRESENCE:
            hr = RenderEnumeratingPresence();
            break;

        case APPSTATE_UPDATING_RICHPRESENCE:
            hr = RenderUpdatingRichPresence();
            break;

        case APPSTATE_SEARCHUI_MATCHMAKING:
            hr = RenderSearchUI();
            break;

        case APPSTATE_SEARCH_MATCHMAKING:
            hr = RenderSearch();
            break;

        case APPSTATE_SEARCH_MATCHMAKING_DONE:
            hr = RenderSearchDone();
            break;

        case APPSTATE_CREATE_PRESENCE_UI:
            hr = RenderCreatePresenceSessionUI();
            break;

        case APPSTATE_CREATING_SESSION:
            hr = RenderCreatingSession();
            break;

        case APPSTATE_ADDING_LOCAL_PLAYERS:
        case APPSTATE_ADDING_REMOTE_PLAYERS:
            hr = RenderAddPlayers();
            break;

        case APPSTATE_REMOVING_LOCAL_PLAYERS:
        case APPSTATE_REMOVING_REMOTE_PLAYERS:
            hr = RenderRemovePlayers();
            break;

        case APPSTATE_DELETING_SESSION:
            hr = RenderDeletingSession();
            break;

        case APPSTATE_CREATE_MATCHMAKING_UI:
            hr = RenderCreateMatchmakingSessionUI();
            break;

        case APPSTATE_CONNECTING_SESSION:
            hr = RenderConnecting();
            break;

        #ifdef _XBOX
        case APPSTATE_VIEWSTATS:
            hr = RenderViewStats();
            break;
        #endif

        case APPSTATE_PREGAME:
        case APPSTATE_WAITINGFORREGISTRATION:
        case APPSTATE_REGISTERING:
        case APPSTATE_REGISTERED:
        case APPSTATE_STARTING:
        case APPSTATE_INGAME:
        case APPSTATE_ENDING:
        case APPSTATE_POSTGAME:
            hr = RenderInSession();
            break;
        case APPSTATE_HOSTMIGRATION:
            m_HostMigration.RenderHostMigration();
            break;

        case APPSTATE_CANCELLING_OVERLAPPED:
            hr = RenderCancellingOverlapped();
            break;
    }

    m_CXPlat_Draw.EndDraw();

    #ifdef _XBOX
    return hr;
    #endif
}


//--------------------------------------------------------------------------------------
// Name: RenderMainMenu()
// Desc: Render the main menu
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderMainMenu()
{
    static const WCHAR* astrItems[] =
    {
        L"Create a presence session",
        L"Create a matchmaking session",
        L"Search for a matchmaking session",
        #ifdef _XBOX
        L"View stats",
        #endif
        L"Changed logged-in users",
        L"Delete presence session",
        #ifdef _XBOX
        L"Find Xbox LIVE Party Sessions",
        #endif
        L"Send Game Invites To Party",
    };

    m_CXPlat_Draw.SetYPosition( m_CXPlat_Draw.GetCenterYPosition() - ( MAINMENU_MAX * 2 - 1 ) * m_CXPlat_Draw.GetFontHeight() / 2 );

    for( UINT i = 0; i < MAINMENU_MAX; ++i )
    {
        D3DCOLOR col = ( m_nMenuItem == i ) ? COLOR_HILIGHT : COLOR_TEXT;

        if( i == MAINMENU_CREATE_PRESENCE && GetPresenceSession() )
        {
            col = COLOR_GRAY;
        }

        if( i == MAINMENU_DELETE_PRESENCE && !GetPresenceSession() )
        {
            col = COLOR_GRAY;
        }

        m_CXPlat_Draw.DrawTextCentered( m_CXPlat_Draw.GetYPosition(),
                                        col,
                                        astrItems[i] );
        
        m_CXPlat_Draw.IncrementYPosition( m_CXPlat_Draw.GetFontHeight() * 2 );

    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RenderUserStatus()
// Desc: Renders user status, including signin info and activity state for local users.
//       Also include Presence session data for local and remote party members
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderUserStatus()
{
    SessionManager* pPresenceSessionMgr = GetPresenceSession();

    static const WCHAR* astrItems[] =
    {
        L" (Local)",
        L" (Live)",
    };

    static const WCHAR* strSignin            = L"<sign in>";
    static const WCHAR* strIdle              = L" (idle)";
    static const WCHAR* strInPartyHostOwner  = L" (PHO)";
    static const WCHAR* strInPartyHost       = L" (PH)";
    static const WCHAR* strInPartyOwner      = L" (PO)";
    static const WCHAR* strInParty           = L" (P)";
    static const WCHAR* strInPartyHostRemote = L" (PHR)";
    static const WCHAR* strInPartyRemote     = L" (PR)";
    static WCHAR str[128];

    // Scale down the font proportionally by 1/4 for rendering user status
    m_CXPlat_Draw.SetScaleFactor( 0.75f, 0.75f );

    m_CXPlat_Draw.SetXPosition( 0 );
    m_CXPlat_Draw.SetYPosition( m_CXPlat_Draw.GetFontHeight() * 1.5f );
    

    for( UINT i = 0; i < m_Local.cPlayers; ++i )
    {
        const XUSER_SIGNIN_INFO& info = m_SignInInfo[ i ];

        swprintf_s( str, L"%d: ", i );

        D3DCOLOR col = LIGHTGREY;

        // Is user signed into Live or signed in locally?
        if( info.UserSigninState != eXUserSigninState_NotSignedIn  )
        {
            wcscat_s( str, m_Local.strGamertags[ i ] );

            // If user is idle, then grey out name and append idle text
            if( m_Local.bIsIdle[ i ] )
            {
                col = GREY;
                wcscat_s( str, strIdle );
            }
            else // User not idle
            {
                col = GREEN;
            }

            // Append signin state depending if signed in locally or into Live
            if( info.UserSigninState == eXUserSigninState_SignedInToLive  )
            {
                // Signed into Live
                wcscat_s( str, astrItems[1] );
            }
            else
            {
                // Signed in locally
                wcscat_s( str, astrItems[0] );
            }

            // Is user in a party?
            if( pPresenceSessionMgr != NULL && m_Local.presenceSessionNonces[ i ] )
            {
                if( pPresenceSessionMgr->IsSessionHost() &&
                    pPresenceSessionMgr->GetSessionOwner() == m_Local.nController[ i ] )
                {
                    wcscat_s( str, strInPartyHostOwner );
                }
                else if( pPresenceSessionMgr->IsSessionHost() )
                {
                    wcscat_s( str, strInPartyHost );
                }
                else if( pPresenceSessionMgr->GetSessionOwner() == m_Local.nController[ i ] )
                {
                    wcscat_s( str, strInPartyOwner );
                }
                else
                {
                    wcscat_s( str, strInParty );
                }
            }
        }
        else
        {
            // No user signed in
            wcscat_s( str, strSignin );
            col = LIGHTGREY;
        }        

        m_CXPlat_Draw.DrawTextFromCurrentXY( col, str, DRAW_LEFT_STYLE );
        m_CXPlat_Draw.IncrementYPosition( m_CXPlat_Draw.GetFontHeight() );
        
    }

    // Render remote Presence session members info
    if( !pPresenceSessionMgr )
    {
        goto Terminate;
    }

    D3DCOLOR col = GREEN;
    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        if( !i->IsInSession( pPresenceSessionMgr->GetSessionID() ) )
        {
            continue;
        }

        for ( UINT j = 0; j < i->cPlayers; j++ )
        {
            // Gamertag
            swprintf_s( str, i->strGamertags[j] );

            // Presence
            const IN_ADDR hostInaddr = pPresenceSessionMgr->GetHostInAddr();
            const BOOL bIsSessionHost = ( hostInaddr == i->addr );

            if( bIsSessionHost )
            {
                wcscat_s( str, strInPartyHostRemote );
            }
            else
            {
                wcscat_s( str, strInPartyRemote );
            }
        }

        m_CXPlat_Draw.DrawTextFromCurrentXY( col, str, DRAW_LEFT_STYLE );
        m_CXPlat_Draw.IncrementYPosition( m_CXPlat_Draw.GetFontHeight() );
    }

Terminate:
    //Restore font scale factors
    m_CXPlat_Draw.SetScaleFactor( 1.0f, 1.0f );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderSearchUI()
// Desc: Render the game search menu
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderSearchUI()
{
    static const WCHAR* astrItems[] =
    {
        L"Game Type",
        L"Game Mode",
        L"Map",
        L"Min. Victory Points",
        L"Max. Victory Points",
        L"Search"
    };

    m_CXPlat_Draw.SetXPosition( m_CXPlat_Draw.GetCenterXPosition() );
    m_CXPlat_Draw.SetYPosition( m_CXPlat_Draw.GetCenterYPosition() - 
        ( SEARCHMENU_MAX * 2 - 1 ) * m_CXPlat_Draw.GetFontHeight() / 2 );

    for( UINT i = 0; i < SEARCHMENU_MAX; ++i )
    {
        WCHAR wchRenderString[ 256 ];
        D3DCOLOR col = ( m_nMenuItem == i ) ? COLOR_HILIGHT : COLOR_TEXT;

        switch( i )
        {
        case SEARCHMENU_GAMETYPE:
            swprintf_s( wchRenderString,
                L"%s:  %s %s %s",
                astrItems[ i ],
                ( m_nMenuItem == i ) ? LEFT_ARROW  : L" ",
                m_astrGameTypes[ m_nGameType ],
                ( m_nMenuItem == i ) ? RIGHT_ARROW : L" " );
            break;

        case SEARCHMENU_GAMEMODE:
            swprintf_s( wchRenderString,
                L"%s:  %s %s %s",
                astrItems[ i ],
                ( m_nMenuItem == i ) ? LEFT_ARROW  : L" ",
                m_astrGameModes[ m_nGameMode ],
                ( m_nMenuItem == i ) ? RIGHT_ARROW : L" " );
            break;

        case SEARCHMENU_MAP:
            swprintf_s( wchRenderString,
                L"%s:  %s %s %s",
                astrItems[ i ],
                ( m_nMenuItem == i ) ? LEFT_ARROW  : L" ",
                m_astrMaps[ m_nMap ],
                ( m_nMenuItem == i ) ? RIGHT_ARROW : L" " );
            break;

        case SEARCHMENU_MINVICTORYPOINTS:
            swprintf_s( wchRenderString,
                L"%s:  %s %d %s",
                astrItems[ i ],
                ( m_nMenuItem == i && m_nMinVictoryPoints > VICTORY_POINTS_MIN ) ?
                    LEFT_ARROW : L" ",
                m_nMinVictoryPoints,
                ( m_nMenuItem == i && m_nMinVictoryPoints < m_nMaxVictoryPoints ) ?
                    RIGHT_ARROW : L" " );
            break;

        case SEARCHMENU_MAXVICTORYPOINTS:
            swprintf_s( wchRenderString,
                L"%s:  %s %d %s",
                astrItems[ i ],
                ( m_nMenuItem == i && m_nMaxVictoryPoints > m_nMinVictoryPoints ) ?
                    LEFT_ARROW : L" ",
                m_nMaxVictoryPoints,
                ( m_nMenuItem == i && m_nMaxVictoryPoints < VICTORY_POINTS_MAX ) ?
                    RIGHT_ARROW : L" " );
            break;

        case SEARCHMENU_SEARCH:
            wcscpy_s( wchRenderString, astrItems[ i ] );
            break;
        }

        m_CXPlat_Draw.DrawTextCentered( m_CXPlat_Draw.GetYPosition(), 
                                        col, 
                                        wchRenderString );

        m_CXPlat_Draw.IncrementYPosition( m_CXPlat_Draw.GetFontHeight() * 2) ;

    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RenderCancellingOverlapped()
// Desc: Render the screen when cancelling an overlapped operation
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderCancellingOverlapped()
{
    switch( m_AppState )
    {
    case APPSTATE_CREATING_SESSION:
        m_CXPlat_Draw.DrawTextCenterScreen( COLOR_TEXT, L"Cancelling creating session..." );
        break;
    case APPSTATE_SEARCH_MATCHMAKING:
        m_CXPlat_Draw.DrawTextCenterScreen( COLOR_TEXT, L"Cancelling searching for sessions..." );
        break;
    case APPSTATE_STARTING:
        m_CXPlat_Draw.DrawTextCenterScreen( COLOR_TEXT, L"Cancelling starting session..." );
        break;
    case APPSTATE_ENDING:
        m_CXPlat_Draw.DrawTextCenterScreen( COLOR_TEXT, L"Cancelling ending session..." );
        break;
    case APPSTATE_DELETING_SESSION:
        m_CXPlat_Draw.DrawTextCenterScreen( COLOR_TEXT, L"Cancelling session deletion..." );
        break;

    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderSearch()
// Desc: Render the screen when searching for a match
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderSearch()
{
    m_CXPlat_Draw.DrawTextCenterScreen( COLOR_TEXT, L"Searching..." );    

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RenderSearchDone()
// Desc: Render the screen when done searching for a match
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderSearchDone()
{
    // Text buffer
    WCHAR strRender[128] = L"";

    // Columns
    enum
    {
        COLUMN_SESSIONID,
        COLUMN_VICTORYPOINTS,
        COLUMN_GAMEMODE,
        COLUMN_PUBLICSLOTS,
        COLUMN_PRIVATESLOTS,
        COLUMN_CUSTOMINFO,
        COLUMN_MAX
    };

    if( m_pSearchResults->dwSearchResults == 0 )
    {
        // If we've got no results, report that too
        m_CXPlat_Draw.DrawTextCenterScreen( COLOR_TEXT, L"No Sessions found" );    
    }
    else
    {
        // Otherwise, we have results. Render them.
        m_CXPlat_Draw.SetScaleFactor( 0.60f, 0.60f );
        
        // Column headers
        static const WCHAR* aHeaders[] =
        {
            L"------ Session ID ------ ",
            L"V.P.",
            L"Game Mode ",
            L"Pub O/F ",
            L"Pri O/F ",
            L"QoS [minRTT medRTT][xmit recv][up down][host contacted?] "
        };

        // Roughly center the results vertically
        m_CXPlat_Draw.SetYPosition( m_CXPlat_Draw.GetCenterYPosition() - 
            ( ( m_pSearchResults->dwSearchResults + 3 ) * ( m_CXPlat_Draw.GetFontHeight() + 2 ) ) / 2 );

        // Loop through the rows
        for( INT row = -1; row < (INT)m_pSearchResults->dwSearchResults; ++row )
        {
            m_CXPlat_Draw.SetXPosition( 20 );

            // get a reference to the current result for readability
            const XSESSION_SEARCHRESULT* pResult = ( row != -1 ) ? &m_pSearchResults->pResults[ row ] : NULL;
            const  __int64 sessionIDAsInt = ( row != -1 ) ? XNKIDToInt64( pResult->info.sessionID ) : 0;

            // get a reference to this result's QoS info
            const XNQOSINFO* pQosInfo = ( row != -1 ) ? &m_pQoSResult->axnqosinfo[ row ] : NULL;

            for( INT col = 0; col < COLUMN_MAX; ++col )
            {
                // Row -1 == headers
                if( row == -1 )
                {
                    wcscpy_s( strRender, aHeaders[ col ] );
                }
                else
                {
                    const XSESSION_SEARCHRESULT& Result = *pResult;
                    const XNQOSINFO& qosInfo = *pQosInfo;
                    const BOOL bHostContacted = ( qosInfo.bFlags & XNET_XNQOSINFO_TARGET_CONTACTED ); 

                    // Locate the columns in the results
                    DWORD nVictoryPointsCol, nGameModeCol;

                    for( nVictoryPointsCol = 0; nVictoryPointsCol < Result.cProperties; nVictoryPointsCol++ )
                    {
                        if( Result.pProperties[ nVictoryPointsCol ].dwPropertyId == PROPERTY_VICTORY_POINTS )
                            break;
                    }

                    for( nGameModeCol = 0; nGameModeCol < Result.cContexts; nGameModeCol++ )
                    {
                        if( Result.pContexts[ nGameModeCol ].dwContextId == X_CONTEXT_GAME_MODE )
                            break;
                    }

                    // find out which column we're rendering
                    switch( col )
                    {
                        case COLUMN_SESSIONID:
                            // Display the Session ID in its byte order, XBox 360 processor is Big Endian
                            swprintf_s( strRender, 
                                L"%016I64X", sessionIDAsInt );
                            break;

                        case COLUMN_VICTORYPOINTS:
                            if( nVictoryPointsCol == Result.cProperties )
                            {
                                wcscpy_s( strRender, L"???" );
                            }
                            else
                            {
                                swprintf_s( strRender,
                                    L"%d", Result.pProperties[ nVictoryPointsCol ].value.nData );
                            }
                            break;

                        case COLUMN_GAMEMODE:
                            wcscpy_s( strRender,
                                nGameModeCol == Result.cContexts ? L"???" :
                                m_astrGameModes[ 
                                    Result.pContexts[ nGameModeCol ].dwValue
                                        ] );
                            break;

                        case COLUMN_PUBLICSLOTS:
                            swprintf_s( strRender,
                                L"%d/%d", 
                                Result.dwOpenPublicSlots, 
                                Result.dwFilledPublicSlots);
                            break;

                        case COLUMN_PRIVATESLOTS:
                            swprintf_s( strRender,
                                L"%d/%d", 
                                Result.dwOpenPrivateSlots, 
                                Result.dwFilledPrivateSlots);
                            break;

                        case COLUMN_CUSTOMINFO:
                            if( m_pQoSResult && m_pQoSResult->axnqosinfo[ row ].bFlags & XNET_XNQOSINFO_COMPLETE )
                            {
                                WORD nBytes = m_pQoSResult->axnqosinfo[ row ].cbData;
                                WORD nWords = nBytes / sizeof( WORD );
                                if( nBytes > 0 )
                                {
                                    WCHAR* chTemp = new WCHAR[ nWords + 1 ];
                                    for( UINT nIter = 0; nIter < nBytes; nIter++)
                                    {
                                        if( m_pQoSResult->axnqosinfo[ row ].pbData == NULL )
                                        {
                                            continue;
                                        }
                                        ((BYTE*)chTemp)[ nIter ] =
                                            m_pQoSResult->axnqosinfo[ row ].pbData[ nIter ];
                                    }
                                    chTemp[ nWords ] = 0L;
                                    swprintf_s( strRender,
                                                L"%s [%d    %d][%d    %d][%d    %d][%c]",
                                                chTemp,
                                                m_pQoSResult->axnqosinfo[row].wRttMinInMsecs,
                                                m_pQoSResult->axnqosinfo[row].wRttMedInMsecs,
                                                m_pQoSResult->axnqosinfo[row].cProbesXmit,
                                                m_pQoSResult->axnqosinfo[row].cProbesRecv,
                                                m_pQoSResult->axnqosinfo[row].dwUpBitsPerSec,
                                                m_pQoSResult->axnqosinfo[row].dwDnBitsPerSec,
                                                ( bHostContacted ) ? 'Y' : 'N' );
                                }
                            }
                            break;
                    }
                }

                // Render the column and increment the x-position
                D3DCOLOR color = ( (INT)m_nMenuItem == row ) ? COLOR_HILIGHT : COLOR_TEXT;

                m_CXPlat_Draw.DrawTextFromCurrentXY( color, strRender );
                m_CXPlat_Draw.IncrementXPosition( m_CXPlat_Draw.GetTextWidth( aHeaders[col] ) * 1.2f + 5 );

            } // for (column)

            // Increment the y-position
            m_CXPlat_Draw.IncrementYPosition( m_CXPlat_Draw.GetFontHeight() + 2 );
        } // for ( row )

        m_CXPlat_Draw.IncrementYPosition( m_CXPlat_Draw.GetFontHeight() + 2 );

        // display the result count
        swprintf_s( strRender, L"%d result%s returned.",
                    m_pSearchResults->dwSearchResults,
                    m_pSearchResults->dwSearchResults == 1 ? L"" : L"s" );

        m_CXPlat_Draw.DrawText( 2, m_CXPlat_Draw.GetYPosition(), COLOR_TEXT, strRender );

        // Restore font scale factor
        m_CXPlat_Draw.SetScaleFactor( 1.0, 1.0 );        
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RenderCreateMatchmakingSessionUI()
// Desc: Render the matchmaking session creation menu
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderCreateMatchmakingSessionUI()
{
    // We need a session manager instance while we navigate this UI.
    // Use m_pSessionMgrCtx if it's non-NULL and not in any state other than
    // SessionStateNone
    SessionManager* pMatchmakingSessionMgr = NULL;
    if( m_pSessionMgrCtx &&
        m_pSessionMgrCtx->GetSessionState() == SessionStateNone )
    {
        pMatchmakingSessionMgr = m_pSessionMgrCtx;
    }
    else
    {
        pMatchmakingSessionMgr = new SessionManager();
        m_pSessionMgrCtx = pMatchmakingSessionMgr;
    }

    pMatchmakingSessionMgr = m_pSessionMgrCtx;

    const SessionManager* const pPresenceSessionMgr = GetPresenceSession();

    static const WCHAR* astrItems[] =
    {
        L"Game type",
        L"Game mode",
        L"Map",
        L"Victory points",
        L"Allow invitations",
        L"Allow join-via-presence",
        L"Allow join-in-progress",
        L"Create"
    };
    
    m_CXPlat_Draw.SetXPosition( m_CXPlat_Draw.GetCenterXPosition() );
    m_CXPlat_Draw.SetYPosition( m_CXPlat_Draw.GetCenterYPosition() - ( CREATE_MATCHMAKING_MENU_MAX * 2 - 1 ) * m_CXPlat_Draw.GetFontHeight() / 2 );

    for( UINT i = 0; i < CREATE_MATCHMAKING_MENU_MAX; ++i )
    {
        WCHAR wchRenderString[ 256 ];
        D3DCOLOR col = ( m_nMenuItem == i ) ? COLOR_HILIGHT : COLOR_TEXT;

        switch( i )
        {
        case CREATE_MATCHMAKING_MENU_GAMETYPE:
            swprintf_s( wchRenderString,
                L"%s:  %s %s %s",
                astrItems[ i ],
                ( m_nMenuItem == i ) ? LEFT_ARROW  : L" ",
                m_astrGameTypes[ m_nGameType ],
                ( m_nMenuItem == i ) ? RIGHT_ARROW : L" " );
            break;

        case CREATE_MATCHMAKING_MENU_GAMEMODE:
            swprintf_s( wchRenderString,
                L"%s:  %s %s %s",
                astrItems[ i ],
                ( m_nMenuItem == i ) ? LEFT_ARROW  : L" ",
                m_astrGameModes[ m_nGameMode ],
                ( m_nMenuItem == i ) ? RIGHT_ARROW : L" " );
            break;

        case CREATE_MATCHMAKING_MENU_MAP:
            swprintf_s( wchRenderString,
                L"%s:  %s %s %s",
                astrItems[ i ],
                ( m_nMenuItem == i ) ? LEFT_ARROW  : L" ",
                m_astrMaps[ m_nMap ],
                ( m_nMenuItem == i ) ? RIGHT_ARROW : L" " );
            break;

        case CREATE_MATCHMAKING_MENU_VICTORYPOINTS:
            swprintf_s( wchRenderString,
                L"%s:  %s %d %s",
                astrItems[ i ],
                ( m_nMenuItem == i && m_nVictoryPoints > VICTORY_POINTS_MIN ) ?
                    LEFT_ARROW : L" ",
                m_nVictoryPoints,
                ( m_nMenuItem == i && m_nVictoryPoints < VICTORY_POINTS_MAX ) ?
                    RIGHT_ARROW : L" " );
            break;

        case CREATE_MATCHMAKING_MENU_INVITES:
            {
                BOOL bInvitesDisabled = ( pMatchmakingSessionMgr->HasSessionFlags( XSESSION_CREATE_INVITES_DISABLED ) );
                if( pPresenceSessionMgr )
                {
                    col = COLOR_GRAY;
                    bInvitesDisabled = ( pPresenceSessionMgr->HasSessionFlags( XSESSION_CREATE_INVITES_DISABLED ) );
                }
                swprintf_s( wchRenderString,
                    L"%s: %s %s %s",
                    astrItems[ i ],
                    m_nMenuItem == i ? LEFT_ARROW : L" ",
                    ( bInvitesDisabled ) ? L"NO" : L"YES",
                    m_nMenuItem == i ? RIGHT_ARROW : L" " );
            }
            break;

        case CREATE_MATCHMAKING_MENU_JOINVIAPRESENCE:
            {
                BOOL bJoinViaPresenceDisabled = ( pMatchmakingSessionMgr->HasSessionFlags( XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED ) );
                if( pPresenceSessionMgr )
                {
                    col = COLOR_GRAY;
                    bJoinViaPresenceDisabled = ( pPresenceSessionMgr->HasSessionFlags( XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED ) );
                }
                swprintf_s( wchRenderString,
                    L"%s: %s %s %s",
                    astrItems[ i ],
                    m_nMenuItem == i ? LEFT_ARROW : L" ",
                    ( bJoinViaPresenceDisabled ) ? L"NO" : L"YES",
                    m_nMenuItem == i ? RIGHT_ARROW : L" " );
            }
            break;

        case CREATE_MATCHMAKING_MENU_JOININPROGRESS:
            {
                BOOL bJoinInProgressDisabled = ( pMatchmakingSessionMgr->HasSessionFlags( XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED ) );
                if( pPresenceSessionMgr )
                {
                    col = COLOR_GRAY;
                    bJoinInProgressDisabled = ( pPresenceSessionMgr->HasSessionFlags( XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED ) );
                }
                swprintf_s( wchRenderString,
                    L"%s: %s %s %s",
                    astrItems[ i ],
                    m_nMenuItem == i ? LEFT_ARROW : L" ",
                    ( bJoinInProgressDisabled ) ? L"NO" : L"YES",
                    m_nMenuItem == i ? RIGHT_ARROW : L" " );
            }
            break;

        case CREATE_MATCHMAKING_MENU_CREATE:
            wcscpy_s( wchRenderString, astrItems[ i ] );
            break;
        }

        m_CXPlat_Draw.DrawTextCentered( m_CXPlat_Draw.GetYPosition(), col, wchRenderString );
        m_CXPlat_Draw.IncrementYPosition( m_CXPlat_Draw.GetFontHeight() * 3 / 2);
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RenderCreatePresenceSessionUI()
// Desc: Render the presence session creation menu
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderCreatePresenceSessionUI()
{
    // We need a session manager instance while we navigate this UI.
    // Use m_pSessionMgrCtx if it's non-NULL and not in any state other than
    // SessionStateNone
    SessionManager* pPresenceSessionMgr = NULL;
    if( m_pSessionMgrCtx &&
        m_pSessionMgrCtx->GetSessionState() == SessionStateNone )
    {
        pPresenceSessionMgr = m_pSessionMgrCtx;
    }
    else
    {
        pPresenceSessionMgr = new SessionManager();
        m_pSessionMgrCtx = pPresenceSessionMgr;
    }

    pPresenceSessionMgr = m_pSessionMgrCtx;

    static const WCHAR* astrItems[] =
    {
        L"Allow invitations",
        L"Allow join-via-presence",
        L"Allow join-in-progress",
        L"Create"
    };

    m_CXPlat_Draw.SetXPosition( m_CXPlat_Draw.GetCenterXPosition() );
    m_CXPlat_Draw.SetYPosition( m_CXPlat_Draw.GetCenterYPosition() - ( CREATE_PRESENCE_MENU_MAX * 2 - 1 ) * m_CXPlat_Draw.GetFontHeight() / 2 );

    for( UINT i = 0; i < CREATE_PRESENCE_MENU_MAX; ++i )
    {
        WCHAR wchRenderString[ 256 ];
        D3DCOLOR col = ( m_nMenuItem == i ) ? COLOR_HILIGHT : COLOR_TEXT;

        switch( i )
        {
        case CREATE_PRESENCE_MENU_INVITES:
            swprintf_s( wchRenderString,
                L"%s: %s %s %s",
                astrItems[ i ],
                m_nMenuItem == i ? LEFT_ARROW : L" ",
                ( pPresenceSessionMgr->HasSessionFlags( XSESSION_CREATE_INVITES_DISABLED ) ) ? L"NO" : L"YES",
                m_nMenuItem == i ? RIGHT_ARROW : L" " );
            break;

        case CREATE_PRESENCE_MENU_JOINVIAPRESENCE:
            swprintf_s( wchRenderString,
                L"%s: %s %s %s",
                astrItems[ i ],
                m_nMenuItem == i ? LEFT_ARROW : L" ",
                ( pPresenceSessionMgr->HasSessionFlags( XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED ) ) ? L"NO" : L"YES",
                m_nMenuItem == i ? RIGHT_ARROW : L" " );
            break;

        case CREATE_PRESENCE_MENU_JOININPROGRESS:
            swprintf_s( wchRenderString,
                L"%s: %s %s %s",
                astrItems[ i ],
                m_nMenuItem == i ? LEFT_ARROW : L" ",
                ( pPresenceSessionMgr->HasSessionFlags( XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED ) ) ? L"NO" : L"YES",
                m_nMenuItem == i ? RIGHT_ARROW : L" " );
            break;

        case CREATE_PRESENCE_MENU_CREATE:
            wcscpy_s( wchRenderString, astrItems[ i ] );
            break;
        }

        m_CXPlat_Draw.DrawTextCentered( m_CXPlat_Draw.GetYPosition(), col, wchRenderString );
        m_CXPlat_Draw.IncrementYPosition( m_CXPlat_Draw.GetFontHeight() * 3 / 2);
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderCenteredText()
// Desc: Render text in the middle of the screen
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderCenteredText( const WCHAR* pwszText )
{    
    m_CXPlat_Draw.DrawTextCenterScreen( COLOR_TEXT, pwszText );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RenderUpdatingRichPresence()
// Desc: Render the screen while rich presence is being updated
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderUpdatingRichPresence()
{
    // If we're still creating, say so
    return RenderCenteredText( L"Updating rich presence..." );
}

//--------------------------------------------------------------------------------------
// Name: RenderEnumeratingPresence()
// Desc: Render the screen while presence is being enumerated
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderEnumeratingPresence()
{
    // If we're still creating, say so
    return RenderCenteredText( L"Enumerating presence..." );
}

//--------------------------------------------------------------------------------------
// Name: RenderCreatingSession()
// Desc: Render the screen while new session is being created
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderCreatingSession()
{
    // If we're still creating, say so
    return RenderCenteredText( L"Creating session..." );
}


//--------------------------------------------------------------------------------------
// Name: RenderDeletingSession()
// Desc: Render the screen while new presence session is being deleted
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderDeletingSession()
{
    // If we're still deleting, say so
    return RenderCenteredText( L"Deleting session..." );
}


//--------------------------------------------------------------------------------------
// Name: RenderAddPlayers()
// Desc: Render the screen while players are added to the session
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderAddPlayers()
{
    // If we're still adding, say so
    return RenderCenteredText( L"Adding players to session..." );
}


//--------------------------------------------------------------------------------------
// Name: RenderRemovePlayers()
// Desc: Render the screen while players are removed from the session
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderRemovePlayers()
{
    // If we're still adding, say so
    return RenderCenteredText( L"Removing players from session..." );
}


//--------------------------------------------------------------------------------------
// Name: RenderConnecting()
// Desc: Render the screen when attempting to join a session
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderConnecting()
{
    // If we're still connecting, say so
    return RenderCenteredText( L"Connecting to session..." );
}


//--------------------------------------------------------------------------------------
// Name: RenderInSession()
// Desc: Render the screen when in a session
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderInSession()
{
    SessionManager* pSessionMgr = GetMatchmakingSession();
    if( !pSessionMgr )
    {
        DebugSpew( "RenderInSession:: No matchmaking session active\n" );
        return S_OK;
    }

    DWORD filledPublicSlots, filledPrivateSlots, maxPublicSlots, maxPrivateSlots;
    pSessionMgr->GetFilledSlotCounts( filledPublicSlots, filledPrivateSlots );
    pSessionMgr->GetMaxSlotCounts( maxPublicSlots, maxPrivateSlots );

    const XNKID sessionID           = pSessionMgr->GetSessionID();
    const __int64 sessionIDAsInt    = pSessionMgr->GetSessionIDAsInt();
    const BOOL bIsHost              = pSessionMgr->IsSessionHost();
    ClientInfo* pHostMatchmaking    = SessionHostFromSessionID( sessionID );

    // Text buffer
    WCHAR strRender[128] = L"";

    // Draw the players in the game
    FLOAT x1 = 10;                                          // Column 1
    FLOAT x2 = m_CXPlat_Draw.GetCenterXPosition() + 10;     // Column 2
    FLOAT y1 =  0;                                          // y-increment
    m_CXPlat_Draw.SetXPosition( x1 );                       // Current x-pos
    m_CXPlat_Draw.SetYPosition( 50 );                       // Current y-pos

    // Draw the header

    // Display the Session ID in its byte order
    swprintf_s( strRender, 
                L"%s (SID:%016I64X)\n %d/%d public slots, %d/%d private slots", 
                bIsHost ? L"HOSTING" : L"IN SESSION",                       
                sessionIDAsInt,
                filledPublicSlots,
                maxPublicSlots,
                filledPrivateSlots,
                maxPrivateSlots );
    
    m_CXPlat_Draw.DrawTextCentered( m_CXPlat_Draw.GetYPosition(), COLOR_HILIGHT, strRender );
    m_CXPlat_Draw.IncrementYPosition( m_CXPlat_Draw.GetFontHeight() * 2);

    // Draw the current status message
    switch( m_AppState )
    {
    case APPSTATE_PREGAME:
        if( bIsHost )
        {
            if( m_vecRemote.empty() && m_nGameType == X_CONTEXT_GAME_TYPE_RANKED )
            {
                wcscpy_s( strRender, L"Waiting for other players to join" );
            }
            else
            {
                wcscpy_s( strRender, UI_TEXT_INSESSION_BEGINGAME );
            }
        }
        else
        {
            wcscpy_s( strRender, L"Waiting for host to start game" );
        }
        break;

    case APPSTATE_WAITINGFORREGISTRATION:
        swprintf_s( strRender, 
            L"Waiting for clients to register, %.1f seconds remaining",
            ( REGISTRATION_TIME - ( GetTickCount() - m_dwRegistrationTimer ) )/ 1000.0f );
        break;

    case APPSTATE_REGISTERING:
        wcscpy_s( strRender, L"Registering for arbitration" );
        break;

    case APPSTATE_REGISTERED:
        wcscpy_s( strRender, L"Waiting for other players" );
        break;

    case APPSTATE_STARTING:
        wcscpy_s( strRender, L"Starting the session" );
        break;

    case APPSTATE_INGAME:
        wcscpy_s( strRender, UI_TEXT_INSESSION_SCOREPOINT );
        break;

    case APPSTATE_WRITINGSTATS:
        wcscpy_s( strRender, L"Writing session stats" );
        break;

    case APPSTATE_ENDING:
        wcscpy_s( strRender, L"Ending the session" );
        break;

    case APPSTATE_POSTGAME:
        wcscpy_s( strRender, L"Game over" );
        if( !pHostMatchmaking )
        {
            wcscpy_s( strRender, L"(Host gone)" );
        }
        break;

    }

    m_CXPlat_Draw.DrawTextCentered( m_CXPlat_Draw.GetYPosition(), COLOR_HILIGHT, strRender );
    m_CXPlat_Draw.IncrementYPosition( m_CXPlat_Draw.GetFontHeight() * 2 );

    // Draw the local machine
    y1 = RenderClient( L"LOCAL:", 
                       m_CXPlat_Draw.GetXPosition(), 
                       m_CXPlat_Draw.GetYPosition(), 
                       &m_Local, 
                       ( bIsHost ) ? NULL : pHostMatchmaking );

    // If we have an error, exit
    if( pSessionMgr->GetSessionError() )
    {
        return S_OK;
    }

    // If we're not the host, draw the host
    if( !bIsHost )
    {
        if( pHostMatchmaking )
        {
            m_CXPlat_Draw.SetXPosition( (x2 + x1) - m_CXPlat_Draw.GetXPosition() ); // Toggle column
            y1 = max( y1, RenderClient( L"HOST:", 
                                        m_CXPlat_Draw.GetXPosition(), 
                                        m_CXPlat_Draw.GetYPosition(), 
                                        pHostMatchmaking, 
                                        NULL )
                    );
        }
    }

    // Draw the remote clients
    UINT nRemote = 1;

    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); ++i )
    {
        if( &(*i) == pHostMatchmaking ) 
        {
            // Don't draw the host again
            continue;
        }

        // Is the client in the Matchmaking session? If not, don't render
        if( !i->IsInSession( pSessionMgr->GetSessionID() ) )
        {
            continue;
        }

        ClientInfo& Remote = *i;
        swprintf_s( strRender, L"REMOTE %d:", nRemote++ );

        m_CXPlat_Draw.SetXPosition( (x2 + x1) - m_CXPlat_Draw.GetXPosition() ); // Toggle column

        if( m_CXPlat_Draw.GetXPosition() == x1 )      // if new column, increment y-pos
        {
            m_CXPlat_Draw.IncrementYPosition( y1 );
            y1 = 0;
        }

        y1 = max( y1, RenderClient( strRender, 
                                    m_CXPlat_Draw.GetXPosition(), 
                                    m_CXPlat_Draw.GetYPosition(), 
                                    &Remote, 
                                    &m_Local ) );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderClient()
// Desc: Render the people on a client
//--------------------------------------------------------------------------------------
FLOAT Sample::RenderClient( WCHAR* strLabel, FLOAT x, FLOAT y, ClientInfo* pClient, ClientInfo *pHost )
{
    DWORD dwTick = GetTickCount();

    // Render the label
    m_CXPlat_Draw.DrawText( x, y, COLOR_TEXT, strLabel );

    // Since we are drawing a TrueSkill(TM) skill bar, the font size has to be reduced to fit the screen 
    m_CXPlat_Draw.SetScaleFactor( 0.7f, 0.7f );

    // Render the gamertags
    for( UINT i = 0; i < pClient->cPlayers; ++i )
    {
        D3DCOLOR col = COLOR_TEXT;

        // If the player has won the game, draw him in a highlight color
        if( m_AppState == APPSTATE_POSTGAME && 
            pClient->nPoints[ i ] == m_nVictoryPoints )
        {
            col = COLOR_HEADER;
        }

        // If the player is waving, maybe draw highlighted
        if( ( dwTick - pClient->dwWave[ i ] ) < WAVE_TIME )
        {
            // In terval from blink-to-blink is equal to
            // WAVE_TIME / WAVE_BLINKS. Draw highlighted if
            // in the first half of that interval
            DWORD dwTickIntervalElapsed = 
                ( dwTick - pClient->dwWave[ i ] ) % ( WAVE_TIME / WAVE_BLINKS );

            if( dwTickIntervalElapsed / ( ( WAVE_TIME / WAVE_BLINKS ) / 2 ) )
            {
                col = COLOR_HILIGHT;
            }
        }

        FLOAT yPos = m_CXPlat_Draw.GetYPosition() + ( i + 1 ) * m_CXPlat_Draw.GetFontHeight();

        // If we're in the game, draw the player's score
        if( m_AppState == APPSTATE_INGAME || m_AppState == APPSTATE_POSTGAME )
        {
            WCHAR wstrScore[ 10 ];

            swprintf_s( wstrScore, L"%2d", pClient->nPoints[ i ] );

            m_CXPlat_Draw.DrawText( m_CXPlat_Draw.GetXPosition() + 2,
                                    yPos,
                                    col,
                                    wstrScore );
        }

        // Draw the gamertag
        m_CXPlat_Draw.DrawText( m_CXPlat_Draw.GetXPosition() + UI_TEXT_INSESSION_GAMERTAGPADDING,
                                yPos,
                                col,
                                pClient->strGamertags[ i ] );

        #ifdef _XBOX
        if( m_nGameType == X_CONTEXT_GAME_TYPE_RANKED ) 
        {
            // compute the gamertag width from the current font
            FLOAT fGamertagWidth, fDummy;
            m_CXPlat_Draw.GetSmallFont().GetTextExtent( L"WWWWWWWWWWWWWWW", &fGamertagWidth, &fDummy );

            // render TrueSkill(TM) skill bar 
            TrueSkill aTrueSkill( pClient->dMu[ i ], pClient->dSigma[ i ] ); 
            TrueSkillBar aTrueSkillBar( &aTrueSkill, 
                                        m_CXPlat_Draw.GetSmallFont(), 
                                        x + fGamertagWidth + m_CXPlat_Draw.GetSmallFont().m_rcWindow.x1 + 27.0f,
                                        yPos + m_CXPlat_Draw.GetSmallFont().m_rcWindow.y1,
                                        ( m_CXPlat_Draw.GetSmallFont().GetFontHeight() - 2.0f ) * 3.0f,
                                        m_CXPlat_Draw.GetSmallFont().GetFontHeight() - 2.0f,
                                        50, 
                                        m_bUseAlphaBlending );
            aTrueSkillBar.Render( m_pd3dDevice );

            // show the quality of the host for each of the clients 
            if( pHost )
            {
                TrueSkill aHostTrueSkill( pHost->dMu[ 0 ], pHost->dSigma[ 0 ] );
                DOUBLE dSessionHostQuality = MatchmakingHostQuality( &aTrueSkill,
                                                                     &aHostTrueSkill );

                WCHAR wstQuality[ 10 ];
                swprintf_s( wstQuality, L"%2.0lf%%",
                            dSessionHostQuality * 100.0 );

                m_CXPlat_Draw.DrawText( x + fGamertagWidth + 30.0f + aTrueSkillBar.GetWidth(),
                                        yPos, 
                                        col, 
                                        wstQuality,
                                        FALSE );
            }
        }
        #endif
    }

    // re-set the old-font scaling
    m_CXPlat_Draw.SetScaleFactor( 1.0f, 1.0f );

    // Return the height of the client, so the caller can calculate how far to skip down
    // for next
    return ( pClient->cPlayers + 2 ) * m_CXPlat_Draw.GetFontHeight();
}

#ifdef _XBOX
//--------------------------------------------------------------------------------------
// Name: RenderViewStats()
// Desc: Render the stats page
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderViewStats()
{
    FLOAT fCenterX = ( m_CXPlat_Draw.GetLargeFont().m_rcWindow.x2 - m_CXPlat_Draw.GetLargeFont().m_rcWindow.x1 ) / 2.0f;
    FLOAT fCenterY = ( m_CXPlat_Draw.GetLargeFont().m_rcWindow.y2 - m_CXPlat_Draw.GetLargeFont().m_rcWindow.y1 ) / 2.0f;

    FLOAT x, y;

    // If we're currently retrieving stats, say so
    if( !XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        m_CXPlat_Draw.DrawText( fCenterX, 
                                fCenterY, 
                                COLOR_TEXT, 
                                L"Retrieving leaderboard", 
                                TRUE,
                                ATGFONT_CENTER_X );
    }
    else
    {
        // String to display at the bottom right
        static const WCHAR* wstrHelpText =
            UI_ELEMENT_BACK L"Back\n";

        // Headers for the two types of leaderboard
        static const WCHAR* awstrHeadersWithTrueSkills[] =
        {
            L"Rank   ",
            L"Gamertag                      ",
            L"TrueSkill(TM) Rating   ",
            L"Games \nPlayed ",
            L"",
            L"",
        };

        static const WCHAR* awstrHeadersWithoutTrueSkills[] =
        {
            L"Rank   ",
            L"Gamertag                      ",
            L"Games \nWon ",
            L"Games \nPlayed ",
            L"Points \nScored ",
            L"Last \nMap            "
        };

        // decide which header to take depending on whether or not we are
        // in a ranked leaderboard
        const WCHAR* awstrHeaders[6];
        for( INT i = 0; i < 6; ++i )
        {
            if( m_aLeaderboards[ m_nGameType ][ m_nLeaderboard ].m_dwSkillIndex )
                awstrHeaders[ i ] = awstrHeadersWithTrueSkills[ i ];
            else
                awstrHeaders[ i ] = awstrHeadersWithoutTrueSkills[ i ];
        }

        static const DWORD nNumColumns = ARRAYSIZE( awstrHeaders );

        // Render the leaderboard label
        x = fCenterX;
        y = m_CXPlat_Draw.GetLargeFont().m_rcWindow.y1 + m_CXPlat_Draw.GetLargeFont().GetFontHeight();

        m_CXPlat_Draw.DrawText( x,
                                y,
                                COLOR_TEXT,
                                m_aLeaderboards[ m_nGameType ][ m_nLeaderboard ].m_wchName,
                                TRUE,
                                ATGFONT_CENTER_X );

        // Render the help text
        FLOAT fxExtent, fyExtent;
        FLOAT fOldScaleX = m_CXPlat_Draw.GetLargeFont().m_fXScaleFactor;
        FLOAT fOldScaleY = m_CXPlat_Draw.GetLargeFont().m_fYScaleFactor;
        m_CXPlat_Draw.GetLargeFont().SetScaleFactors( 0.75f, 0.75f );
        m_CXPlat_Draw.GetLargeFont().GetTextExtent( wstrHelpText, &fxExtent, &fyExtent );

        x = m_CXPlat_Draw.GetLargeFont().m_rcWindow.x2 - m_CXPlat_Draw.GetLargeFont().m_rcWindow.x1 - fxExtent;
        y = m_CXPlat_Draw.GetLargeFont().m_rcWindow.y2 - m_CXPlat_Draw.GetLargeFont().m_rcWindow.y1 - fyExtent;

        m_CXPlat_Draw.DrawText( x,
                                y,
                                COLOR_TEXT,
                                wstrHelpText,
                                TRUE );

        m_CXPlat_Draw.GetLargeFont().SetScaleFactors( fOldScaleX, fOldScaleY );

        // Render the leaderboard
        FLOAT fLeaderboardWidth = 0.0f;

        for( INT i = 0; i < ARRAYSIZE( awstrHeaders ); ++i )
        {
            fLeaderboardWidth += m_CXPlat_Draw.GetSmallFont().GetTextWidth( awstrHeaders[ i ] );
        }

        // Center the leaderboard horizontally
        FLOAT fLeftEdge = 
            ( m_CXPlat_Draw.GetSmallFont().m_rcWindow.x2 - m_CXPlat_Draw.GetSmallFont().m_rcWindow.x1 - fLeaderboardWidth ) / 2.0f;

        // Position the leaderboard below the header
        y = m_CXPlat_Draw.GetLargeFont().m_rcWindow.y1 + m_CXPlat_Draw.GetLargeFont().GetFontHeight() * 3.0f;

        WCHAR wchRender[ 256 ];

        // Special case: if there are no rows, say so
        if( m_pStats->pViews[ 0 ].dwNumRows == 0 )
        {
            m_CXPlat_Draw.DrawText( fCenterX,
                                    y,
                                    COLOR_TEXT,
                                    L"No rows in leaderboard",
                                    FALSE,
                                    ATGFONT_CENTER_X );

            return S_OK;
        }

        // allocate the pointers to the TrueSkill(TM) skill bars and true skills
        TrueSkillBar** pTrueSkillBars = new TrueSkillBar*[ m_pStats->pViews[ 0 ].dwNumRows ];
        TrueSkill** pTrueSkills = new TrueSkill*[ m_pStats->pViews[ 0 ].dwNumRows ];

        //
        // Render the rows and columns
        x = fLeftEdge;

        // Render the header first
        for( INT nCol = 0; nCol < nNumColumns; ++nCol )
        {
            wcscpy_s( wchRender, awstrHeaders[ nCol ] );

            // Render the value
            DWORD dwColor = COLOR_HEADER;  // Header

            m_CXPlat_Draw.DrawText( x,
                                    y,
                                    dwColor,
                                    wchRender,
                                    FALSE );

            x += m_CXPlat_Draw.GetSmallFont().GetTextWidth( awstrHeaders[ nCol ] );
        }

        y += m_CXPlat_Draw.GetSmallFont().GetFontHeight() * 2.0f;

        // Render the rest of the rows
        for( UINT nRow = 0; nRow < m_pStats->pViews[ 0 ].dwNumRows; ++nRow )
        {
            // set the pointers to the TrueSkill(TM) skill bars and skills to NULL
            pTrueSkillBars[ nRow ] = NULL;
            pTrueSkills[ nRow ] = NULL;

            x = fLeftEdge;

            // Special case: if this is the first row and we're not at the top of
            // the leaderboard, render an up arrow
            if( ( nRow == 0 ) && 
                ( m_pStats->pViews[ 0 ].pRows[ nRow ].dwRank != 1 ) )
            {
                m_CXPlat_Draw.DrawText( fLeftEdge + fLeaderboardWidth,
                                        y,
                                        COLOR_TEXT,
                                        GLYPH_UP_ARROW,
                                        TRUE );
            }

            // Special case: if this is the last row and we're not at the bottom of
            // the leaderboard, render a down arrow
            if( ( nRow == m_pStats->pViews[ 0 ].dwNumRows - 1 ) &&
                ( m_pStats->pViews[ 0 ].pRows[ nRow ].dwRank != m_pStats->pViews[ 0 ].dwTotalViewRows ) )
            {
                m_CXPlat_Draw.DrawText( fLeftEdge + fLeaderboardWidth,
                                        y,
                                        COLOR_TEXT,
                                        GLYPH_DOWN_ARROW,
                                        TRUE );
            }

            for( INT nCol = 0; nCol < nNumColumns; ++nCol )
            {
                // if we are in a ranked leaderboard
                if( m_aLeaderboards[ m_nGameType ][ m_nLeaderboard ].m_dwSkillIndex )
                {

                    // Render the appropriate text
                    switch( nCol )
                    {
                    case 0:
                        // Rank
                        swprintf_s( wchRender,
                            L"%d", m_pStats->pViews[ 0 ].pRows[ nRow ].dwRank );
                        break;

                    case 1:
                        // Gamertag
                        // convert the ANSI gamertag to WCHAR
                        swprintf_s( wchRender,
                            L"%S", m_pStats->pViews[ 0 ].pRows[ nRow ].szGamertag );
                        break;

                    case 2:
                        swprintf_s( wchRender, L"" );
                        {
                            // TrueSkill(TM) skill bar 
                            pTrueSkills[ nRow ] = new TrueSkill( m_pStats->pViews[ 0 ].pRows[ nRow ].pColumns[ 0 ].Value.dblData,
                                m_pStats->pViews[ 0 ].pRows[ nRow ].pColumns[ 1 ].Value.dblData );
                            pTrueSkillBars[ nRow ] = new TrueSkillBar ( pTrueSkills[ nRow ], m_CXPlat_Draw.GetSmallFont(),
                                x + m_CXPlat_Draw.GetSmallFont().m_rcWindow.x1, y + m_CXPlat_Draw.GetSmallFont().m_rcWindow.y1,
                                ( m_CXPlat_Draw.GetSmallFont().GetFontHeight() - 2.0f ) * 4.0f, m_CXPlat_Draw.GetSmallFont().GetFontHeight() - 2.0f,
                                50, m_bUseAlphaBlending );
                            pTrueSkillBars[ nRow ]->Render( m_pd3dDevice );
                        }
                        break;

                    case 3:
                        // Display the games played in the appropriate column
                        swprintf_s( wchRender,
                            L"%I64u", m_pStats->pViews[ 0 ].pRows[ nRow ].pColumns[ 2 ].Value.i64Data );
                        break;

                    default:
                        // No column to display
                        swprintf_s( wchRender, L"" );
                        break;
                    }
                }
                else
                {
                    // Render the appropriate text
                    switch( nCol )
                    {
                    case 0:
                        // Rank
                        swprintf_s( wchRender,
                            L"%d", m_pStats->pViews[ 0 ].pRows[ nRow ].dwRank );
                        break;

                    case 1:
                        // Gamertag
                        // convert the ANSI gamertag to WCHAR
                        swprintf_s( wchRender,
                            L"%S", m_pStats->pViews[ 0 ].pRows[ nRow ].szGamertag );
                        break;

                    case 2:
                        // Games won
                        // Output the rating as a 64-bit integer
                        swprintf_s( wchRender,
                            L"%I64u", m_pStats->pViews[ 0 ].pRows[ nRow ].i64Rating );
                        break;

                    case 5:
                        // Last map
                        wcscpy_s( wchRender,
                            m_astrMaps[ m_pStats->pViews[ 0 ].pRows[ nRow ].pColumns[ 2 ].Value.nData ] );
                        break;

                    default:
                        // Display the value in the appropriate column
                        swprintf_s( wchRender,
                            L"%d", 
                            m_pStats->pViews[ 0 ].pRows[ nRow ].pColumns[ nCol - 3 ].Value.nData );
                        break;
                    }
                }

                // Render the value

                DWORD dwColor;
                if( nRow == m_nMenuItem )
                {
                    dwColor = COLOR_HILIGHT;  // Currently-selected entry
                }
                else
                {
                    dwColor = COLOR_TEXT;  // Generic menu item
                }

                m_CXPlat_Draw.DrawText( x, 
                                        y, 
                                        dwColor, 
                                        wchRender,
                                        FALSE );

                x += m_CXPlat_Draw.GetSmallFont().GetTextWidth( awstrHeaders[ nCol ] );
            }

            // Go to the next row
            y += m_CXPlat_Draw.GetSmallFont().GetFontHeight();
        }

        // if we are in a ranked leaderboard then possibly also draw the outcome probabilities
        if( m_aLeaderboards[ m_nGameType ][ m_nLeaderboard ].m_dwSkillIndex )
        {
            // check if one of the current players in visible on the screen and render the outcome probabilities
            BOOL bOutcomeRendered = FALSE;
            for( UINT nController = 0; nController < MAX_USER_COUNT && !bOutcomeRendered; ++nController )
            {
                const XUSER_SIGNIN_INFO& info = m_SignInInfo[ nController ];
                if( info.UserSigninState == eXUserSigninState_NotSignedIn )
                {
                    continue;
                }
 
                for( UINT j = 0; j < m_pStats->pViews[ 0 ].dwNumRows && !bOutcomeRendered; ++j )
                {
                    if( pTrueSkillBars[ j ] == NULL )
                    {
                        continue;
                    }
                    if( m_pStats->pViews[ 0 ].pRows[ j ].xuid == info.xuid )
                    {
                        if( j != m_nMenuItem )
                        {
                            if( j < m_nMenuItem )
                                // HACK: The 0.1 has to be EXACTLY the draw probability as specified for this mode in XLAST. Right now, in 
                                // this sample every game mode has 10% draw probability. In a real title, one may want to hard-code the
                                // XLAST draw probabilities and point to the right one based on m_nGameMode
                                pTrueSkillBars[ j ]->RenderOutcomeProbabilities( m_pd3dDevice, pTrueSkillBars[ m_nMenuItem ], 0.1 );
                            else
                                // HACK: The 0.1 has to be EXACTLY the draw probability as specified for this mode in XLAST. Right now, in 
                                // this sample every game mode has 10% draw probability. In a real title, one may want to hard-code the
                                // XLAST draw probabilities and point to the right one based on m_nGameMode
                                pTrueSkillBars[ ( INT ) m_nMenuItem ]->RenderOutcomeProbabilities( m_pd3dDevice, pTrueSkillBars[ j ], 0.1 );

                            bOutcomeRendered = TRUE;
                        }
                    }
                }
            }
        }


        // free the TrueSkill(TM) skill bars and skills
        for( UINT i = 0; i < m_pStats->pViews[ 0 ].dwNumRows; ++i ) 
        {
            delete pTrueSkills[ i ];
            delete pTrueSkillBars[ i ];
        }
        delete [] pTrueSkills;
        delete [] pTrueSkillBars;
    }

    return S_OK;
}


#endif