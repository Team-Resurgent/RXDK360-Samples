//--------------------------------------------------------------------------------------
// ForwardMovement.cpp
//
// This sample demonstrates a first person based navigation system.
//
// Advanced Technology Group (ATG)
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

#include "Environment.h"
#include "Person.h"
#include "CameraManager.h"





//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,  ATG::HELP_PLACEMENT_1, L"Toggle Rails" }
};


static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );
static const FLOAT g_fGravity = 1.0f;
static const FLOAT g_fJumpDelta = 0.1f;
static const FLOAT g_fJumpScalar = 0.020f;
static CONST INT g_iWaitFramesBetweenPlayerTransition = 50;

static const XMVECTOR g_vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
static const XMVECTOR g_vZeroY = XMVectorSet( 1.0f, 0.0f, 1.0f, 1.0f );


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
   
    // Gameplay data
    Person m_Person;
    Environment m_Environment;

    // Natural Input data
    CameraManager m_CameraManager;

    FLOAT m_fYVelocity;
    XMFLOAT2 m_fWorldPosition;
    XMFLOAT2 m_fDirection;   
    FLOAT m_fPlayerHeight;
    BOOL m_bOnRails;


public:
    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();
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
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{

    m_bDrawHelp = FALSE;
    m_fYVelocity = 0.0f;
    m_fWorldPosition = XMFLOAT2( 25.0f, 7.0f );
    m_fDirection = XMFLOAT2( 1.0f, 0.0f );   
    m_fPlayerHeight = 0;

    m_bOnRails = FALSE;


    HRESULT hr;

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

    // Load the Environment's graphical resources
    m_Environment.CreateGraphicsResources( m_pd3dDevice, m_Resource );
    // Initialize the natural input device
    if( FAILED( hr = m_CameraManager.InitializeCamera( m_pd3dDevice ) ) )
    {
        ATG_PrintError( "Couldn't create the natural input device.\n" );
        return hr;
    }

    // Set the transform matrices
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, .10f, 10000.0f );

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

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_bOnRails = !m_bOnRails;

    // Update the natural input device
    BOOL bNewFrameReceived = m_CameraManager.Update( 0 );
    INT iFirstTrackedSkeleton = -1;

    if ( bNewFrameReceived )
    {
        static INT iLastFrameTrackedSkeleton = -1;
        static INT iTimeOUTFrames = g_iWaitFramesBetweenPlayerTransition;
        CONST NUI_SKELETON_FRAME* pFrame =  m_CameraManager.GetSkeleton();

        for ( INT iIndex = 0; iIndex < NUI_SKELETON_COUNT && iFirstTrackedSkeleton == -1; ++iIndex )
        {
            if ( pFrame->SkeletonData[iIndex].eTrackingState == NUI_SKELETON_TRACKED )
            {
                iFirstTrackedSkeleton = iIndex;
            }
        }
        if ( iLastFrameTrackedSkeleton != iFirstTrackedSkeleton )
        {
            iTimeOUTFrames = g_iWaitFramesBetweenPlayerTransition;
            m_Person.Reset();
        }
        iLastFrameTrackedSkeleton = iFirstTrackedSkeleton;
        if ( iFirstTrackedSkeleton != -1 )
        {
            if ( iTimeOUTFrames > 0 )
            {
                --iTimeOUTFrames;
            }
            else 
            {
                m_Person.UpdateNUI( &pFrame->SkeletonData[iFirstTrackedSkeleton] );
            }
        }
    }

    FLOAT fTurnDirectionSin = sinf( m_Person.GetTurnDirection() );
    FLOAT fTurnDirectionCos = cosf( m_Person.GetTurnDirection() );
    XMFLOAT2 fNewDirection = XMFLOAT2( m_fDirection.x * fTurnDirectionCos - m_fDirection.y * fTurnDirectionSin,
        m_fDirection.x * fTurnDirectionSin + m_fDirection.y * fTurnDirectionCos );
    FLOAT fmag = sqrtf( fNewDirection.x * fNewDirection.x + fNewDirection.y * fNewDirection.y);
    XMFLOAT3 fPersonPosition = m_Person.GetPosition();

    if ( iFirstTrackedSkeleton != -1 )
    {

        m_fDirection.x = fNewDirection.x / fmag;
        m_fDirection.y = fNewDirection.y  / fmag;

        XMFLOAT2 fInc = m_fDirection;
        
        fInc.x *= m_Person.GetSpeed();
        fInc.y *= m_Person.GetSpeed();
        m_fWorldPosition.x += fInc.x;
        m_fWorldPosition.y += fInc.y;

    }

    XMVECTOR vForward = XMVector3Normalize( XMVectorSet( m_fDirection.x, 0, m_fDirection.y, 0 ) );
    XMVECTOR vRight = XMVector3Cross( g_vUp, vForward );
    vRight *= fPersonPosition.x;
    vForward *= fPersonPosition.z;
    

    if ( m_bOnRails )
    {
        m_Environment.UpdateTrailLoc( m_Person.GetSpeed() );
        m_fDirection = m_Environment.GetDirection();
        m_fWorldPosition = m_Environment.GetLocation();
    }

    XMVECTOR vPosition = XMVectorSet( m_fWorldPosition.x, 0.0f, m_fWorldPosition.y, 1);
    vPosition += vRight;
    vPosition += vForward;
    

    FLOAT fGroundHeight = m_Environment.HeightAt( XMVectorGetX( vPosition ), XMVectorGetZ( vPosition ) );

    FLOAT fPersonHeightDiff = m_Person.GetPosition().y - m_Person.GetLastPosition().y;
    FLOAT fTerrainHeightDiff = fGroundHeight - m_fPlayerHeight;

    if ( fPersonHeightDiff > g_fJumpDelta && fTerrainHeightDiff > g_fJumpDelta )
    {
        if ( m_fYVelocity < 0.0f )
        {
            m_fYVelocity = 0.0f;
        }
        m_fYVelocity += g_fJumpScalar * ( fPersonHeightDiff + fTerrainHeightDiff );
    }

    m_fYVelocity -= fElapsedTime * g_fGravity;

    if ( fGroundHeight > m_fPlayerHeight )
    {
        m_fPlayerHeight = 0.8f * m_fPlayerHeight + fGroundHeight * 0.2f;
        m_fYVelocity = max( m_fYVelocity, ( fGroundHeight - m_fPlayerHeight ) * 0.1f ) ;
    }
    else 
    {
        if ( m_fPlayerHeight + m_fYVelocity - fGroundHeight < 0 )
        {
            m_fYVelocity = 0.0f;      
        }
        m_fPlayerHeight = max ( fGroundHeight, ( m_fPlayerHeight + m_fYVelocity ) );
    }

    XMVECTOR vAddInY = XMVectorSet( 0.0f,  
         m_fPlayerHeight + max ( 0.2f, fPersonPosition.y - 3.0f ),
        0.0f, 0.0f );

    vPosition *= g_vZeroY;
    vPosition += vAddInY;
   
    XMVECTOR vAt = XMVectorSet(m_fDirection.x, 0, m_fDirection.y, 0) + vPosition;
    m_matView = XMMatrixLookAtLH( vPosition, vAt, g_vUp );
    return S_OK;
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

    m_Environment.Draw( m_matView, m_matProj );
    m_CameraManager.DisplayPIP();

    // Render the HUD
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0.0f, 0.0f, 0xffffffff, L"Forward Movement" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0.0f, 0.0f, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );    
    if ( m_bOnRails )
        m_Font.DrawText( 0.0f, 30.0f, 0xffffff00, L"On Rails", ATGFONT_LEFT );    
    else
        m_Font.DrawText( 0.0f, 30.0f, 0xffffff00, L"Free Moving", ATGFONT_LEFT );    
    m_Font.End();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}