//--------------------------------------------------------------------------------------
// FluidFlow.cpp
//
// Sample of fluid flow simulation with particle advection on GPU
//
// Authored by Pedro Sander, ATI Research
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// ATI 3D Application Research Group.
// Copyright (C) ATI Research, Inc. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include "CFluidFlowSim.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle\nparticles" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_2, L"Add & remove\n particles"  },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_2, L"Restart\nsimulation"  },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//-----------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//-----------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::PackedResource m_Resource;  // Packed resource for the textures
    ATG::Font m_Font;      // Font for drawing text
    ATG::Timer m_Timer;     // Timer
    ATG::Help m_Help;      // Help

    BOOL m_bDrawHelp;
    BOOL m_bDrawParticles;

    CFluidFlowWithInjectors m_Flow;

public:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects.
//-----------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;
    m_bDrawParticles = TRUE;

    srand( ( UINT )( m_Timer.GetAbsoluteTime() * 1000.0f ) );

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize the fluid flow simulator
    if( FAILED( m_Flow.Initialize( 256, 256 ) ) )
        return E_FAIL;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Update()
// Desc: Animate the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Update()
{
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Toggle the drawing of particles
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_bDrawParticles = !m_bDrawParticles;

    // Restart the simulation
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
        m_Flow.Reinit( 0.0f );

    // Increase the number of particles
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        m_Flow.Reinit( 1.0f );

    // Decrease the number of particles
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        m_Flow.Reinit( -1.0f );

    // Update the fluid flow simulation
    m_Flow.Update();

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, to render the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Render the fluid flow simulation results
    m_Flow.Render();

    // Optionally draw the particles
    if( m_bDrawParticles )
        m_Flow.RenderParticles();

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
        m_Font.DrawText( 0, 0, 0xffffffff, L"FluidFlow" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

