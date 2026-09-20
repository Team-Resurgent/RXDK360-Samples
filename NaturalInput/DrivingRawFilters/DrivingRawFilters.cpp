//--------------------------------------------------------------------------------------
// DrivingRawFilters.cpp
//
// The sample consists of driving on a road while using your hands as the steering wheel
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
#include <ATGNUIVisualization.h>

#include "Road.h"
#include "Vehicle.h"
#include "CameraManager.h"



//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_LEFT_SHOULDER, ATG::HELP_PLACEMENT_1, L"Toggle Smoothing" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_1, L"Toggle Tilt Correction" },
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

    ATG::NuiVisualization m_PIP;
    // Transform matrices
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    // Gameplay data
    Vehicle m_Vehicle;
    Road m_Road;

    // Natural Input data
    CameraManager m_CameraManager;
    BOOL m_bJointSmoothingEnabled;
    BOOL m_bJointTiltCorrectionEnabled;


    HRESULT InitializeCameraNetwork( LPSTR strServerAddr );

public:
    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();
};

Sample g_atgApp;

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    ATG::GetVideoSettings( &g_atgApp.m_d3dpp.BackBufferWidth, &g_atgApp.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    g_atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    g_atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    g_atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    g_atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;

    m_bJointSmoothingEnabled = FALSE;
    m_bJointTiltCorrectionEnabled = FALSE;

    HRESULT hr;

    m_PIP.Initialize( m_pd3dDevice, NUI_INITIALIZE_FLAG_USES_SKELETON, NUI_IMAGE_RESOLUTION_640x480 );

    // Initialize simple shaders.
    ATG::SimpleShaders::Initialize( NULL, NULL );

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

    // Load the road's graphical resources
    m_Road.CreateGraphicsResources( m_pd3dDevice, m_Resource );

    // Initialize the natural input device
    if( FAILED( hr = m_CameraManager.InitializeCamera( m_pd3dDevice ) ) )
    {
        ATG_PrintError( "Couldn't create the natural input device.\n" );
        return hr;
    }

    // Create the natural input filters
    if( FAILED( hr = m_Vehicle.CreateFilters() ) )
    {
        ATG_PrintError( "Couldn't create the natural input filters.\n" );
        return hr;
    }


    // Set the transform matrices
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, 1.0f, 10000.0f );

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

    // Toggle smoothing?
    if (pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
    {
        m_bJointSmoothingEnabled = !m_bJointSmoothingEnabled;
    }

    // Toggle tilt correction?
    if (pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
    {
        m_bJointTiltCorrectionEnabled = !m_bJointTiltCorrectionEnabled;
    }

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Update the natural input device
    BOOL bNewFrameReceived = m_CameraManager.CheckForNewSkeletonAndDepthMaps( fElapsedTime );
    m_PIP.SetSkeletons( m_CameraManager.GetSkeletonFrame() );

    if ( bNewFrameReceived )
    {
        // Update the natural input filters
        if ( m_Vehicle.GetAccelerationPedalFilter() )
        {
            m_Vehicle.GetAccelerationPedalFilter()->Update( fElapsedTime, m_CameraManager.GetTrackedSkeleton() );
        }
        if ( m_Vehicle.GetSteeringWheelFilter() )
        {
            m_Vehicle.GetSteeringWheelFilter()->Update( fElapsedTime, m_CameraManager.GetTrackedSkeleton() );
        }
        m_CameraManager.ReleaseDepthMaps();
    }

    // Update the vehicle
    m_Vehicle.Update( fElapsedTime );

    // Update the view from the vehicle's position and facing
    XMFLOAT3 vPositionF3 = m_Vehicle.GetPosition();
    XMVECTOR vPosition = XMLoadFloat3( &vPositionF3 );
    XMFLOAT3 vFacingF3 = m_Vehicle.GetFacing();
    XMVECTOR vFacing = XMLoadFloat3( &vFacingF3 );
    XMVECTOR vAt = vPosition + vFacing;
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matView = XMMatrixLookAtLH( vPosition, vAt, vUp );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This 
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    const static WCHAR strConfidenceLevel[3][5] = { {L"None"}, {L"Low"}, {L"High"} };


    // Draw a gradient filled background
    ATG::RenderBackground( 0xff3f6385, 0xffcebdad );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();

    m_Road.Draw( m_matView, m_matProj );

    // Render the HUD
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
     m_Font.DrawText( 0.0f, 0.0f, 0xffffffff, L"DrivingRawFilters" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0.0f, 0.0f, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
    
    // Render Smoothing and Tilt Correction options
    if ( m_bJointSmoothingEnabled && m_bJointTiltCorrectionEnabled )
    {
        m_Font.DrawText( 0.0f, 40.0f, 0xffffff00, L"Joint Tracking Flags:  Smoothing & Tilt Correction" );
    }
    else if ( m_bJointSmoothingEnabled )
    {
        m_Font.DrawText( 0.0f, 40.0f, 0xffffff00, L"Joint Tracking Flags:  Smoothing" );
    }
    else if ( m_bJointTiltCorrectionEnabled )
    {
        m_Font.DrawText( 0.0f, 40.0f, 0xffffff00, L"Joint Tracking Flags:  Tilt Correction" );
    }
    else
    {
        m_Font.DrawText( 0.0f, 40.0f, 0xffffff00, L"Joint Tracking Flags:  None" );
    }

    WCHAR strRender[1024];
    FLOAT fTextY = 65.0f;

    // Render the steering wheel filter status
    if ( m_Vehicle.GetSteeringWheelFilter() )
    {
        DWORD dwColor = 0xffff0000;
        if ( m_Vehicle.GetSteeringWheelFilter()->GetConfidence() != NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            if ( m_Vehicle.GetUsingPedal() == FALSE )
            {
                dwColor = 0xff00ff00;
            }
            else
            {
                dwColor = 0xffffff00;
            }
        }
        swprintf_s( strRender, L"Steering Confidence: %s", strConfidenceLevel[ m_Vehicle.GetSteeringWheelFilter()->GetConfidence() ] );
        m_Font.DrawText( 0, fTextY, dwColor, strRender );
        fTextY += 25.0f;
        swprintf_s( strRender, L"Steering Rotation: %02f", m_Vehicle.GetSteeringWheelFilter()->GetWheelRotation() );
        m_Font.DrawText( 0, fTextY, dwColor, strRender );
        fTextY += 25.0f;
    }

    // Render the acceleration pedal filter status
    if ( m_Vehicle.GetAccelerationPedalFilter() )
    {
        DWORD dwColor = 0xffff0000;
        if ( m_Vehicle.GetAccelerationPedalFilter()->GetConfidence() != NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            if ( m_Vehicle.GetUsingPedal() == TRUE )
            {
                dwColor = 0xff00ff00;
            }
            else
            {
                dwColor = 0xffffff00;
            }
        }
        swprintf_s( strRender, L"Pedal Confidence: %s", strConfidenceLevel[ m_Vehicle.GetAccelerationPedalFilter()->GetConfidence() ] );
        m_Font.DrawText( 0, fTextY, dwColor, strRender );
        fTextY += 25.0f;
        swprintf_s( strRender, L"Pedal Acceleration: %02f", m_Vehicle.GetAccelerationPedalFilter()->GetAcceleration() );
        m_Font.DrawText( 0, fTextY, dwColor, strRender );
        fTextY += 25.0f;
        
    } 

    m_Font.End();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    
 
    float fRadiansSteeringValue = m_Vehicle.GetSteeringWheelFilter()->GetWheelRotation() * XM_PI * 3.0f;
    float fWheelSize = 80.0f;
    
    XMFLOAT2 xmf2Pivot = XMFLOAT2( cosf(fRadiansSteeringValue) * fWheelSize, sinf(fRadiansSteeringValue) * fWheelSize );
    XMFLOAT2 xmf2Pivot2 = xmf2Pivot;
    xmf2Pivot2.x *= -1;
    xmf2Pivot2.y *= -1;
    xmf2Pivot.x += 100.0f;
    xmf2Pivot.y += 600.0f;
    xmf2Pivot2.x += 100.0f;
    xmf2Pivot2.y += 600.0f;

    ATG::DebugDraw::DrawScreenSpaceLine( xmf2Pivot, xmf2Pivot2, 0xFFFFFFFF, 5 );

    const FLOAT drawWidth = 320.0f;
    const FLOAT drawHeight = 240.0f;
    const FLOAT drawX = ( m_d3dpp.BackBufferWidth - drawWidth ) / 2;
    const FLOAT drawY = m_d3dpp.BackBufferHeight - drawHeight;
    m_PIP.RenderSkeletons( drawX, drawY, drawWidth, drawHeight );

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}