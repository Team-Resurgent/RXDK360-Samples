//--------------------------------------------------------------------------------------
// SimpleSeatedSkeletonTracking.cpp
//
// Demonstrates how to retrieve skeleton data from the seated skeleton tracking pipeline
// from Kinect.
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <nuiapi.h>

#include "SkeletonTracking.h"
#include "Visualization.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] = 
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_2,    L"Toggle seated\nskeleton tracking" },    
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_1,    L"Toggle smoothing" },    
};
static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE(g_HelpCallouts);


//--------------------------------------------------------------------------------------
// Color values used in this sample
//--------------------------------------------------------------------------------------
#define BACK_COLOR          0xff282E35          // Background color
#define TEXT_COLOR          0xffffffff

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    // General sample data
    ATG::Timer m_Timer;
    ATG::Font  m_Font;
    ATG::Help  m_Help;
    BOOL       m_bDrawHelp;
    BOOL       m_bUseSeatedST;

    BOOL       m_bManualExposure;
    FLOAT      m_fExposureTime;
};

extern NUI_SKELETON_FRAME          g_SkeletonFrame;

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;
    m_bUseSeatedST = TRUE;

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

    // Initialize the camera and skeleton tracking
    if( FAILED( InitializeSkeletonTracking( m_pd3dDevice, m_bUseSeatedST ) ) )
        return E_FAIL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Update skeleton tracking
    UpdateSkeletonTracking( m_pd3dDevice );

    // Get input from all the gamepads
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Exit help screen if a player presses BACK
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_bUseSeatedST = !m_bUseSeatedST;
        EnableSeatedSkeletonTracking(m_bUseSeatedST);
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        SetSmoothingState( !GetSmoothingState() ); 

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( BACK_COLOR, BACK_COLOR );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        WCHAR tempBuffer[ 200 ];

        BOOL pbTracking[ NUI_SKELETON_COUNT ];
        // Visualize the data streaming from the camera and the skeletons
        VisualizeSkeletonTracking( m_pd3dDevice, pbTracking );

        // Draw title text
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff,  L"Simple Seated Skeleton Tracking" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.DrawText( 525, 460, 0xffffff00, L"Skeleton Tracking:", ATGFONT_RIGHT );
        m_Font.DrawText( 535, 460, 0xffffff00, m_bUseSeatedST ? L"Seated" : L"Full Body" );        
        m_Font.DrawText( 650, 460, 0xffffffff, GLYPH_A_BUTTON );

        m_Font.DrawText( 525, 500, 0xffffff00, L"Smoothing:", ATGFONT_RIGHT );
        m_Font.DrawText( 535, 500, 0xffffff00, GetSmoothingState() ? L"On" : L"Off" );
        m_Font.DrawText( 650, 500, 0xffffffff, GLYPH_B_BUTTON );

        UINT uNumTracked = 0;
        for ( UINT i = 0 ; i < NUI_SKELETON_COUNT ; ++i )
        {
            if (pbTracking[i])
            {
                swprintf_s ( tempBuffer, 200, L"Skeleton %d is Tracked", i );
                m_Font.DrawText( 0, (FLOAT)(505 + uNumTracked * 30), 0xFFFFFFFF, tempBuffer, ATGFONT_RIGHT );
                uNumTracked++;
            }
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}