//--------------------------------------------------------------------------------------
// FirstPerson.cpp
//
// This sample demonstrates visually how various filters smooth and their latency.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xnamath.h>
#include <xffb.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgDebugDraw.h>
#include <AtgSimpleShaders.h>
#include <AtgNuiJointFilter.h>
#include <AtgNuiRelativeCoordinates.h>

#include "CameraManager.h"

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" }
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;    // Timer
    ATG::Font m_Font;     // Font for drawing text
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    ATG::PackedResource m_Resource; // Bundled textures in a packed resource

    // Transform matrices
    XMMATRIX m_matView;
    XMMATRIX m_matProj;
   
    // Natural Input data
    CameraManager m_CameraManager;

    HRESULT InitializeCameraNetwork( LPSTR strServerAddr );
    VOID UpdateFilters( FLOAT fElapsedTime );
    VOID DrawFilters();

public:
    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();

private:
    // The filters that will be used
    ATG::FilterBlendJoint m_FilterBlendJoint;
    ATG::FilterVelDamp m_FilterVelDamp;
    ATG::FilterTaylorSeries m_FilterTaylor1;
    ATG::FilterTaylorSeries m_FilterTaylor2;
    ATG::FilterTaylorSeries m_FilterTaylor3;
    ATG::FilterDoubleExponential m_FilterDoubleExponential1;
    ATG::FilterDoubleExponential m_FilterDoubleExponential2;
    ATG::FilterAdaptiveDoubleExponential m_FilterAdaptiveDoubleExponential;

    ATG::SpineRelativeCameraSpaceCoordinateSystem m_CoordBlendJoint;
    ATG::SpineRelativeCameraSpaceCoordinateSystem m_CoordVeldamp;
    ATG::SpineRelativeCameraSpaceCoordinateSystem m_CoordTaylor1;
    ATG::SpineRelativeCameraSpaceCoordinateSystem m_CoordTaylor2;
    ATG::SpineRelativeCameraSpaceCoordinateSystem m_CoordTaylor3;
    ATG::SpineRelativeCameraSpaceCoordinateSystem m_CoordDoubleExponential1;
    ATG::SpineRelativeCameraSpaceCoordinateSystem m_CoordDoubleExponential2;
    ATG::SpineRelativeCameraSpaceCoordinateSystem m_CoordDoubleExponential3;
    ATG::SpineRelativeCameraSpaceCoordinateSystem m_CoordUnFiltered;

    LPDIRECT3DTEXTURE9 m_pSmileTexture[9];
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
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{

    m_bDrawHelp = FALSE;

    HRESULT hr;

    // Initialize simple shaders and set the renderstates.
    ATG::SimpleShaders::Initialize( NULL, NULL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    // Create the font
    if( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create font\n" );
        return hr;
    }

    // Create the help
    if( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create help\n" );
        return hr;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the textures resource
    if( FAILED( hr = m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create Resource.xpr\n" );
        return hr;
    }
    // Load the Icons that will be used to visualize smoothing
    m_pSmileTexture[0] = m_Resource.GetTexture("Smile1");
    m_pSmileTexture[1] = m_Resource.GetTexture("Smile2");
    m_pSmileTexture[2] = m_Resource.GetTexture("Smile3");
    m_pSmileTexture[3] = m_Resource.GetTexture("Smile4");
    m_pSmileTexture[4] = m_Resource.GetTexture("Smile5");
    m_pSmileTexture[5] = m_Resource.GetTexture("Smile6");
    m_pSmileTexture[6] = m_Resource.GetTexture("Smile7");
    m_pSmileTexture[7] = m_Resource.GetTexture("Smile8");
    m_pSmileTexture[8] = m_Resource.GetTexture("Smile9");

    // Initialize the natural input device
    if( FAILED( hr = m_CameraManager.InitializeCamera( m_pd3dDevice ) ) )
    {
        ATG_PrintError( "Couldn't create the natural input device.\n" );
        return hr;
    }

    // Set the transform matrices
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, .10f, 10000.0f );
    
    m_FilterBlendJoint.Init( 10 );
    m_FilterVelDamp.Init();
    m_FilterTaylor1.Init( 0.25f );
    m_FilterTaylor2.Init( 0.5f );
    m_FilterTaylor3.Init( 0.75f );
    m_FilterDoubleExponential1.Init( 0.5f, 0.5f, 0.5f, 0.05f, 0.04f );
    m_FilterDoubleExponential2.Init( 0.5f, 0.2f, 0.5f, 0.1f, 0.1f );
    m_FilterAdaptiveDoubleExponential.Init();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the elapsed time
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Update the natural input device
    BOOL bNewFrameReceived = m_CameraManager.Update( fElapsedTime, 0 );
    if ( bNewFrameReceived )
    {
        UpdateFilters( fElapsedTime ); 
    }

    return S_OK;
}

VOID Sample::UpdateFilters( FLOAT fElapsedTime )
{
    // Update if skeleton was tracked
    INT iCurrentSkeleton = m_CameraManager.GetClosestPlayerIndex();
    
    if ( m_CameraManager.GetSkeletonFrame()->SkeletonData[iCurrentSkeleton].eTrackingState != NUI_SKELETON_TRACKED )
    {
        // Reset the filters and return
        m_FilterBlendJoint.Reset();
        m_FilterVelDamp.Reset();
        m_FilterTaylor1.Reset();
        m_FilterTaylor2.Reset();
        m_FilterTaylor3.Reset();
        m_FilterDoubleExponential1.Reset();
        m_FilterDoubleExponential2.Reset();
        m_FilterAdaptiveDoubleExponential.Reset();
        return;
    }

    const XMVECTOR *pJoints = &m_CameraManager.GetTrackedSkeleton()->SkeletonPositions[0];//&m_CameraManager.GetSkeleton()->SkeletonData[iCurrentSkeleton].SkeletonPositions[0];
    
    // Pass the joints to the filters
    m_FilterBlendJoint.Update( pJoints );
    m_FilterVelDamp.Update( pJoints );
    m_FilterTaylor1.Update( pJoints );
    m_FilterTaylor2.Update( pJoints );
    m_FilterTaylor3.Update( pJoints );
    m_FilterDoubleExponential1.Update( &m_CameraManager.GetSkeletonFrame()->SkeletonData[iCurrentSkeleton] );
    m_FilterDoubleExponential2.Update( &m_CameraManager.GetSkeletonFrame()->SkeletonData[iCurrentSkeleton] );
    m_FilterAdaptiveDoubleExponential.Update( &m_CameraManager.GetSkeletonFrame()->SkeletonData[iCurrentSkeleton], fElapsedTime );

    // Pass filtered joints to coord systems
    CONST NUI_SKELETON_FRAME* pFrame = m_CameraManager.GetSkeletonFrame();
    INT iSkeletonIndex = m_CameraManager.GetTrackedSkeletonIndex();
    m_CoordUnFiltered.Update( pFrame, iSkeletonIndex, pJoints[NUI_SKELETON_POSITION_HAND_LEFT], pJoints[NUI_SKELETON_POSITION_HAND_RIGHT] );
    m_CoordBlendJoint.Update( pFrame, iSkeletonIndex, m_FilterBlendJoint.GetFilteredJoints()[NUI_SKELETON_POSITION_HAND_LEFT], m_FilterBlendJoint.GetFilteredJoints()[NUI_SKELETON_POSITION_HAND_RIGHT] );
    m_CoordVeldamp.Update( pFrame, iSkeletonIndex, m_FilterVelDamp.GetFilteredJoints()[NUI_SKELETON_POSITION_HAND_LEFT], m_FilterVelDamp.GetFilteredJoints()[NUI_SKELETON_POSITION_HAND_RIGHT]  );
    m_CoordTaylor1.Update( pFrame, iSkeletonIndex, m_FilterTaylor1.GetFilteredJoints()[NUI_SKELETON_POSITION_HAND_LEFT], m_FilterTaylor1.GetFilteredJoints()[NUI_SKELETON_POSITION_HAND_RIGHT] );
    m_CoordTaylor2.Update( pFrame, iSkeletonIndex, m_FilterTaylor2.GetFilteredJoints()[NUI_SKELETON_POSITION_HAND_LEFT], m_FilterTaylor2.GetFilteredJoints()[NUI_SKELETON_POSITION_HAND_RIGHT] );
    m_CoordTaylor3.Update( pFrame, iSkeletonIndex, m_FilterTaylor3.GetFilteredJoints()[NUI_SKELETON_POSITION_HAND_LEFT], m_FilterTaylor3.GetFilteredJoints()[NUI_SKELETON_POSITION_HAND_RIGHT] );
    m_CoordDoubleExponential1.Update( pFrame, iSkeletonIndex, m_FilterDoubleExponential1.GetFilteredJoints()[NUI_SKELETON_POSITION_HAND_LEFT], m_FilterDoubleExponential1.GetFilteredJoints()[NUI_SKELETON_POSITION_HAND_RIGHT] );
    m_CoordDoubleExponential2.Update( pFrame, iSkeletonIndex, m_FilterDoubleExponential2.GetFilteredJoints()[NUI_SKELETON_POSITION_HAND_LEFT], m_FilterDoubleExponential2.GetFilteredJoints()[NUI_SKELETON_POSITION_HAND_RIGHT] );
    m_CoordDoubleExponential3.Update( pFrame, iSkeletonIndex, m_FilterAdaptiveDoubleExponential.GetFilteredJoints()[NUI_SKELETON_POSITION_HAND_LEFT], m_FilterAdaptiveDoubleExponential.GetFilteredJoints()[NUI_SKELETON_POSITION_HAND_RIGHT] );

}

VOID Sample::DrawFilters()
{
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAXANISOTROPY, 16 );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );

    XMVECTOR vFrom = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f );
    XMVECTOR vAt = XMVectorSet( 15.0f, -4.0f, 0.0f, 1.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 1.0f );
    XMMATRIX mView = XMMatrixLookAtLH( vFrom, vAt, vUp );
    
    FLOAT fAspectRatio = 1280.0f / 720.0f;
    XMMATRIX mProjection = XMMatrixPerspectiveFovLH( XM_PI / 12.0f, fAspectRatio, 20.0f, 100.0f );
    
    ATG::SpineRelativeCameraSpaceCoordinateSystem* pCoordSystems[9];
    
    pCoordSystems[0] = &m_CoordUnFiltered;
    pCoordSystems[1] = &m_CoordVeldamp;
    pCoordSystems[2] = &m_CoordBlendJoint;
    pCoordSystems[3] = &m_CoordTaylor1;
    pCoordSystems[4] = &m_CoordTaylor2;
    pCoordSystems[5] = &m_CoordTaylor3;
    pCoordSystems[6] = &m_CoordDoubleExponential1;
    pCoordSystems[7] = &m_CoordDoubleExponential2;
    pCoordSystems[8] = &m_CoordDoubleExponential3;
 
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    static const WCHAR* text[] = {
        L"UnFiltered", L"",
        L"VelDamp", L"", 
        L"Blend", L"", 
        L"Taylor Series", L"Smoothing = 0.25f",
        L"Taylor Series", L"Smoothing = 0.5f",
        L"Taylor Series", L"Smoothing = 0.75", 
        L"Double Exponential", L"0.5f, 0.5f, 0.5f, 0.05f, 0.04f",
        L"Double Exponential", L"0.5f, 0.2f, 0.5f, 0.1f, 0.1f", 
        L"Adaptive Double Exponential", L""};

    for ( int index = 0; index < 9; ++index )
    {
        m_Font.DrawText( 20.0f, (FLOAT)index * 65.0f , 0xffffff00, text[index*2] );
        m_Font.DrawText( 20.0f, (FLOAT)index * 65.0f +28.0f, 0xffffffff, text[index*2+1] );
    }
    m_Font.End();
    
    for ( int iY = 0; iY < 3; ++iY )
    {
        for ( int iX = 0 ; iX < 3; ++iX )
        {
            // Draw the icons that make up the key
            D3DRECT KeyRect;
            KeyRect.x1 = 70;
            KeyRect.x2 = 130;
            KeyRect.y1 = ( iX + iY * 3 ) * 65 + 65;
            KeyRect.y2 = KeyRect.y1 + 60;
            ATG::DebugDraw::DrawScreenSpaceTexturedRect( KeyRect, m_pSmileTexture[iY * 3 + iX] );  

            // Draw the icons in a 3x3 grid based on the filters.
            XMVECTOR vRHand =pCoordSystems[iY * 3 + iX]->GetRightHandReletive();
            XMFLOAT2 fRHandVis;
            XMStoreFloat2( &fRHandVis, vRHand );
            fRHandVis.x *= 640.0f * 2.0f;
            fRHandVis.x += 320.0f + 100.0f * (FLOAT )iX;
            fRHandVis.y *= 360.0f * 2.0f;    
            fRHandVis.y += 360.0f + 100.0f * (FLOAT)iY;  
            // Flip coordinate system because we're moving to screen space
            fRHandVis.y = 720.0f - fRHandVis.y;
            D3DRECT rect;
            rect.x1 = ( LONG )( fRHandVis.x );
            rect.x2 = ( LONG )( fRHandVis.x + 80.0f );
            rect.y1 = ( LONG )( fRHandVis.y );
            rect.y2 = ( LONG )( fRHandVis.y + 80.0f );
            ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect, m_pSmileTexture[iY * 3 + iX] );  
        }    
    }
}

//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This 
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff3f6385, 0xffcebdad );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();

    m_CameraManager.DisplayPIP();
    DrawFilters();
    
    // Render the HUD
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 800.0f, 0.0f, 0xffffffff, L"Filtering" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0.0f, 0.0f, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
    
    // Render Smoothing and Tilt Correction options

    m_Font.End();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }


    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
