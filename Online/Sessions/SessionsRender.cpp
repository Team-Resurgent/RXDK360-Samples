//--------------------------------------------------------------------------------------
// SessionsRender.cpp
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
#include "TrueSkillBar.h"
#include "XGUI.h"

// Disable warning C6211: Leaking memory <pointer> due to an exception.
// We "know" that we won't run out of memory in this sample
#pragma warning ( disable : 6211 )

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
static const DWORD NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    HRESULT hr = S_OK;

    // Draw a gradient filled background
    ATG::RenderBackground( COLOR_BACKGROUND1, COLOR_BACKGROUND2 );

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font16, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font16.Begin();
        m_Font16.SetScaleFactors( 1.2f, 1.2f );
        m_Font16.DrawText( 0, 0, COLOR_TEXT, L"Sessions" );
        m_Font16.SetScaleFactors( 1.0f, 1.0f );
        m_Font16.End();

        FLOAT x = m_Font16.GetTextWidth( GLYPH_LEFT_BUTTON L": " );
        FLOAT y = -m_Font16.GetFontHeight();

        if( m_AppState != APPSTATE_VIEWSTATS )
        {
            // Draw the help text
            m_Font16.DrawText( 0, y, COLOR_TEXT, GLYPH_B_BUTTON L": " );
            m_Font12.DrawText( x, y, COLOR_TEXT, L"Back" );
            y -= m_Font16.GetFontHeight();

            const WCHAR* strNotification = L"Notify ";
            m_Font16.DrawText( 0, y, COLOR_TEXT, GLYPH_LEFT_BUTTON L": " );
            m_Font12.DrawText( x, y, COLOR_TEXT, strNotification );

            m_Font12.DrawText( x + m_Font12.GetTextWidth( strNotification ), y,
                               COLOR_TEXT, m_astrNotificationPosition[ m_nNotificationPosition ] );
            y -= m_Font16.GetFontHeight();

            m_Font16.DrawText( 0, y, COLOR_TEXT, GLYPH_Y_BUTTON L": " );
            m_Font12.DrawText( x, y, COLOR_TEXT, L"Friends" );
            y -= m_Font16.GetFontHeight();
        }
        else
        {
            // Draw the help text
            m_Font16.DrawText( 0, y, COLOR_TEXT, GLYPH_B_BUTTON L": " );
            m_Font12.DrawText( x, y, COLOR_TEXT, L"Back" );
            y -= m_Font16.GetFontHeight();
        }

        if( m_AppState & APPSTATE_INSESSIONFLAG )
        {
            m_Font16.DrawText( 0, y, COLOR_TEXT, GLYPH_X_BUTTON L": " );
            m_Font12.DrawText( x, y, COLOR_TEXT, L"Wave " );
        }

        // if there are session errors to report, do so
        if( m_Session.GetSessionError() )
        {
            FLOAT fCenterX = ( m_Font16.m_rcWindow.x2 - m_Font16.m_rcWindow.x1 ) / 2.0f;
            m_Font12.DrawText( fCenterX, -m_Font16.GetFontHeight(), COLOR_HIGHLIGHT,
                               m_Session.GetSessionError(), ATGFONT_CENTER_X );
        }

        switch( m_AppState )
        {
            case APPSTATE_MAINMENU:
                hr = RenderMainMenu();
                break;

            case APPSTATE_SEARCHUI:
                hr = RenderSearchUI();
                break;

            case APPSTATE_SEARCH:
                hr = RenderSearch();
                break;

            case APPSTATE_CREATEUI:
                hr = RenderCreateUI();
                break;

            case APPSTATE_CREATE:
                hr = RenderCreate();
                break;

            case APPSTATE_CONNECTING:
                hr = RenderConnecting();
                break;

            case APPSTATE_VIEWSTATS:
                hr = RenderViewStats();
                break;

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

        }
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: RenderMainMenu()
// Desc: Render the main menu
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderMainMenu()
{
    static const WCHAR* astrItems[] =
    {
        L"Create a session",
        L"Search for a session",
        L"View stats",
        L"Changed logged-in users"
    };

    FLOAT fCenterX = ( m_Font16.m_rcWindow.x2 - m_Font16.m_rcWindow.x1 ) / 2.0f;
    FLOAT fCenterY = ( m_Font16.m_rcWindow.y2 - m_Font16.m_rcWindow.y1 ) / 2.0f;

    FLOAT x = fCenterX;
    FLOAT y = fCenterY - ( MAINMENU_MAX * 2 - 1 ) * m_Font16.GetFontHeight() / 2;

    for( UINT i = 0; i < MAINMENU_MAX; i++ )
    {
        D3DCOLOR col = ( m_nMenuItem == i ) ? COLOR_HIGHLIGHT : COLOR_TEXT;

        m_Font16.DrawText( x, y, col, astrItems[ i ], ATGFONT_CENTER_X );
        y += m_Font16.GetFontHeight() * 2;
    }

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

    FLOAT fCenterX = ( m_Font16.m_rcWindow.x2 - m_Font16.m_rcWindow.x1 ) / 2.0f;
    FLOAT fCenterY = ( m_Font16.m_rcWindow.y2 - m_Font16.m_rcWindow.y1 ) / 2.0f;

    FLOAT x = fCenterX;
    FLOAT y = fCenterY - ( SEARCHMENU_MAX * 2 - 1 ) * m_Font16.GetFontHeight() / 2;

    for( UINT i = 0; i < SEARCHMENU_MAX; i++ )
    {
        WCHAR wchRenderString[ 256 ];
        D3DCOLOR col = ( m_nMenuItem == i ) ? COLOR_HIGHLIGHT : COLOR_TEXT;

        switch( i )
        {
            case SEARCHMENU_GAMETYPE:
                swprintf_s( wchRenderString,
                            L"%s:  %s %s %s",
                            astrItems[ i ],
                            ( m_nMenuItem == i ) ? GLYPH_LEFT_ARROW  : L" ",
                            m_astrGameTypes[ m_nGameType ],
                            ( m_nMenuItem == i ) ? GLYPH_RIGHT_ARROW : L" " );
                break;

            case SEARCHMENU_GAMEMODE:
                swprintf_s( wchRenderString,
                            L"%s:  %s %s %s",
                            astrItems[ i ],
                            ( m_nMenuItem == i ) ? GLYPH_LEFT_ARROW  : L" ",
                            m_astrGameModes[ m_nGameMode ],
                            ( m_nMenuItem == i ) ? GLYPH_RIGHT_ARROW : L" " );
                break;

            case SEARCHMENU_MAP:
                swprintf_s( wchRenderString,
                            L"%s:  %s %s %s",
                            astrItems[ i ],
                            ( m_nMenuItem == i ) ? GLYPH_LEFT_ARROW  : L" ",
                            m_astrMaps[ m_nMap ],
                            ( m_nMenuItem == i ) ? GLYPH_RIGHT_ARROW : L" " );
                break;

            case SEARCHMENU_MINVICTORYPOINTS:
                swprintf_s( wchRenderString,
                            L"%s:  %s %d %s",
                            astrItems[ i ],
                            ( m_nMenuItem == i && m_nMinVictoryPoints > VICTORY_POINTS_MIN ) ?
                            GLYPH_LEFT_ARROW : L" ",
                            m_nMinVictoryPoints,
                            ( m_nMenuItem == i && m_nMinVictoryPoints < m_nMaxVictoryPoints ) ?
                            GLYPH_RIGHT_ARROW : L" " );
                break;

            case SEARCHMENU_MAXVICTORYPOINTS:
                swprintf_s( wchRenderString,
                            L"%s:  %s %d %s",
                            astrItems[ i ],
                            ( m_nMenuItem == i && m_nMaxVictoryPoints > m_nMinVictoryPoints ) ?
                            GLYPH_LEFT_ARROW : L" ",
                            m_nMaxVictoryPoints,
                            ( m_nMenuItem == i && m_nMaxVictoryPoints < VICTORY_POINTS_MAX ) ?
                            GLYPH_RIGHT_ARROW : L" " );
                break;

            case SEARCHMENU_SEARCH:
                wcscpy_s( wchRenderString, astrItems[ i ] );
                break;
        }

        m_Font16.DrawText( x, y, col, wchRenderString, ATGFONT_CENTER_X );
        y += m_Font16.GetFontHeight() * 2;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderSearch()
// Desc: Render the screen when searching for a match
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderSearch()
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

    FLOAT fCenterX = ( m_Font16.m_rcWindow.x2 - m_Font16.m_rcWindow.x1 ) / 2.0f;
    FLOAT fCenterY = ( m_Font16.m_rcWindow.y2 - m_Font16.m_rcWindow.y1 ) / 2.0f;

    // If we're searching, say so
    if( !XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        m_Font16.DrawText( fCenterX, fCenterY, COLOR_TEXT,
                           L"Searching...", ATGFONT_CENTER_X );
    }
    else if( FAILED( XGetOverlappedExtendedError( &m_Overlapped ) ) )
    {
        // If we've got an error, report it
        swprintf_s( strRender, L"Failed to search for sessions, error 0x%08x",
                    XGetOverlappedExtendedError( &m_Overlapped ) );

        m_Font16.DrawText( fCenterX, fCenterY, COLOR_TEXT,
                           strRender, ATGFONT_CENTER_X );
    }
    else if( m_pSearchResults->dwSearchResults == 0 )
    {
        // If we've got no results, report that too
        m_Font16.DrawText( fCenterX, fCenterY, COLOR_TEXT,
                           L"No sessions found", ATGFONT_CENTER_X );
    }
    else
    {
        // Otherwise, we have results. Render them.

        // Column headers
        static const WCHAR* aHeaders[] =
        {
            L"------ Session ID ------ ",
            L"V.P.",
            L"Game Mode ",
            L"Pub O/F ",
            L"Pri O/F ",
            L"QoS [minRTT medRTT][xmit recv][up down] "
        };

        // Roughly center the results vertically
        FLOAT y = fCenterY - ( ( m_pSearchResults->dwSearchResults + 3 ) *
                               ( m_Font12.GetFontHeight() + 2 ) ) / 2;

        // Loop through the rows
        for( INT row = -1; row < ( INT )m_pSearchResults->dwSearchResults; row++ )
        {
            FLOAT x = 0;

            for( INT col = 0; col < COLUMN_MAX; col++ )
            {
                // Row -1 == headers
                if( row == -1 )
                {
                    wcscpy_s( strRender, aHeaders[ col ] );
                }
                else
                {
                    // get a reference to the current result for readability
                    XSESSION_SEARCHRESULT& Result = m_pSearchResults->pResults[ row ];

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
                                        L"%016I64X", ATG::XNKIDToInt64(Result.info.sessionID) );
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
                                        Result.dwFilledPublicSlots );
                            break;

                        case COLUMN_PRIVATESLOTS:
                            swprintf_s( strRender,
                                        L"%d/%d",
                                        Result.dwOpenPrivateSlots,
                                        Result.dwFilledPrivateSlots );
                            break;

                        case COLUMN_CUSTOMINFO:
                            if( m_pQoSResult->axnqosinfo[ row ].bFlags & XNET_XNQOSINFO_COMPLETE )
                            {
                                WORD nBytes = m_pQoSResult->axnqosinfo[ row ].cbData;
                                WORD nWords = nBytes / sizeof( WORD );
                                if( nBytes > 0 )
                                {
                                    WCHAR* chTemp = new WCHAR[ nWords + 1 ];
                                    for( UINT nIter = 0; nIter < nBytes; nIter++ )
                                    {
                                        ( ( BYTE* )chTemp )[ nIter ] =
                                            m_pQoSResult->axnqosinfo[ row ].pbData[ nIter ];
                                    }
                                    chTemp[ nWords ] = 0L;
                                    swprintf_s( strRender,
                                                L"%s [%d %d][%d %d][%d %d]",
                                                chTemp,
                                                m_pQoSResult->axnqosinfo[row].wRttMinInMsecs,
                                                m_pQoSResult->axnqosinfo[row].wRttMedInMsecs,
                                                m_pQoSResult->axnqosinfo[row].cProbesXmit,
                                                m_pQoSResult->axnqosinfo[row].cProbesRecv,
                                                m_pQoSResult->axnqosinfo[row].dwUpBitsPerSec,
                                                m_pQoSResult->axnqosinfo[row].dwDnBitsPerSec);
                                }
                            }
                            break;
                    }
                }

                // Render the column
                D3DCOLOR color = ( ( INT )m_nMenuItem == row ) ? COLOR_HIGHLIGHT : COLOR_TEXT;

                m_Font12.DrawText( x, y, color, strRender );

                // Increment the x-position
                x += m_Font12.GetTextWidth( aHeaders[ col ] ) + 10;
            } // for (column)

            // Increment the y-position
            y += m_Font12.GetFontHeight() + 2;
        } // for ( row )

        // display the result count
        y += m_Font12.GetFontHeight() + 2;

        swprintf_s( strRender, L"%d result%s returned.",
                    m_pSearchResults->dwSearchResults,
                    m_pSearchResults->dwSearchResults == 1 ? L"" : L"s" );

        m_Font12.DrawText( 2, y, COLOR_TEXT, strRender );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderCreateUI()
// Desc: Render the game creation menu
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderCreateUI()
{
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

    FLOAT fCenterX = ( m_Font16.m_rcWindow.x2 - m_Font16.m_rcWindow.x1 ) / 2.0f;
    FLOAT fCenterY = ( m_Font16.m_rcWindow.y2 - m_Font16.m_rcWindow.y1 ) / 2.0f;

    FLOAT x = fCenterX;
    FLOAT y = fCenterY - ( CREATEMENU_MAX * 1.5f - 0.5f ) * m_Font16.GetFontHeight() / 2;

    for( UINT i = 0; i < CREATEMENU_MAX; i++ )
    {
        WCHAR wchRenderString[ 256 ];
        D3DCOLOR col = ( m_nMenuItem == i ) ? COLOR_HIGHLIGHT : COLOR_TEXT;

        switch( i )
        {
            case CREATEMENU_GAMETYPE:
                swprintf_s( wchRenderString,
                            L"%s:  %s %s %s",
                            astrItems[ i ],
                            ( m_nMenuItem == i ) ? GLYPH_LEFT_ARROW  : L" ",
                            m_astrGameTypes[ m_nGameType ],
                            ( m_nMenuItem == i ) ? GLYPH_RIGHT_ARROW : L" " );
                break;

            case CREATEMENU_GAMEMODE:
                swprintf_s( wchRenderString,
                            L"%s:  %s %s %s",
                            astrItems[ i ],
                            ( m_nMenuItem == i ) ? GLYPH_LEFT_ARROW  : L" ",
                            m_astrGameModes[ m_nGameMode ],
                            ( m_nMenuItem == i ) ? GLYPH_RIGHT_ARROW : L" " );
                break;

            case CREATEMENU_MAP:
                swprintf_s( wchRenderString,
                            L"%s:  %s %s %s",
                            astrItems[ i ],
                            ( m_nMenuItem == i ) ? GLYPH_LEFT_ARROW  : L" ",
                            m_astrMaps[ m_nMap ],
                            ( m_nMenuItem == i ) ? GLYPH_RIGHT_ARROW : L" " );
                break;

            case CREATEMENU_VICTORYPOINTS:
                swprintf_s( wchRenderString,
                            L"%s:  %s %d %s",
                            astrItems[ i ],
                            ( m_nMenuItem == i && m_nVictoryPoints > VICTORY_POINTS_MIN ) ?
                            GLYPH_LEFT_ARROW : L" ",
                            m_nVictoryPoints,
                            ( m_nMenuItem == i && m_nVictoryPoints < VICTORY_POINTS_MAX ) ?
                            GLYPH_RIGHT_ARROW : L" " );
                break;

            case CREATEMENU_INVITES:
                if( m_nGameType == X_CONTEXT_GAME_TYPE_RANKED )
                {
                    swprintf_s( wchRenderString,
                                L"%s: %s %s %s",
                                astrItems[ i ],
                                m_nMenuItem == i ? GLYPH_LEFT_ARROW : L" ",
                                L"N/A",
                                m_nMenuItem == i ? GLYPH_RIGHT_ARROW : L" " );

                    col = GRAY_TEXT;
                }
                else
                {
                    swprintf_s( wchRenderString,
                                L"%s: %s %s %s",
                                astrItems[ i ],
                                m_nMenuItem == i ? GLYPH_LEFT_ARROW : L" ",
                                ( m_Session.HasSessionFlags( XSESSION_CREATE_INVITES_DISABLED ) ) ? L"NO" : L"YES",
                                m_nMenuItem == i ? GLYPH_RIGHT_ARROW : L" " );
                }
                break;

            case CREATEMENU_JOINVIAPRESENCE:
                if( m_nGameType == X_CONTEXT_GAME_TYPE_RANKED )
                {
                    swprintf_s( wchRenderString,
                                L"%s: %s %s %s",
                                astrItems[ i ],
                                m_nMenuItem == i ? GLYPH_LEFT_ARROW : L" ",
                                L"N/A",
                                m_nMenuItem == i ? GLYPH_RIGHT_ARROW : L" " );

                    col = GRAY_TEXT;
                }
                else
                {

                    swprintf_s( wchRenderString,
                                L"%s: %s %s %s",
                                astrItems[ i ],
                                m_nMenuItem == i ? GLYPH_LEFT_ARROW : L" ",
                                ( m_Session.HasSessionFlags( XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED ) ) ? L"NO" :
                                L"YES",
                                m_nMenuItem == i ? GLYPH_RIGHT_ARROW : L" " );
                }
                break;

            case CREATEMENU_JOININPROGRESS:
                if( m_nGameType == X_CONTEXT_GAME_TYPE_RANKED )
                {
                    swprintf_s( wchRenderString,
                                L"%s: %s %s %s",
                                astrItems[ i ],
                                m_nMenuItem == i ? GLYPH_LEFT_ARROW : L" ",
                                L"N/A",
                                m_nMenuItem == i ? GLYPH_RIGHT_ARROW : L" " );

                    col = GRAY_TEXT;
                }
                else
                {
                    swprintf_s( wchRenderString,
                                L"%s: %s %s %s",
                                astrItems[ i ],
                                m_nMenuItem == i ? GLYPH_LEFT_ARROW : L" ",
                                ( m_Session.HasSessionFlags( XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED ) ) ? L"NO" :
                                L"YES",
                                m_nMenuItem == i ? GLYPH_RIGHT_ARROW : L" " );
                }
                break;

            case CREATEMENU_CREATE:
                wcscpy_s( wchRenderString, astrItems[ i ] );
                break;
        }

        m_Font16.DrawText( x, y, col, wchRenderString, ATGFONT_CENTER_X );
        y += m_Font16.GetFontHeight() * 1.5f;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderCreate()
// Desc: Render the screen when creating a new session
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderCreate()
{
    // Text buffer
    FLOAT fCenterX = ( m_Font16.m_rcWindow.x2 - m_Font16.m_rcWindow.x1 ) / 2.0f;
    FLOAT fCenterY = ( m_Font16.m_rcWindow.y2 - m_Font16.m_rcWindow.y1 ) / 2.0f;

    // If we're still creating, say so
    m_Font16.DrawText( fCenterX, fCenterY, COLOR_TEXT,
                       L"Creating...", ATGFONT_CENTER_X );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderConnecting()
// Desc: Render the screen when attempting to join a session
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderConnecting()
{
    FLOAT fCenterX = ( m_Font16.m_rcWindow.x2 - m_Font16.m_rcWindow.x1 ) / 2.0f;
    FLOAT fCenterY = ( m_Font16.m_rcWindow.y2 - m_Font16.m_rcWindow.y1 ) / 2.0f;
    m_Font16.DrawText( fCenterX, fCenterY, COLOR_TEXT,
                       L"Connecting...", ATGFONT_CENTER_X );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderInSession()
// Desc: Render the screen when in a session
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderInSession()
{
    // Text buffer
    WCHAR strRender[128] = L"";

    FLOAT fCenterX = ( m_Font16.m_rcWindow.x2 - m_Font16.m_rcWindow.x1 ) / 2.0f;

    // Draw the players in the game
    FLOAT x1 = 10.f;                               // Column 1
    FLOAT x2 = fCenterX + 10.f;                    // Column 2

    FLOAT x = x1;
    FLOAT y = 50.f;                               // Current y-pos
    FLOAT y1 = 0.f;                               // y-increment

    // Draw the header
    // Display the Session ID in its byte order, XBox 360 processor is Big Endian
    swprintf_s( strRender, L"%s (SID:%016I64X)\n %d/%d public slots, %d/%d private slots",
                m_Session.IsHost() ? L"HOSTING" : L"IN SESSION",
                ATG::XNKIDToInt64 (m_Session.GetSessionInfo().sessionID),
                m_Session.GetSessionSlots( SLOTS_FILLEDPUBLIC ),
                m_Session.GetSessionSlots( SLOTS_TOTALPUBLIC ),
                m_Session.GetSessionSlots( SLOTS_FILLEDPRIVATE ),
                m_Session.GetSessionSlots( SLOTS_TOTALPRIVATE ) );

    m_Font12.DrawText( fCenterX, y, COLOR_HIGHLIGHT, strRender, ATGFONT_CENTER_X );

    y += m_Font12.GetFontHeight() * 2;

    // Draw the current status message
    switch( m_AppState )
    {
        case APPSTATE_PREGAME:
            if( m_Session.IsHost() )
            {
                if( m_vecRemote.empty() && m_nGameType == X_CONTEXT_GAME_TYPE_RANKED )
                {
                    wcscpy_s( strRender, L"Waiting for other players to join" );
                }
                else
                {
                    wcscpy_s( strRender, L"Press START to begin game" );
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
                        ( REGISTRATION_TIME - ( GetTickCount() - m_dwRegistrationTimer ) ) / 1000.0f );
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
            wcscpy_s( strRender, L"Press A button to score a point" );
            break;

        case APPSTATE_ENDING:
            wcscpy_s( strRender, L"Ending the session" );
            break;

        case APPSTATE_POSTGAME:
            wcscpy_s( strRender, L"Game over" );
            break;

    }

    m_Font12.DrawText( fCenterX, y, COLOR_HIGHLIGHT, strRender, ATGFONT_CENTER_X );

    y += m_Font12.GetFontHeight() * 2;

    // Draw the local machine
    y1 = RenderClient( L"LOCAL:", x, y, &m_Local, ( m_Session.IsHost() ) ? NULL : m_pHost );

    // If we have an error, exit
    if( m_Session.GetSessionError() )
    {
        return S_OK;
    }

    // If we're not the host, draw the host
    if( !m_Session.IsHost() )
    {
        x = ( x2 + x1 ) - x; // Toggle column
        FLOAT renderClient = RenderClient( L"HOST:", x, y, m_pHost, NULL );
        y1 = max( y1, renderClient );
    }

    // Draw the remote clients
    UINT nRemote = 1;

    for( ClientInfoVec::iterator i = m_vecRemote.begin(); i != m_vecRemote.end(); i++ )
    {
        if( &( *i ) == m_pHost )
        {
            // Don't draw the host again
            continue;
        }

        ClientInfo& Remote = *i;
        swprintf_s( strRender, L"REMOTE %d:", nRemote++ );

        x = ( x2 + x1 ) - x; // toggle column
        if( x == x1 )      // if new column, increment y-pos
        {
            y += y1;
            y1 = 0;
        }

        FLOAT renderClient = RenderClient( strRender, x, y, &Remote, &m_Local );
        y1 = max( y1, renderClient );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderClient()
// Desc: Render the people on a client
//--------------------------------------------------------------------------------------
FLOAT Sample::RenderClient( WCHAR* strLabel, FLOAT x, FLOAT y, ClientInfo* pClient, ClientInfo* pHost )
{
    DWORD dwTick = GetTickCount();

    // Render the label
    m_Font12.DrawText( x, y, COLOR_TEXT, strLabel );

    // Since we are drawing a TrueSkill(TM) skill bar, the font size has to be reduced to fit the screen 
    const FLOAT fScale = 0.7f;
    FLOAT fOldScaleX = m_Font12.m_fXScaleFactor;
    FLOAT fOldScaleY = m_Font12.m_fYScaleFactor;
    m_Font12.SetScaleFactors( fScale, fScale );

    // Render the gamertags
    for( int i = 0; i < pClient->cPlayers; i++ )
    {
        D3DCOLOR col = COLOR_TEXT;

        // If the player has won the game, draw him in a highlight color
        if( m_AppState == APPSTATE_POSTGAME &&
            pClient->nPoints[ i ] == m_nVictoryPoints )
        {
            col = COLOR_HEADER;
        }

        // If the player is in loopback mode, show him as such
        if( pClient == &m_Local && m_Voice.IsLoopbackModeActive( i ) )
        {
            col = COLOR_LOOPBACK;
        }

        // If the player is speaking, show him as such
        if( ( pClient == &m_Local && m_Voice.IsLocalTalking( i ) ) ||
            ( pClient != &m_Local && m_Voice.IsRemoteTalking( pClient->xuids[ i ] ) ) )
        {
            col = COLOR_TALKING;
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
                col = COLOR_HIGHLIGHT;
            }
        }

        FLOAT yPos = y + ( i + 1 ) * m_Font12.GetFontHeight();

        // If we're in the game, draw the player's score
        if( m_AppState == APPSTATE_INGAME || m_AppState == APPSTATE_POSTGAME )
        {
            WCHAR wstrScore[ 10 ];

            swprintf_s( wstrScore, L"%2d",
                        pClient->nPoints[ i ] );

            m_Font12.DrawText( x + 2, yPos, col, wstrScore );
        }

        // Draw the gamertag
        m_Font12.DrawText( x + 25, yPos, col, pClient->strGamertags[ i ] );

        if( m_nGameType == X_CONTEXT_GAME_TYPE_RANKED )
        {
            // compute the gamertag width from the current font
            FLOAT fGamertagWidth, fDummy;
            m_Font12.GetTextExtent( L"WWWWWWWWWWWWWWW", &fGamertagWidth, &fDummy );

            // render TrueSkill(TM) skill bar 
            TrueSkill aTrueSkill( pClient->dMu[ i ], pClient->dSigma[ i ] );
            TrueSkillBar aTrueSkillBar( &aTrueSkill, m_Font12, x + fGamertagWidth +
                                        m_Font12.m_rcWindow.x1 + 27.0f,
                                        yPos + m_Font12.m_rcWindow.y1,
                                        ( m_Font12.GetFontHeight() - 2.0f ) * 4.0f,
                                        m_Font12.GetFontHeight() - 2.0f,
                                        50, m_bUseAlphaBlending );
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
                m_Font12.DrawText( x + fGamertagWidth + 30.0f + aTrueSkillBar.GetWidth(),
                                   yPos, col, wstQuality );
            }
        }
    }

    // re-set the old-font scaling
    m_Font12.SetScaleFactors( fOldScaleX, fOldScaleY );

    // Return the height of the client, so the caller can calculate how far to skip down
    // for next
    return ( pClient->cPlayers + 2 ) * m_Font12.GetFontHeight();
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
    if( !XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        m_Font16.DrawText( fCenterX,
                           fCenterY,
                           COLOR_TEXT,
                           L"Retrieving leaderboard",
                           ATGFONT_CENTER_X );
    }
    else
    {
        // String to display at the bottom right
        static const WCHAR* wstrHelpText =
            GLYPH_BACK_BUTTON L" Show help";

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
        y = m_Font16.m_rcWindow.y1 + m_Font16.GetFontHeight();
        m_Font16.DrawText( x, y, COLOR_TEXT,
                           m_aLeaderboards[ m_nGameType ][ m_nLeaderboard ].m_wchName, ATGFONT_CENTER_X );

        // Render the help text
        FLOAT fxExtent, fyExtent;
        FLOAT fOldScaleX = m_Font16.m_fXScaleFactor;
        FLOAT fOldScaleY = m_Font16.m_fYScaleFactor;
        m_Font16.SetScaleFactors( 0.75f, 0.75f );
        m_Font16.GetTextExtent( wstrHelpText, &fxExtent, &fyExtent );

        x = m_Font16.m_rcWindow.x2 - m_Font16.m_rcWindow.x1 - fxExtent;
        y = m_Font16.m_rcWindow.y2 - m_Font16.m_rcWindow.y1 - fyExtent;

        m_Font16.DrawText(
            x,
            y,
            COLOR_TEXT,
            wstrHelpText );
        m_Font16.SetScaleFactors( fOldScaleX, fOldScaleY );

        // Render the leaderboard
        FLOAT fLeaderboardWidth = 0.0f;

        for( INT i = 0; i < ARRAYSIZE( awstrHeaders ); ++i )
        {
            fLeaderboardWidth += m_Font12.GetTextWidth( awstrHeaders[ i ] );
        }

        // Center the leaderboard horizontally
        FLOAT fLeftEdge =
            ( m_Font12.m_rcWindow.x2 - m_Font12.m_rcWindow.x1 - fLeaderboardWidth ) / 2.0f;

        // Position the leaderboard below the header
        y = m_Font16.m_rcWindow.y1 + m_Font16.GetFontHeight() * 3.0f;

        WCHAR wchRender[ 256 ];

        // Special case: if there are no rows, say so
        if( m_pStats->pViews[ 0 ].dwNumRows == 0 )
        {
            m_Font12.DrawText( fCenterX, y, COLOR_TEXT, L"No rows in leaderboard", ATGFONT_CENTER_X );

            return S_OK;
        }

        // allocate the pointers to the TrueSkill(TM) skill bars and true skills
        TrueSkillBar** pTrueSkillBars = new TrueSkillBar*[ ( INT )m_pStats->pViews[ 0 ].dwNumRows ];
        TrueSkill** pTrueSkills = new TrueSkill*[ ( INT )m_pStats->pViews[ 0 ].dwNumRows ];

        // Render the rows and columns
        for( INT nRow = -1; nRow < ( INT )m_pStats->pViews[ 0 ].dwNumRows; ++nRow )
        {
            // set the pointers to the TrueSkill(TM) skill bars and skills to NULL
            if( nRow >= 0 )
            {
                pTrueSkillBars[ nRow ] = NULL;
                pTrueSkills[ nRow ] = NULL;
            }

            x = fLeftEdge;

            // Special case: if this is the first row and we're not at the top of
            // the leaderboard, render an up arrow
            if( ( nRow == 0 ) &&
                ( m_pStats->pViews[ 0 ].pRows[ nRow ].dwRank != 1 ) )
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
                ( m_pStats->pViews[ 0 ].pRows[ nRow ].dwRank != m_pStats->pViews[ 0 ].dwTotalViewRows ) )
            {
                m_Font16.DrawText(
                    fLeftEdge + fLeaderboardWidth,
                    y,
                    COLOR_TEXT,
                    GLYPH_DOWN_ARROW );
            }

            for( INT nCol = 0; nCol < nNumColumns; ++nCol )
            {
                // If we're rendering the header, draw it!
                if( nRow == -1 )
                {
                    wcscpy_s( wchRender,
                              awstrHeaders[ nCol ] );
                }
                else
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
                                    pTrueSkills[ nRow ] = new TrueSkill(
                                        m_pStats->pViews[ 0 ].pRows[ nRow ].pColumns[ 0 ].Value.dblData,
                                        m_pStats->pViews[ 0 ].pRows[ nRow ].pColumns[ 1 ].Value.dblData );
                                    pTrueSkillBars[ nRow ] = new TrueSkillBar ( pTrueSkills[ nRow ], m_Font12,
                                                                                x + m_Font12.m_rcWindow.x1,
                                                                                y + m_Font12.m_rcWindow.y1,
                                                                                ( m_Font12.GetFontHeight() - 2.0f ) *
                                                                                4.0f, m_Font12.GetFontHeight() - 2.0f,
                                                                                50, m_bUseAlphaBlending );
                                    pTrueSkillBars[ nRow ]->Render( m_pd3dDevice );
                                }
                                break;

                            case 3:
                                // Display the games played in the appropriate column
                                swprintf_s( wchRender,
                                            L"%I64d",
                                            m_pStats->pViews[ 0 ].pRows[ nRow ].pColumns[ 2 ].Value.i64Data );
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
                                            L"%I64d", m_pStats->pViews[ 0 ].pRows[ nRow ].i64Rating );
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

        // if we are in a ranked leaderboard then possibly also draw the outcome probabilities
        if( m_aLeaderboards[ m_nGameType ][ m_nLeaderboard ].m_dwSkillIndex )
        {
            // check if one of the current players in visible on the screen and render the outcome probabilities
            BOOL bOutcomeNotRendered = TRUE;
            for( UINT nController = 0; nController < XUSER_MAX_COUNT && bOutcomeNotRendered; ++nController )
            {
                if( !ATG::SignIn::IsUserSignedIn( nController ) ) continue;


                XUID aCurrentXUID;
                XUserGetXUID( nController, &aCurrentXUID );

                for( UINT j = 0; j < ( INT )m_pStats->pViews[ 0 ].dwNumRows && bOutcomeNotRendered; ++j )
                {
                    if( m_pStats->pViews[ 0 ].pRows[ j ].xuid == aCurrentXUID )
                    {
                        if( j != ( INT )m_nMenuItem )
                        {
                            if( j < ( INT )m_nMenuItem )
                                // HACK: The 0.1 has to be EXACTLY the draw probability as specified for this mode in XLAST. Right now, in 
                                // this sample every game mode has 10% draw probability. In a real title, one may want to hard-code the
                                // XLAST draw probabilities and point to the right one based on m_nGameMode
                                pTrueSkillBars[ j ]->RenderOutcomeProbabilities( m_pd3dDevice,
                                                                                 pTrueSkillBars[ ( INT )m_nMenuItem ],
                                                                                 0.1 );
                            else
                                // HACK: The 0.1 has to be EXACTLY the draw probability as specified for this mode in XLAST. Right now, in 
                                // this sample every game mode has 10% draw probability. In a real title, one may want to hard-code the
                                // XLAST draw probabilities and point to the right one based on m_nGameMode
                                pTrueSkillBars[ ( INT )m_nMenuItem ]->RenderOutcomeProbabilities( m_pd3dDevice,
                                                                                                  pTrueSkillBars[ j ],
                                                                                                  0.1 );
                            bOutcomeNotRendered = FALSE;
                        }
                    }
                }
            }
        }


        // free the TrueSkill(TM) skill bars and skills
        for( INT i = 0; i < ( INT )m_pStats->pViews[ 0 ].dwNumRows; ++i )
        {
            if( pTrueSkills[ i ] )
                delete pTrueSkills[ i ];
            if( pTrueSkillBars[ i ] )
                delete pTrueSkillBars[ i ];
        }
        delete [] pTrueSkills;
        delete [] pTrueSkillBars;
    }

    return S_OK;
}
