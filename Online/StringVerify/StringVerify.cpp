//--------------------------------------------------------------------------------------
// StringVerify.cpp
//
// The sample shows how to use the XStringVerify api to take a user inputted string
// and check if it is acceptable. This is done by comparing the string against a
// "bad words" list.
//
// This can be used in many places in the title, such as chatting, choosing names of
// characters or other game objects, basically anytime a user is allowed text input.
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
#define TOP_BACK_COLOR      0xff00007f          // background gradient colors
#define BOTTOM_BACK_COLOR   0xff000000


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,    ATG::HELP_PLACEMENT_1, L"Enter string" },
    { ATG::HELP_B_BUTTON,    ATG::HELP_PLACEMENT_1, L"Signin" },
};
static const DWORD NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    DWORD m_dwUserIndex;
    WCHAR           m_strUserString[128];   // Entered string
    BOOL m_bGoodString;          // Was string verification successful
    XOVERLAPPED m_Overlapped;           // Overlapped struct for virtual keyboard
    BOOL m_bKeyboardActive;      // Is keyboard showing?

    VOID            ShowKeyboardUI();
    BOOL            VerifyString( WCHAR* strUserString );

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
// Name: Initialize()
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

    // Initialize login
    ATG::SignIn::Initialize( 1, 1, TRUE, 1 );

    m_strUserString[0] = L'\0';
    m_bGoodString = TRUE;

    m_bKeyboardActive = FALSE;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get merged input to display help
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Update login
    ATG::SignIn::Update();

    if( ATG::SignIn::AreUsersSignedIn() )
    {
        // Signin UI is not showing anymore, get signed in user info
        for( DWORD i = 0; i < XUSER_MAX_COUNT; ++i )
        {
            if( ATG::SignIn::IsUserSignedIn( i ) )
            {
                m_dwUserIndex = i;
                break;
            }
        }

        // Display the Signin UI again
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        {
            ATG::SignIn::ShowSignInUI();
        }

        // Show the keyboard UI
        if( !m_bKeyboardActive && ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A ) )
        {
            ShowKeyboardUI();
        }

        // Once the virtual keyboard is closed, verify the string
        if( m_bKeyboardActive )
        {
            if( XHasOverlappedIoCompleted( &m_Overlapped ) )
            {
                m_bGoodString = VerifyString( m_strUserString );
                m_bKeyboardActive = FALSE;
            }
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
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
        m_Font.DrawText( 0, 0, 0xffffffff, L"StringVerify" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        if( ATG::SignIn::AreUsersSignedIn() )
        {
            m_Font.DrawText( 0, 150, 0xffffffff, L"String: " );
            m_Font.DrawText( 80, 150, 0xffffffff, m_strUserString );

            // Display the result
            m_Font.DrawText( 0, 250, 0xffffffff, L"String verification result: " );

            if( m_bGoodString )
                m_Font.DrawText( 0xffffff00, L"GOOD" );
            else
                m_Font.DrawText( 0xffffff00, L"BAD" );

            // Draw help
            m_Font.DrawText( 0, -70, 0xffffffff, GLYPH_BACK_BUTTON L" Show help" );
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ShowKeyboardUI()
// Desc: Display the virtual keyboard UI
//--------------------------------------------------------------------------------------
VOID Sample::ShowKeyboardUI()
{
    DWORD dwRet;

    ZeroMemory( &m_Overlapped, sizeof( XOVERLAPPED ) );

    dwRet = XShowKeyboardUI( m_dwUserIndex,     // User from whom to accept input, can be USERINDEX_ANY
                             VKBD_DEFAULT,      // Flags
                             NULL,              // Default entry text
                             L"StringVerify Sample", // Title text
                             L"Enter a string:", // Prompt text
                             m_strUserString,   // Result text
                             sizeof( m_strUserString ) / sizeof( m_strUserString[0] ), // Size of result buffer in characters
                             &m_Overlapped );   // Pointer to XOVERLAPPED object

    assert( dwRet == ERROR_IO_PENDING );

    m_bKeyboardActive = TRUE;
}


//--------------------------------------------------------------------------------------
// Name: VerifyString()
// Desc: Verifies the supplied string and returns TRUE if it is a good word
//--------------------------------------------------------------------------------------
BOOL Sample::VerifyString( WCHAR* strUserString )
{
    STRING_DATA stringData;

    stringData.wStringSize = ( WORD )wcslen( strUserString );
    stringData.pszString = strUserString;

    STRING_VERIFY_RESPONSE* stringResults = NULL;

    // Allocate enough data to hold the returned results
    DWORD dwNumStrings = 1;
    DWORD cbStringResults = sizeof( STRING_VERIFY_RESPONSE ) + ( sizeof( HRESULT ) * dwNumStrings );
    stringResults = ( STRING_VERIFY_RESPONSE* )malloc( cbStringResults );
    assert( stringResults != NULL );

    ZeroMemory( stringResults, cbStringResults );

    DWORD dwRet;
    dwRet = XStringVerify( 0,              // No special flags
                           "en-us",        // Only this is supported for now
                           dwNumStrings,   // Number of strings to verify
                           &stringData,    // String(s) to verify
                           cbStringResults,// Size of the returned struct
                           stringResults,  // Returned results struct
                           NULL );         // Not using OVERLAPPED

    assert( dwRet == ERROR_SUCCESS );

    HRESULT hrStringResult = stringResults->pStringResult[0];

    free( ( VOID* )stringResults );

    // The the string maybe a bad word or may be in a different language, either way
    // mark it as a bad word.
    if( hrStringResult != S_OK )
    {
        return FALSE;
    }

    return TRUE;
}

