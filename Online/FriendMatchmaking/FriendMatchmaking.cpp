//--------------------------------------------------------------------------------------
// FriendMatchmaking.cpp
//
// Sample to demonstrate matchmaking with friends
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <AtgUtil.h>
#include <AtgSignin.h>
#include <AtgInput.h>
#include <malloc.h>
#include "FriendMatchmaking.h"
#include "TextMenu.h"
#include "FriendMatchmaking.spa.h"

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
    { MAINMENU_SEARCH, L"Search for friend sessions", TextMenu::FLAG_NORMAL, 0 }
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
    ATG::SignIn::Initialize( 1, 4, TRUE, 4 );

    // Initialize variables
    m_wstrError[ 0 ] = L'\0';
    m_nSelectedSession = 0;
    srand( GetTickCount() );
    m_nNextPlayerID = rand();

    m_hEndpoint = NULL;
    m_hSession = INVALID_HANDLE_VALUE;

    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

    m_pFriendsEnumMemory = NULL;
    m_dwNumFriends = 0;
    m_hFriendsEnum = NULL;

    ZeroMemory( &m_SearchSessionIDs, sizeof( m_SearchSessionIDs ) );
    m_pSearchResults = NULL;

    ZeroMemory( &m_FriendSessions, sizeof( m_FriendSessions ) );
    m_dwNumFriendSessions = 0;

    // Start XOnline
    DWORD dwResult = XOnlineStartup();
    if( FAILED( dwResult ) )
    {
        swprintf_s( m_wstrError,
                    L"XOnlineStartup failed with error %d", dwResult );
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
    m_Font16.DrawText( 0, 0, COLOR_TEXT, L"FriendMatchmaking Sample" );

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


//--------------------------------------------------------------------------------------
// Name: ClearFriendsData()
// Desc: Clear out our old friend data
//--------------------------------------------------------------------------------------
VOID Sample::ClearFriendsData( VOID )
{
    delete[] m_pFriendsEnumMemory;
    m_pFriendsEnumMemory = NULL;
    m_dwNumFriends = 0;
    if( m_hFriendsEnum != NULL )
    {
        XCloseHandle( m_hFriendsEnum );
        m_hFriendsEnum = NULL;
    }

    ZeroMemory( &m_SearchSessionIDs, sizeof( m_SearchSessionIDs ) );
    delete[] m_pSearchResults;
    m_pSearchResults = NULL;

    ZeroMemory( &m_FriendSessions, sizeof( m_FriendSessions ) );
    m_dwNumFriendSessions = 0;
}

//--------------------------------------------------------------------------------------
// Name: ShouldSearchForFriend()
// Desc: Should we consider the given friend when searching for friend sessions?
//--------------------------------------------------------------------------------------
BOOL Sample::ShouldSearchForFriend( const XONLINE_FRIEND* pFriend )
{
    // Ignore pending friends and friends that are not playing this title
    if( ( ( pFriend->dwFriendState & XONLINE_FRIENDSTATE_FLAG_RECEIVEDREQUEST ) == 0 ) &&
        ( ( pFriend->dwFriendState & XONLINE_FRIENDSTATE_FLAG_SENTREQUEST ) == 0 ) &&
        ( pFriend->dwTitleID == TITLEID_XBL_SAMPLES_FRIEND_MATCHMAKING_SAMPLE ) &&
        ( ATG::XNKIDToInt64( pFriend->sessionID ) != (__int64)0 ) )
    {
        return TRUE;
    }

    return FALSE;
}

//--------------------------------------------------------------------------------------
// Name: EnumerateFriends()
// Desc: Enumerate our friends
//--------------------------------------------------------------------------------------
DWORD Sample::EnumerateFriends( VOID )
{
    DWORD dwUser = ATG::SignIn::GetSignedInUser();    

    // Create the enumerator
    DWORD cbResults = 0;
    DWORD dwResult = XFriendsCreateEnumerator(
        dwUser,
        0, MAX_FRIENDS,
        &cbResults,
        &m_hFriendsEnum
        );

    if( dwResult != ERROR_SUCCESS )
    {
        HRESULT hr = HRESULT_FROM_WIN32( dwResult );
        swprintf_s( m_wstrError,
                    L"XFriendsCreateEnumerator failed with error 0x%08x", (DWORD)hr );
        return dwResult;
    }

    // Allocate memory to store our friends information
    m_pFriendsEnumMemory = new BYTE[ cbResults ];

    // Zero out the overlapped structure to prepare for new task
    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

    // Enumerate friends
    dwResult = XEnumerate(
        m_hFriendsEnum,
        m_pFriendsEnumMemory,
        cbResults,
        NULL,
        &m_Overlapped
        );

    if( dwResult != ERROR_IO_PENDING )
    {
        delete[] m_pFriendsEnumMemory;
        m_pFriendsEnumMemory = NULL;
        if( m_hFriendsEnum != NULL )
        {
            XCloseHandle( m_hFriendsEnum );
            m_hFriendsEnum = NULL;
        }

        HRESULT hr = HRESULT_FROM_WIN32( dwResult );
        swprintf_s( m_wstrError,
                    L"XEnumerate failed with error 0x%08x", (DWORD)hr );
        return dwResult;
    }

    return dwResult;
}

//--------------------------------------------------------------------------------------
// Name: SearchForSessions()
// Desc: Query friend sessions
//--------------------------------------------------------------------------------------
DWORD Sample::SearchForSessions(DWORD dwEnumFriendsResult)
{
    DWORD dwResult = ERROR_SUCCESS;

    // We now have presence information for our friends, which includes session IDs.
    // We need to extract these session IDs and use them to perform a search.
    DWORD dwNumSessions = 0;
    const XONLINE_FRIEND* pFriendsList = (const XONLINE_FRIEND*)m_pFriendsEnumMemory;
    if( ( dwEnumFriendsResult == ERROR_SUCCESS ) ||
        ( dwEnumFriendsResult == ERROR_NO_MORE_FILES ) ||
        ( dwEnumFriendsResult == ERROR_FUNCTION_FAILED ) )
    {
        // Note: the XDK says that ERROR_NO_MORE_FILES means that you
        // have no friends. In practice it seems to return
        // ERROR_FUNCTION_FAILED in this case.

        for( DWORD dwFriend = 0; dwFriend < m_dwNumFriends; ++dwFriend )
        {
            ATG::DebugSpew("%s: %S\n", pFriendsList[ dwFriend ].szGamertag, pFriendsList[ dwFriend ].wszRichPresence);

            if( ShouldSearchForFriend(&pFriendsList[dwFriend]) )
            {
                m_SearchSessionIDs[ dwNumSessions++ ] = pFriendsList[ dwFriend ].sessionID;
            }
        }
    }

    // Return immediately if there are no friends in sessions
    if ( dwNumSessions == 0 )
    {
        return dwResult;
    }

    DWORD dwUser = ATG::SignIn::GetSignedInUser();

    // Determine the correct buffer size for our session search
    DWORD cbResults = 0;
    dwResult = XSessionSearchByIds(
        dwNumSessions,
        m_SearchSessionIDs,
        dwUser,
        &cbResults,
        NULL,
        NULL
        );

    if ( dwResult == ERROR_INSUFFICIENT_BUFFER && cbResults > 0 )
    {
        // Allocate memory for our search results
        m_pSearchResults = ( XSESSION_SEARCHRESULT_HEADER * ) new BYTE[ cbResults ];
        ZeroMemory( m_pSearchResults, cbResults );

        dwResult = XSessionSearchByIds(
            dwNumSessions,
            m_SearchSessionIDs,
            dwUser,
            &cbResults,
            m_pSearchResults,
            &m_Overlapped
            );
    }

    if ( dwResult != ERROR_IO_PENDING )
    {
        delete[] m_pSearchResults;
        m_pSearchResults = NULL;

        HRESULT hr = HRESULT_FROM_WIN32( dwResult );
        swprintf_s( m_wstrError,
                    L"XSessionSearchByIds failed with error 0x%08x", (DWORD)hr );
        return dwResult;
    }

    return dwResult;
}

//--------------------------------------------------------------------------------------
// Name: ProcessSearchResults()
// Desc: Gather our search results and populate m_FriendSessions
//--------------------------------------------------------------------------------------
VOID Sample::ProcessSearchResults()
{
    // Loop through our friends again and find the corresponding search results
    const XONLINE_FRIEND* pFriendsList = (const XONLINE_FRIEND*)m_pFriendsEnumMemory;
    for( DWORD dwFriend = 0; dwFriend < m_dwNumFriends; ++dwFriend )
    {
        if( ShouldSearchForFriend( &pFriendsList[dwFriend] ) )
        {
            // Find the search result (if any) for this friend
            XSESSION_SEARCHRESULT* pSearchResult = NULL;
            for( DWORD dwSearchResult = 0; dwSearchResult < m_pSearchResults->dwSearchResults; dwSearchResult++ )
            {
                if ( ATG::XNKIDToInt64( pFriendsList[ dwFriend ].sessionID ) ==
                        ATG::XNKIDToInt64( m_pSearchResults->pResults[ dwSearchResult ].info.sessionID ) )
                {
                    pSearchResult = &m_pSearchResults->pResults[ dwSearchResult ];
                    break;
                }
            }

            if(pSearchResult != NULL)
            {
                // We found a search result for this friend, so add an entry in the m_FriendSessions list
                m_FriendSessions[ m_dwNumFriendSessions ].friendXuid = pFriendsList[ dwFriend ].xuid;
                m_FriendSessions[ m_dwNumFriendSessions ].sessionID = pFriendsList[ dwFriend ].sessionID;
                m_FriendSessions[ m_dwNumFriendSessions ].pSessionSearchResult = pSearchResult;

                MultiByteToWideChar( CP_ACP, 0,
                    pFriendsList[ dwFriend ].szGamertag, XUSER_NAME_SIZE,
                    m_FriendSessions[ m_dwNumFriendSessions ].wszGamertag, XUSER_NAME_SIZE );

                m_dwNumFriendSessions++;
            }
        }
    }
}

//**************************************************************************************
// APPSTATE_SEARCHING
// Search for friend sessions
//**************************************************************************************
HRESULT Sample::InitializeSearching( VOID )
{
    // Clear our previous results
    ClearFriendsData();

    // Start enumerating our friends
    DWORD dwResult = EnumerateFriends();

    if( dwResult == ERROR_SUCCESS || dwResult == ERROR_IO_PENDING )
    {
        m_FriendSearchState = FRIENDSEARCHSTATE_ENUMERATEFRIENDS;
    }
    else
    {
        m_FriendSearchState = FRIENDSEARCHSTATE_DONE;
    }

    return S_OK;
}


HRESULT Sample::UpdateSearching( VOID )
{
    // Don't advance until any overlapped operations have completed
    if( !XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        return S_OK;
    }

    switch( m_FriendSearchState )
    {
    case FRIENDSEARCHSTATE_ENUMERATEFRIENDS:
        {
            // We've finished enumerating our friends
            DWORD dwEnumFriendsResult = XGetOverlappedResult(
                &m_Overlapped,
                &m_dwNumFriends,
                TRUE
                );

            if( dwEnumFriendsResult != ERROR_SUCCESS )
            {
                HRESULT hr = HRESULT_FROM_WIN32( dwEnumFriendsResult );
                swprintf_s( m_wstrError,
                            L"XEnumerate overlapped task failed with error 0x%08x",
                            (DWORD)hr );
                
                m_FriendSearchState = FRIENDSEARCHSTATE_DONE;
            }
            else
            {
                // Now that we have our friend presence data, start searching for sessions
                DWORD dwSessionSearchResult = SearchForSessions(dwEnumFriendsResult);

                if( dwSessionSearchResult == ERROR_SUCCESS || dwSessionSearchResult == ERROR_IO_PENDING )
                {
                    m_FriendSearchState = FRIENDSEARCHSTATE_SESSIONSEARCH;
                }
                else
                {
                    m_FriendSearchState = FRIENDSEARCHSTATE_DONE;
                }
            }

            break;
        }

    case FRIENDSEARCHSTATE_SESSIONSEARCH:
        {
            // We're done searching for our friend sessions
            ProcessSearchResults();
            m_FriendSearchState = FRIENDSEARCHSTATE_DONE;

            break;
        }

    case FRIENDSEARCHSTATE_DONE:
        {
            // We have our search results, so allow the user to select one
            if( m_dwNumFriendSessions )
            {
                INT nActiveSessions = (INT)m_dwNumFriendSessions;

                if( m_nSelectedSession >= nActiveSessions )
                {
                    m_nSelectedSession = max( 0, nActiveSessions - 1 );
                }

                // Check to see if the user is trying to change the selected session
                if( m_pGamepad->wPressedButtons & ( XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_UP ) )
                {
                    BOOL bUp = m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP;

                    m_nSelectedSession += bUp ? -1 : 1;
                    m_nSelectedSession += nActiveSessions;
                    m_nSelectedSession %= nActiveSessions;
                }

                // Check to see if the user is trying to join the selected session!
                if( IS_FORWARD(m_pGamepad) )
                {
                    assert( m_nSelectedSession >= 0 && m_nSelectedSession < MAX_FRIENDS );
                    memcpy( &m_SessionInfo, &m_FriendSessions[ m_nSelectedSession ].pSessionSearchResult->info, sizeof( XSESSION_INFO ) );

                    PushState( APPSTATE_JOINING );
                }
            }

            // Check to see if the user is trying to back out
            if( IS_BACKWARD(m_pGamepad) )
            {
                PopState();
            }

            break;
        }
    }

    return S_OK;
}


HRESULT Sample::RenderSearching( VOID )
{
    HRESULT hr = S_OK;

    static const WCHAR* awstrHeaders[] =
    {
        L"Friend                              ",
        L"Session ID                          ",
    };

    if( m_FriendSearchState != FRIENDSEARCHSTATE_DONE )
    {
        m_Font12.DrawText( m_fCenterX, m_fCenterY, COLOR_HILIGHT, L"Searching...",
                           ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
    }
    else if( m_dwNumFriendSessions == 0 )
    {
        m_Font12.DrawText( m_fCenterX, m_fCenterY, COLOR_HILIGHT, L"No Friend Sessions found.",
                           ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
    }
    else
    {
        FLOAT fY = 4 * m_fFontHeight;
        FLOAT fWidth = 0;

        for( int i = 0; i < ARRAYSIZE( awstrHeaders ); i++ )
        {
            fWidth += m_Font12.GetTextWidth( awstrHeaders[ i ] );
        }

        // Center the list horizontally
        FLOAT fLeftEdge = m_fCenterX - fWidth / 2;

        for( INT i = 0; i < (INT)m_dwNumFriendSessions; i++ )
        {
            FLOAT fX = fLeftEdge;

            for( INT column = 0; column < 2; column++ )
            {
                D3DCOLOR col;
                LPCWSTR wstrRender = NULL;
                WCHAR wstrOutput[ MAX_STRING ];

                // Draw the column headers above the first entry
                if( i == 0 )
                {
                    // draw headers
                    wstrRender = awstrHeaders[ column ];
                    col = COLOR_HEADER;

                    // We know what to draw, so draw it
                    m_Font12.DrawText( fX, fY - m_fFontHeight, col, wstrRender );
                }

                // draw session
                col = i == m_nSelectedSession ? COLOR_HILIGHT : COLOR_TEXT;
                switch( column )
                {
                    case 0:
                        wstrRender = m_FriendSessions[ i ].wszGamertag;
                        break;
                    case 1:
                        wstrRender = wstrOutput;
                        swprintf_s( wstrOutput, L"%016I64X", ATG::XNKIDToInt64(m_FriendSessions[ i ].sessionID) );
                        break;
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
    return S_OK;
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

    m_Font12.DrawText( 380, 5, COLOR_HILIGHT, L"Local color" );
    m_Font12.DrawText( 480, 5, COLOR_TEXT, L"Remote color" );


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

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: CreateSession()
// Desc: Create a new game session
//--------------------------------------------------------------------------------------
HRESULT Sample::CreateSession( UCHAR nHostIndex, BOOL bHost )
{
    HRESULT hr = S_OK;

    DWORD dwUser = ATG::SignIn::GetSignedInUser();

    // Set the context to standard (not ranked)
    XUserSetContext( dwUser, X_CONTEXT_GAME_TYPE, X_CONTEXT_GAME_TYPE_STANDARD );

    DWORD dwFlags = bHost ? ( XSESSION_CREATE_HOST | XSESSION_CREATE_USES_PEER_NETWORK | XSESSION_CREATE_USES_PRESENCE )
        : ( XSESSION_CREATE_USES_PEER_NETWORK | XSESSION_CREATE_USES_PRESENCE );

    ULONGLONG Nonce; // we don't need to hold onto this

    // Create a (non-matchmaking) session
    DWORD dwResult = XSessionCreate(
        dwFlags,
        dwUser,
        PUBLICSLOTS,
        PRIVATESLOTS,
        &Nonce,
        &m_SessionInfo,
        NULL,
        &m_hSession );

    if( dwResult != ERROR_SUCCESS )
    {
        hr = XGetOverlappedExtendedError( NULL );
        swprintf_s( m_wstrError, L"XSessionCreate failed with hresult 0x%08x\n", hr );
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

    DWORD dwResult = XSessionJoinLocal(
        m_hSession,
        m_Consoles.front().cPlayers,
        dwControllers,
        fPrivateSlots,
        NULL );

    if( dwResult != ERROR_SUCCESS )
    {
        hr = XGetOverlappedExtendedError( NULL );
        swprintf_s( m_wstrError, L"XSessionJoinLocal failed with hresult 0x%08x\n", hr );
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

    DWORD dwResult = XSessionJoinRemote(
        m_hSession,
        pConsole->cPlayers,
        xuids,
        fPrivateSlots,
        NULL );

    if( dwResult != ERROR_SUCCESS )
    {
        hr = XGetOverlappedExtendedError( NULL );
        swprintf_s( m_wstrError, L"XSessionJoinRemote failed with hresult 0x%08x\n", hr );
    }

    m_cPlayers += pConsole->cPlayers;

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

    DWORD dwResult = XSessionLeaveLocal(
        m_hSession,
        m_Consoles.front().cPlayers,
        dwControllers,
        NULL );

    if( dwResult != ERROR_SUCCESS )
    {
        hr = XGetOverlappedExtendedError( NULL );
        swprintf_s( m_wstrError, L"XSessionLeaveLocal failed with hresult 0x%08x\n", hr );
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

    DWORD dwResult = XSessionLeaveRemote(
        m_hSession,
        pConsole->cPlayers,
        xuids,
        NULL );

    if( dwResult != ERROR_SUCCESS )
    {
        hr = XGetOverlappedExtendedError( NULL );
        swprintf_s( m_wstrError, L"XSessionLeaveRemote failed with hresult 0x%08x\n", hr );
    }

    m_cPlayers -= pConsole->cPlayers;

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
    }

    m_hSession = INVALID_HANDLE_VALUE;

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
