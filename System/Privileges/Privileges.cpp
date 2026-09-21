//--------------------------------------------------------------------------------------
// Privileges.cpp
//
// The sample illustrates the use of account privileges. Privileges are downloaded from
// Xbox Live at logon. They are used to control various permissions like multiplayer
// allowed or communications with friends only among others.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <d3d9.h>
#include "xbox.h"
#include "xonline.h"
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgResource.h"
#include "AtgSignIn.h"
#include "AtgUtil.h"


//--------------------------------------------------------------------------------------
// Color values
//--------------------------------------------------------------------------------------
#define INFO_COLOR          0xffffff00          // information display color

#define TOP_BACK_COLOR      0xff00007f          // background gradient colors
#define BOTTOM_BACK_COLOR   0xff000000

struct SIGNEDIN_STATUS
{
    XUSER_SIGNIN_STATE SignInState;
    WCHAR* strStatus;
};

SIGNEDIN_STATUS SignedInStatus[] =
{
    { eXUserSigninState_NotSignedIn, L"Not signed in\n" },
    { eXUserSigninState_SignedInLocally, L"Signed in locally\n" },
    { eXUserSigninState_SignedInToLive, L"Signed in to Live\n" },
};

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_1, L"Signin" },
};
static const DWORD NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


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
    HANDLE m_hNotification;
    BOOL m_bDrawHelp;
    BOOL m_bUserSignedIn;
    BOOL m_bPrivMultiplayer;
    BOOL m_bPrivCommunications;
    BOOL m_bPrivCommunicationsFriends;
    BOOL m_bPrivProfileViewing;
    BOOL m_bPrivProfileViewingFriends;
    BOOL m_bPrivUserContent;
    BOOL m_bPrivUserContentFriends;
    BOOL m_bPrivPurchaseContent;
    BOOL m_bPrivPresence;
    BOOL m_bPrivPresenceFriends;

    XUSER_SIGNIN_STATE m_SignedInStatus;

    VOID            ReadPrivileges( DWORD dwUserIndex );

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
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

    // Initialize Live
    if( FAILED( XOnlineStartup() ) )
        return E_FAIL;

    // Initialize signin
    ATG::SignIn::Initialize( 1, 1, FALSE, 1 );

    // Create notification listener for UI events
    m_hNotification = XNotifyCreateListener( XNOTIFY_SYSTEM );
    if( m_hNotification == NULL || m_hNotification == INVALID_HANDLE_VALUE )
        return E_FAIL;

    m_bUserSignedIn = FALSE;
    m_bPrivMultiplayer = FALSE;
    m_bPrivCommunications = FALSE;
    m_bPrivCommunicationsFriends = FALSE;
    m_bPrivProfileViewing = FALSE;
    m_bPrivProfileViewingFriends = FALSE;
    m_bPrivUserContent = FALSE;
    m_bPrivUserContentFriends = FALSE;
    m_bPrivPurchaseContent = FALSE;
    m_bPrivPresence = FALSE;
    m_bPrivPresenceFriends = FALSE;
    m_SignedInStatus = eXUserSigninState_NotSignedIn;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get merged input to display help
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Update login
    if( ATG::SignIn::Update() )
    {
        m_SignedInStatus = XUserGetSigninState( ATG::SignIn::GetSignedInUser() );
        m_bUserSignedIn = FALSE;
    }

    DWORD dwNotificationId;
    ULONG ulParam;

    if( XNotifyGetNext( m_hNotification, 0, &dwNotificationId, &ulParam ) )
    {
        switch( dwNotificationId )
        {
            case XN_SYS_SIGNINCHANGED:
                //
                // Read privileges when user signin state has changed, if user has signed out,
                // all privileges will be set to FALSE
                //
                if( ATG::SignIn::AreUsersSignedIn() )
                    ReadPrivileges( ATG::SignIn::GetSignedInUser() );
                break;
        }
    }

    if( ATG::SignIn::AreUsersSignedIn() && !ATG::SignIn::IsSystemUIShowing() )
    {
        // Signin UI is not showing anymore, get signed in user info
        if( !m_bUserSignedIn )
        {
            ReadPrivileges( ATG::SignIn::GetSignedInUser() );
            m_bUserSignedIn = TRUE;
        }
    }

    // Display the Signin UI
    if( !ATG::SignIn::IsSystemUIShowing() && pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_bUserSignedIn = FALSE;
        ATG::SignIn::ShowSignInUI();
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
    else
    {
        // Draw title text
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"Privileges" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        if( !m_bUserSignedIn )
        {
            m_Font.SetScaleFactors( 1.2f, 1.2f );
            m_Font.DrawText( 0, 64, 0xffffffff, L"There is no user signed in. Press " GLYPH_B_BUTTON L" to sign in." );
        }
        else
        {
            m_Font.SetScaleFactors( 0.8f, 0.8f );

            // Display privileges

            WCHAR strInfo[256];
            INT i;

            for( i = 0; i < ARRAYSIZE( SignedInStatus ); ++i )
            {
                if( m_SignedInStatus == SignedInStatus[ i ].SignInState )
                {
                    m_Font.DrawText( 0, 50, INFO_COLOR, SignedInStatus[ i ].strStatus );
                    break;
                }
            }


            swprintf_s( strInfo, L"Allow Multiplayer Sessions: %s\n", m_bPrivMultiplayer ? L"Yes" : L"No" );
            m_Font.DrawText( INFO_COLOR, strInfo );

            swprintf_s( strInfo, L"Allow Communications with: %s\n", m_bPrivCommunications ? L"Everyone" :
                        ( m_bPrivCommunicationsFriends ? L"Friends only" : L"Nobody" ) );
            m_Font.DrawText( INFO_COLOR, strInfo );

            swprintf_s( strInfo, L"Allow Profile Viewing of: %s\n", m_bPrivProfileViewing ? L"Everyone" :
                        ( m_bPrivProfileViewingFriends ? L"Friends only" : L"Nobody" ) );
            m_Font.DrawText( INFO_COLOR, strInfo );

            swprintf_s( strInfo, L"Access to User Created Content: %s\n", m_bPrivUserContent ? L"Yes" :
                        ( m_bPrivUserContentFriends ? L"of Friends only" : L"No" ) );
            m_Font.DrawText( INFO_COLOR, strInfo );

            swprintf_s( strInfo, L"Allow Content Purchases: %s\n", m_bPrivPurchaseContent ? L"Yes" : L"No" );
            m_Font.DrawText( INFO_COLOR, strInfo );

            swprintf_s( strInfo, L"Allow Presence: %s\n", m_bPrivPresence ? L"Yes" :
                        ( m_bPrivPresenceFriends ? L"Friends only" : L"No" ) );
            m_Font.DrawText( INFO_COLOR, strInfo );
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ReadPrivilegs
// Desc: Reads user privileges. Because privileges are downloaded at logon, they only
//       need to be read once.
//--------------------------------------------------------------------------------------
VOID Sample::ReadPrivileges( DWORD dwUserIndex )
{
    DWORD dwRes;

    dwRes = XUserCheckPrivilege( dwUserIndex, XPRIVILEGE_MULTIPLAYER_SESSIONS, &m_bPrivMultiplayer );
    if( dwRes != ERROR_SUCCESS )
    {
        m_bPrivMultiplayer = FALSE;
        ATG::DebugSpew( "XUserCheckPrivilege for XPRIVILEGE_MULTIPLAYER_SESSIONS failed.\n" );
    }

    dwRes = XUserCheckPrivilege( dwUserIndex, XPRIVILEGE_COMMUNICATIONS, &m_bPrivCommunications );
    if( dwRes != ERROR_SUCCESS )
    {
        m_bPrivCommunications = FALSE;
        ATG::DebugSpew( "XUserCheckPrivilege for XPRIVILEGE_COMMUNICATIONS failed.\n" );
    }

    dwRes = XUserCheckPrivilege( dwUserIndex, XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY, &m_bPrivCommunicationsFriends );
    if( dwRes != ERROR_SUCCESS )
    {
        m_bPrivCommunicationsFriends = FALSE;
        ATG::DebugSpew( "XUserCheckPrivilege for XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY failed.\n" );
    }

    dwRes = XUserCheckPrivilege( dwUserIndex, XPRIVILEGE_PROFILE_VIEWING, &m_bPrivProfileViewing );
    if( dwRes != ERROR_SUCCESS )
    {
        m_bPrivProfileViewing = FALSE;
        ATG::DebugSpew( "XUserCheckPrivilege for XPRIVILEGE_PROFILE_VIEWING failed.\n" );
    }

    dwRes = XUserCheckPrivilege( dwUserIndex, XPRIVILEGE_PROFILE_VIEWING_FRIENDS_ONLY, &m_bPrivProfileViewingFriends );
    if( dwRes != ERROR_SUCCESS )
    {
        m_bPrivProfileViewingFriends = FALSE;
        ATG::DebugSpew( "XUserCheckPrivilege for XPRIVILEGE_PROFILE_VIEWING_FRIENDS_ONLY failed.\n" );
    }

    dwRes = XUserCheckPrivilege( dwUserIndex, XPRIVILEGE_USER_CREATED_CONTENT, &m_bPrivUserContent );
    if( dwRes != ERROR_SUCCESS )
    {
        m_bPrivUserContent = FALSE;
        ATG::DebugSpew( "XUserCheckPrivilege for XPRIVILEGE_USER_CREATED_CONTENT failed.\n" );
    }

    dwRes = XUserCheckPrivilege( dwUserIndex,
                                 XPRIVILEGE_USER_CREATED_CONTENT_FRIENDS_ONLY, &m_bPrivUserContentFriends );
    if( dwRes != ERROR_SUCCESS )
    {
        m_bPrivUserContentFriends = FALSE;
        ATG::DebugSpew( "XUserCheckPrivilege for XPRIVILEGE_USER_CREATED_CONTENT_FRIENDS_ONLY failed.\n" );
    }

    dwRes = XUserCheckPrivilege( dwUserIndex, XPRIVILEGE_PURCHASE_CONTENT, &m_bPrivPurchaseContent );
    if( dwRes != ERROR_SUCCESS )
    {
        m_bPrivPurchaseContent = FALSE;
        ATG::DebugSpew( "XUserCheckPrivilege for XPRIVILEGE_PURCHASE_CONTENT failed.\n" );
    }

    dwRes = XUserCheckPrivilege( dwUserIndex, XPRIVILEGE_PRESENCE, &m_bPrivPresence );
    if( dwRes != ERROR_SUCCESS )
    {
        m_bPrivPresence = FALSE;
        ATG::DebugSpew( "XUserCheckPrivilege for XPRIVILEGE_PRESENCE failed.\n" );
    }

    dwRes = XUserCheckPrivilege( dwUserIndex, XPRIVILEGE_PRESENCE_FRIENDS_ONLY, &m_bPrivPresenceFriends );
    if( dwRes != ERROR_SUCCESS )
    {
        m_bPrivPresenceFriends = FALSE;
        ATG::DebugSpew( "XUserCheckPrivilege for XPRIVILEGE_PRESENCE_FRIENDS_ONLY failed.\n" );
    }
}




