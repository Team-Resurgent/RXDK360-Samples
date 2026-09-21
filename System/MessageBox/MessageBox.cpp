//--------------------------------------------------------------------------------------
// MessageBox.cpp
//
// The sample shows how to display the message box UI.
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
#include "AtgResource.h"
#include "AtgSignIn.h"
#include "AtgUtil.h"


//--------------------------------------------------------------------------------------
// Color values
//--------------------------------------------------------------------------------------
#define MSG_COLOR           D3DCOLOR_ARGB( 0xff, 0xff, 0xff, 0xff )    // message color
#define INFO_COLOR          D3DCOLOR_ARGB( 0xff, 0xff, 0xff, 0x00 )    // information display color

#define TOP_BACK_COLOR      D3DCOLOR_ARGB( 0xff, 0x00, 0x00, 0xff )    // background gradient colors
#define BOTTOM_BACK_COLOR   D3DCOLOR_ARGB( 0xff, 0x00, 0x00, 0x00 )

//--------------------------------------------------------------------------------------
// Button captions
//--------------------------------------------------------------------------------------
LPCWSTR g_pwstrButtons[3] =
{
    L"Yes", L"No", L"Maybe"
};

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nMessage Box" },
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
    BOOL m_bDrawHelp;
    XOVERLAPPED m_Overlapped;               // Overlapped object for message box UI
    WCHAR           m_wstrMessage[256];         // Message box result message
    MESSAGEBOX_RESULT m_Result;                   // Message box button pressed result
    BOOL m_bMessageBoxShowing;

    VOID            ShowMessageBoxUI();

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

    // Initialize logon
    ATG::SignIn::Initialize( 1, 1, FALSE, 1 );

    m_bDrawHelp = FALSE;

    m_wstrMessage[0] = L'\0';
    m_bMessageBoxShowing = FALSE;

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
    if( !ATG::SignIn::AreUsersSignedIn() || ATG::SignIn::IsSystemUIShowing() )
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

    // Show message box if it is not showing already
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A && !m_bMessageBoxShowing )
    {
        ShowMessageBoxUI();
    }

    if( m_bMessageBoxShowing )
    {
        if( XHasOverlappedIoCompleted( &m_Overlapped ) )
        {
            m_bMessageBoxShowing = FALSE;
            DWORD dwResult = XGetOverlappedResult( &m_Overlapped, NULL, TRUE );
            if( dwResult == ERROR_SUCCESS )
            {
                swprintf_s( m_wstrMessage, L"MessageBox button %d pressed.", m_Result.dwButtonPressed );
            }
            else if( dwResult == ERROR_CANCELLED )
            {
                swprintf_s( m_wstrMessage, L"MessageBox was cancelled by user." );
            }
            else
            {
                swprintf_s( m_wstrMessage, L"MessageBox completed with failure 0x%08x.", dwResult );
            }

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
        m_Font.DrawText( 0, 0, MSG_COLOR, L"MessageBox" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, INFO_COLOR, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Display message
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 100, MSG_COLOR, m_wstrMessage );

        m_Font.DrawText( 0, -40, MSG_COLOR, L"Press " GLYPH_A_BUTTON L" to show message box." );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: ShowMessageBoxUI
// Desc: Display the message box UI.
//--------------------------------------------------------------------------------------
VOID Sample::ShowMessageBoxUI()
{
    DWORD dwRet;

    ZeroMemory( &m_Overlapped, sizeof( XOVERLAPPED ) );

    dwRet = XShowMessageBoxUI( ATG::SignIn::GetSignedInUser(),
                               L"Title",                   // Message box title
                               L"Your message goes here",  // Message string
                               ARRAYSIZE( g_pwstrButtons ),// Number of buttons
                               g_pwstrButtons,             // Button captions
                               1,                          // Button that gets focus
                               XMB_ERRORICON,              // Icon to display
                               &m_Result,                  // Button pressed result
                               &m_Overlapped );

    assert( dwRet == ERROR_IO_PENDING );

    m_bMessageBoxShowing = TRUE;
    m_wstrMessage[0] = L'\0';
}
