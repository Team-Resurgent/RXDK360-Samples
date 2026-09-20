//--------------------------------------------------------------------------------------
// SkeletonTracking.cpp
//
// Defines functions used for simple skeleton tracking.
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


#include <xtl.h>
#include <nuiapi.h>
#include <AtgUtil.h>
#include <AtgNuiCommon.h>

#include "SkeletonTracking.h"
#include "Visualization.h"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------

HANDLE g_DepthStreamHandle;
HANDLE g_ColorStreamHandle;
HANDLE g_FrameEndEventHandle;

BOOL   g_bSmoothing = TRUE;
BOOL   g_bUseSeatedST = TRUE;

NUI_SKELETON_FRAME  g_SkeletonFrame;


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------

VOID DestroyStreamBuffers();

//--------------------------------------------------------------------------------------
// Name: InitializeSkeletonTracking()
// Desc: Initialize the Nui API library, and enable skeleton tracking.
//       Also allocate any memory needed for visualizing data
//--------------------------------------------------------------------------------------
HRESULT InitializeSkeletonTracking( D3DDevice* pd3dDevice, BOOL bUseSeatedST )
{
    // Create event which will be signaled when frame processing ends
    g_FrameEndEventHandle = CreateEvent( NULL,
                                         FALSE,  // auto-reset
                                         FALSE,  // create unsignaled
                                         "NuiFrameEndEvent" );

    if ( !g_FrameEndEventHandle )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent\n" );
        return E_FAIL;
    }
    
    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON |
                                NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                                NUI_INITIALIZE_FLAG_USES_COLOR,
                                NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );


    if ( FAILED(hr) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

   
    // Register frame end event with NUI
    hr = NuiSetFrameEndEvent( g_FrameEndEventHandle, 0 );
    if( FAILED(hr) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }

    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR,
        NUI_IMAGE_RESOLUTION_640x480,
        0, 
        1, 
        NULL, 
        &g_ColorStreamHandle); 

    if ( FAILED( hr ) ) 
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;        
    }

     hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX,
        NUI_IMAGE_RESOLUTION_320x240,
        0, 
        1, 
        NULL, 
        &g_DepthStreamHandle); 
               

    if ( FAILED( hr ) ) 
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;        
    }

    // Enable seated skeleton tracking
    g_bUseSeatedST = bUseSeatedST;
    DWORD dwFlags = g_bUseSeatedST ? NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT : 0;
    hr = NuiSkeletonTrackingEnable( NULL, dwFlags );

    if ( FAILED ( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
    }

    // In this release, color buffer is always 640x480. Depth buffer is always 320x240 from here on out.
    hr = InitializeVisualization( pd3dDevice,
                                        640, 480,
                                        320, 240 );

    if( hr != ERROR_SUCCESS )
    {
        ATG::DebugSpew( ": FAILED\nFailed to initialize visualization\n" );
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
    const NUI_IMAGE_FRAME* pColorImageFrame;
    const NUI_IMAGE_FRAME* pDepthImageFrame;

    // Wait for frame processing to end
    PIXBeginNamedEvent( 0, "Wait for m_hFrameEndEvent" );
    if( WAIT_OBJECT_0 != WaitForSingleObject( g_FrameEndEventHandle, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        PIXEndNamedEvent();
        return;
    }
    PIXEndNamedEvent();

    // Update the color and depth buffer streams.
    HRESULT hrImage = NuiImageStreamGetNextFrame( g_ColorStreamHandle, 0, &pColorImageFrame );
    HRESULT hrDepth = NuiImageStreamGetNextFrame( g_DepthStreamHandle, 0, &pDepthImageFrame );
    
    // Get the next skeleton frame
    HRESULT hrSkeleton = NuiSkeletonGetNextFrame( 0, &g_SkeletonFrame );

    // Color
    if( SUCCEEDED( hrImage ) )
    {
        UpdateColorTexture( pD3DDevice, pColorImageFrame );
        NuiImageStreamReleaseFrame( g_ColorStreamHandle, pColorImageFrame  );
    }

    // Depth
    if( SUCCEEDED( hrDepth ) )
    {
        UpdateDepthTexture( pD3DDevice, pDepthImageFrame );
        NuiImageStreamReleaseFrame( g_DepthStreamHandle, pDepthImageFrame  );
    }

    // Skeleton smoothing
    if ( SUCCEEDED( hrSkeleton ))
    {
        if (g_bSmoothing)
        {
            if ( FAILED( NuiTransformSmooth( &g_SkeletonFrame, NULL ) ) )
            {
                ATG::DebugSpew( "Couldn't smooth incoming skeleton data" );
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: VisualizeSkeletonTracking()
// Desc: Renders the stream data from the camera and the results from skeleton tracking
//--------------------------------------------------------------------------------------
VOID VisualizeSkeletonTracking( D3DDevice* pd3dDevice, BOOL* pbTracking )
{

    // Visualize the data streaming from the camera

    VisualizeStreams( pd3dDevice );

    VisualizeSkeleton( &g_SkeletonFrame);

    // Return whether we're tracking skeletons or not. (Solely for HUD purposes).

    for ( UINT i = 0; i < NUI_SKELETON_COUNT; ++i )
    {
        pbTracking[i] = g_SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED;
    }
}


//--------------------------------------------------------------------------------------
// Name: EnableSeatedSkeletonTracking()
// Desc: When seated ST is enabled, the seated ST pipeline will be used, otherwise the
//       the full body ST pipeline will be used
//--------------------------------------------------------------------------------------
VOID EnableSeatedSkeletonTracking( BOOL bEnableSeatedST )
{
    if ( bEnableSeatedST != g_bUseSeatedST )
    {
        // Disable skeleton tracking
        HRESULT hr = NuiSkeletonTrackingDisable();

        // Enable seated skeleton tracking with the new flags
        DWORD dwFlags = bEnableSeatedST ? NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT : 0;
        hr = NuiSkeletonTrackingEnable( NULL, dwFlags );

        if ( FAILED ( hr ) )
        {
            ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
        }
        else
        {
            g_bUseSeatedST = bEnableSeatedST;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: SetSmoothingState()
// Desc: Activate or deactivate the smoothing of the joints.
//--------------------------------------------------------------------------------------
VOID SetSmoothingState( BOOL state )
{
    g_bSmoothing = state;
}


//--------------------------------------------------------------------------------------
// Name: GetSmoothingState()
// Desc: Return wether the smoothing of the joints is active or not.
//--------------------------------------------------------------------------------------
BOOL GetSmoothingState()
{
    return g_bSmoothing;
}
