//--------------------------------------------------------------------------------------
// XRNMSample.cpp
//
// Sample to demonstrate the XRNM low-level networking library
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <AtgUtil.h>
#include <AtgSignin.h>
#include <AtgInput.h>
#include <malloc.h>
#include "XRNMSample.h"
#include "TextMenu.h"

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display help\nCancel(with Left TRIGGER)" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_1, L"Select(with Left TRIGGER)" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Select" },
    { ATG::HELP_LEFT_STICK,   ATG::HELP_PLACEMENT_1, L"Move object" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_1, L"Host/Search option" },

};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );

//--------------------------------------------------------------------------------------
// Name: Sample::StateImplementation
// Desc: Declaration of static constant array to assist in managing state stack
//--------------------------------------------------------------------------------------
const Sample::_StateImplementation Sample::m_StateImplementation[] =
{
    { &Sample::InitializeMainMenu,   &Sample::UpdateMainMenu,   &Sample::RenderMainMenu,   NULL,
        TRUE },
    { NULL,                          &Sample::UpdateError,      &Sample::RenderError,      &Sample::TerminateError,
        FALSE },
    { &Sample::InitializeHosting,    &Sample::UpdateHosting,    &Sample::RenderHosting,
        &Sample::TerminateHosting,   FALSE },
    { &Sample::InitializeSearching,  &Sample::UpdateSearching,  &Sample::RenderSearching,
        &Sample::TerminateSearching, FALSE },
    { &Sample::InitializeJoining,    &Sample::UpdateJoining,    &Sample::RenderJoining,    NULL,
        FALSE },
    { &Sample::InitializeInSession,  &Sample::UpdateInSession,  &Sample::RenderInSession,  NULL,
        FALSE },
    { &Sample::InitializeLeaving,    &Sample::UpdateLeaving,    &Sample::RenderLeaving,
        &Sample::TerminateLeaving,   FALSE },
};

const WCHAR*        Sample::m_AppStateNames[] =
{
    L"Main Menu",
    L"Error",
    L"Hosting",
    L"Searching"
};

//--------------------------------------------------------------------------------------
// Name: Sample::MainMenuItems
// Desc: Array of item descriptors for the main menu
//--------------------------------------------------------------------------------------
TextMenu::Item Sample::MainMenuItems[] =
{
    { MAINMENU_HOST,   L"Host a session",       TextMenu::FLAG_NORMAL, 0 },
    { MAINMENU_SEARCH, L"Search for a session", TextMenu::FLAG_NORMAL, 0 }
};

const UINT          Sample::MainMenuItemCount = ARRAYSIZE( Sample::MainMenuItems );


//--------------------------------------------------------------------------------------
// Name: Sample::m_PlayerColors[] and Sample::m_PlayerGlyphs[]
// Desc: Variety of ways to render players
//--------------------------------------------------------------------------------------
const D3DCOLOR Sample::PlayerColors[] =
{
    D3DCOLOR_ARGB( 0xFF, 0x40, 0x40, 0xFF ),
    D3DCOLOR_ARGB( 0xFF, 0xFF, 0x40, 0x40 ),
    D3DCOLOR_ARGB( 0xFF, 0x40, 0xFF, 0x40 ),
    D3DCOLOR_ARGB( 0xFF, 0x40, 0xFF, 0xFF ),
    D3DCOLOR_ARGB( 0xFF, 0xFF, 0x40, 0xFF ),
    D3DCOLOR_ARGB( 0xFF, 0xFF, 0xFF, 0x00 ),
    D3DCOLOR_ARGB( 0xFF, 0x00, 0x00, 0x00 ),
};

const UINT          Sample::PlayerColorCount = ARRAYSIZE( Sample::PlayerColors );

const LPCWSTR Sample::PlayerGlyphs[] =
{
    GLYPH_STAR_1,
    GLYPH_SKULL,
    GLYPH_STAR_2,
    GLYPH_HOLLOW_CIRCLE,
    GLYPH_STAR_4,
    GLYPH_FILLED_CIRCLE,
    GLYPH_BULLET,
    GLYPH_STAR_3,
    GLYPH_STAR_4
};

const UINT          Sample::PlayerGlyphCount = ARRAYSIZE( Sample::PlayerGlyphs );


//--------------------------------------------------------------------------------------
// Name: Sample::GameButtons
// Desc: Buttons that the game will recognize and transmit to other players
//
// Reliable XRNM messages can request receipts be returned at the time they
// are transmitted, received, processed, or any combination. Eight different buttons 
// are used to demonstrate every combination.
//--------------------------------------------------------------------------------------

const Sample::_GameButton Sample::GameButtons[] =
{
    { XINPUT_GAMEPAD_A,           GLYPH_A_BUTTON,
        0 },
    { XINPUT_GAMEPAD_B,           GLYPH_B_BUTTON,
        XRNM_FLAG_SEND_RECEIPT_TRANSMIT },
    { XINPUT_GAMEPAD_X,           GLYPH_X_BUTTON,
        XRNM_FLAG_SEND_RECEIPT_RECEIVE },
    { XINPUT_GAMEPAD_Y,           GLYPH_Y_BUTTON,
        XRNM_FLAG_SEND_RECEIPT_TRANSMIT | XRNM_FLAG_SEND_RECEIPT_RECEIVE},
    { XINPUT_GAMEPAD_LEFT_THUMB,  GLYPH_LEFT_BUTTON,
        XRNM_FLAG_SEND_RECEIPT_PROCESS },
    { XINPUT_GAMEPAD_RIGHT_THUMB, GLYPH_RIGHT_BUTTON,
        XRNM_FLAG_SEND_RECEIPT_PROCESS | XRNM_FLAG_SEND_RECEIPT_TRANSMIT },
    { XINPUT_GAMEPAD_START,       GLYPH_START_BUTTON,
        XRNM_FLAG_SEND_RECEIPT_PROCESS | XRNM_FLAG_SEND_RECEIPT_RECEIVE },
    { XINPUT_GAMEPAD_DPAD_UP,     GLYPH_UP_ARROW,
        XRNM_FLAG_SEND_RECEIPT_PROCESS | XRNM_FLAG_SEND_RECEIPT_TRANSMIT | XRNM_FLAG_SEND_RECEIPT_RECEIVE }
};

const UINT          Sample::GameButtonCount = ARRAYSIZE( Sample::GameButtons );


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample app;
    ATG::GetVideoSettings( &app.m_d3dpp.BackBufferWidth, &app.m_d3dpp.BackBufferHeight );
    app.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Set up the application
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize( VOID )
{
    // Create the fonts
    if( FAILED( m_Font16.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( m_Font12.Create( "game:\\Media\\Fonts\\Arial_12.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;


    // Confine text drawing to the title safe area
    m_Font16.SetWindow( ATG::GetTitleSafeArea() );
    m_Font12.SetWindow( ATG::GetTitleSafeArea() );

    // Initialize the console
    m_Console.Initialize( &m_Font12, COLOR_TEXT );

    // Calculate the center points; this will save code later
    D3DRECT rcSafeArea = ATG::GetTitleSafeArea();
    m_fScreenWidth = ( FLOAT )( rcSafeArea.x2 - rcSafeArea.x1 );
    m_fScreenHeight = ( FLOAT )( rcSafeArea.y2 - rcSafeArea.y1 );
    m_fCenterX = m_fScreenWidth / 2;
    m_fCenterY = m_fScreenHeight / 2;
    m_fFontHeight = m_Font12.GetFontHeight();

    m_bDrawHelp = FALSE;

    // Initialize the menu system
    m_TextMenu.SetColors( COLOR_TEXT, COLOR_HILIGHT );

    // Initialize autologin
    ATG::SignIn::Initialize( 1, 4, FALSE, 4 );

    // Initialize variables
    m_wstrError[ 0 ] = L'\0';
    m_nSelectedSession = 0;
    srand( GetTickCount() );
    m_nNextPlayerID = rand();

    m_hEndpoint = NULL;
    m_hSession = INVALID_HANDLE_VALUE;

    // Start up the SNL with default initialization parameters
    if( XNetStartup( NULL ) != 0 )
    {
        ATG::FatalError( "XNetStartup failed.\n" );
    }

    // Start up Winsock
    WORD wVersion = MAKEWORD( 2, 2 );   // request version 2.2 of Winsock
    WSADATA wsaData;

    INT err = WSAStartup( wVersion, &wsaData );
    if( err != 0 )
    {
        ATG::FatalError( "WSAStartup failed, error %d.\n", err );
    }

    // Verify that we got the right version of Winsock
    if( wsaData.wVersion != wVersion )
    {
        ATG::FatalError( "Failed to get proper version of Winsock, got %d.%d.\n",
                         LOBYTE( wsaData.wVersion ), HIBYTE( wsaData.wVersion ) );
    }

    // Create our XRNM endpoint
    HRESULT hr;

    if( FAILED( hr = CreateXRNMEndpoint() ) )
    {
        ATG::FatalError( "Failed to create XRNM endpoint, error 0x%08x\n", hr );
    }

    // Begin in the main menu state
    PushState( APPSTATE_MAINMENU );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Handle per-frame processing
//--------------------------------------------------------------------------------------
HRESULT Sample::Update( VOID )
{
    HRESULT hr = S_OK;

    // Make sure we're logged in
    DWORD dwRet = ATG::SignIn::Update();

    if( !ATG::SignIn::GetSignedInUserMask() || dwRet == ATG::SignIn::SIGNIN_USERS_CHANGED )
    {
        // If we are in session or hosting, switch to leaving first
        if( ( m_AppState.top() == APPSTATE_JOINED ) || ( m_AppState.top() == APPSTATE_HOSTING ) )
        {
            PopState( APPSTATE_LEAVING );
        }

        // And then bail to the menu
        while( m_AppState.top() != APPSTATE_MAINMENU )
        {
            PopState();
        }

        // If Session deletion failed, force the handle to be invalid,
        // so we don't process the event anymore.
        if( m_hSession != INVALID_HANDLE_VALUE )
            m_hSession = INVALID_HANDLE_VALUE;

        return S_OK;
    }

    // Retrieve the current state of the controllers
    m_pGamepad = ATG::Input::GetMergedInput( ATG::SignIn::GetSignedInUserMask() );

    // Toggle help
    if( ( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK ) && !m_pGamepad->bLeftTrigger )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Check for inbound XRNM events
    hr = ProcessXRNMEvents();

    // Call the appropriate update function for the current state
    hr = SUCCEEDED( hr ) ? ( this->*m_StateImplementation[ m_AppState.top() ].Update )() : hr;

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Render the current frame
//--------------------------------------------------------------------------------------
HRESULT Sample::Render( VOID )
{
    // Draw a gradient filled background
    ATG::RenderBackground( COLOR_BACKGROUND1, COLOR_BACKGROUND2 );

    // Draw the header
    m_Font16.DrawText( 0, 0, COLOR_TEXT, L"XRNM Sample" );

    HRESULT hr;

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font16, g_HelpCallouts, NUM_HELP_CALLOUTS );
        hr = S_OK;
    }
    else
    {

        // Call the appropriate render function for the current state
        hr = ( this->*m_StateImplementation[ m_AppState.top() ].Render )();

        m_Console.Render();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: PushState()
// Desc: Add a state to the top of the state stack
//--------------------------------------------------------------------------------------
VOID Sample::PushState( APPSTATE state )
{
    m_AppState.push( state );

    // Call the appropriate initialization function for the new state
    if( m_StateImplementation[ state ].Initialize )
    {
        HRESULT hr = ( this->*m_StateImplementation[ m_AppState.top() ].Initialize )();

        if( FAILED( hr ) )
        {
            swprintf_s( m_wstrError, L"Failed to initialize %s with error 0x%08x",
                        m_AppStateNames[ state ], hr );

            m_AppState.pop();

            m_AppState.push( APPSTATE_ERROR );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: PopState()
// Desc: Remove a state from the top of the state stack
//--------------------------------------------------------------------------------------
VOID Sample::PopState( APPSTATE replace )
{
    if( m_AppState.empty() )
    {
        ATG::FatalError( "Attempt to pop an empty state stack!" );
    }

    APPSTATE state = m_AppState.top();

    HRESULT hr = S_OK;

    if( m_StateImplementation[ state ].Terminate )
    {
        hr = ( this->*m_StateImplementation[ m_AppState.top() ].Terminate )();

        if( FAILED( hr ) )
        {
            swprintf_s( m_wstrError, L"Failed to terminate %s with error 0x%08x",
                        m_AppStateNames[ state ], hr );
        }
    }

    m_AppState.pop();

    if( SUCCEEDED( hr ) && replace != APPSTATE_NONE )
    {
        PushState( replace );
    }

    if( SUCCEEDED( hr ) &&
        !m_AppState.empty() &&
        m_StateImplementation[ m_AppState.top() ].bInitializeOnReturn &&
        replace == APPSTATE_NONE )
    {
        hr = ( this->*m_StateImplementation[ m_AppState.top() ].Initialize )();

        if( FAILED( hr ) )
        {
            swprintf_s( m_wstrError, L"Failed to reinitialize %s with error 0x%08x",
                        m_AppStateNames[ m_AppState.top() ], hr );
        }

    }

    if( FAILED( hr ) )
    {
        PushState( APPSTATE_ERROR );
    }
}


//**************************************************************************************
// APPSTATE_ERROR
// Something has gone wrong; display an error message and wait for the user to 
// acknowledge
//**************************************************************************************
HRESULT Sample::UpdateError( VOID )
{
    // See if a button has been pressed
    if( IS_FORWARD(m_pGamepad) )
    {
        while( m_AppState.top() != APPSTATE_MAINMENU )
        {
            PopState();
        }
    }

    return S_OK;
}

HRESULT Sample::RenderError( VOID )
{
    m_Font16.DrawText( m_fCenterX, m_fCenterY - 2 * m_fFontHeight, COLOR_TEXT, L"ERROR",
                       ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
    m_Font12.DrawText( m_fCenterX, m_fCenterY, COLOR_HILIGHT, m_wstrError, ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
    m_Font16.DrawText( m_fCenterX, m_fCenterY + 2*m_fFontHeight, COLOR_TEXT,    L"Press " GLYPH_A_BUTTON L" to continue", ATGFONT_CENTER_X | ATGFONT_CENTER_Y );

    return S_OK;
}

HRESULT Sample::TerminateError( VOID )
{
    // Reset the error message
    m_wstrError[ 0 ] = L'\0';

    return S_OK;
}


//**************************************************************************************
// APPSTATE_MAINMENU
// Drive the main menu: choose to host a session, or to find one
//**************************************************************************************
HRESULT Sample::InitializeMainMenu( VOID )
{
    // Clear out the peer list
    m_Consoles.clear();

    // Start the menu going
    return m_TextMenu.BeginMenu( &m_Font12, MainMenuItems, MainMenuItemCount );
}

HRESULT Sample::UpdateMainMenu( VOID )
{
    HRESULT hr = m_TextMenu.Update();

    if( m_TextMenu.IsDone() )
    {
        if( m_TextMenu.WasCanceled() )
        {
            // if the user tries to back out of the main menu, invoke the signin UI,
            // but stay here

            ATG::SignIn::ShowSignInUI();
            PopState( APPSTATE_MAINMENU );
        }
        else
        {
            BOOL bHosting = m_TextMenu.GetSelectedID() == MAINMENU_HOST;

            // When we're about to leave the main menu, determine and store our local
            // user information
            SConsole local = { 0 };
            local.cPlayers = 0;

            // Retrieve our XNADDR
            while( XNetGetTitleXnAddr( &local.xnaddr ) == XNET_GET_XNADDR_PENDING );

            for( UCHAR i = 0; i < XUSER_MAX_COUNT; i++ )
            {
                if( !ATG::SignIn::IsUserSignedIn( i ) )
                {
                    continue;
                }

                // we have a signed in user; fill in his info
                local.Players[ local.cPlayers ].nLocalController = i;
                local.Players[ local.cPlayers ].nPlayerID = m_nNextPlayerID++;
                local.Players[ local.cPlayers ].fXPos = rand() / ( 1.0f * RAND_MAX );
                local.Players[ local.cPlayers ].fYPos = rand() / ( 1.0f * RAND_MAX );
                local.Players[ local.cPlayers ].bLocal = TRUE;

                // make special note of the player who pressed the button
                if( IS_FORWARD( &ATG::Input::m_Gamepads[ i ] ) )
                {
                    m_nHost = local.cPlayers;
                }


                CHAR szUsername[ XUSER_NAME_SIZE ];
                XUserGetXUID( i, &local.Players[ local.cPlayers ].xuid );
                XUserGetName( i, szUsername, XUSER_NAME_SIZE );
                szUsername[ XUSER_NAME_SIZE - 1 ] = '\0';
                MultiByteToWideChar( CP_ACP, 0, szUsername, -1,
                                     local.Players[ local.cPlayers ].wstrGamertag, XUSER_NAME_SIZE );

                local.cPlayers++;
            }

            m_cPlayers = local.cPlayers;
            m_Consoles.push_back( local );

            // switch to the state the user selected
            PushState( bHosting ? APPSTATE_HOSTING : APPSTATE_SEARCHING );
        }
    }

    return hr;
}


HRESULT Sample::RenderMainMenu( VOID )
{
    m_Font16.DrawText(
        m_fCenterX,
        ( FLOAT )m_Font16.m_rcWindow.y2 - 2 * m_Font16.GetFontHeight(),
        COLOR_TEXT,
        L"Press" GLYPH_A_BUTTON L"to select.  Press LT +" GLYPH_BACK_BUTTON L" to exit.",
        ATGFONT_CENTER_X );

    return m_TextMenu.Render();
}


//**************************************************************************************
// APPSTATE_SEARCHING
// Send out system link pings and look for a host
//**************************************************************************************
HRESULT Sample::InitializeSearching( VOID )
{
    // Get the system link helper started searching
    HRESULT hr = m_SystemLinkHelper.BeginSearching();

    return hr;
}


HRESULT Sample::UpdateSearching( VOID )
{
    HRESULT hr = S_OK;

    hr = m_SystemLinkHelper.Update();
    const std::vector <SystemLinkHelper::Session>& sessions = m_SystemLinkHelper.GetSessions();

    if( SUCCEEDED( hr ) && sessions.size() )
    {

        if( m_nSelectedSession >= ( INT )sessions.size() )
        {
            m_nSelectedSession = max( 0, sessions.size() - 1 );
        }

        // Check to see if the user is trying to change the selected session
        if( m_pGamepad->wPressedButtons & ( XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_UP ) )
        {
            BOOL bUp = m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP;

            m_nSelectedSession += bUp ? -1 : 1;
            m_nSelectedSession += sessions.size();
            m_nSelectedSession %= sessions.size();
        }

        // Check to see if the user is trying to join the selected session!
        if( IS_FORWARD(m_pGamepad) )
        {
            memcpy( &m_SessionInfo, &sessions[ m_nSelectedSession ].SessionInfo, sizeof( XSESSION_INFO ) );

            m_SystemLinkHelper.ClearSessions();
            PushState( APPSTATE_JOINING );
        }
    }

    // Check to see if the user is trying to back out
    if( IS_BACKWARD(m_pGamepad) )
    {
        PopState();
    }

    return hr;

}


HRESULT Sample::RenderSearching( VOID )
{
    HRESULT hr = S_OK;

    static const WCHAR* awstrHeaders[] =
    {
        L"Host                          ",
        L"# Players",
    };

    const std::vector <SystemLinkHelper::Session>& sessions = m_SystemLinkHelper.GetSessions();

    if( sessions.empty() )
    {
        m_Font12.DrawText( m_fCenterX, m_fCenterY, COLOR_HILIGHT, L"No sessions found yet. Searching...",
                           ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
    }
    else
    {
        FLOAT fY = 3 * m_fFontHeight;
        FLOAT fWidth = 0;

        for( int i = 0; i < ARRAYSIZE( awstrHeaders ); i++ )
        {
            fWidth += m_Font12.GetTextWidth( awstrHeaders[ i ] );
        }

        // Center the list horizontally
        FLOAT fLeftEdge = m_fCenterX - fWidth / 2;

        for( INT i = -1; i < ( INT )sessions.size(); i++ )
        {
            FLOAT fX = fLeftEdge;

            for( INT column = 0; column < 2; column++ )
            {
                D3DCOLOR col;
                LPCWSTR wstrRender = NULL;
                WCHAR wstrOutput[ MAX_STRING ];

                if( i == -1 )
                {
                    // draw headers
                    wstrRender = awstrHeaders[ column ];
                    col = COLOR_HEADER;
                }
                else
                {
                    // draw session
                    col = i == m_nSelectedSession ? COLOR_HILIGHT : COLOR_TEXT;
                    switch( column )
                    {
                        case 0:
                            wstrRender = sessions[ i ].wstrHostGamerTag;
                            break;
                        case 1:
                            wstrRender = wstrOutput;
                            swprintf_s( wstrOutput, L"%d", sessions[ i ].nPlayerCount );
                            break;
                    }
                }

                // We know what to draw, so draw it
                m_Font12.DrawText( fX, fY, col, wstrRender );

                fX += m_Font12.GetTextWidth( awstrHeaders[ column ] );
            }

            fY += m_fFontHeight;
        }
    }


    m_Font16.DrawText(
        m_fCenterX,
        ( FLOAT )m_Font16.m_rcWindow.y2 - 2 * m_Font16.GetFontHeight(),
        COLOR_TEXT,
        L"Press" GLYPH_A_BUTTON L"to join.  Press LT +" GLYPH_BACK_BUTTON L" to exit.",
        ATGFONT_CENTER_X );


    return hr;
}


HRESULT Sample::TerminateSearching( VOID )
{
    HRESULT hr = S_OK;

    hr = m_SystemLinkHelper.End();

    return hr;
}


//**************************************************************************************
// APPSTATE_HOSTING
// Be in a session, and respond to seeking requests
//**************************************************************************************
HRESULT Sample::InitializeHosting( VOID )
{
    HRESULT hr = S_OK;

    // Create the session
    if( SUCCEEDED( hr ) )
    {
        hr = CreateSession( m_nHost, TRUE );
    }

    // Add the local players
    if( SUCCEEDED( hr ) )
    {
        hr = AddLocalUsersToSession();
    }

    // Start responding to discovery
    if( SUCCEEDED( hr ) )
    {
        hr = m_SystemLinkHelper.BeginHosting( &m_SessionInfo, m_Consoles.front().cPlayers,
                                              m_Consoles.front().Players[ m_nHost ].wstrGamertag );
    }

    // Start responding to incoming links
    if( SUCCEEDED( hr ) )
    {
        hr = AllowInboundLinks( TRUE );
    }

    if( SUCCEEDED( hr ) )
    {
        hr = InitializeInSession();
    }

    return hr;
}


HRESULT Sample::UpdateHosting( VOID )
{
    HRESULT hr = S_OK;

    hr = m_SystemLinkHelper.Update();

    if( SUCCEEDED( hr ) )
    {
        hr = UpdateInSession();
    }

    return hr;
}


HRESULT Sample::RenderHosting( VOID )
{
    HRESULT hr = S_OK;

    // Draw the header
    m_Font16.DrawText( ( FLOAT )m_Font16.m_rcWindow.x2 - m_Font16.m_rcWindow.x1, 0, COLOR_TEXT, L"Hosting",
                       ATGFONT_RIGHT );

    hr = RenderInSession();

    return hr;
}



HRESULT Sample::TerminateHosting( VOID )
{
    HRESULT hr = S_OK;

    // Shut down the system link helper
    hr = m_SystemLinkHelper.End();

    // Stop accepting inbound links
    if( SUCCEEDED( hr ) )
    {
        hr = AllowInboundLinks( FALSE );
    }

    return hr;
}


//**************************************************************************************
// APPSTATE_JOINING
// The user has selected a session, so let's attempt to join it
//**************************************************************************************
HRESULT Sample::InitializeJoining( VOID )
{
    HRESULT hr = S_OK;

    // Create the session
    if( SUCCEEDED( hr ) )
    {
        hr = CreateSession( m_nHost, FALSE );
    }

    // Add the local players
    if( SUCCEEDED( hr ) )
    {
        hr = AddLocalUsersToSession();
    }

    // Create a SConsole for the host
    if( SUCCEEDED( hr ) )
    {
        SConsole host = { 0 };
        m_Consoles.push_back( host );

        // Sanity check: there should be us and the host at this point, that's it
        if( m_Consoles.size() != 2 )
        {
            hr = E_FAIL;
        }
    }

    // Now we should be able to resolve the host's XNADDR
    XRNM_ADDRESS addr = { 0 };
    addr.wPort = htons( 1000 );
    int err;

    if( SUCCEEDED( hr ) )
    {
        err = XNetXnAddrToInAddr( &m_SessionInfo.hostAddress, &m_SessionInfo.sessionID, &addr.ina );

        if( err )
        {
            hr = HRESULT_FROM_WIN32( err );
        }
    }

    // Open a link!
    if( SUCCEEDED( hr ) )
    {
        // Create a ClientInfoMsg for ourselves
        CClientInfoMsg msg;
        msg.cConsoles = 1;

        hr = CopyConsoleToMsg( &msg, &m_Consoles.front(), 0 );

        // Calculate the buffer length
        UINT cbData = 0;

        hr = SUCCEEDED( hr ) ? ByteStream::CalculateSize( &msg, &cbData ) : hr;

        if( SUCCEEDED( hr ) )
        {
            BYTE* pbData = ( BYTE* )_malloca( cbData );

            ByteStream stream( TRUE, pbData, cbData );
            stream.Stream( &msg );

            hr = stream.GetHRESULT();

            if( SUCCEEDED( hr ) )
            {
                // Create the outbound link, storing a pointer to the SConsole
                // in the user data for the link
                hr = CreateOutboundLink(
                    &addr,
                    pbData,
                    cbData,
                    ( ULONG_PTR )&m_Consoles.back(),
                    &m_Consoles.back().hLink );
            }

            _freea( pbData );
        }

    }


    return hr;
}


HRESULT Sample::RenderJoining( VOID )
{
    m_Font16.DrawText( m_fCenterX, m_fCenterY, COLOR_TEXT, L"Attempting to join the session...", ATGFONT_CENTER_X );

    return S_OK;
}


HRESULT Sample::UpdateJoining( VOID )
{
    // Nothing to do here, just wait for XRNM events

    return S_OK;
}


//**************************************************************************************
// INSESSION
// Used for both APPSTATE_HOSTING and APPSTATE_JOINED
//**************************************************************************************
HRESULT Sample::InitializeInSession( VOID )
{
    m_dwLastPhysicsUpdate = GetTickCount();
    m_dwLastPositionUpdate = GetTickCount();

    return S_OK;
}


HRESULT Sample::UpdateInSession( VOID )
{
    HRESULT hr = S_OK;

    DWORD dwTickCount = GetTickCount();
    DWORD dwElapsed = dwTickCount - m_dwLastPhysicsUpdate;
    m_dwLastPhysicsUpdate = dwTickCount;

    // Update all players
    for( std::list <SConsole>::iterator i = m_Consoles.begin(); i != m_Consoles.end(); i++ )
    {
        for( UCHAR n = 0; n < i->cPlayers; n++ )
        {
            SPlayer& p = i->Players[ n ];

            if( i == m_Consoles.begin() )
            {
                // Update our thumbstick positions
                p.fXStick = ATG::Input::m_Gamepads[ p.nLocalController ].fX1;
                p.fYStick = ATG::Input::m_Gamepads[ p.nLocalController ].fY1;

                // Check for button presses
                UINT j;
                for( j = 0; j < GameButtonCount; j++ )
                {
                    if( ATG::Input::m_Gamepads[ p.nLocalController ].wPressedButtons & GameButtons[ j ].Button )
                    {
                        break;
                    }
                }

                if( j < GameButtonCount )
                {
                    p.nButton = j;
                    p.dwButtonTime = dwTickCount;

                    CButtonPressedMsg msg;

                    msg.nPlayerID = p.nPlayerID;
                    msg.nButton = p.nButton;

                    // Let everybody know about our button press
                    //
                    // Button presses must be sent reliably!
                    hr = SendMessageToAll(
                        XRNM_DEFAULT_GAMEDATA_SEND_CHANNEL_ID,
                        &msg,
                        GameButtons[ j ].SendFlags | XRNM_FLAG_SEND_RELIABLE );
                }

            }

            // Apply acceleration
            p.fXVelocity += ( p.fXStick / ACCELERATIONFACTOR ) * dwElapsed;
            p.fYVelocity += ( -p.fYStick / ACCELERATIONFACTOR ) * dwElapsed;

            // Cap the velocity
            FLOAT fVelocityMagnitude = sqrt( p.fXVelocity * p.fXVelocity + p.fYVelocity * p.fYVelocity );
            if( fVelocityMagnitude > 1 )
            {
                p.fXVelocity *= ( 1 / fVelocityMagnitude );
                p.fYVelocity *= ( 1 / fVelocityMagnitude );
            }

            // Apply motion
            p.fXPos += ( p.fXVelocity / VELOCITYFACTOR ) * dwElapsed;
            p.fYPos += ( p.fYVelocity / VELOCITYFACTOR ) * dwElapsed;

            // Bounce if need be
            if( ( p.fXPos < 0 && p.fXVelocity < 0 ) ||
                ( p.fXPos > 1 && p.fXVelocity > 0 ) )
            {
                p.fXVelocity *= -1;
                p.fXPos = max( 0, min( 1, p.fXPos ) );
            }

            if( ( p.fYPos < 0 && p.fYVelocity < 0 ) ||
                ( p.fYPos > 1 && p.fYVelocity > 0 ) )
            {
                p.fYVelocity *= -1;
                p.fYPos = max( 0, min( 1, p.fYPos ) );
            }
        }
    }

    // Send a position update if needed
    if( dwTickCount - m_dwLastPositionUpdate > POSITIONUPDATE_INTERVAL )
    {
        m_dwLastPositionUpdate = dwTickCount;

        CPositionUpdateMsg msg;

        SConsole& local = m_Consoles.front();
        msg.NumPlayers = ( UCHAR )local.cPlayers;
        msg.xnaddr = local.xnaddr;

        for( UCHAR n = 0; n < msg.NumPlayers; n++ )
        {
            msg.Updates[ n ].nPlayerID = local.Players[ n ].nPlayerID;

            msg.Updates[ n ].fXPos = local.Players[ n ].fXPos;
            msg.Updates[ n ].fYPos = local.Players[ n ].fYPos;
            msg.Updates[ n ].fXStick = local.Players[ n ].fXStick;
            msg.Updates[ n ].fYStick = local.Players[ n ].fYStick;
            msg.Updates[ n ].fXVelocity = local.Players[ n ].fXVelocity;
            msg.Updates[ n ].fYVelocity = local.Players[ n ].fYVelocity;
        }

        // Position updates need not be sent reliably
        hr = SendMessageToAll(
            XRNM_DEFAULT_GAMEDATA_SEND_CHANNEL_ID,
            &msg,
            0 );
    }

    // Are we trying to back out?
    if( IS_BACKWARD(m_pGamepad) )
    {
        PopState( APPSTATE_LEAVING );
    }

    return hr;
}


HRESULT Sample::RenderInSession( VOID )
{
    HRESULT hr = S_OK;

    m_Font12.DrawText( 180, 5, COLOR_HILIGHT, L"Local color" );
    m_Font12.DrawText( 280, 5, COLOR_TEXT, L"Remote color" );


    m_Font16.DrawText(
        m_fCenterX,
        ( FLOAT )m_Font16.m_rcWindow.y2 - 2 * m_Font16.GetFontHeight(),
        COLOR_TEXT,
        L"Press LT +" GLYPH_BACK_BUTTON L" to exit.",
        ATGFONT_CENTER_X );

    // Draw the players
    for( std::list <SConsole>::iterator i = m_Consoles.begin(); i != m_Consoles.end(); i++ )
    {
        for( UCHAR nPlayer = 0; nPlayer < i->cPlayers; nPlayer++ )
        {
            RenderPlayer( i->Players[ nPlayer ] );
        }
    }

    return hr;
}


HRESULT Sample::RenderPlayer( _Inout_ SPlayer& p )
{
    // The origin will be dead center
    FLOAT fOriginX = m_fScreenWidth * p.fXPos;
    FLOAT fOriginY = m_fScreenHeight * p.fYPos;

    // First draw the arrow
    FLOAT fRotation = atan2( -p.fYStick, p.fXStick );
    m_Font16.SetRotationFactor( fRotation );
    m_Font16.SetScaleFactors( sqrt( p.fYStick * p.fYStick + p.fXStick * p.fXStick ) * PLAYERARROWSCALE / 100.0f, 1 );
    if( p.bLocal )
        m_Font16.DrawText( fOriginX, fOriginY, COLOR_HILIGHT, GLYPH_RIGHT_ARROW, ATGFONT_LEFT | ATGFONT_CENTER_Y );
    else
        m_Font16.DrawText( fOriginX, fOriginY, COLOR_TEXT, GLYPH_RIGHT_ARROW, ATGFONT_LEFT | ATGFONT_CENTER_Y );
    m_Font16.SetRotationFactor( 0 );
    m_Font16.SetScaleFactors( 1, 1 );

    // Then the glyph
    m_Font16.SetScaleFactors( PLAYERICONSCALE / 100.0, PLAYERICONSCALE / 100.0 );
    m_Font16.DrawText( fOriginX, fOriginY, PlayerColors[ p.nPlayerID % PlayerColorCount ],
                       PlayerGlyphs[ p.nPlayerID % PlayerGlyphCount ],
                       ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
    m_Font16.SetScaleFactors( 1.0, 1.0 );

    // If the player has pressed a button, render that, too.
    if( p.dwButtonTime )
    {
        DWORD dwElapsed = GetTickCount() - p.dwButtonTime;
        FLOAT flScale = 0;

        if( dwElapsed < BUTTON_GROW_TIME )
        {
            flScale = BUTTON_START + ( BUTTON_MAX - BUTTON_START ) * ( 1.0f * dwElapsed / BUTTON_GROW_TIME );
        }
        else if( dwElapsed < BUTTON_SHRINK_TIME + BUTTON_GROW_TIME )
        {
            flScale = BUTTON_MAX * ( 1.0f * BUTTON_SHRINK_TIME - ( dwElapsed - BUTTON_GROW_TIME ) ) /
                BUTTON_SHRINK_TIME;
        }
        else
        {
            p.dwButtonTime = 0;
        }

        if( flScale )
        {
            m_Font16.SetScaleFactors( flScale, flScale );
            m_Font16.DrawText( fOriginX, fOriginY, COLOR_TEXT, GameButtons[ p.nButton ].Glyph,
                               ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
            m_Font16.SetScaleFactors( 1, 1 );
        }
    }

    // Last the text label
    m_Font12.SetScaleFactors( PLAYERLABELSCALE / 100.0, PLAYERLABELSCALE / 100.0 );
    if( p.bLocal )
        m_Font12.DrawText( fOriginX, fOriginY, COLOR_HILIGHT, p.wstrGamertag, ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
    else
        m_Font12.DrawText( fOriginX, fOriginY, COLOR_TEXT, p.wstrGamertag, ATGFONT_CENTER_X | ATGFONT_CENTER_Y );

    m_Font12.SetScaleFactors( 1.0, 1.0 );

    return S_OK;
}


//**************************************************************************************
// APPSTATE_LEAVING
// Gracefully shut down the session and say goodbye
//**************************************************************************************
HRESULT Sample::InitializeLeaving( VOID )
{
    HRESULT hr = S_OK;

    // Shut down all outbound links
    BOOL bErased = FALSE;

    for( std::list <SConsole>::iterator i = ++m_Consoles.begin(); SUCCEEDED( hr ) && i != m_Consoles.end();
         bErased ? 0 : ( VOID )++i )
    {
        bErased = i->hLink == 0;

        if( bErased )
        {
            i = m_Consoles.erase( i );
        }
        else
        {
            hr = TerminateLink( i->hLink );
        }
    }

    return hr;
}


HRESULT Sample::UpdateLeaving( VOID )
{
    // Our SConsoles should get removed as links are closed, when we've shut down,
    // we should be the only ones remaining
    if( m_Consoles.size() == 1 )
    {
        PopState();
    }

    return S_OK;
}


HRESULT Sample::RenderLeaving( VOID )
{
    m_Font16.DrawText( m_fCenterX, m_fCenterY, COLOR_TEXT, L"Leaving the session...", ATGFONT_CENTER_X );

    return S_OK;
}


HRESULT Sample::TerminateLeaving( VOID )
{
    HRESULT hr = S_OK;

    // Remove ourselves from the session
    hr = RemoveLocalUsersFromSession();

    if( SUCCEEDED( hr ) )
    {
        // Delete the session
        hr = CloseSession();
    }

    // If Session deletion failed, force the handle to be invalid,
    // so we don't process the event anymore 
    if( m_hSession != INVALID_HANDLE_VALUE )
        m_hSession = INVALID_HANDLE_VALUE;

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: CreateSession()
// Desc: Create a new system link session
//--------------------------------------------------------------------------------------
HRESULT Sample::CreateSession( UCHAR nHostIndex, BOOL bHost )
{
    HRESULT hr = S_OK;

    DWORD dwFlags = bHost ? ( XSESSION_CREATE_SYSTEMLINK | XSESSION_CREATE_HOST ) : XSESSION_CREATE_SYSTEMLINK;

    ULONGLONG Nonce; // we don't need to hold onto this
    UCHAR nHostController = m_Consoles.front().Players[ nHostIndex ].nLocalController;

    XUserSetContext( nHostController, X_CONTEXT_GAME_TYPE, X_CONTEXT_GAME_TYPE_STANDARD );

    DWORD err = XSessionCreate(
        dwFlags,
        nHostController,
        PUBLICSLOTS,
        PRIVATESLOTS,
        &Nonce,
        &m_SessionInfo,
        NULL,
        &m_hSession );

    if( err != ERROR_SUCCESS )
    {
        hr = XGetOverlappedExtendedError( NULL );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: AddLocalUsersToSession()
// Desc: Put everybody who's on the local console into the session
//--------------------------------------------------------------------------------------
HRESULT Sample::AddLocalUsersToSession( VOID )
{
    HRESULT hr = S_OK;

    DWORD dwControllers[ XUSER_MAX_COUNT ];
    BOOL fPrivateSlots[ 4 ] = { 0 };

    for( UCHAR i = 0; i < m_Consoles.front().cPlayers; i++ )
    {
        dwControllers[ i ] = m_Consoles.front().Players[ i ].nLocalController;
    }

    DWORD err = XSessionJoinLocal(
        m_hSession,
        m_Consoles.front().cPlayers,
        dwControllers,
        fPrivateSlots,
        NULL );

    if( err != ERROR_SUCCESS )
    {
        hr = XGetOverlappedExtendedError( NULL );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: AddRemoteUsersToSession()
// Desc: Put everybody on a remote console into the session
//--------------------------------------------------------------------------------------
HRESULT Sample::AddRemoteUsersToSession( const SConsole* pConsole )
{
    HRESULT hr = S_OK;

    XUID xuids[ XUSER_MAX_COUNT ];
    BOOL fPrivateSlots[ 4 ] = { 0 };

    for( UCHAR i = 0; i < pConsole->cPlayers; i++ )
    {
        xuids[ i ] = pConsole->Players[ i ].xuid;
    }

    DWORD err = XSessionJoinRemote(
        m_hSession,
        pConsole->cPlayers,
        xuids,
        fPrivateSlots,
        NULL );

    if( err != ERROR_SUCCESS )
    {
        hr = XGetOverlappedExtendedError( NULL );
    }

    m_cPlayers += pConsole->cPlayers;
    m_SystemLinkHelper.UpdatePlayerCount( m_cPlayers );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: RemoveLocalUsersFromSession()
// Desc: Remove everybody on the local console from the session
//--------------------------------------------------------------------------------------
HRESULT Sample::RemoveLocalUsersFromSession( VOID )
{
    HRESULT hr = S_OK;

    DWORD dwControllers[ XUSER_MAX_COUNT ];

    for( UCHAR i = 0; i < m_Consoles.front().cPlayers; i++ )
    {
        dwControllers[ i ] = m_Consoles.front().Players[ i ].nLocalController;
    }

    DWORD err = XSessionLeaveLocal(
        m_hSession,
        m_Consoles.front().cPlayers,
        dwControllers,
        NULL );

    if( err != ERROR_SUCCESS )
    {
        hr = XGetOverlappedExtendedError( NULL );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: RemoveRemoteUsersFromSession()
// Desc: Remove everybody on a remote console from the session
//--------------------------------------------------------------------------------------
HRESULT Sample::RemoveRemoteUsersFromSession( const SConsole* pConsole )
{
    HRESULT hr = S_OK;

    XUID xuids[ XUSER_MAX_COUNT ];

    for( UCHAR i = 0; i < pConsole->cPlayers; i++ )
    {
        xuids[ i ] = pConsole->Players[ i ].xuid;
    }

    DWORD err = XSessionLeaveRemote(
        m_hSession,
        pConsole->cPlayers,
        xuids,
        NULL );

    if( err != ERROR_SUCCESS )
    {
        hr = XGetOverlappedExtendedError( NULL );
    }

    m_cPlayers -= pConsole->cPlayers;
    m_SystemLinkHelper.UpdatePlayerCount( m_cPlayers );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: CloseSession()
// Desc: End a session
//--------------------------------------------------------------------------------------
HRESULT Sample::CloseSession( VOID )
{
    HRESULT hr = S_OK;

    hr = XSessionDelete( m_hSession, NULL );

    if( SUCCEEDED( hr ) )
    {
        hr = XCloseHandle( m_hSession ) ? S_OK : S_FALSE;
        m_hSession = INVALID_HANDLE_VALUE;
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: CopyConsoleToMsg()
// Desc: Copy an SConsole into a CClientInfoMsg
//--------------------------------------------------------------------------------------
HRESULT Sample::CopyConsoleToMsg( _Inout_ CClientInfoMsg* pMsg, const SConsole* pConsole, UINT nIndex )
{
    pMsg->rgConsoles[ nIndex ].cPlayers = pConsole->cPlayers;
    pMsg->rgConsoles[ nIndex ].xnaddr = pConsole->xnaddr;

    for( UCHAR i = 0; i < pConsole->cPlayers; i++ )
    {
        CClientInfoMsg::_Console::_Player& msgPlayer = pMsg->rgConsoles[ nIndex ].rgPlayers[ i ];
        const SPlayer& player = pConsole->Players[ i ];

        msgPlayer.xuid = player.xuid;
        msgPlayer.nPlayerID = player.nPlayerID;
        msgPlayer.nLocalController = player.nLocalController;

        wcscpy_s( msgPlayer.wstrGamertag, player.wstrGamertag );

        msgPlayer.fXPos = player.fXPos;
        msgPlayer.fYPos = player.fYPos;
        msgPlayer.fXVelocity = player.fXVelocity;
        msgPlayer.fYVelocity = player.fYVelocity;
        msgPlayer.fXStick = player.fXStick;
        msgPlayer.fYStick = player.fYStick;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CopyMsgToConsole()
// Desc: Copy a CClientInfoMsg into an SConsole
//--------------------------------------------------------------------------------------
HRESULT Sample::CopyMsgToConsole( _Inout_ SConsole* pConsole, const CClientInfoMsg* pMsg, UINT nIndex )
{
    pConsole->cPlayers = pMsg->rgConsoles[ nIndex ].cPlayers;
    pConsole->xnaddr = pMsg->rgConsoles[ nIndex ].xnaddr;

    for( UCHAR i = 0; i < pConsole->cPlayers; i++ )
    {
        const CClientInfoMsg::_Console::_Player& msgPlayer = pMsg->rgConsoles[ nIndex ].rgPlayers[ i ];
        SPlayer& player = pConsole->Players[ i ];

        player.xuid = msgPlayer.xuid;
        player.nPlayerID = msgPlayer.nPlayerID;
        player.nLocalController = msgPlayer.nLocalController;

        wcscpy_s( player.wstrGamertag, msgPlayer.wstrGamertag );

        player.fXPos = msgPlayer.fXPos;
        player.fYPos = msgPlayer.fYPos;
        player.fXVelocity = msgPlayer.fXVelocity;
        player.fYVelocity = msgPlayer.fYVelocity;
        player.fXStick = msgPlayer.fXStick;
        player.fYStick = msgPlayer.fYStick;
    }

    return S_OK;
}
