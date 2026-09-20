//--------------------------------------------------------------------------------------
// Running.cpp
//
// This sample demonstrates how to use the data retrieved from the sensor array to
// measure the player's running speed. The sample uses NuiFitness to determine the 
// number of joules expended while running.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xnamath.h>
#include <AtgApp.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgSimpleShaders.h>
#include <AtgNuiVisualization.h>
#include <AtgNuiCommon.h>

#include <NuiApi.h>
#include "RunningSpeedDetection.h"

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_B_BUTTON,    ATG::HELP_PLACEMENT_1, L"Reset exercise" },
    { ATG::HELP_Y_BUTTON,    ATG::HELP_PLACEMENT_2, L"Select manual or\nautomatic fitness mode" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


// Values used to compute the metabolic equivalent (MET) in manual fitness mode.
const FLOAT JOGGING_TYPICAL_MET      = 8.0f;     // When jogging at 6 miles an hour
const FLOAT JOGGING_STEPS_PER_SECOND = 180 / 60; // Jogging at 6 miles an hour is equivalent to
                                                 // 180 steps per minute for an average person


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer                  m_Timer; // Used to compute framerate
    ATG::Font                   m_Font;
    ATG::Help                   m_Help;
    BOOL                        m_bDrawHelp;

    // NuiFitness data
    DWORD                       m_dwFitnessTrackingID; // TrackingID currently used for tracking fitness.
    BOOL                        m_bPaused;             // The sample pauses fitness tracking when no skeletons are present in automatic mode only
    BOOL                        m_bManualMode;         // TRUE if manual mode has been activated
    LARGE_INTEGER               m_liTimeStamp;         // Reference timestamp use to compute elapsed time between two records in manual mode

    FLOAT                       m_fDepthStreamWidth;
    FLOAT                       m_fDepthStreamHeight;
    NUI_SKELETON_FRAME          m_SkeletonFrame;

    UINT                        m_iCurrentSkeletonIndex; 

	CONST NUI_IMAGE_FRAME*      m_pImageFrame;
	CONST NUI_IMAGE_FRAME*      m_pDepthFrame;

	ATG::NuiVisualization       m_pip;
	HANDLE                      m_hImage;
	HANDLE                      m_hDepth;
	HANDLE                      m_hFrameEndEvent;

	CRunningSpeedDetection      m_RunningSpeedDetection;


private:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();

    HRESULT StartupCamera();
    VOID DrawSpeedGauge();
    VOID VisualizeSkeleton( NUI_SKELETON_DATA* pSkeleton);

};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    atgApp.Run();
}

//--------------------------------------------------------------------------------------
// Setup the sensor array for RGB and depth streaming
//--------------------------------------------------------------------------------------
HRESULT Sample::StartupCamera()
{
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

    // Initializes the Natural Input system on the default thread
    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON |
                                NUI_INITIALIZE_FLAG_USES_COLOR |
                                NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                                NUI_INITIALIZE_FLAG_USES_FITNESS,                  // <== Enable fitness
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

    // Enable skeletal tracking
    hr = NuiSkeletonTrackingEnable( NULL, 0 );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
        return E_FAIL;
    }

    // Depth buffer will always be 320*240 from here on
    m_fDepthStreamWidth = 320;
    m_fDepthStreamHeight = 240;

    return hr;
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    StartupCamera();
    
    // Initialize base member variables    
    m_bDrawHelp             = FALSE;
    m_bPaused               = FALSE;
    m_bManualMode           = FALSE;
    m_liTimeStamp.QuadPart  = 0;
    m_dwFitnessTrackingID   = NUI_SKELETON_INVALID_TRACKING_ID;
    m_iCurrentSkeletonIndex = 0;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;    

	ATG::SimpleShaders::Initialize( NULL, NULL );

	if( FAILED( m_pip.Initialize( m_pd3dDevice, 
                                  NUI_INITIALIZE_FLAG_USES_COLOR |
                                  NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                                  NUI_INITIALIZE_FLAG_USES_SKELETON, 
		                          NUI_IMAGE_RESOLUTION_640x480 ) ) )
    {
        ATG_PrintError ( "Picture in Picture initialization failed" );
	}

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Draws a speed gauge at the center of the window. The faster you run, the longer it is
//--------------------------------------------------------------------------------------
VOID Sample::DrawSpeedGauge()
{
    XMFLOAT2 pntArray[ 2 ];
    pntArray[ 0 ].x = (FLOAT)m_d3dpp.BackBufferWidth / 2;
    pntArray[ 0 ].y = (FLOAT)m_d3dpp.BackBufferHeight;
    pntArray[ 1 ].x = (FLOAT)m_d3dpp.BackBufferWidth / 2;
    pntArray[ 1 ].y = (FLOAT)m_d3dpp.BackBufferHeight * (1.0f - m_RunningSpeedDetection.GetRunningSpeed() / 10.0f) ;
    ATG::DebugDraw::DrawScreenSpaceLine( pntArray[ 0 ], pntArray[ 1 ], D3DXCOLOR( 0.9f, 0.8f, 0.5f, 1.0f ), 100 );
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame to update the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = ! m_bDrawHelp;   

    // (B) resets the fitness tracking data
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_RunningSpeedDetection.ResetStepsSoFar();

        // Stop fitness tracking. The sample will automatically restart fitness 
        // tracking at a later point, ffectively reseting the internal fitness tracking data
        if( m_dwFitnessTrackingID != NUI_SKELETON_INVALID_TRACKING_ID )
        {
            HRESULT hr = NuiFitnessStopTracking( m_dwFitnessTrackingID );
            ATG_Verify( SUCCEEDED( hr ) );
            m_dwFitnessTrackingID = NUI_SKELETON_INVALID_TRACKING_ID;
            m_bPaused = FALSE;
        }
    }

    // (Y) Switch the sample between using the manual or automatic fitness mode, 
    // forcing a data reset at the same time.
    // In manual mode the sample provides the MET value for the player,
    // while in automatic mode, NuiFitness determine the MET based
    // on player movements
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        m_RunningSpeedDetection.ResetStepsSoFar();

        // Stop fitness tracking. The sample will automatically restart fitness 
        // tracking at a later point, effectively reseting the internal fitness tracking data
        if( m_dwFitnessTrackingID != NUI_SKELETON_INVALID_TRACKING_ID )
        {                
            HRESULT hr = NuiFitnessStopTracking( m_dwFitnessTrackingID );
            ATG_Verify( SUCCEEDED(  hr ) );
            m_dwFitnessTrackingID = NUI_SKELETON_INVALID_TRACKING_ID;
            m_bPaused = FALSE;
        }

        m_bManualMode = ! m_bManualMode;
    }

    // Wait for frame processing to end
    if( WAIT_OBJECT_0 != WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        return E_FAIL;
    }

    // Get data from the next image frame
    HRESULT hrImage = NuiImageStreamGetNextFrame( m_hImage, 0, &m_pImageFrame );
    // Get data from the next camera depth frame
    HRESULT hrDepth = NuiImageStreamGetNextFrame( m_hDepth, 0, &m_pDepthFrame );
    // Finally, get the skeletal frame
	HRESULT hrSkeleton = NuiSkeletonGetNextFrame( 0, &m_SkeletonFrame );

    if ( SUCCEEDED( hrImage ) )
    {
        m_pip.SetColorTexture( m_pImageFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_hImage, m_pImageFrame );
    }
	
    if ( SUCCEEDED( hrDepth ) )
    {
        m_pip.SetDepthTexture( m_pDepthFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_hDepth, m_pDepthFrame );
    }
	
    if ( SUCCEEDED( hrSkeleton ) )
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

        // Start fitness tracking as soon as a skeleton is being tracked
        if( m_dwFitnessTrackingID == NUI_SKELETON_INVALID_TRACKING_ID &&
            m_SkeletonFrame.SkeletonData[ m_iCurrentSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED )
        {
            HRESULT hr = NuiFitnessStartTracking( m_bManualMode ? NUI_FITNESS_TRACKING_MANUAL : NUI_FITNESS_TRACKING_AUTO, 
                                                  m_SkeletonFrame.SkeletonData[ m_iCurrentSkeletonIndex ].dwTrackingID, 
                                                  XUSER_INDEX_NONE );
            ATG_Verify( SUCCEEDED( hr ) );
            m_dwFitnessTrackingID = m_SkeletonFrame.SkeletonData[ m_iCurrentSkeletonIndex ].dwTrackingID;
        }

        // In automatic mode, we pause NuiFitness as soon as there isn't a skeleton to track. 
        // We also pause NuiFitness if there is a switch in tracked skeleton. In this case the code will fall 
        // through the next if and will immediately resume tracking with the new tracking ID.
        if( ! m_bManualMode  && ! m_bPaused && m_dwFitnessTrackingID != NUI_SKELETON_INVALID_TRACKING_ID &&
            ( m_SkeletonFrame.SkeletonData[ m_iCurrentSkeletonIndex ].eTrackingState != NUI_SKELETON_TRACKED  ||
              m_SkeletonFrame.SkeletonData[ m_iCurrentSkeletonIndex ].dwTrackingID   != m_dwFitnessTrackingID    ) )
        {
            HRESULT hr =  NuiFitnessPauseTracking( m_dwFitnessTrackingID );
            ATG_Verify( SUCCEEDED( hr ) );
            m_bPaused = TRUE;
        }

        // We resume fitness tracking as soon as we have re-acquired a skeleton.
        if( ! m_bManualMode  && m_bPaused && 
            m_SkeletonFrame.SkeletonData[ m_iCurrentSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED )
        {
            HRESULT hr = NuiFitnessResumeTracking( m_dwFitnessTrackingID, 
                                                   m_SkeletonFrame.SkeletonData[ m_iCurrentSkeletonIndex ].dwTrackingID );
            ATG_Verify( SUCCEEDED( hr ) );
            m_dwFitnessTrackingID =  m_SkeletonFrame.SkeletonData[ m_iCurrentSkeletonIndex ].dwTrackingID;
            m_bPaused = FALSE;
        }

        m_RunningSpeedDetection.Update( &m_SkeletonFrame.SkeletonData[ m_iCurrentSkeletonIndex ] );
        if( m_bManualMode )
        {
            if( m_SkeletonFrame.SkeletonData[ m_iCurrentSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED && 
                m_liTimeStamp.QuadPart > 0 )
            {
                // In manual fitness mode, the sample provides the MET value to NuiFitness
                FLOAT MET = JOGGING_TYPICAL_MET * m_RunningSpeedDetection.GetRunningSpeed() / JOGGING_STEPS_PER_SECOND;
                MET = max( MET, 1.0f ); // A MET of 1 represents a typical individual at rest.

                HRESULT hr = NuiFitnessRecordMETValue( m_dwFitnessTrackingID, MET,  ( DWORD )( m_SkeletonFrame.liTimeStamp.QuadPart - m_liTimeStamp.QuadPart ) );
                ATG_Verify( SUCCEEDED( hr ) );
            }
        }
        m_liTimeStamp = m_SkeletonFrame.liTimeStamp;

		m_pip.SetSkeletons( &m_SkeletonFrame );
    }
    return S_OK;
}

VOID Sample::VisualizeSkeleton( NUI_SKELETON_DATA* pSkeleton)
{
    struct ScreenSpaceData
    {
        INT x;
        INT y;
    };

    ScreenSpaceData ScreenSpaceJoints[ NUI_SKELETON_POSITION_COUNT ];
    
    struct BoneJoints
    {
        NUI_SKELETON_POSITION_INDEX   StartJoint;
        NUI_SKELETON_POSITION_INDEX   EndJoint;
    };
    

    // Define the bones in the skeleton using joint indices
    static const BoneJoints Bones[] =
    {
        // Head
        { NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER },              // Top of head to top of neck

        // Right arm
        { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT },    // Neck bottom to right shoulder internal
        { NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT },        // Right shoulder internal to right elbow
        { NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT },            // Right elbow to right wrist

        // Left arm
        { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT },     // Neck bottom to left shoulder internal
        { NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT },          // Left shoulder internal to left elbow
        { NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_HAND_LEFT },              // Left elbow to left wrist

        // Right leg and foot
        { NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT },              // Right hip internal to right knee
        { NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT },             // Right knee to right ankle

        // Left leg and foot
        { NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT },                // Left hip internal to left knee
        { NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT },               // Left knee to left ankle

        // Spine
        { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SPINE },             // Neck bottom to spine
        { NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_HIP_CENTER },                  // Spine to hip center

        // Hips
        { NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_HIP_CENTER },              // Right hip to hip center
        { NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT }                // Hip center to left hip
    };

    const UINT uNumBones = ARRAYSIZE( Bones );
    const FLOAT fDepthDisplayScaleX = (FLOAT)m_d3dpp.BackBufferWidth  / m_fDepthStreamWidth;
    const FLOAT fDepthDisplayScaleY = (FLOAT)m_d3dpp.BackBufferHeight / m_fDepthStreamHeight;
    
    // Project the world space joints into screen space
    for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
    {
       float fX;
       float fY;
       NuiTransformSkeletonToDepthImage(
             pSkeleton->SkeletonPositions[i],
             &fX,
             &fY
             );
       ScreenSpaceJoints[ i ].x = static_cast<INT>(fX * fDepthDisplayScaleX);
       ScreenSpaceJoints[ i ].y = static_cast<INT>(fY * fDepthDisplayScaleY);
    }

    // Draw each bone in the skeleton using the screen space joints
    for ( UINT i = 0; i < uNumBones; i++ )
    {
        // Use the minimum joint confidence for the bone confidence
        NUI_SKELETON_POSITION_TRACKING_STATE JointTrackingState = pSkeleton->eSkeletonPositionTrackingState[ Bones[ i ].StartJoint ];

        D3DXCOLOR color[2];
        if ( JointTrackingState == NUI_SKELETON_POSITION_TRACKED )
        {
            color[0] = D3DXCOLOR( 0, 1, 0, 1 );
        }
        else if ( JointTrackingState == NUI_SKELETON_POSITION_INFERRED )
        {
            color[0] = D3DXCOLOR( 1, 0, 0, 1 );
        }
        else
        {
            // A joint in the bone wasn't tracked during skeleton tracking, so just skip over it
            continue;
        }

        JointTrackingState = pSkeleton->eSkeletonPositionTrackingState[ Bones[ i ].EndJoint ];

        if ( JointTrackingState == NUI_SKELETON_POSITION_TRACKED )
        {
            color[1] = D3DXCOLOR( 0, 1, 0, 1 );
        }
        else if ( JointTrackingState == NUI_SKELETON_POSITION_INFERRED )
        {
            color[1] = D3DXCOLOR( 1, 0, 0, 1 );
        }
        else
        {
            // A joint in the bone wasn't tracked during skeleton tracking, so just skip over it
            continue;
        }

        XMFLOAT2 pntArray[ 2 ];
        pntArray[ 0 ].x = (FLOAT)ScreenSpaceJoints[ Bones[ i ].StartJoint ].x;
        pntArray[ 0 ].y = (FLOAT)ScreenSpaceJoints[ Bones[ i ].StartJoint ].y;
        pntArray[ 1 ].x = (FLOAT)ScreenSpaceJoints[ Bones[ i ].EndJoint ].x;
        pntArray[ 1 ].y = (FLOAT)ScreenSpaceJoints[ Bones[ i ].EndJoint ].y;

        ATG::DebugDraw::DrawScreenSpaceLine( pntArray[ 0 ], color[ 0 ], pntArray[ 1 ], color[ 1 ], 5 );
    }
}

//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame to render the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff00ffff );

    // Set default states
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    
    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Draw the raw depth and image map with skeleton overlaid as visualization.
        const FLOAT drawWidth = 200.0f;
        const FLOAT drawHeight = drawWidth * 3.0f / 4.0f;
        const FLOAT drawX = 50.0f;
        const FLOAT drawY = 720.0f - 50.0f - drawHeight;
        m_pip.BeginRender();
        m_pip.RenderColorStream( drawX, drawY, drawWidth, drawHeight );
        m_pip.RenderSkeletons( drawX, drawY, drawWidth, drawHeight, FALSE, TRUE );
        m_pip.RenderDepthStream( drawX + drawWidth + 10, drawY, drawWidth, drawHeight );
        m_pip.RenderSkeletons( drawX + drawWidth + 10, drawY, drawWidth, drawHeight );
        m_pip.EndRender();

        if ( m_SkeletonFrame.SkeletonData[ m_iCurrentSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED )
        {
            DrawSpeedGauge();
            VisualizeSkeleton( &m_SkeletonFrame.SkeletonData[ m_iCurrentSkeletonIndex ]);
        }


        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );

        if( m_SkeletonFrame.SkeletonData[ m_iCurrentSkeletonIndex ].eTrackingState != NUI_SKELETON_TRACKED )
        {
            m_Font.DrawText( 0, 0, 0xffffffff, L"Please adjust your standing position," );            
            m_Font.DrawText( 0, 30, 0xffffffff, L"so that the camera captures you" );
        }        

        m_Font.DrawText( 0, 90, 0xffffffff, m_bManualMode ? L"NuiFitness - Manual mode" : L"NuiFitness - Automatic mode" );

        if ( m_dwFitnessTrackingID != NUI_SKELETON_INVALID_TRACKING_ID )        
        {
            WCHAR buf[256];
            swprintf_s( buf, L"Total steps so far: %d steps", m_RunningSpeedDetection.GetStepsSoFar() );
            m_Font.DrawText( 0, 150, 0xffffffff, buf );

            NUI_FITNESS_DATA m_FitnessData;
            ATG_Verify( SUCCEEDED( NuiFitnessGetCurrentFitnessData( m_dwFitnessTrackingID, &m_FitnessData ) ) ); 

            DWORD dwDurationInSeconds =  m_FitnessData.DurationInMS / 1000;
            DWORD dwDurationInMinutes =  dwDurationInSeconds / 60;
            DWORD dwDurationInHours   =  dwDurationInMinutes / 60;
            swprintf_s( buf, L"Duration: %02uH:%02uM:%02uS", dwDurationInHours, 
                                                             dwDurationInMinutes % 60, 
                                                             dwDurationInSeconds % 60 );
            m_Font.DrawText( 0, 180, 0xffffffff, buf );
            swprintf_s( buf, L"AverageMET: %.4f", m_FitnessData.AverageMET );
            m_Font.DrawText( 0, 210, 0xffffffff, buf );
            swprintf_s( buf, L"Joules: %lu", m_FitnessData.Joules );
            m_Font.DrawText( 0, 240, 0xffffffff, buf );          
        }

        if( m_bPaused )
        {
            m_Font.DrawText( 0, 300, 0xffffffff, L"Paused!" );
        }

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
