//--------------------------------------------------------------------------------------
// Friends.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <d3d9.h>
#include <winsockx.h>
#include <xonline.h>
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgResource.h"
#include "AtgSignIn.h"
#include "AtgUtil.h"


//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
const DWORD         TEXT_COLOR = 0xFFFFFFFF;

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );

//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
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
    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Start up Xbox Live functionality using default Secure Network Layer settings
    if( XOnlineStartup() != ERROR_SUCCESS )
    {
        ATG::FatalError( "Failed to start Xbox Live\n" );
    }

    // Initialize signin
    ATG::SignIn::Initialize( 1, 4, TRUE, 4 );

    m_bDrawHelp = FALSE;

    // Set the position for notification popups
    XNotifyPositionUI( XNOTIFYUI_POS_BOTTOMLEFT );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    HRESULT hr = S_OK;

    ATG::SignIn::Update();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // check controllers for input
    for( UINT i = 0; i < XUSER_MAX_COUNT; i++ )
    {
        if( !ATG::SignIn::IsUserOnline( i ) )
        {
            // no player using this controller
            continue;
        }

        ATG::GAMEPAD& gamepad = ATG::Input::m_Gamepads[ i ];

        if( gamepad.wPressedButtons & XINPUT_GAMEPAD_A )
        {
            // Bring up the Friends UI for this player
            DWORD ret = XShowFriendsUI(
                i );                        // PlayerIndex

            if( ret != ERROR_SUCCESS )
            {
                ATG::FatalError( "Unable to launch friends UI, error %d\n", ret );
            }
        }
        else if( gamepad.wPressedButtons & XINPUT_GAMEPAD_B )
        {
            ATG::SignIn::ShowSignInUI();
        }
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    HRESULT hr = S_OK;

    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"Friends" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        m_Font.DrawText( 0, 100, TEXT_COLOR,
            GLYPH_A_BUTTON L": Display Friends UI", ATGFONT_LEFT );
        m_Font.DrawText( 0, 140, TEXT_COLOR,
            GLYPH_B_BUTTON L": Back to sign-in screen", ATGFONT_LEFT );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return hr;
}

