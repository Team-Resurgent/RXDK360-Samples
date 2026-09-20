//--------------------------------------------------------------------------------------
// BigButtonController.cpp
//
// The sample demonstrates how to get input from the Big-Button controllers. The idea
// is to accuratelly determine which controller was pressed first.
//
// XNA Developer Connection.
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
#include "AtgSimpleShaders.h"
#include "AtgDebugDraw.h"


//--------------------------------------------------------------------------------------
// Invalid user index
//--------------------------------------------------------------------------------------
#define INVALID_USER_INDEX  0xffffffff

//--------------------------------------------------------------------------------------
// Color values
//--------------------------------------------------------------------------------------
#define SEL_COLOR           0xff505050          // selection color
#define UNSEL_COLOR         0xffffffff          // unselection color

#define MSG_COLOR           0xffffffff          // message color
#define INFO_COLOR          0xffffff00          // information display color

#define TOP_BACK_COLOR      0xff7f7f7f          // background gradient colors
#define BOTTOM_BACK_COLOR   0xff000000


//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
static const D3DCOLOR g_ControllerColors[] = { 0xff00ff00, 0xffff0000, 0xff0000ff, 0xffffff00 };

static const D3DCOLOR g_ControllerColorsSelected[] = { 0xff005000, 0xff500000, 0xff000050, 0xff505000 };

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
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
    BOOL m_bDrawHelp;

    DWORD m_dwFirstUserIndex;
    XINPUT_STATE m_InputState;

    VOID            GetFirstControllerPressed();

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
    m_bDrawHelp = FALSE;

    m_dwFirstUserIndex = INVALID_USER_INDEX;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Initialize the simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize Live
    if( FAILED( XOnlineStartup() ) )
        return E_FAIL;

    // Initialize logon
    ATG::SignIn::Initialize( 1, 4, FALSE, 4 );

    // Start sample by showing the signin UI for one user
    ATG::SignIn::ShowSignInUI();

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

    // Find which controller was pressed first
    GetFirstControllerPressed();

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
        m_Font.DrawText( 0, 0, MSG_COLOR, L"BigButtonController" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, INFO_COLOR, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();

        // Display controllers and their state
        for( DWORD dwUserIndex = 0; dwUserIndex < XUSER_MAX_COUNT; ++dwUserIndex )
        {
            XINPUT_STATE InputState = {0};

            // This controller was pressed first
            FLOAT fLineWidth = 1.0f;
            if( dwUserIndex == m_dwFirstUserIndex )
            {
                fLineWidth = 4.0f;
                InputState = m_InputState;
            }

            FLOAT fXOffset = ( FLOAT )( dwUserIndex * 140 );

            // Draw controller rectangle
            D3DRECT ControllerRect;
            ControllerRect.x1 = ( LONG )fXOffset + ATG::GetTitleSafeArea().x1;
            ControllerRect.y1 = 80 + ATG::GetTitleSafeArea().y1;
            ControllerRect.x2 = ControllerRect.x1 + 80;
            ControllerRect.y2 = ControllerRect.y1 + 270;

            ATG::DebugDraw::DrawScreenSpaceRect( ControllerRect, fLineWidth, g_ControllerColors[dwUserIndex] );

            // Draw controller buttons and their state
            m_Font.Begin();
            m_Font.SetScaleFactors( 2.0f, 2.0f );
            m_Font.DrawText( 15 + fXOffset, 94, InputState.Gamepad.wButtons & XINPUT_GAMEPAD_BIGBUTTON ?
                             g_ControllerColorsSelected[dwUserIndex] : g_ControllerColors[dwUserIndex],
                             GLYPH_FILLED_CIRCLE );

            m_Font.SetScaleFactors( 1.0f, 1.0f );

            m_Font.DrawText( 6 + fXOffset, 105, InputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT ?
                             g_ControllerColorsSelected[dwUserIndex] : g_ControllerColors[dwUserIndex],
                             GLYPH_LEFT_ARROW );
            m_Font.DrawText( 48 + fXOffset, 105, InputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ?
                             g_ControllerColorsSelected[dwUserIndex] : g_ControllerColors[dwUserIndex],
                             GLYPH_RIGHT_ARROW );
            m_Font.DrawText( 26 + fXOffset, 85, InputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP ?
                             g_ControllerColorsSelected[dwUserIndex] : g_ControllerColors[dwUserIndex],
                             GLYPH_UP_ARROW );
            m_Font.DrawText( 28 + fXOffset, 125, InputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN ?
                             g_ControllerColorsSelected[dwUserIndex] : g_ControllerColors[dwUserIndex],
                             GLYPH_DOWN_ARROW );

            m_Font.DrawText( 28 + fXOffset, 160, InputState.Gamepad.wButtons & XINPUT_GAMEPAD_A ? SEL_COLOR :
                             UNSEL_COLOR, GLYPH_A_BUTTON );
            m_Font.DrawText( 28 + fXOffset, 200, InputState.Gamepad.wButtons & XINPUT_GAMEPAD_B ? SEL_COLOR :
                             UNSEL_COLOR, GLYPH_B_BUTTON );
            m_Font.DrawText( 28 + fXOffset, 240, InputState.Gamepad.wButtons & XINPUT_GAMEPAD_X ? SEL_COLOR :
                             UNSEL_COLOR, GLYPH_X_BUTTON );
            m_Font.DrawText( 28 + fXOffset, 280, InputState.Gamepad.wButtons & XINPUT_GAMEPAD_Y ? SEL_COLOR :
                             UNSEL_COLOR, GLYPH_Y_BUTTON );
            m_Font.DrawText( 28 - 20 + fXOffset, 310, InputState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK ? SEL_COLOR :
                             UNSEL_COLOR, GLYPH_WHITE_BUTTON );
            m_Font.DrawText( 28 + 20 + fXOffset, 310, InputState.Gamepad.wButtons & XINPUT_GAMEPAD_START ? SEL_COLOR :
                             UNSEL_COLOR, GLYPH_WHITE_BUTTON );

            m_Font.End();
        }
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        m_Font.DrawText( 40.0f, 100.0f, MSG_COLOR, L"Please use the Guide to signin." );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GetFirstControllerPressed
// Desc: Gets the user who pressed any button on a controller. The algorithm accuratelly
//       determines the first user who pressed a button.
//--------------------------------------------------------------------------------------
VOID Sample::GetFirstControllerPressed()
{
    DWORD dwCurrentUserIndex = m_dwFirstUserIndex;

    m_dwFirstUserIndex = INVALID_USER_INDEX;

    DWORD dwFirstPacketNumber = ( DWORD )-1;
    XINPUT_STATE InputState;
    BOOL bAnyControllerActive = FALSE;

    // Loop through all the controllers and if any button is pressed,
    // determine the first controller from the packet number.
    for( DWORD dwUserIndex = 0; dwUserIndex < XUSER_MAX_COUNT; ++dwUserIndex )
    {
        if( ATG::SignIn::IsUserSignedIn( dwUserIndex ) )
        {
            if( XInputGetStateEx( dwUserIndex, XINPUT_FLAG_BIGBUTTON, &InputState ) == ERROR_SUCCESS )
            {
                if( InputState.Gamepad.wButtons )
                {
                    if( InputState.dwPacketNumber < dwFirstPacketNumber )
                    {
                        dwFirstPacketNumber = InputState.dwPacketNumber;
                        m_dwFirstUserIndex = dwUserIndex;
                        m_InputState = InputState;
                    }
                    bAnyControllerActive = TRUE;
                }
            }
        }
    }

    // Do not allow another controller to "steal" the current user's input
    if( bAnyControllerActive && dwCurrentUserIndex != INVALID_USER_INDEX )
    {
        m_dwFirstUserIndex = dwCurrentUserIndex;
        XInputGetStateEx( m_dwFirstUserIndex, XINPUT_FLAG_BIGBUTTON, &m_InputState );
    }
}
