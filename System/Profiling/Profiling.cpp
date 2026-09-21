//--------------------------------------------------------------------------------------
// File: Profiling.cpp
//
// Desc: Sample to demonstrate how to setup a project for easy profiling.
//
// Hist: 14.06.05 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <assert.h>
#include "ProfilingSupport.h"


// Global variables to force an actual divide.
static float    g_divisor = 1.0f;
static float    g_offset = 0.0f;


// Rendering color constants.
const DWORD     blue = 0xff0000ff;
const DWORD     black = 0xff000000;
const DWORD     white = 0xffffffff;
const DWORD     cyan = 0xffffff00;


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_B_BUTTON,  ATG::HELP_PLACEMENT_2, L"Toggle\nCalculations" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Application class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    BOOL m_bDoCalculations;

    // Do some calculations to give the performance counters something to record.
    void            DoCalculations();

public:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
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
    // Initialize base member variables
    m_bDrawHelp = FALSE;
    m_bDoCalculations = TRUE;

    // Create the fonts
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Label this function in PIX
    // Labeling large blocks of CPU processing in PIX can be useful.
    CXBBeginEventObject nameEvent( "update" );

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Toggle whether we are doing calculations.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_bDoCalculations = !m_bDoCalculations;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DoCalculations()
// Desc: This function does some meaningless calculations to give the performance
// counters and the trace recording something to report.
//--------------------------------------------------------------------------------------
void Sample::DoCalculations()
{
    // Label this function in PIX
    // Labeling large blocks of CPU processing in PIX can be useful.
    CXBBeginEventObject nameEvent( "doCalculations" );

    const DWORD numItems = 10000;
    static float floatData[ numItems ];
    static int intData[ numItems ];

    // Do some square roots. These will show up on the tracedump report.
    for( DWORD i = 0; i < numItems; ++i )
    {
        floatData[ i ] = sqrt( floatData[ i ] );
    }

    // Do some division--divide by a variable to make it harder for the compiler
    // to replace division by multiplication. These will show up on the
    // tracedump report.
    float divisor = g_divisor;
    for( DWORD i = 0; i < numItems; ++i )
    {
        floatData[ i ] = floatData[ i ] / divisor;
        divisor += g_offset;
    }

    // Do some float to int conversions. Because we use the data immediately
    // after converting these should cause some load-hit-store penalties.
    // These will show up on the tracedump report and the PMC counter display.
    for( DWORD i = 0; i < numItems; ++i )
    {
        intData[ i ] = ( int )( floatData[ i ] )*4;
    }
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Label this function in PIX
    CXBBeginEventObject nameEvent( "render" );

    // Normally this profiling object would be at the top of your master frame loop
    // so that you coud profile a full frame. However, the samples frame loop is in
    // the shared library.
    ProfileRecording profile;

    // Record a trace of the rendering function when requested with "XBTrigger main"
    if( Profiling_CheckCommand( "main" ) )
    {
        // Store the trace file on e:\ (also known as devkit:\) which is always
        // available for trace recording. To make e: available for other non-ship
        // purposes use DmMapDevkitDrive.
        profile.BeginCapture( "devkit:\\trace_main.pix2" );
    }

    // Record and printout performance counters when requested with "XBTrigger pmcmain".
    // To use a different set of counters specify it as a second parameter, i.e.;
    // "XBTrigger pmcmain 16"
    // See pmcpbsetup.h for the available values.
    if( Profiling_CheckCommand( "pmcmain" ) )
        profile.BeginPMC( PMC_SETUP_OVERVIEW_PB0T0 + Profiling_GetInt() );

    // Draw a gradient filled background
    ATG::RenderBackground( blue, black );

    // Do some calculations to give the profiling something to record.
    if( m_bDoCalculations )
        DoCalculations();

    m_Font.Begin();

    if( m_bDoCalculations )
        m_Font.DrawText( 0, 60, white, L"Doing expensive calculations." );
    else
        m_Font.DrawText( 0, 60, white, L"Not doing expensive calculations." );

    m_Font.End();

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
        m_Font.DrawText( 0, 0, white, L"Profiling" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, cyan, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
