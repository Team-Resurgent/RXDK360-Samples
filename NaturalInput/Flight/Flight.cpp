//--------------------------------------------------------------------------------------
// Flight.cpp
//
//
// Microsoft XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xnamath.h>
#include <vector>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgApp.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>
#include <AtgSceneAll.h>
#include <AtgNuiVisualization.h>
#include <AtgNuiCommon.h>

#include "ParameterPool.h"
#include "Airplane.h"
#include "RingLayout.h"
#include "AudioEngine.h"
#include "SkeletonTracking.h"

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_1, L"Pause game" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Reset game" },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_1, L"Change course" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))

// When possible, it is a good idea to offload some of the camera and joint work to 
// threads other than main.
#define CAMERA_HW_THREAD 5
#define JOINT_HW_THREAD  2

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: The skinned character sample class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    Sample();
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();

private:
    HRESULT UpdateNaturalInput();
    VOID    RenderModel( ATG::Model* pModel, XMMATRIX* pmatWVP );
    VOID    RenderEnvironment();
    VOID    RenderUI();
    VOID    RenderHelpText();

private:
    // Sample framework objects
    ATG::Font m_Font;
    ATG::Timer m_Timer;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    FLOAT m_fDeltaTime;
    BOOL m_bPaused;
    BOOL m_bUserPaused;
    DWORD m_dwCourseIndex;

    // Scene members
    ATG::Scene*             m_pEnvironmentScene;
    FLOAT                   m_fEnvironmentScale;
    UbershaderParameterPool m_ParameterPool;

    XMFLOAT4X4              m_matVP;
    XMFLOAT3                m_vDirLightWorldDirection;

    Airplane                m_Airplane;
    RingLayout              m_RingLayout;
    SkeletonTracking        m_SkeletonTracking;
    NUI_SKELETON_FRAME      m_SkeletonFrame;
    UINT                    m_iCurrentSkeletonIndex; 

    // Visualize the depth and color buffers
    CONST NUI_IMAGE_FRAME*      m_pImageFrame;
	CONST NUI_IMAGE_FRAME*      m_pDepthFrame;
	ATG::NuiVisualization       m_pip;
	HANDLE                      m_hImage;
	HANDLE                      m_hDepth;
	HANDLE                      m_hFrameEndEvent;

};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the sample.
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample FlightSample;
    ATG::GetVideoSettings( &FlightSample.m_d3dpp.BackBufferWidth, &FlightSample.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    FlightSample.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    FlightSample.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    FlightSample.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    FlightSample.Run();
    
}

//--------------------------------------------------------------------------------------
// Name: Sample::Sample()
// Desc: 
//--------------------------------------------------------------------------------------
Sample::Sample()
{
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the objects for the sample, and loads various resources.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize member variables.
    m_bDrawHelp = FALSE;

    m_iCurrentSkeletonIndex = 0;

    // Create event which will be signaled when frame processing ends
    m_hFrameEndEvent = CreateEvent( NULL,
                                    FALSE,  // auto-reset
                                    FALSE,  // create unsignaled
                                    "NuiFrameEndEvent" );

    if ( !m_hFrameEndEvent )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent\n" );
        return E_FAIL;
    }

    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON |
                                NUI_INITIALIZE_FLAG_USES_COLOR |
                                NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX,
                                NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );

    if ( FAILED(hr) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

    // Register frame end event with NUI
    hr = NuiSetFrameEndEvent( m_hFrameEndEvent, 0 );
    if( FAILED(hr) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }

    // Open the color stream
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 0, 1, NULL, &m_hImage );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    // Open the depth stream
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, NUI_IMAGE_RESOLUTION_320x240, 0, 1, NULL, &m_hDepth );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    hr = NuiSkeletonTrackingEnable( NULL, 0 );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
    }

    // Create font.
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        ATG::FatalError( "Could not load font." );

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create help screen.
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        ATG::FatalError( "Could not load help resources." );

    // Initialize simple shaders.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create scene object.
    m_pEnvironmentScene = new ATG::Scene();

    // Load the ubershader into the scene resource database.
    ATG::FXLiteMaterialImplementation::SetParameterPool( m_pEnvironmentScene->GetEffectParameterPool() );
    ATG::BaseMaterial* pUbershaderBaseMaterial = ATG::BaseMaterial::CreateFXLiteMaterial( L"Default",
                                                                                          L"game:\\media\\effects\\ubershader_final.fxobj", L"Ubershader_Nested" );
    pUbershaderBaseMaterial->InitializeImplementation();
    pUbershaderBaseMaterial->ChangeDevice( m_pd3dDevice );
    ATG::ResourceDatabase* pRDB = m_pEnvironmentScene->GetResourceDatabase();
    pRDB->AddResource( pUbershaderBaseMaterial );

    // Bind the parameter pool interface to the FXLite parameter pool.
    // This allows us to easily set the shader constants for the ubershader.
    m_ParameterPool.Initialize( m_pEnvironmentScene->GetEffectParameterPool() );

    // Create default resources.
    pRDB->CreateDefaultResources();

    // Load content.
    RingLayout::LoadContent( pUbershaderBaseMaterial );
    Airplane::LoadContent( pUbershaderBaseMaterial );

    hr = ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\ground.xatg", m_pEnvironmentScene, NULL, 0, NULL );
    if( FAILED(hr) )
    {
        ATG::FatalError( "Could not load ground content file." );
    }
    m_fEnvironmentScale = 5.0f;

    XMStoreFloat3( &m_vDirLightWorldDirection, XMVector3Normalize( XMVectorSet( 0.1f, -1, 0.1f, 0 ) ) );

    g_pAudioEngine = new AudioEngine();
    g_pAudioEngine->Initialize();

    // Set up gameplay.
    m_dwCourseIndex = 0;
    hr = m_RingLayout.CreateCourse( m_dwCourseIndex );
    assert( SUCCEEDED(hr) );
    m_Airplane.Reset();
    m_bPaused = FALSE;
    m_bUserPaused = FALSE;

     // Initialiez the Picture in Picture visualization
	if( FAILED( m_pip.Initialize( m_pd3dDevice, NUI_INITIALIZE_FLAG_USES_COLOR |
                                                NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                                                NUI_INITIALIZE_FLAG_USES_SKELETON, 
		                          NUI_IMAGE_RESOLUTION_640x480 ) ) )
    {
        ATG_PrintError ( "Picture in Picture initialization failed" );
	}
    
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Gets controller input and updates the camera.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Update FPS in the timer class.
    m_Timer.MarkFrame();

    // Get frame delta time.
    m_fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get input from controllers (check for exit button sequence).
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // The A button resets the game.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_Airplane.Reset();
        m_RingLayout.Reset();
    }

    // The Start button pauses the game.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        m_bUserPaused = !m_bUserPaused;
    }

    // The Y button changes the course and resets the game.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        ++m_dwCourseIndex;
        HRESULT hr = m_RingLayout.CreateCourse( m_dwCourseIndex );
        if( FAILED(hr) )
        {
            m_dwCourseIndex = 0;
            hr = m_RingLayout.CreateCourse( m_dwCourseIndex );
            assert( SUCCEEDED(hr) );
        }
        m_Airplane.Reset();
    }

    // Get the latest input from the skeleton tracker.
    HRESULT hr = UpdateNaturalInput();
    if ( SUCCEEDED( hr ) )
    {
        // Apply skeleton derived inputs to airplane controls.
        const FlightStatus& FS = m_SkeletonTracking.GetFlightStatus();
        m_Airplane.SetControls( FS.m_fThrottle, -FS.m_fYAxis, -FS.m_fXAxis );

        // Pause the game if we have lost skeletal tracking.
        m_bPaused = m_bUserPaused || ( FS.m_eConfidence == NUI_SKELETON_POSITION_NOT_TRACKED );
    }

    // Pause the gameplay elements if we are paused.
    FLOAT fGameDeltaTime = m_fDeltaTime;
    if( m_bPaused )
    {
        fGameDeltaTime = 0.0f;
    }

    // Update the airplane flight model.
    m_Airplane.Update( fGameDeltaTime );

    // Update the scoring system.
    m_RingLayout.Update( fGameDeltaTime, m_Airplane.GetWorldTransform() );

    // Determine if the plane has crashed, and reset the plane 5 seconds after crashing
    static FLOAT s_fCrashCounter = 0.0f;
    if( m_Airplane.GetFlightState() == Airplane::Crashed )
    {
        s_fCrashCounter += m_fDeltaTime;
        if( s_fCrashCounter >= 5.0f )
        {
            s_fCrashCounter = 0.0f;
            m_Airplane.Reset();
            m_RingLayout.Reset();
        }
    }

    XMMATRIX matAirplane = m_Airplane.GetWorldTransform();
    XMVECTOR vPlaneForward = matAirplane.r[2];
    XMVECTOR vPlaneUp = matAirplane.r[1];
    XMVECTOR vPlanePosition = matAirplane.r[3];

    // Place the camera 20 meters behind the plane.
    const FLOAT fCameraDistance = 20.0f;
    XMVECTOR vCameraPos = vPlanePosition + vPlaneForward * -fCameraDistance;

    // Ensure that the camera is always at least 5 meters above the ground.
    const FLOAT fCameraMinHeight = 5.0f;
    XMVECTOR vCameraMin = XMVectorSet( -FLT_MAX, fCameraMinHeight, -FLT_MAX, 0 );
    vCameraPos = XMVectorMax( vCameraMin, vCameraPos );

    // Construct a view matrix.
    XMMATRIX matView = XMMatrixLookAtLH( vCameraPos, vPlanePosition, vPlaneUp );

    // Update camera view * projection matrix.
    const XMMATRIX matProj = XMMatrixPerspectiveFovLH( XM_PIDIV4, 16.0f / 9.0f, 0.5f, 10000.0f );
    XMMATRIX matVP = matView * matProj;
    XMStoreFloat4x4( &m_matVP, matVP );

    // Set camera matrix into the debug draw system.
    ATG::DebugDraw::SetViewProjection( matVP );

    // Update XACT audio.
    g_pAudioEngine->Update();



    return S_OK;
}

//--------------------------------------------------------------------------------------
// Retrieves the latest joint buffer and runs the raw filter to generate control values
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateNaturalInput()
{
    if( WAIT_OBJECT_0 != WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        return E_FAIL;
    }

    // Get data from the next image frame
    HRESULT hrImage = NuiImageStreamGetNextFrame( m_hImage, 0, &m_pImageFrame );
    // Get data from the next camera depth frame
    HRESULT hrDepth = NuiImageStreamGetNextFrame( m_hDepth, 0, &m_pDepthFrame );
    // Get data from the next skeleton frame
    HRESULT hrSkeleton = NuiSkeletonGetNextFrame( 0, &m_SkeletonFrame );

    if( SUCCEEDED ( hrImage ) )
    {
        m_pip.SetColorTexture( m_pImageFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_hImage, m_pImageFrame );
    }
	// Initialize function result with the result of the first NUI call
	HRESULT hr = hrImage;
	
    if( SUCCEEDED( hrDepth ) )
    {

        m_pip.SetDepthTexture( m_pDepthFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_hDepth, m_pDepthFrame );
    }
    //Update the function result on previous success
    if( SUCCEEDED( hr ) )
    {
        hr = hrDepth;
    }    

    if( SUCCEEDED( hrSkeleton ) )
    {
        // The skeletal pipeline will always try to detect and track as many as NUI_SKELETON_COUNT
        // skeletons. When a single player is standing in front of the camera, his skeleton will usually be 
        // assigned to index 0 but the skeletal pipeline makes no promises about it. There are situations
        // that may cause the player being assigned a different index.
        // Shipping titles should use the Identity API to ensure effective and robust tracking of the players.
        if( m_SkeletonFrame.SkeletonData[ m_iCurrentSkeletonIndex ].eTrackingState != NUI_SKELETON_TRACKED )
        {
            // Since we don't have a lock on the skeleton currently being used by the sample, try switching 
            // to the first tracked skeleton we can find. If none is tracked, then leave the current 
            // index as is and try again next frame.
            for( UINT i = 0; i < NUI_SKELETON_COUNT; ++ i )
            {
                if( m_SkeletonFrame.SkeletonData[ i ].eTrackingState == NUI_SKELETON_TRACKED )
                {
                    m_iCurrentSkeletonIndex = i;
                    break;
                }
            }
        }

        m_pip.SetSkeletons( &m_SkeletonFrame );
        hrSkeleton = m_SkeletonTracking.Update( &m_SkeletonFrame.SkeletonData[ m_iCurrentSkeletonIndex ] );
    }
    // Update the function result on previous success
    if( SUCCEEDED( hr ) )
    {
        hr = hrSkeleton;
    }

    // First error encountered will be returned
    return hr;
}


//--------------------------------------------------------------------------------------
// Name: RenderModel()
// Desc: Draws a model.  The models are bound to materials specified in the content file.
//--------------------------------------------------------------------------------------
VOID Sample::RenderModel( ATG::Model* pModel, XMMATRIX* pmatWVP )
{
    if( pmatWVP == NULL )
    {
        // Compute world * view * projection matrix for this model and set into constants.
        XMMATRIX matVP = XMLoadFloat4x4( &m_matVP );
        XMMATRIX matWVP = pModel->GetWorldTransform() * matVP;
        m_ParameterPool.SetWorldViewProjMatrix( matWVP );
    }
    else
    {
        m_ParameterPool.SetWorldViewProjMatrix( *pmatWVP );
    }

    // Loop over mesh mappings.
    DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
    for( DWORD dwMapIndex = 0; dwMapIndex < dwMeshMappingCount; ++dwMapIndex )
    {
        ATG::MeshMapping& mm = pModel->GetMeshMapping( dwMapIndex );
        ATG::BaseMesh* pMesh = mm.pMesh;

        // Loop over mesh subsets.
        DWORD dwSubsetCount = pMesh->GetNumSubsets();
        for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
        {
            ATG::MaterialInstance* pMaterial = mm.Materials[ dwSubsetIndex ];

            // Set the FXLite material.
            pMaterial->BeginMaterialSinglePass( m_pd3dDevice );
            // Render the mesh subset.
            pMesh->RenderSubset( dwSubsetIndex, m_pd3dDevice );
            // End the FXLite material.
            pMaterial->EndMaterialSinglePass();
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderModel()
// Desc: Draws all of the models in the environment scene.
//--------------------------------------------------------------------------------------
VOID Sample::RenderEnvironment()
{
    const XMMATRIX matVP = XMLoadFloat4x4( &m_matVP );
    const XMMATRIX matScaledVP = XMMatrixScaling( m_fEnvironmentScale, m_fEnvironmentScale, m_fEnvironmentScale ) * matVP;

    // Find all of the point lights in the scene and store pointers to them in a vector.
    ATG::NameIndexedCollection::iterator iter = m_pEnvironmentScene->GetInstanceList()->begin();
    ATG::NameIndexedCollection::iterator end = m_pEnvironmentScene->GetInstanceList()->end();
    while( iter != end )
    {
        if( ( *iter )->IsDerivedFrom( ATG::Model::TypeID ) )
        {
            ATG::Model* pModel = (ATG::Model*)*iter;

            XMMATRIX matWVP = pModel->GetWorldTransform() * matScaledVP;

            RenderModel( pModel, &matWVP );
        }
        iter++;
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Draws some statistics and other UI elements
//--------------------------------------------------------------------------------------
VOID Sample::RenderUI()
{
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();

        // Draw a title and FPS indicator.
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"Flight" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Draw the aircraft HUD.
        D3DRECT TextWindow;
        m_Font.GetWindow( TextWindow );
        D3DRECT HUDWindow;
        HUDWindow = TextWindow;
        HUDWindow.y1 += ( HUDWindow.y2 - HUDWindow.y1 ) / 2;
        m_Font.SetWindow( HUDWindow );

        DWORD dwHUDColor = 0xff80ff80;
        if( m_Airplane.GetFlightState() == Airplane::Crashed )
        {
            dwHUDColor = 0xffff8080;
        }

        const FlightStatus& FS = m_SkeletonTracking.GetFlightStatus();
        const FLOAT fAirspeed = m_Airplane.GetVelocity();
        const FLOAT fAltitude = m_Airplane.GetAltitude();

        m_Font.SetScaleFactors( 1.5f, 1.5f );
        WCHAR strText[200];
        swprintf_s( strText, L"Airspeed\n%0.0f", fAirspeed );
        m_Font.DrawText( 0, 0, dwHUDColor, strText, ATGFONT_LEFT | ATGFONT_CENTER_Y );
        swprintf_s( strText, L"Altitude\n%0.1f", fAltitude );
        m_Font.DrawText( 0, 0, dwHUDColor, strText, ATGFONT_RIGHT | ATGFONT_CENTER_Y );

        m_Font.SetWindow( TextWindow );

        // Draw text UI elements.
        FLOAT fXCenter = ( TextWindow.x2 - TextWindow.x1 ) * 0.5f;
        FLOAT fYCenter = ( TextWindow.y2 - TextWindow.y1 ) * 0.5f;
        if( m_Airplane.GetFlightState() == Airplane::Crashed )
        {
            m_Font.SetScaleFactors( 3.0f, 3.0f );
            m_Font.DrawText( fXCenter, fYCenter, 0xffff0000, L"Crashed!", ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
        }
        else if( m_bPaused )
        {
            m_Font.SetScaleFactors( 3.0f, 3.0f );
            m_Font.DrawText( fXCenter, fYCenter, 0xffffffff, L"Paused", ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
        }

        if( FS.m_eConfidence == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            m_Font.SetScaleFactors( 2.0f, 2.0f );
            m_Font.DrawText( fXCenter, 20, 0xffffff80, L"Tracking Lost", ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
        }

        m_Font.SetScaleFactors( 1.5f, 1.5f );
        m_Font.DrawText( fXCenter, -20, 0xffffffff, m_RingLayout.GetMessage(), ATGFONT_CENTER_X | ATGFONT_CENTER_Y );

        RenderHelpText();

        m_Font.End();

        // Draw control rect representing XY input and throttle
        const INT iControlDisplaySize = 100;
        D3DRECT ControlRect = TextWindow;
        ControlRect.x2 = ControlRect.x1 + iControlDisplaySize;
        ControlRect.y1 = ControlRect.y2 - iControlDisplaySize;
        INT iControlXPos = (INT)( ( -FS.m_fXAxis * 0.5f + 0.5f ) * (FLOAT)iControlDisplaySize );
        INT iControlYPos = (INT)( ( -FS.m_fYAxis * 0.5f + 0.5f ) * (FLOAT)iControlDisplaySize );
        INT iThrottleYPos = (INT)( m_Airplane.GetThrottle() * (FLOAT)iControlDisplaySize );

        D3DRECT VerticalRect = ControlRect;
        VerticalRect.x1 += ( iControlXPos - 2 );
        VerticalRect.x2 = VerticalRect.x1 + 4;

        D3DRECT HorizontalRect = ControlRect;
        HorizontalRect.y1 += ( iControlYPos - 2 );
        HorizontalRect.y2 = HorizontalRect.y1 + 4;

        D3DRECT ThrottleBarRect = ControlRect;
        ThrottleBarRect.x1 = ControlRect.x2 + 10;
        ThrottleBarRect.x2 = ThrottleBarRect.x1 + 4;

        D3DRECT ThrottleThumbRect = ThrottleBarRect;
        ThrottleThumbRect.x1 -= 6;
        ThrottleThumbRect.x2 += 6;
        ThrottleThumbRect.y2 -= ( iThrottleYPos - 6 );
        ThrottleThumbRect.y1 = ThrottleThumbRect.y2 - 8;

        ATG::DebugDraw::DrawScreenSpaceRect( ControlRect, 3.0f, 0xFFC0C0C0 );
        ATG::DebugDraw::DrawScreenSpaceRect( ThrottleBarRect, 0.0f, 0xFFC0C0C0 );
        ATG::DebugDraw::DrawScreenSpaceRect( VerticalRect, 0.0f, 0xFF80FFFF );
        ATG::DebugDraw::DrawScreenSpaceRect( HorizontalRect, 0.0f, 0xFF80FFFF );
        
        ATG::DebugDraw::DrawScreenSpaceRect( ThrottleThumbRect, 0.0f, 0xFFFF0000 );
        ThrottleThumbRect.x1 +=2;
        ThrottleThumbRect.x2 -=2;
        ThrottleThumbRect.y1 +=3;
        ThrottleThumbRect.y2 -=2;
        ATG::DebugDraw::DrawScreenSpaceRect( ThrottleThumbRect, 0.0f, 0xFF000000 );

        // Draw the raw depth and image map with skeleton overlaid as visualization.
        const FLOAT drawWidth = 150.0f;
        const FLOAT drawHeight = drawWidth * 3.0f / 4.0f;
        const FLOAT border = 50.0f;
        const FLOAT drawX = 1280.0f - border - drawWidth * 2.0f - 10.0f;
        const FLOAT drawY = 140;
        
        m_pip.BeginRender();
        m_pip.RenderColorStream( drawX, drawY, drawWidth, drawHeight );
        m_pip.RenderSingleSkeleton( m_iCurrentSkeletonIndex, drawX, drawY, drawWidth, drawHeight, TRUE, TRUE );
        m_pip.RenderDepthStream( drawX + drawWidth + 10, drawY, drawWidth, drawHeight );
	    m_pip.RenderSingleSkeleton( m_iCurrentSkeletonIndex, drawX + drawWidth + 10, drawY, drawWidth, drawHeight, TRUE );
        m_pip.EndRender();

        static ATG::Timer time;
        if( FS.m_eConfidence == NUI_SKELETON_POSITION_NOT_TRACKED )
            time.Reset();
    }
}


//--------------------------------------------------------------------------------------
// Render a series of helpful tips, changing every few seconds.  Tips are disabled when
// the aircraft is moving.
//--------------------------------------------------------------------------------------
VOID Sample::RenderHelpText()
{
    static FLOAT s_fMessageTime = 0.0f;
    static DWORD s_dwMessageIndex = 0;

    if( !m_bPaused && m_Airplane.GetVelocity() > 0.0f )
    {
        s_fMessageTime = 0.0f;
        return;
    }

    const WCHAR* strMessages[] =
    {
        L"Stand facing the sensor with your arms\n at your sides until tracking succeeds.",
        L"To pause the game, press the " GLYPH_START_BUTTON L" button.",
        L"To increase throttle, move your right leg forwards.",
        L"To pitch the aircraft, lean backwards and forwards.",
        L"To turn the aircraft left and right,\ntilt your shoulders like you're flying with your arms.",
        L"Fly through the rings in order to score points.\nYou can skip rings by flying past them towards the next ring.",
        L"Successful rings score 100 points,\nwhile skipping a ring scores 0 points.\nFinishing the course scores 1000 points.",
        L"A game timer starts when you fly through or skip the first ring.",
        L"If you crash, your previous score is displayed\nuntil the next attempt is started.",
        L"To select a different course, press the " GLYPH_Y_BUTTON L" button.",
    };

    const FLOAT fMessageDuration = 5.0f;
    const FLOAT fMessageGap = 0.5f;

    s_fMessageTime += m_fDeltaTime;
    if( s_fMessageTime >= ( fMessageDuration + fMessageGap ) )
    {
        s_fMessageTime = 0.0f;
        ++s_dwMessageIndex;
    }
    if( s_fMessageTime <= fMessageGap )
    {
        return;
    }
    DWORD dwIndex = s_dwMessageIndex % ARRAYSIZE( strMessages );
    const WCHAR* strCurrentMessage = strMessages[dwIndex];

    D3DRECT TextWindow;
    m_Font.GetWindow( TextWindow );
    FLOAT fXCenter = ( TextWindow.x2 - TextWindow.x1 ) * 0.5f;

    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( fXCenter, -80, 0xffffff80, strCurrentMessage, ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene and UI.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    m_pd3dDevice->BeginScene();

    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFFC0C0FF, 1.0f, 0 );

    // Set default renderstate.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Set some default textures.
    D3DBaseTexture* pWhiteTexture = m_pEnvironmentScene->GetResourceDatabase()->GetWhiteTexture()->GetD3DTexture();
    for( DWORD i = 8; i < 16; ++i )
    {
        m_pd3dDevice->SetTexture( i, pWhiteTexture );
    }

    XMVECTOR vDirLightWorldDirection = XMLoadFloat3( &m_vDirLightWorldDirection );

    m_ParameterPool.SetAmbient( XMVectorReplicate( 0.2f ) );
    m_ParameterPool.SetDirLightColor( 0, XMVectorSet( 1, 1, 0.9f, 1 ) );
    m_ParameterPool.SetDirLightCount( 1 );

    XMMATRIX matViewProjection = XMLoadFloat4x4( &m_matVP );
    m_Airplane.Render( m_pd3dDevice, &m_ParameterPool, matViewProjection, vDirLightWorldDirection );

    m_RingLayout.Render( m_pd3dDevice, &m_ParameterPool, matViewProjection, vDirLightWorldDirection );

    m_ParameterPool.SetDirLightObjectDir( 0, vDirLightWorldDirection );
    RenderEnvironment();

    RenderUI();

    m_pd3dDevice->EndScene();

    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
