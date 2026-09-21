//--------------------------------------------------------------------------------------
// MultiDisc.cpp
//
// This sample demonstrates using the XSwapDisc to changed discs in a multiple-
// disc layout, as well as how to handle canceling a disc swap.
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
#include "AtgSimpleShaders.h"
#include "AtgDebugDraw.h"

//--------------------------------------------------------------------------------------
// Color values
//--------------------------------------------------------------------------------------
const DWORD         MSG_COLOR = 0xffffffff;         // message color
const DWORD         DISABLED_COLOR = 0xFF888888;    // message color
const DWORD         INFO_COLOR = 0xffffff00;        // information display color

const DWORD         TOP_BACK_COLOR = 0xff0000ff;    // background gradient colors
const DWORD         BOTTOM_BACK_COLOR = 0xff000000;


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_1, L"Load Disc 1" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_1, L"Load Disc 2" },
    { ATG::HELP_X_BUTTON, ATG::HELP_PLACEMENT_1, L"Load Disc 3" },
    { ATG::HELP_Y_BUTTON, ATG::HELP_PLACEMENT_1, L"Load Disc 4" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_1, L"Cancel Load" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );

const int DISC_SWAP_THREAD_SUCCESS = 1;
const int DISC_SWAP_THREAD_FAIL = 0;

//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer  m_Timer;
    ATG::Font   m_Font;
    ATG::Help   m_Help;
    BOOL        m_bDrawHelp;
    HANDLE      m_hSwapDiscThread;
    WCHAR       m_strMessage[256];
    UCHAR       m_CurrentDisc;
    UCHAR       m_RequestedDisc;
    BOOL        m_bSwappingDiscs;
    BOOL        m_bInitialPromptForFirstDisc;
private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
    static DWORD WINAPI XSwapDiscThreadProc ( LPVOID lpvNextDisc );
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
    m_bSwappingDiscs = FALSE;
    m_strMessage[0] = 0;
    m_hSwapDiscThread = INVALID_HANDLE_VALUE;
    VOID *pData = 0;
    ULONG Size = 0;

    //
    // During initialization, we will retrieve the DiscID that was embedded
    // into the XEX using ImageXEX at build time (BuildLayoutroot.cmd 
    // and the <section> tag within each disc's XEX.xml file).
    // 
    // Titles may want to have an alternate default.xex images for each disc,
    // we use the same executable for all discs to simplify the sample.
    //

    HMODULE hXEX_Handle = GetModuleHandle( "game:\\default.xex" );
    if ( hXEX_Handle == 0 )
        return E_FAIL;

    //
    // The DiscID section stored at build time, using the 1/2/3/4.bin files as a data source.
    // Each of these files stores a CHAR of 0x01, 0x02, 0x03, or 0x04 to identify the disc.
    // When ImageXEX is launched against the title, it stores this CHAR into a read-only 
    // encrypted section block of the XEX, which we can read out here with XGetModuleSection.
    //
    // To test starting with a disc other than Disc 1, build the project, then open the
    // MultiDisc.XGD file directly and launch the emulator from within the layout editor.
    // 

    if ( !XGetModuleSection( hXEX_Handle, "DiscID", &pData, &Size ) )
    {
        ATG::FatalError("XGetModuleSection failed to retrieve the DiscID section.\n"
			"Note: The MultiDisc sample must be launched using disc emulation.\n");
    }
    m_CurrentDisc = *static_cast<UCHAR*>( pData );
    if ( Size != sizeof(UCHAR) || m_CurrentDisc < 1 || m_CurrentDisc > 4 )
    {
        ATG::FatalError("XGetModuleSection returned an unexpected value.\n");
    }


    m_RequestedDisc = m_CurrentDisc;
    m_bInitialPromptForFirstDisc = ( m_CurrentDisc != 1 );

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

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Sample::XSwapDiscThreadProc
// Desc: Asynchronously handles disc swapping.
//--------------------------------------------------------------------------------------
DWORD WINAPI Sample::XSwapDiscThreadProc ( LPVOID lpvNextDisc )
{
    HANDLE          hSwapComplete = NULL;
    DWORD           dwWaitResult = 0;
    DWORD           dwSwapResult = 0;
    UCHAR           ucNextDisc = (UCHAR)lpvNextDisc;

    WCHAR           wszEjectText[100];
    XSWAPDISC_ERROR_TEXT  XSwapDiscErrorText;

    // Create an event that XSwapDisc will use to notify us when it's done
    hSwapComplete = CreateEvent( NULL, FALSE, FALSE, NULL );
    if ( hSwapComplete == 0 )
    {
        ATG::FatalError( "Failure creating the swap disc event object.\n" );
    }
    // Build the string we will use to prompt the gamer if the wrong disc is inserted.
    // Note: You must indicate from within the _game_ UI which disc is required,
    //       as this message is *not* updated if XSwapCancel is called.
    swprintf_s( wszEjectText, L"Please insert the specified disc." );

    // Build the string set for XSwapDisc if the incorrect disc is inserted
    XSwapDiscErrorText.wszTitle   = L"Incorrect Disc";
    XSwapDiscErrorText.wszText    = wszEjectText;
    XSwapDiscErrorText.wszButton  = L"OK";

    // Call the system's XSwapDisc service. 
    // ** NOTE: Before reaching this point, the title should close all file handles to files on the optical disc. **
    dwSwapResult = XSwapDisc( ucNextDisc,
                              hSwapComplete, 
                             &XSwapDiscErrorText );

    if ( ERROR_SUCCESS != dwSwapResult )
    { 
        CloseHandle( hSwapComplete );
        ATG::FatalError( "XSwapDisc return a failure code, possibly due to swapping to the same disc, a swap already in progress, or insufficient resources.\n" );
    }
    else
    { 
        // The request to commence the disc swap procedure completed successfully;
        //  at this point, the gamer has been prompted to insert the next disc. 
        // Suspend the current thread until the disc swap procedure has completed
        dwWaitResult = WaitForSingleObject( hSwapComplete, INFINITE );

        // The system successfully completed the disc swap procedure;
        //  at this point the correct disc is now in the tray and ready to run. 
        CloseHandle( hSwapComplete );
        ExitThread( DISC_SWAP_THREAD_SUCCESS );
    }
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get input from all the gamepads
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;
    
    //
    // Display the cancel button and test for swap success while swapping discs.
    //

    if ( m_bSwappingDiscs )
    {
        if ( ERROR_SUCCESS != WaitForSingleObject( m_hSwapDiscThread, 0 ) )
        { 
            // The Swap Disc Thread is still running:
            // While swapping discs, the only option available is to cancel the disc swap.
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
            {
                // Unless this is the request to install the initial disc: That cannot be cancelled.
                if ( !m_bInitialPromptForFirstDisc )
                {
                    // Cancel swapping disks. This means the user must replace the original disc.
                    XSwapCancel();
                    m_RequestedDisc = m_CurrentDisc;
                    swprintf_s( m_strMessage, L"Cancelling Disc Swap: Re-Insert Disc %u", m_RequestedDisc );
                }
            }
        }
        else
        {
            // The disc swap thread has exited, now test for success.
            m_bSwappingDiscs = false;
            DWORD ExitCode = 0;

            if ( !GetExitCodeThread( m_hSwapDiscThread, &ExitCode ) )
            {
                ATG::FatalError( "An error occurred retrieving the state of the swap disc thread.\n" );
            }
            else
            {
                if ( DISC_SWAP_THREAD_SUCCESS == ExitCode )
                {
                    swprintf_s( m_strMessage, L"Disc swapped successfully." );
                    m_CurrentDisc = m_RequestedDisc;
                    CloseHandle( m_hSwapDiscThread );
                    m_hSwapDiscThread = INVALID_HANDLE_VALUE;
                    
                    // Always turn off this flag once the discs have swapped.
                    m_bInitialPromptForFirstDisc = false;
                }
            }
        }
    }

    if ( !m_bSwappingDiscs )
    {
        // At this point, disc swapping is not occurring. Users may be requesting a disc swap.
        // The requested disc should be the same as the current disc.
        assert( m_RequestedDisc == m_CurrentDisc );

        // Select Disc 1 with A
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A && m_CurrentDisc != 1 )
        {
            m_RequestedDisc = 1;
        }
        // Select Disc 2 with B
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B && m_CurrentDisc != 2 )
        {
            m_RequestedDisc = 2;
        }
        // Select Disc 3 with X
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X && m_CurrentDisc != 3 )
        {
            m_RequestedDisc = 3;
        }
        // Select Disc 4 with Y
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y && m_CurrentDisc != 4 )
        {
            m_RequestedDisc = 4;
        }
        
        if ( m_bInitialPromptForFirstDisc )
        {
            // Hardcode a request for the first disc if the user put in the wrong disc.
            m_RequestedDisc = 1;
            m_bSwappingDiscs = true;
            m_hSwapDiscThread = CreateThread( NULL, 0, XSwapDiscThreadProc, (VOID*)m_RequestedDisc, 0, NULL );
            swprintf_s( m_strMessage, L"The first disc to insert must be Disc 1. Please insert Disc 1 now." );
        }
        else if ( m_RequestedDisc != m_CurrentDisc )
        {
            // If a new requested disc has been selected, start the swap disc thread.
            m_bSwappingDiscs = true;
            m_hSwapDiscThread = CreateThread( NULL, 0, XSwapDiscThreadProc, (VOID*)m_RequestedDisc, 0, NULL );
            swprintf_s( m_strMessage, L"Please insert Disc %u.", m_RequestedDisc );
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
    else 
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, MSG_COLOR, L"Multi Disc" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, INFO_COLOR, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Display message
        m_Font.SetScaleFactors( 0.9f, 0.9f );
        m_Font.DrawText( 55, -80, MSG_COLOR, m_strMessage );

        // Display list of commands
        if ( m_bSwappingDiscs )
        {
            // Animate to indicate rendering thread is still active
            float width = 0;
            float height = 0;
            m_Font.GetTextExtent( m_strMessage, &width, &height, true );
            float Offset = floor( fmodf( static_cast<float>(m_Timer.GetAbsoluteTime()) * 10.0f, 30.0f ) ); 

            m_Font.DrawText( Offset, -80, MSG_COLOR, GLYPH_RIGHT_TICK );
            m_Font.DrawText( width+85-Offset, -80, MSG_COLOR, GLYPH_LEFT_TICK );

            // Only show the Cancel dialog if this isn't the swap to initially start with disc 1.
            if ( !m_bInitialPromptForFirstDisc )
            {
                m_Font.SetScaleFactors( 1.0f, 1.0f );
                m_Font.DrawText( 0, -36, MSG_COLOR, GLYPH_START_BUTTON L" Cancel Disc Swap" );
            }

        }
        else
        {
            m_Font.SetScaleFactors( 1.0f, 1.0f );
            m_Font.DrawText( 0, -36, m_CurrentDisc==1? DISABLED_COLOR : MSG_COLOR, m_CurrentDisc==1? GLYPH_FILLED_CIRCLE L" Load Disc 1" : GLYPH_A_BUTTON L" Load Disc 1" );
            m_Font.DrawText( 0, -10, m_CurrentDisc==2? DISABLED_COLOR : MSG_COLOR, m_CurrentDisc==2? GLYPH_FILLED_CIRCLE L" Load Disc 2" : GLYPH_B_BUTTON L" Load Disc 2" );
            m_Font.DrawText( 200, -36, m_CurrentDisc==3? DISABLED_COLOR : MSG_COLOR, m_CurrentDisc==3? GLYPH_FILLED_CIRCLE L" Load Disc 3" : GLYPH_X_BUTTON L" Load Disc 3" );
            m_Font.DrawText( 200, -10, m_CurrentDisc==4? DISABLED_COLOR : MSG_COLOR, m_CurrentDisc==4? GLYPH_FILLED_CIRCLE L" Load Disc 4" : GLYPH_Y_BUTTON L" Load Disc 4" );
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


