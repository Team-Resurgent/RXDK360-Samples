//--------------------------------------------------------------------------------------
// ArcadeLicenseCheck.cpp
//
// Demonstrates how and when to check for license changes for the current title.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgResource.h"
#include "AtgSignIn.cpp"
#include "AtgUtil.h"


//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
const DWORD         TEXT_COLOR = 0xFFFFFFFF;


//--------------------------------------------------------------------------------------
// License masks, in enum and wide char forms.  These license masks should match
// the values specified in the offers within the title's content-package .xlast file.
//--------------------------------------------------------------------------------------
enum LICENSE_MASK
{
    LICENSE_MASK_TRIAL = 0x00000000,
    LICENSE_MASK_FULL = 0x00000001,
    LICENSE_MASK_FALLBACK = LICENSE_MASK_TRIAL,
};

//--------------------------------------------------------------------------------------
// Name: GetLicenseMaskName
// Desc: Retrieve the name for a license mask.  An array of names may not work 
//       if the license mask values are sparse.
//--------------------------------------------------------------------------------------
static const WCHAR* GetLicenseMaskName(DWORD dwLicenseMask )
{
    switch( dwLicenseMask )
    {
        case LICENSE_MASK_TRIAL:
            return L"Trial";

        case LICENSE_MASK_FULL:
            return L"Full";
    }

    // The license mask was not recognized
    return L"Unknown";
}


//--------------------------------------------------------------------------------------
// License check locations, in enum and wide char forms
//--------------------------------------------------------------------------------------
enum LICENSE_CHECK_LOCATION
{
    LICENSE_CHECK_LOCATION_TITLE_INITIALIZE,
    LICENSE_CHECK_LOCATION_SIGNINCHANGED,
    LICENSE_CHECK_LOCATION_CONTENT_INSTALLED,
};

const WCHAR* g_LicenseCheckLocationNames[] =
{
    L"Title Initialize",          // LICENSE_CHECK_LOCATION_TITLE_INITIALIZE,
    L"XN_SYS_SIGNINCHANGED",      // LICENSE_CHECK_LOCATION_SIGNINCHANGED,
    L"XN_LIVE_CONTENT_INSTALLED", // LICENSE_CHECK_LOCATION_CONTENT_INSTALLED,
};


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
const ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_1, L"Signin" },
};
const DWORD NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


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

    HANDLE m_hNotification;
    DWORD m_dwLicenseMask;
    BOOL m_bTitleFullMode;
    LICENSE_CHECK_LOCATION m_dwLastLicenseCheckLocation;
    DWORD m_dwLastLicenseCheckReturnValue;

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    VOID CheckLicenseMask( LICENSE_CHECK_LOCATION dwLocation );
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

    // Initialize autologin
    ATG::SignIn::Initialize( 0, 1, FALSE, 1 );

    // Initialize the user interface
    m_bDrawHelp = FALSE;

    // Register our notification listener
    m_hNotification = XNotifyCreateListener( XNOTIFY_SYSTEM | XNOTIFY_LIVE );
    if( m_hNotification == NULL || m_hNotification == INVALID_HANDLE_VALUE )
    {
        ATG::FatalError( "Failed to create state notification listener.\n" );
    }

    // Initialize the license mask
    m_dwLicenseMask = LICENSE_MASK_FALLBACK;
    m_bTitleFullMode = FALSE;
    m_dwLastLicenseCheckReturnValue = 0xFFFFFFFF;

    // Check the license mask (location #1, title initialization)
    CheckLicenseMask( LICENSE_CHECK_LOCATION_TITLE_INITIALIZE );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    assert( m_hNotification != NULL );  // ensure Initialize() was called

    HRESULT hr = S_OK;

    // Update the current sign-in state
    ATG::SignIn::Update();

    // Check for system notifications
    DWORD dwNotificationID;
    ULONG_PTR ulParam;

    if( XNotifyGetNext( m_hNotification, 0, &dwNotificationID, &ulParam ) )
    {
        switch( dwNotificationID )
        {
            case XN_SYS_SIGNINCHANGED:
                // Check the license mask (location #2, XN_SYS_SIGNINCHANGED notification)
                CheckLicenseMask( LICENSE_CHECK_LOCATION_SIGNINCHANGED );
                break;

            case XN_LIVE_CONTENT_INSTALLED:
                // Check the license mask (location #3, XN_LIVE_CONTENT_INSTALLED notification)
                CheckLicenseMask( LICENSE_CHECK_LOCATION_CONTENT_INSTALLED );
                break;

        } // switch( dwNotificationID )
    } // if( XNotifyGetNext() )

    // Update the full-mode flag.  Note that the title will not revert from full to trial.
    if( ( m_bTitleFullMode == FALSE ) && ( m_dwLicenseMask == LICENSE_MASK_FULL ) )
    {
        m_bTitleFullMode = TRUE;
    }

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Show signin UI
    if( !ATG::SignIn::IsSystemUIShowing() && pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        ATG::SignIn::ShowSignInUI();
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
        m_Font.DrawText( 0, 0, 0xffffffff, L"ArcadeLicenseCheck" );
        m_Font.End();

        m_Font.Begin();

        WCHAR wchRender[ 1024 ]; // text buffer

        swprintf_s( wchRender, L"License Mask: %s", GetLicenseMaskName( m_dwLicenseMask ) );
        m_Font.DrawText( 0, -120, TEXT_COLOR, wchRender );
        swprintf_s( wchRender, L"Last License Check Location: %s", g_LicenseCheckLocationNames[m_dwLastLicenseCheckLocation] );
        m_Font.DrawText( 0, -80, TEXT_COLOR, wchRender );
        swprintf_s( wchRender, L"Last License Check Return Value: %#010x", m_dwLastLicenseCheckReturnValue );
        m_Font.DrawText( 0, -40, TEXT_COLOR, wchRender );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: CheckLicenseMask
// Desc: Checks the license mask, falling back to a default value if the call fails.
//--------------------------------------------------------------------------------------
VOID Sample::CheckLicenseMask( LICENSE_CHECK_LOCATION dwLocation )
{
    // Assign the last-check location, pass or fail
    m_dwLastLicenseCheckLocation = dwLocation;

    // Check the license mask
    m_dwLastLicenseCheckReturnValue = XContentGetLicenseMask( &m_dwLicenseMask, NULL );

    // If the check failed, then fall back to the default license mask
    if( m_dwLastLicenseCheckReturnValue != ERROR_SUCCESS )
    {
        m_dwLicenseMask = LICENSE_MASK_FALLBACK;
    }
}