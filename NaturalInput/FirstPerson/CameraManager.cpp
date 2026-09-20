//--------------------------------------------------------------------------------------
// CameraManager.cpp
//
// Manages the state and data associated with the natural input device.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "CameraManager.h"
#include <AtgUtil.h>
#include <AtgNuiCommon.h>


//--------------------------------------------------------------------------------------
// Constructor that initializes the natural input device.
//--------------------------------------------------------------------------------------
CameraManager::CameraManager() : 
    m_dwCurrentFrameIndex( 1 ),
    m_hImage( NULL ),
    m_hDepth320x240( NULL ),
    m_hDepth80x60( NULL )

{
    ZeroMemory( &m_SkeletonFrame, sizeof( m_SkeletonFrame ) );
}


//--------------------------------------------------------------------------------------
// Destructor that shuts down the natural input device.
//--------------------------------------------------------------------------------------
CameraManager::~CameraManager() 
{
}


//--------------------------------------------------------------------------------------
// Initialize the camera from the natural input device and skeleton tracking.
// Also allocate any memory needed for streaming data.
//--------------------------------------------------------------------------------------
HRESULT CameraManager::InitializeCamera( D3DDevice* pd3dDevice )
{
    // NOTE: Error codes can be looked up in the XDK documentation under
    // "System Error Codes"

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

    hr = NuiSetFrameEndEvent( m_hFrameEndEvent, 0 );
    
    if ( FAILED(hr) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }

    hr = NuiSkeletonTrackingEnable( NULL, 0 );

    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
    }

    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 0, 1, NULL, &m_hImage );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

       // Open the depth stream
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, NUI_IMAGE_RESOLUTION_320x240, 0, 1, NULL, &m_hDepth320x240 );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

       // Open the depth stream
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX_80x60, NUI_IMAGE_RESOLUTION_80x60, 0, 1, NULL, &m_hDepth80x60 );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }
     // Initialiez the Picture in Picture visualization
	hr = m_pip.Initialize( pd3dDevice, NUI_INITIALIZE_FLAG_USES_COLOR |
                                                NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                                                NUI_INITIALIZE_FLAG_USES_SKELETON, 
		                          NUI_IMAGE_RESOLUTION_640x480 );
    if ( FAILED (hr) )
    {
        ATG_PrintError ( "Picture in Picture failed initialization" );
        return E_FAIL;
    }


    return S_OK;
}


//--------------------------------------------------------------------------------------
// Shuts down the camera and skeleton tracking
//--------------------------------------------------------------------------------------
VOID CameraManager::ShutdownCamera()
{
    NuiShutdown();
}

//--------------------------------------------------------------------------------------
// Draw the Depth Map, Color Map and Skeleton.
//--------------------------------------------------------------------------------------
VOID CameraManager::DisplayPIP()
{
    static const FLOAT drawWidth = 160.0f;
    static const FLOAT drawHeight = 120.0f;
    static const FLOAT drawX = 475.0f;
    static const FLOAT drawY = 45.0f;
    m_pip.BeginRender();
    m_pip.RenderDepthStream( drawX, drawY, drawWidth, drawHeight );
    m_pip.RenderSkeletons( drawX, drawY, drawWidth, drawHeight );
    m_pip.RenderColorStream( drawX + 165, drawY, drawWidth, drawHeight );
    m_pip.EndRender();

}

//--------------------------------------------------------------------------------------
// Read the joint data from the camera stream by polling for the latest data.
//--------------------------------------------------------------------------------------
BOOL CameraManager::Update( FLOAT fElapsedTime, DWORD dwFilterFlags )
{    
    if( WAIT_OBJECT_0 != WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        return FALSE;
    }

    // Get data from the next camera image frame
    HRESULT hrImage = NuiImageStreamGetNextFrame( m_hImage, 0, &m_pImageFrame );
    // Get data from the next camera depth frame
    HRESULT hr320x240 = NuiImageStreamGetNextFrame( m_hDepth320x240, 0, &m_pDepthFrame320x240 );
    // Get data from the next camera depth frame (80x60 resolution)
    HRESULT hr80x60 = NuiImageStreamGetNextFrame( m_hDepth80x60, 0, &m_pDepthFrame80x60 );
    // Get data from the next skeleton frame
    HRESULT hrSkeleton = NuiSkeletonGetNextFrame( 0, &m_SkeletonFrame );
    
    if( SUCCEEDED ( hrImage ) )
    {
        m_pip.SetColorTexture( m_pImageFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_hImage, m_pImageFrame );
    }
	
    if( SUCCEEDED( hr320x240 ) )
    {

        m_pip.SetDepthTexture( m_pDepthFrame320x240->pFrameTexture );
    }

    if ( SUCCEEDED( hrSkeleton ) && GetTrackedSkeleton() != NULL && SUCCEEDED( hr320x240 ) && SUCCEEDED( hr80x60 ) )
    {
        ATG::RefineHands(
            m_pDepthFrame320x240->pFrameTexture,
            m_pDepthFrame80x60->pFrameTexture,
            GetActiveSkeletonIndex(),
            GetSkeletonFrame(),
            0,
            &m_RefinmentData,
            NULL,
            NULL,
            NULL
        );

        NUI_SKELETON_DATA *pSkeletonData = (NUI_SKELETON_DATA *)GetTrackedSkeleton();
        pSkeletonData->SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT] = 
            m_RefinmentData.GetRefinedLeft();
        pSkeletonData->SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT] = 
            m_RefinmentData.GetRefinedRight();
    
        m_pip.SetSkeletons( &m_SkeletonFrame );

        ApplyTiltCorrection( &m_SkeletonFrame );

        m_dwCurrentFrameIndex = ( m_dwCurrentFrameIndex + 1 ) & 1;


    }

    if( SUCCEEDED( hr320x240 ) )
    {
        NuiImageStreamReleaseFrame( m_hDepth320x240, m_pDepthFrame320x240 );
    }
    if( SUCCEEDED( hr80x60 ) )
    {
        NuiImageStreamReleaseFrame( m_hDepth80x60, m_pDepthFrame80x60 );
    }


    return ( SUCCEEDED ( hr320x240 ) && SUCCEEDED ( hr80x60 ) && SUCCEEDED( hrSkeleton ) && GetTrackedSkeleton() != NULL );
}

//--------------------------------------------------------------------------------------
// Applies tilt correction to the skeleton data.
//--------------------------------------------------------------------------------------
VOID CameraManager::ApplyTiltCorrection( NUI_SKELETON_FRAME *pSkeleton )
{
    // Apply tilt correction. First we get our Up vector.
    static XMVECTOR vAverageNormalToGravity = pSkeleton->vNormalToGravity;
    XMVECTOR vNormToGrav = pSkeleton->vNormalToGravity;
    // Average this a lot so that it doesn't add jumpiness ot the scene.
    vAverageNormalToGravity = vAverageNormalToGravity * 0.9f + vNormToGrav * 0.1f;

    // In this release only, until final hardware with built in accelerometer ships,
    // we need to check for an invalid up vector (we will synthesize it from
    // the floor plane if that data is present). If we can't get an up
    // vector, we default to 0.0, 1.0, 0.0 instead.

    if ( fabs(vNormToGrav.x) < FLT_EPSILON &&
        fabs(vNormToGrav.y) < FLT_EPSILON &&
        fabs(vNormToGrav.z) < FLT_EPSILON )
    {
        static const XMVECTOR c_vUp = { 0.0, 1.0, 0.0, 0.0 };
        vNormToGrav = c_vUp;
    }

    // Generate the leveling matrix and apply it to all points on any skeletons
    // which are currently being tracked. 

    XMMATRIX matLevel = NuiTransformMatrixLevel( vAverageNormalToGravity );

    for ( UINT i = 0 ; i < NUI_SKELETON_COUNT ; ++i )
    {
        if ( pSkeleton->SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED )
        {
            for ( UINT j = 0; j < NUI_SKELETON_POSITION_COUNT ; ++j )
            {
                pSkeleton->SkeletonData[i].SkeletonPositions[j] = 
                    XMVector3Transform( pSkeleton->SkeletonData[i].SkeletonPositions[j], matLevel);
            }
        }
    }
}