//--------------------------------------------------------------------------------------
// SkeletonTracking.cpp
//
// Defines functions used for simple skeleton tracking.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <nuiapi.h>
#include <AtgUtil.h>
#include <AtgNuiCommon.h>
#include <AtgNuiVisualization.h>

#include "SkeletonTracking.h"


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------

HANDLE  g_DepthStreamHandle;
HANDLE  g_FrameEndEventHandle;
BOOL    g_bSmoothing = FALSE;
BOOL    g_bTiltCorrection = FALSE;
NUI_SKELETON_FRAME      g_SkeletonFrame;
ATG::NuiVisualization   g_NuiVisualization;


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------

VOID DestroyStreamBuffers();


//--------------------------------------------------------------------------------------
// Name: InitializeSkeletonTracking()
// Desc: Initialize the Nui API library, and enable skeleton tracking.
//       Also allocate any memory needed for visualizing data
//--------------------------------------------------------------------------------------

HRESULT InitializeSkeletonTracking( D3DDevice* pd3dDevice )
{
    // Create event which will be signaled when frame processing ends
    g_FrameEndEventHandle = CreateEvent( NULL, FALSE, FALSE, "NuiFrameEndEvent" );
    if ( !g_FrameEndEventHandle )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent\n" );
        return E_FAIL;
    }

    // Initialize Nui
    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON |
                                NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX,
                                NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );
    if ( FAILED(hr) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }
   
    // Register frame end event with NUI
    hr = NuiSetFrameEndEvent( g_FrameEndEventHandle, 0 );
    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }

    // Open depth stream
    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, NUI_IMAGE_RESOLUTION_320x240, 0, 1, NULL, &g_DepthStreamHandle );
    if ( FAILED( hr ) ) 
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;        
    }

    // Enable skeleton tracking
    hr = NuiSkeletonTrackingEnable( NULL, 0 );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
    }

    // Initialiez ATG's debug visualization for NUI data
	hr = g_NuiVisualization.Initialize( pd3dDevice, NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_SKELETON, NUI_IMAGE_RESOLUTION_640x480 );
    if ( FAILED (hr) )
    {
        ATG_PrintError ( "NuiVisualization failed to initialize" );
        return E_FAIL;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateSkeletonTracking()
// Desc: Updates the skeleton tracking with the latest data. Note that if there is
//       no new data, then we will skip updates of the skeleton
//--------------------------------------------------------------------------------------

VOID UpdateSkeletonTracking( D3DDevice *pD3DDevice )
{
    const NUI_IMAGE_FRAME* pDepthImageFrame;

    // Wait for frame processing to end
    PIXBeginNamedEvent( 0, "WaitFor m_hFrameEndEvent" );
    if ( WAIT_OBJECT_0 != WaitForSingleObject( g_FrameEndEventHandle, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        PIXEndNamedEvent();
        return;
    }
    PIXEndNamedEvent();

    // Update the depth buffer stream
    HRESULT hrDepth = NuiImageStreamGetNextFrame( g_DepthStreamHandle, 0, &pDepthImageFrame );

    // Get the next skeleton frame
    HRESULT hrSkeleton = NuiSkeletonGetNextFrame( 0, &g_SkeletonFrame );

    // Depth
    if ( SUCCEEDED( hrDepth ) )
    {
        g_NuiVisualization.SetDepthTexture( pDepthImageFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( g_DepthStreamHandle, pDepthImageFrame  );
    }

    // Skeleton
    g_NuiVisualization.SetSkeletons( &g_SkeletonFrame );
    if ( SUCCEEDED( hrSkeleton ))
    {
        if ( g_bSmoothing )
        {
            if ( FAILED( NuiTransformSmooth( &g_SkeletonFrame, NULL ) ) )
            {
                ATG::DebugSpew( "Couldn't smooth incoming skeleton data" );
            }
        }

        if ( g_bTiltCorrection )
        {
            ATG::ApplyTiltCorrectionInPlayerSpace( &g_SkeletonFrame, &g_SkeletonFrame );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: VisualizeSkeletonTracking()
// Desc: Renders the stream data from the camera and the results from skeleton tracking
//--------------------------------------------------------------------------------------

VOID VisualizeSkeletonTracking( D3DDevice* pd3dDevice )
{
    PIXBeginNamedEvent( 0, "Nui Visualization"  );
    const FLOAT fDrawWidth = 100.0f;
    const FLOAT fDrawHeight = 75.0f;
    const FLOAT fDrawX = 1280 - 50 - fDrawWidth;
    const FLOAT fDrawY = 720 - 50 - fDrawHeight;
    g_NuiVisualization.BeginRender();
    g_NuiVisualization.RenderDepthStream( fDrawX, fDrawY, fDrawWidth, fDrawHeight );
    g_NuiVisualization.RenderSkeletons( fDrawX, fDrawY, fDrawWidth, fDrawHeight );
    g_NuiVisualization.EndRender();
    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------
// Name: SetSmoothingState()
// Desc: Activate or deactivate the smoothing of the joints.
//--------------------------------------------------------------------------------------

VOID SetSmoothingState( const BOOL bState )
{
    g_bSmoothing = bState;
}


//--------------------------------------------------------------------------------------
// Name: GetSmoothingState()
// Desc: Return wether the smoothing of the joints is active or not.
//--------------------------------------------------------------------------------------

BOOL GetSmoothingState()
{
    return g_bSmoothing;
}


//--------------------------------------------------------------------------------------
// Name: SetTiltCorrectionState()
// Desc: Activate or deactivate the tilt correction.
//--------------------------------------------------------------------------------------

VOID SetTiltCorrectionState( const BOOL bState )
{
    g_bTiltCorrection = bState;
}


//--------------------------------------------------------------------------------------
// Name: GetTiltCorrectionState()
// Desc: Return wether the tilt correction is active or not.
//--------------------------------------------------------------------------------------

BOOL GetTiltCorrectionState()
{
    return g_bTiltCorrection;
}


//--------------------------------------------------------------------------------------
// Name: GetClosestSkeletonData()
// Desc: Returns data from the closest skeleton to the camera
//--------------------------------------------------------------------------------------

NUI_SKELETON_DATA* GetClosestSkeletonData()
{
    INT iClosestSkeletonIdx = 0;
    FLOAT fMinDistance = FLT_MAX;

    // Find the closest skeleton to the center of the playspace
    for ( UINT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &g_SkeletonFrame.SkeletonData[ i ];

        // If not tracked, then ignore
        if ( pSkeletonData->eTrackingState != NUI_SKELETON_TRACKED )
        {
            continue;
        }

        // Find the positions closest to the camera.
        XMVECTOR vDistance = XMVector3LengthSq( pSkeletonData->Position );
        FLOAT fDistance = XMVectorGetX( vDistance );
        if ( fDistance < fMinDistance )
        {
            iClosestSkeletonIdx = i;
            fMinDistance = fDistance;
        }              
    }

    return &g_SkeletonFrame.SkeletonData[ iClosestSkeletonIdx ];
}


//--------------------------------------------------------------------------------------
// Name: GetCurrentTimeStamp()
// Desc: Returns the time stamp of the current nui frame data
//--------------------------------------------------------------------------------------

LARGE_INTEGER GetCurrentTimeStamp()
{
    return g_SkeletonFrame.liTimeStamp;
}

//--------------------------------------------------------------------------------------
// Name: GetCurrentNormalToGravity()
// Desc: Returns the up vector from the Kinect sensor
//--------------------------------------------------------------------------------------

XMVECTOR GetCurrentNormalToGravity()
{
    return g_SkeletonFrame.vNormalToGravity;
}