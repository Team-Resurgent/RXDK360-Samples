//--------------------------------------------------------------------------------------
// GestureDetection.cpp
//
// This sample implements gesture detection filters to establish when a player is
// ducking or jumping.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <nuiapi.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <AtgNuiCommon.h>
#include <AtgDebugDraw.h>
#include <deque>

#include "Game.h"
#include "SkeletonTracking.h"
#include "GestureDetectionFilters.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------

ATG::HELP_CALLOUT g_HelpCallouts[] = 
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_A_BUTTON,    ATG::HELP_PLACEMENT_1, L"Restart game" }
};

static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

//--------------------------------------------------------------------------------------
// Defines and constants
//--------------------------------------------------------------------------------------

static const FLOAT fDuckThreshold = 0.80f;  // 80% probability results in bool decision
static const FLOAT fJumpThreshold = 0.80f;

// visualize 3 30Hz frames of probability data from filters
static const UINT g_uNumValuesToVisualize = 3 * 30;


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

    HRESULT InitializeFilters();
    VOID ResetFilters();
    VOID UpdateFilters();

    VOID RenderProbabilityValues();
    VOID RenderUI();

    // General sample data
    ATG::Timer              m_Timer;
    ATG::Font               m_Font;
    ATG::Help               m_Help;
    ATG::PackedResource     m_Resource;
    BOOL                    m_bDrawHelp;
    D3DTexture*             m_pGraphBackgroundTexture;

    // The mini game
    Game                   m_Game;

    // Detection filters
    DuckDetectionFilter    m_DuckGesture;
    JumpDetectionFilter    m_JumpGesture;

    // For debug visualization of probability values
    std::deque<FLOAT>       m_RingBufferDuckProbabilities;
    std::deque<FLOAT>       m_RingBufferJumpProbabilities;
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------

VOID __cdecl main()
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

    // Initialize the simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create the help, font and resources
    RETURN_ON_FAIL( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) );
    RETURN_ON_FAIL( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) );
    RETURN_ON_FAIL( m_Resource.Create( "d:\\Media\\Resource.xpr" ) );
    RETURN_ON_NULL( m_pGraphBackgroundTexture = m_Resource.GetTexture( "GraphBackground" ) );

    // Initialize game
    RETURN_ON_FAIL( m_Game.Initialize( m_pd3dDevice, &m_Resource ) );

    // Initialize the camera and skeleton tracking
    RETURN_ON_FAIL( InitializeSkeletonTracking( m_pd3dDevice) );

    // Initialize filters
    RETURN_ON_FAIL( InitializeFilters() );

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Fill the ring buffer with values
    m_RingBufferDuckProbabilities.resize( g_uNumValuesToVisualize, 0.0f );
    m_RingBufferJumpProbabilities.resize( g_uNumValuesToVisualize, 0.0f );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------

HRESULT Sample::Update()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Exit help screen if a player presses BACK
    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A ||
         !PlayerIsTracked() )
    {
        // Reset filter and game state
        ResetFilters();
        m_Game.Reset();
    }

    static EMovement eMovement = MOVEMENT_NONE;

    // Update skeleton tracking
    BOOL bNewSTData = UpdateSkeletonTracking();
    UINT uSkeletonIdx = GetSelectedSkeleton();   

    // Only update the filter if we have new ST data.
    if ( bNewSTData )
    {
        // Update detection filters with new skeleton data
        UpdateFilters();

        if ( m_JumpGesture.IsDetected( uSkeletonIdx, fJumpThreshold ) )
        {
            eMovement = MOVEMENT_UP;
        }
        else if ( m_DuckGesture.IsDetected( uSkeletonIdx, fDuckThreshold ) )
        {
            eMovement = MOVEMENT_DOWN;
        }
        else
        {
            eMovement = MOVEMENT_NONE;
        }

        // Update the probability debug visualizations
        m_RingBufferDuckProbabilities.pop_front();
        m_RingBufferDuckProbabilities.push_back( m_DuckGesture.GetProbability( uSkeletonIdx ) );
        m_RingBufferJumpProbabilities.pop_front();
        m_RingBufferJumpProbabilities.push_back( m_JumpGesture.GetProbability( uSkeletonIdx ) );
    }

    // Update game
    m_Game.Update( eMovement );

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

    // Show title, frame rate, and help
    m_Timer.MarkFrame();

    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFFC0C0FF, 1.0f, 0 );

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Render the game
        m_Game.Render();
   
        // Render the history of probability values
        RenderProbabilityValues();

        // Render skeleton tracking debug data
        VisualizeSkeletonTracking();

        // Render UI
        RenderUI();
    }

    PIXEndNamedEvent();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Renders the sample's UI
//--------------------------------------------------------------------------------------

VOID Sample::RenderUI()
{
    static UINT uNumFrames          = 0;
    const UINT uDisplayNumFrames    = 30 * 5;   // Display instructions for 5 seconds

    // Draw title text
    m_Font.Begin();

    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, 0xffffffff,  L"Gesture Detection" );

    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

    m_Font.SetScaleFactors( 0.6f, 0.6f );
    m_Font.DrawText( 0, 160, 0xff8f8f8f, L"Jump Probability" );
    m_Font.DrawText( 0, 520, 0xff8f8f8f, L"Duck Probability" );

    // Output the game score
    WCHAR tempBuffer[ 256 ];
    swprintf_s ( tempBuffer, 200, L"Score: %d\nHigh Score: %d",  m_Game.GetScore(), m_Game.GetHighScore() );
    m_Font.SetScaleFactors( 1.5f, 1.5f );
    m_Font.DrawText( 0, 50, 0xff8f8f8f, tempBuffer, ATGFONT_RIGHT );

    // Show game play instructions for the first N frames
    if ( uNumFrames < uDisplayNumFrames )
    {
        m_Font.SetScaleFactors( 1.5f, 1.5f );
        m_Font.DrawText( m_d3dpp.BackBufferWidth / 2.0f, m_d3dpp.BackBufferHeight - 150.0f, 0xffffff00, L"Jump and duck through the hoops", ATGFONT_CENTER_X );
        uNumFrames++;
    }

    m_Font.End();
}


//--------------------------------------------------------------------------------------
// Name: InitializeFilters()
// Desc: Initialize filters
//--------------------------------------------------------------------------------------

HRESULT Sample::InitializeFilters()
{
    RETURN_ON_FAIL( m_DuckGesture.Initialize() );
    RETURN_ON_FAIL( m_JumpGesture.Initialize( GetSkeletonFrame()) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ResetFilters()
// Desc: Reset filters
//--------------------------------------------------------------------------------------

VOID Sample::ResetFilters()
{
    m_DuckGesture.Reset();
    m_JumpGesture.Reset();
}


//--------------------------------------------------------------------------------------
// Name: UpdateFilters()
// Desc: Update filters
//--------------------------------------------------------------------------------------

VOID Sample::UpdateFilters()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    NUI_SKELETON_FRAME* pSkeletonFrame = GetSkeletonFrame();

    // Low latency smoothing to mainly get rid of jitters
    NuiTransformSmooth( pSkeletonFrame, NULL );

    // Apply tilt correction since some of the heuristics of the filters operate in world space
    ATG::ApplyTiltCorrectionInPlayerSpace( pSkeletonFrame, pSkeletonFrame );
    
    // Update the filters with smoothed tilt corrected data
    m_DuckGesture.Update( pSkeletonFrame );
    m_JumpGesture.Update( pSkeletonFrame );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderProbabilityValues()
// Desc: Debug rendering to show probability values for detection
//--------------------------------------------------------------------------------------
VOID Sample::RenderProbabilityValues()
{
   PIXBeginNamedEvent( 0, __FUNCTION__ );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    const UINT uWidth   = 200;
    const UINT uHeight  = 100;
    const UINT uLeft    = 65;
    const UINT uTop     = 130;
    const UINT uBorderX = 10;
    const UINT uBorderY = 5;
    
    D3DRECT Rect;
    Rect.x1 = uLeft;
    Rect.x2 = Rect.x1 + uWidth;
    Rect.y1 = uTop;
    Rect.y2 = Rect.y1 + uHeight;
    ATG::DebugDraw::DrawScreenSpaceTexturedRectColored( Rect, m_pGraphBackgroundTexture, D3DCOLOR_ARGB( 127, 127, 127, 127 ) );

    const UINT uNumValues = m_RingBufferJumpProbabilities.size();
    for ( UINT i = 0; i < uNumValues; i++ )
    {
        Rect.x1 = uLeft + uBorderX + ( i * 2 );
        Rect.x2 = uLeft + uBorderX + ( i * 2 ) + 2;
        Rect.y1 = uTop + uBorderY;
        Rect.y2 = uTop + uHeight - uBorderY;
        FLOAT fProbabilty = m_RingBufferJumpProbabilities[ i ];
        Rect.y1 = Rect.y2 - (LONG)( fProbabilty * (FLOAT)( Rect.y2 - Rect.y1 ) );
        ATG::DebugDraw::DrawScreenSpaceRect( Rect, 1.0f, D3DCOLOR_ARGB( 127, 255, 255, 255 ) );
    }

    Rect.x1 = uLeft + uBorderX;
    Rect.x2 = uLeft + uWidth - uBorderX;
    Rect.y1 = uTop + uBorderY;
    Rect.y2 = uTop + uHeight - uBorderY;
    Rect.y1 = Rect.y2 - (LONG)( fJumpThreshold * (FLOAT)( Rect.y2 - Rect.y1 ) );
    Rect.y2 = Rect.y1;
    ATG::DebugDraw::DrawScreenSpaceRect( Rect, 1.0f, D3DCOLOR_ARGB( 127, 0, 0, 255 ) );


    Rect.x1 = uLeft;
    Rect.x2 = uLeft + uWidth;
    Rect.y1 = m_d3dpp.BackBufferHeight - uTop - uHeight;
    Rect.y2 = m_d3dpp.BackBufferHeight - uTop;    
    ATG::DebugDraw::DrawScreenSpaceTexturedRectColored( Rect, m_pGraphBackgroundTexture, D3DCOLOR_ARGB( 127, 127, 127, 127 ) );

    for ( UINT i = 0; i < m_RingBufferDuckProbabilities.size(); i++ )
    {
        Rect.x1 = uLeft + uBorderX + ( i * 2 );
        Rect.x2 = uLeft + uBorderX + ( i * 2 ) + 2;
        Rect.y1 = m_d3dpp.BackBufferHeight - uTop - uHeight + uBorderY;
        Rect.y2 = m_d3dpp.BackBufferHeight - uTop - uBorderY;
        FLOAT fProbabilty = m_RingBufferDuckProbabilities[ i ];
        Rect.y1 = Rect.y2 - (LONG)( fProbabilty * (FLOAT)( Rect.y2 - Rect.y1 ) );
        ATG::DebugDraw::DrawScreenSpaceRect( Rect, 1.0f, D3DCOLOR_ARGB( 127, 255, 255, 255 ) );
    }

    Rect.x1 = uLeft + uBorderX;
    Rect.x2 = uLeft + uWidth - uBorderX;
    Rect.y1 = m_d3dpp.BackBufferHeight - uTop - uHeight + uBorderY;
    Rect.y2 = m_d3dpp.BackBufferHeight - uTop - uBorderY;
    Rect.y1 = Rect.y2 - (LONG)( fDuckThreshold * (FLOAT)( Rect.y2 - Rect.y1 ) );
    Rect.y2 = Rect.y1;
    ATG::DebugDraw::DrawScreenSpaceRect( Rect, 1.0f, D3DCOLOR_ARGB( 127, 0, 0, 255 ) );

    PIXEndNamedEvent();
}