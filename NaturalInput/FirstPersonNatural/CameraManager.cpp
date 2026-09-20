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
	m_hDepth( NULL )

{
    ZeroMemory( &m_SkeletonData, sizeof( m_SkeletonData ) );
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
    HRESULT hrDepth = NuiImageStreamGetNextFrame( m_hDepth, 0, &m_pDepthFrame );
    // Get data from the next skeleton frame
    HRESULT hrSkeleton = NuiSkeletonGetNextFrame( 0, &m_SkeletonData );
    
    if( SUCCEEDED ( hrImage ) )
    {
        m_pip.SetColorTexture( m_pImageFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_hImage, m_pImageFrame );
    }
	
    if( SUCCEEDED( hrDepth ) )
    {

        m_pip.SetDepthTexture( m_pDepthFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_hDepth, m_pDepthFrame );
    }

    if ( SUCCEEDED( hrSkeleton ) )
    {
        m_pip.SetSkeletons( &m_SkeletonData );

        ApplyTiltCorrection( &m_SkeletonData );

        m_dwCurrentFrameIndex = ( m_dwCurrentFrameIndex + 1 ) & 1;
    }
    return ( SUCCEEDED ( hrImage ) && SUCCEEDED ( hrDepth ) && SUCCEEDED ( hrSkeleton ) );
}

//--------------------------------------------------------------------------------------
// Applies tilt correction to the skeleton data.
//--------------------------------------------------------------------------------------
VOID CameraManager::ApplyTiltCorrection( NUI_SKELETON_FRAME *pSkeleton )
{
    // Apply tilt correction. First we get our Up vector.
    static XMVECTOR vAverageNormalToGravity = pSkeleton->vNormalToGravity;
    XMVECTOR vNormToGrav = pSkeleton->vNormalToGravity;
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
    XMVECTOR vSpine = pSkeleton->SkeletonData[0].SkeletonPositions[ NUI_SKELETON_POSITION_SPINE ];
    static XMVECTOR vAverageSpine = vSpine;
    vAverageSpine = 0.9f * vAverageSpine + 0.1f * vSpine;
    XMFLOAT4 fAverageSpine;
    XMStoreFloat4( &fAverageSpine, vAverageSpine );
    XMMATRIX matTranslateToOrigin = XMMatrixTranslation( -fAverageSpine.x, 0, -fAverageSpine.z );
    XMMATRIX matTranslateFromOrigin = XMMatrixTranslation( fAverageSpine.x, 0, fAverageSpine.z );
    XMMATRIX matTransformation = matTranslateToOrigin * matLevel * matTranslateFromOrigin;


    for ( UINT i = 0 ; i < NUI_SKELETON_COUNT ; ++i )
    {
        if ( pSkeleton->SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED )
        {

            for ( UINT j = 0; j < NUI_SKELETON_POSITION_COUNT ; ++j )
            {
                pSkeleton->SkeletonData[i].SkeletonPositions[j] = 
                    XMVector3Transform( pSkeleton->SkeletonData[i].SkeletonPositions[j], matTransformation);
            }
        }
    }
}