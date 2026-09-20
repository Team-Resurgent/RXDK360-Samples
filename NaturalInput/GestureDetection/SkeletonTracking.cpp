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

HANDLE                  g_hFrameEndEventHandle;
HANDLE                  g_DepthStreamHandle;
NUI_SKELETON_FRAME      g_SkeletonFrame;
ATG::NuiVisualization   g_Visualization;
INT                     g_iSelectedSkeleton;


//--------------------------------------------------------------------------------------
// Name: InitializeSkeletonTracking()
// Desc: Initialize the Nui API library, and enable skeleton tracking.
//--------------------------------------------------------------------------------------

HRESULT InitializeSkeletonTracking( D3DDevice* pd3dDevice )
{  
    g_iSelectedSkeleton = -1;

    g_hFrameEndEventHandle = CreateEvent( NULL,
                                          FALSE,  // auto-reset
                                          FALSE,  // create unsignaled
                                          "NuiFrameEndEvent" );
    if ( !g_hFrameEndEventHandle )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent\n" );
        return E_FAIL;
    }

    HRESULT hr;

    // Initialize
    if ( FAILED( hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON |                // Needed for filters
                                     NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX,   // Only needed for visualization
                                     NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD ) ) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }
   
    // Set the frame processing ended event handle
    if ( FAILED( hr = NuiSetFrameEndEvent( g_hFrameEndEventHandle, 0 ) ) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }

    // Open depth stream
    if ( FAILED( hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, NUI_IMAGE_RESOLUTION_320x240, 0, 1, NULL, &g_DepthStreamHandle ) ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;        
    }

    // Enable skeleton tracking
    if ( FAILED( NuiSkeletonTrackingEnable( NULL, NUI_SKELETON_TRACKING_FLAG_TITLE_SETS_TRACKED_SKELETONS ) ) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
        return E_FAIL;
    }

    // Initialize debug visualization
	if ( FAILED( g_Visualization.Initialize( pd3dDevice, NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                                             NUI_INITIALIZE_FLAG_USES_SKELETON, NUI_IMAGE_RESOLUTION_640x480 ) ) )
    {
        ATG_PrintError ( "Debug visualization failed initialization" );
        return E_FAIL;
    }

    return S_OK;

}


//--------------------------------------------------------------------------------------
// Name: SelectSkeleton()
// Desc: Selects the closest skeleton to the camera to actively track
//--------------------------------------------------------------------------------------

INT SelectSkeleton()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Intialize to no skeletons active
    DWORD dwTrackingIDs[ NUI_SKELETON_MAX_TRACKED_COUNT ] = { 0, 0 };

    INT iClosestBody = -1;
    FLOAT fMinDistance = FLT_MAX;

    // Find the closest skeleton to the center of the playspace
    for ( UINT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        // If not tracked, then ignore
        if ( g_SkeletonFrame.SkeletonData[ i ].eTrackingState == NUI_SKELETON_NOT_TRACKED )
        {
            continue;
        }

        // Find the positions closest to the camera
        XMVECTOR vTemp = XMVectorSubtract( g_SkeletonFrame.SkeletonData[ i ].Position, XMVectorZero() );

        // Calculate the distance between the center and tracked position
        FLOAT fCurDistance = XMVector3LengthSq( vTemp ).x; 
        if ( fCurDistance < fMinDistance )
        {
            iClosestBody = i;
            fMinDistance = fCurDistance;
        }              
    }

    // Only swap if there is a closest and it is not already in the first slot
    if ( iClosestBody != -1 )
    {
        dwTrackingIDs[ 0 ] = g_SkeletonFrame.SkeletonData[ iClosestBody ].dwTrackingID;
    }

    // This call will either specify no players to have skeletons for if none have been found
    // or the one closest to the 'center' as the one to have a skeleton
    NuiSkeletonSetTrackedSkeletons( dwTrackingIDs );

    PIXEndNamedEvent();

    return iClosestBody;
}


//--------------------------------------------------------------------------------------
// Name: UpdateSkeletonTracking()
// Desc: Updates the skeleton tracking with the latest data.
//--------------------------------------------------------------------------------------

BOOL UpdateSkeletonTracking()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );
  
    PIXBeginNamedEvent( 0, "Wait for g_hFrameEndEventHandle" );

    if ( WAIT_OBJECT_0 != WaitForSingleObject( g_hFrameEndEventHandle, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        PIXEndNamedEvent();
        PIXEndNamedEvent();
        return FALSE;
    }

    PIXEndNamedEvent();

    const NUI_IMAGE_FRAME* pDepthImageFrame;
    HRESULT hrDepth = NuiImageStreamGetNextFrame( g_DepthStreamHandle, 0, &pDepthImageFrame );
    HRESULT hrSkeleton = NuiSkeletonGetNextFrame( 0, &g_SkeletonFrame );
    
    // Get the latest depth and update the debug visualization with it
    if ( SUCCEEDED( hrDepth ) )
    {
        g_Visualization.SetDepthTexture( pDepthImageFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( g_DepthStreamHandle, pDepthImageFrame  );
    }

    // Get the latest skeleton data and update the debug visualization with it
    if ( SUCCEEDED( hrSkeleton ) )
    {
        g_Visualization.SetSkeletons( &g_SkeletonFrame );

        g_iSelectedSkeleton = SelectSkeleton();
    }
    else
    {
        PIXEndNamedEvent();
        return FALSE;
    }

    PIXEndNamedEvent();
    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: VisualizeSkeletonTracking()
// Desc: Renders a debug visualization of depth and skeleton
//--------------------------------------------------------------------------------------

VOID VisualizeSkeletonTracking()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // And render it
    g_Visualization.BeginRender();
    g_Visualization.RenderDepthStream( 65.0f, 285.0f, 200.0f, 150.0f );
    g_Visualization.RenderSkeletons( 65.0f, 285.0f, 200.0f, 150.0f );
    g_Visualization.EndRender();

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: GetSkeletonFrame()
// Desc: Returns the current skeleton frame
//--------------------------------------------------------------------------------------

NUI_SKELETON_FRAME* GetSkeletonFrame()
{
    return &g_SkeletonFrame;
}


//--------------------------------------------------------------------------------------
// Name: GetSelectedSkeleton()
// Desc: Returns the currently selected skeleton
//--------------------------------------------------------------------------------------

UINT GetSelectedSkeleton()
{
    return ( g_iSelectedSkeleton < 0 ||
             g_iSelectedSkeleton >= NUI_SKELETON_COUNT ) ? 0 : g_iSelectedSkeleton;
}


//--------------------------------------------------------------------------------------
// Name: PlayerIsTracked()
// Desc: Returns if a player is tracked or not
//--------------------------------------------------------------------------------------

BOOL PlayerIsTracked()
{
    return ( g_iSelectedSkeleton >= 0 );
}