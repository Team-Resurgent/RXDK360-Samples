//--------------------------------------------------------------------------------------
// TurnDetection.cpp
//
// This sample implements a turn detection filter to establish when a player is
// is turning around, by evaluating the shoulder and hip positions.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <AtgNuiCommon.h>

#include "SkeletonTracking.h"
#include "TurnDetectionFilter.h"
#include "Visualization.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------

ATG::HELP_CALLOUT g_HelpCallouts[] = 
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_1, L"Reset filter state" },
};

static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );


//--------------------------------------------------------------------------------------
// Color values used in this sample
//--------------------------------------------------------------------------------------

#define TOP_BACK_COLOR      0xff00007f
#define BOTTOM_BACK_COLOR   0xff000000
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
};


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
    if( FAILED( InitializeSkeletonTracking( m_pd3dDevice) ) )
        return E_FAIL;

    // Initialize visualization
    InitVisualization();

    // Reset the filter state
    ResetTurnDetectionFilter();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------

HRESULT Sample::Update()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Get input from all the gamepads
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Exit help screen if a player presses BACK
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        // Reset the state if requested by user
        ResetTurnDetectionFilter();
    }

    // Update skeleton tracking
    BOOL bNewSTData = UpdateSkeletonTracking();

    // Only update the filter if we have new ST data. Our sample runs at 60Hz
    // but we only get ST data at 30Hz
    if ( bNewSTData )
    {
        // Update turn detection filter with new skeleton data
        NUI_SKELETON_FRAME* pSkeletonFrame = GetSkeletonFrame();
        ATG::ApplyTiltCorrectionInPlayerSpace( pSkeletonFrame, pSkeletonFrame );
        UpdateTurnDetectionFilter( pSkeletonFrame );

        // Update the visualization with the output of the turn detection filter
        INT iSelectedSkeleton = GetSelectedSkeleton();
        FLOAT fTurnAngleInDegrees = GetTurnAngleInDegrees( iSelectedSkeleton );
        BOOL bSearchForFlip, bFoundFlip;
        GetDebugInfo( iSelectedSkeleton, bSearchForFlip, bFoundFlip );
        UpdateVisualization( fTurnAngleInDegrees, bSearchForFlip, bFoundFlip );
    }

    PIXEndNamedEvent();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------

HRESULT Sample::Render()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

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
        // Render the filter visualization
        RenderVisualization( m_pd3dDevice );

        // Render the skeleton tracking visualization
        VisualizeSkeletonTracking();

        // Draw title text
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff,  L"Turn Detection" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Output the number of full turns
        WCHAR tempBuffer[ 256 ];
        INT iNumFlips = GetNumFlips( GetSelectedSkeleton() );
        swprintf_s ( tempBuffer, 200, L"Num Turns: %d\nNum Flips: %d",  iNumFlips / 2, iNumFlips );
        m_Font.SetScaleFactors( 2.0f, 2.0f );
        m_Font.DrawText( 0, 450, 0xffffffff, tempBuffer, ATGFONT_RIGHT );

        m_Font.End();
    }

    PIXEndNamedEvent();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}