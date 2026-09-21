//--------------------------------------------------------------------------------------
// GuideExtensibility.cpp
//
// The sample demonstrates the usage of custom action strings in the Xbox360 Guide.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <d3d9.h>
#include <xam.h>
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgSignIn.h"
#include "AtgUtil.h"



//--------------------------------------------------------------------------------------
// Color values
//--------------------------------------------------------------------------------------
static const DWORD  MSG_COLOR = 0xffffffff;          // message color
static const DWORD  INFO_COLOR = 0xffffff00;         // information display color

static const DWORD  TOP_BACK_COLOR = 0xff0000ff;     // background gradient colors
static const DWORD  BOTTOM_BACK_COLOR = 0xff000000;


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_1, L"Signin" },
    { ATG::HELP_X_BUTTON, ATG::HELP_PLACEMENT_2, L"Enable dynamic\nactions" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_2, L"Show player\nlist UI" },
    { ATG::HELP_Y_BUTTON, ATG::HELP_PLACEMENT_2, L"Show custom\nmessage UI" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    HANDLE m_hNotification;                        // Custom notification handle
    BOOL m_bDynamicActionsEnabled;               // Are dynamic actions enabled?
    XPLAYERLIST_RESULT m_PlayerListResult;                     // Pressed button results from UI
    XOVERLAPPED m_PlayerListOverlapped;
    XOVERLAPPED m_MessageUIOverlapped;
    BOOL m_bUIResultMessageShown;
    WCHAR               m_strMessage[256];
    XMSG_CUSTOMACTION   m_CustomActions[3];
    XPLAYERLIST_USER    m_PlayerList[1];
    XPLAYERLIST_BUTTON m_XButton, m_YButton;


    VOID                InitCustomStaticActions();
    VOID                InitCustomDynamicActions();

private:
    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create custom notification handler
    m_hNotification = XNotifyCreateListener( XNOTIFY_CUSTOM );
    if( m_hNotification == NULL || m_hNotification == INVALID_HANDLE_VALUE )
    {
        ATG_PrintError( "Failed to create custom notification listener.\n" );
        return E_FAIL;
    }

    // Initialize custom actions
    InitCustomStaticActions();

    // Initialize Live
    if( FAILED( XOnlineStartup() ) )
        return E_FAIL;

    // Initialize logon
    ATG::SignIn::Initialize( 1, 1, FALSE, 1 );

    // Start sample by showing the signin UI for one user
    ATG::SignIn::ShowSignInUI();

    m_strMessage[0] = L'\0';
    m_bDynamicActionsEnabled = FALSE;
    m_bUIResultMessageShown = TRUE;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Update login
    ATG::SignIn::Update();

    // If we're not signed in, wait until we are
    if( !ATG::SignIn::AreUsersSignedIn() )
    {
        return S_OK;
    }

    // Get input from all the gamepads
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Show the signin UI
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        ATG::SignIn::ShowSignInUI();
    }

    // Enable dynamic actions
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        InitCustomDynamicActions();
        swprintf_s( m_strMessage, L"Dynamic custom actions enabled." );
    }

    // Show the custom player list UI for the first friend, if one exists
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        DWORD dwErr;

        // Enumerate one friend of the current user
        XONLINE_FRIEND Friends[1];
        HANDLE hFriendsEnum;
        DWORD cbBuffer;
        BOOL fShowPlayerList = FALSE;
        dwErr = XFriendsCreateEnumerator( ATG::SignIn::GetSignedInUser(), 0,
                                          ARRAYSIZE( Friends ), &cbBuffer, &hFriendsEnum );

        if( dwErr == ERROR_SUCCESS )
        {
            DWORD dwFriendsCount;
            dwErr = XEnumerate( hFriendsEnum, Friends, sizeof( Friends ), &dwFriendsCount, NULL );

            if( dwErr == ERROR_SUCCESS )
            {
                fShowPlayerList = TRUE;
                assert( dwFriendsCount == ARRAYSIZE( Friends ) );
            }

            CloseHandle( hFriendsEnum );
        }

        ZeroMemory( &m_PlayerListOverlapped, sizeof( m_PlayerListOverlapped ) );

        if( fShowPlayerList )
        {
            // Use the first friend's XUID
            m_PlayerList[0].xuid = Friends[0].xuid;
            wcscpy_s( m_PlayerList[0].wszCustomText, L"Custom string: Score 2397" );

            // Populate button strings
            m_XButton.dwType = XPLAYERLIST_BUTTON_TYPE_TITLECUSTOM;
            wcscpy_s( m_XButton.wszCustomText, L"Custom button" );

            ZeroMemory( &m_YButton, sizeof( m_YButton ) );
            m_YButton.dwType = XPLAYERLIST_BUTTON_TYPE_GAMEINVITE;

            dwErr = XShowCustomPlayerListUI(
                ATG::SignIn::GetSignedInUser(),
                XPLAYERLIST_FLAG_CUSTOMTEXT,    // Display title supplied text instead of presence strings
                L"Sample Application",          // Title text
                L"Custom player list UI demo",  // Description text
                NULL,                           // Optional buffer containing a 96x96 PNG image file
                0,                              // Size of image buffer
                m_PlayerList,                   // List of players to be displayed
                ARRAYSIZE( m_PlayerList ),      // Number of players
                &m_XButton,                     // X button behavior
                &m_YButton,                     // Y button behavior
                &m_PlayerListResult,            // Button pressed result
                &m_PlayerListOverlapped );

            assert( dwErr == ERROR_IO_PENDING );

            m_bUIResultMessageShown = FALSE;
        }
        else
        {
            LPCWSTR strButtons[] = { L"OK" };
            static MESSAGEBOX_RESULT msgResult;

            // Show message error message box if the current user doesn't have any friends assigned
            dwErr = XShowMessageBoxUI(
                ATG::SignIn::GetSignedInUser(),
                L"Custom Player List UI Error",
                L"In order to display the custom player list UI, the signed in user must have at least one friend.",
                1,
                strButtons,
                0,
                XMB_ERRORICON,
                &msgResult,
                &m_PlayerListOverlapped );

            assert( dwErr == ERROR_IO_PENDING );
        }
    }

    // Handle player list UI results
    if( XHasOverlappedIoCompleted( &m_PlayerListOverlapped ) &&
        XGetOverlappedResult( &m_PlayerListOverlapped, NULL, TRUE ) == ERROR_SUCCESS &&
        !m_bUIResultMessageShown )
    {
        swprintf_s( m_strMessage, L"Game XUID selected: %8x key pressed: %s", ( DWORD )m_PlayerListResult.xuidSelected,
                    m_PlayerListResult.dwKeyCode == VK_PAD_X ? L"X" : L"Y" );

        m_bUIResultMessageShown = TRUE;
    }

    // Show the custom message compose UI
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        DWORD dwErr;

        ZeroMemory( &m_MessageUIOverlapped, sizeof( m_MessageUIOverlapped ) );

        ZeroMemory( m_CustomActions, sizeof( m_CustomActions ) );
        m_CustomActions[0].dwActionId = 0;
        wcscpy_s( m_CustomActions[0].wszEnActionText, L"Join a team" );
        m_CustomActions[0].dwFlags = 0;
        m_CustomActions[1].dwActionId = 1;
        wcscpy_s( m_CustomActions[1].wszEnActionText, L"Show extended stats" );
        m_CustomActions[1].dwFlags = 0;
        m_CustomActions[2].dwActionId = 2;
        wcscpy_s( m_CustomActions[2].wszEnActionText, L"Close the Guide" );
        m_CustomActions[2].dwFlags = XCUSTOMACTION_FLAG_CLOSES_GUIDE;

        dwErr = XShowCustomMessageComposeUI(
            ATG::SignIn::GetSignedInUser(),
            NULL,                           // array of message receipients
            0,                              // number of message recipients
            0,                              // no flags
            L"Message title",
            L"Message body",
            L"Player editable message",
            NULL,                           // buffer containing 96x96 PNG image
            0,                              // image buffer size
            m_CustomActions,                // Custom action buttons
            ARRAYSIZE( m_CustomActions ),
            NULL,                           // Up to 1K custom payload
            0,                              // Custom payload size
            0,                              // Number of minutes before the message expires
            &m_MessageUIOverlapped );

        assert( dwErr == ERROR_IO_PENDING );
    }

    // Handle custom action events
    DWORD dwNotificationId;
    ULONG_PTR ulParam;

    if( XNotifyGetNext( m_hNotification, 0, &dwNotificationId, &ulParam ) )
    {
        switch( dwNotificationId )
        {
                // Custom action buttons have been pressed. The title should be ready to handle custom action
                // messages at all times, as the Guide can be envoked by the user at any time, including during
                // the loading screen.
            case XN_CUSTOM_ACTIONPRESSED:
            {
                DWORD dwUserIndex;
                DWORD dwActionIndex;
                XUID xuid;

                // For dynamic actions, the XCustomGetLastActionPressEx must be used to be able to retrieve the
                // optional up to 1K payload
                if( m_bDynamicActionsEnabled )
                {
                    BYTE bPayload[1024];
                    WORD wPayloadSize = sizeof( bPayload );
                    XCustomGetLastActionPressEx( &dwUserIndex, &dwActionIndex, &xuid, bPayload, &wPayloadSize );

                    swprintf_s( m_strMessage, L"Custom dynamic action %d selected by user %d.", dwActionIndex,
                                dwUserIndex );
                }
                else
                {
                    XCustomGetLastActionPress( &dwUserIndex, &dwActionIndex, &xuid );

                    swprintf_s( m_strMessage, L"Custom static action %d selected by user %d.", dwActionIndex,
                                dwUserIndex );
                }
            }
                break;

            case XN_CUSTOM_GAMERCARD:
                if( ulParam && m_bDynamicActionsEnabled )
                {
                    // Retreive the gamercard currently being veiwed by the player, and if it is,
                    // set appropriate custom dynamic actions
                    DWORD dwViewingUser;
                    XUID ViewedXuid;
                    if( XCustomGetCurrentGamercard( &dwViewingUser, &ViewedXuid ) )
                    {
                        XCUSTOMACTION CustomActions[3];
                        CustomActions[0].wActionId = 0;
                        wcscpy_s( CustomActions[0].wszActionText, L"Join a team" );
                        CustomActions[0].dwFlags = 0;
                        CustomActions[1].wActionId = 1;
                        wcscpy_s( CustomActions[1].wszActionText, L"Show extended stats" );
                        CustomActions[1].dwFlags = 0;
                        CustomActions[2].wActionId = 2;
                        wcscpy_s( CustomActions[2].wszActionText, L"Close the Guide" );
                        CustomActions[2].dwFlags = XCUSTOMACTION_FLAG_CLOSES_GUIDE;

                        XCustomSetDynamicActions( dwViewingUser, ViewedXuid, CustomActions,
                                                  ARRAYSIZE( CustomActions ) );
                    }
                }
                break;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( TOP_BACK_COLOR, BOTTOM_BACK_COLOR );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else if( ATG::SignIn::AreUsersSignedIn() && !ATG::SignIn::IsSystemUIShowing() )
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, MSG_COLOR, L"GuideExtensibility" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, INFO_COLOR, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Draw list of content display names
        m_Font.SetScaleFactors( 1.f, 1.f );
        m_Font.DrawText( 0, -40, MSG_COLOR, GLYPH_X_BUTTON L" Dynamic actions" );
        m_Font.DrawText( 250, -40, MSG_COLOR, GLYPH_Y_BUTTON L" Custom message UI" );
        m_Font.DrawText( 0, -10, MSG_COLOR, GLYPH_B_BUTTON L" Re-sign in" );

        m_Font.DrawText( 0, 100, MSG_COLOR, L"Press the Xbox Guide button to show the Guide." );

        m_Font.DrawText( 0, 200, INFO_COLOR, m_strMessage );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitCustomActions
// Desc: Initializes three (the maximum allowed) Guide static custom actions.
//--------------------------------------------------------------------------------------
VOID Sample::InitCustomStaticActions()
{
    // Custom actions to be handled by the sample
    XCustomSetAction( 0, L"Join a team", 0 );
    XCustomSetAction( 1, L"Show extended stats", 0 );

    // Custom action that is handled by the system (closes the Guide)
    XCustomSetAction( 2, L"Close the Guide", CUSTOMACTION_FLAG_CLOSESUI );
}


//--------------------------------------------------------------------------------------
// Name: InitCustomDynamicActions
// Desc: Initializes dynamic actions. Only one type of action can be used at a time, 
// dynamic or static. When dynamic actions are no longer used, 
// XCustomUnregisterDynamicActions must be called.
//--------------------------------------------------------------------------------------
VOID Sample::InitCustomDynamicActions()
{
    XCustomRegisterDynamicActions();
    m_bDynamicActionsEnabled = TRUE;
}

