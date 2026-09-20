//--------------------------------------------------------------------------------------
// CameraManager.cpp
//
// Manages the state and data associated with the natural input device.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


#include "CameraManager.h"
#include <AtgDebugDraw.h>
#include "AtgUtil.h"
#include <AtgNuiCommon.h>


//--------------------------------------------------------------------------------------
// Constructor that initializes the natural input device.
//--------------------------------------------------------------------------------------
CameraManager::CameraManager() : 
	m_hDepth320x240( NULL ),
    m_hDepth80x60( NULL )
{

    m_pDepthFrame320x240 = NULL;
    m_pDepthFrame80x60 = NULL;
    ZeroMemory( &m_SkeletonFrame, sizeof( m_SkeletonFrame ) );
}


//--------------------------------------------------------------------------------------
// Destructor that shuts down the natural input device.
//--------------------------------------------------------------------------------------
CameraManager::~CameraManager() 
{
    ShutdownCamera();
}


//--------------------------------------------------------------------------------------
// Initialize the camera from the natural input device and skeleton tracking.
// Also allocate any memory needed for streaming data.
//--------------------------------------------------------------------------------------
HRESULT CameraManager::InitializeCamera( D3DDevice* pd3dDevice )
{

    // NOTE: Error codes can be looked up in the XDK documentation under
    // "System Error Codes"
    m_pd3dDevice = pd3dDevice;

    m_hFrameEndEvent = CreateEvent(NULL,
        FALSE,  // auto-reset
        FALSE,  // create unsignaled
        "NuiFrameEndEvent");

    if ( !m_hFrameEndEvent )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent\n" );
        return E_FAIL;
    }

    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_SUPPRESS_AUTOMATIC_UI | NUI_INITIALIZE_FLAG_USES_SKELETON | 
                                NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX,
                                NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );

    if ( FAILED(hr) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

    hr = NuiSetFrameEndEvent( m_hFrameEndEvent, 0 );
    
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }

    hr = NuiSkeletonTrackingEnable( NULL, 0 );

    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
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
	hr = m_pip.Initialize( pd3dDevice, NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                                                NUI_INITIALIZE_FLAG_USES_SKELETON, 
		                                        NUI_IMAGE_RESOLUTION_640x480 );
    if ( FAILED (hr) )
    {
        ATG_PrintError ( "Picture in Picture failed initialization" );
        return E_FAIL;
    }

    pd3dDevice->CreateTexture( 80, 60, 1, 0, ATG::GetAs16SRGBFormat( D3DFMT_LIN_X8R8G8B8 ), 0, &m_p80x60DepthVis, NULL );
    pd3dDevice->CreateTexture( 320, 240, 1, 0, ATG::GetAs16SRGBFormat( D3DFMT_LIN_X8R8G8B8 ), 0, &m_p320x240DepthVis, NULL );

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
// Draw the Depth Map and Skeleton.
//--------------------------------------------------------------------------------------
VOID CameraManager::DisplayPIP()
{
    UINT uWidth, uHeight;
    ATG::GetVideoSettings( &uWidth, &uHeight );

    static const FLOAT drawWidth = 320.0f;
    static const FLOAT drawHeight = 240.0f;
    static const FLOAT drawX = 70;
    static const FLOAT drawY = 430 ;
    
    m_pip.BeginRender();
    m_pip.RenderCustomDepthStream( drawX, drawY, drawWidth, drawHeight, m_p320x240DepthVis );
    m_pip.RenderCustomDepthStream( 70, 150, 320, 240, m_p80x60DepthVis );
    m_pip.RenderSkeletons( drawX, drawY, drawWidth, drawHeight );
    m_pip.EndRender();


    

}

//--------------------------------------------------------------------------------------
// Release depth maps if we're done with them.
//--------------------------------------------------------------------------------------
VOID CameraManager::ReleaseDepthMaps ()
{
        
        if ( m_pDepthFrame320x240 != NULL ) 
        {
            NuiImageStreamReleaseFrame( m_hDepth320x240, m_pDepthFrame320x240 );
            m_pDepthFrame320x240 = NULL;
        }
        if ( m_pDepthFrame80x60 != NULL ) 
        {
            NuiImageStreamReleaseFrame( m_hDepth80x60, m_pDepthFrame80x60 );
            m_pDepthFrame80x60 = NULL;
        }

}

VOID CameraManager::Copy8060Texture()
{
    D3DLOCKED_RECT DepthRect;
    D3DLOCKED_RECT RGBRect;

    GetDepthMap80x60()->LockRect(0, &DepthRect, NULL, 0 );
    m_p80x60DepthVis->LockRect( 0, &RGBRect, NULL, 0 );
    D3DCOLOR *pColorTable = m_pip.GetColorTable();

        // Fill in the depthmap data
    DWORD* lpBits = ( DWORD* )RGBRect.pBits;
    USHORT* pDepthMapCur = ( USHORT* )DepthRect.pBits;
    
    for( UINT y = 0; y < 60; ++ y )
    {
        for( UINT x = 0; x < 80; ++ x )
        {
            // To colorize the depth values, normalize the depth value against a maximum depth
            // value to be colorized and lookup into the color table
            const static FLOAT fMaxDepth = 3500.0f;                           // use 3.5 meters as max depth to colorize
            const static UINT uNormalize = (UINT) ceil( fMaxDepth / 511.0 );
            UINT uIndex = min( (USHORT)( pDepthMapCur[ x ] >> 3 ) / uNormalize, 511 );
            lpBits[x] = pColorTable[ uIndex ];
        }
        lpBits += RGBRect.Pitch / sizeof( DWORD );
        pDepthMapCur += DepthRect.Pitch / sizeof( USHORT );
    }

    GetDepthMap80x60()->UnlockRect( 0 );
    m_p80x60DepthVis->UnlockRect( 0 );

}

VOID CameraManager::Copy320x240Texture()
{
    D3DLOCKED_RECT DepthRect;
    D3DLOCKED_RECT RGBRect;

    GetDepthMap320x240()->LockRect(0, &DepthRect, NULL, 0 );
    m_p320x240DepthVis->LockRect( 0, &RGBRect, NULL, 0 );
    D3DCOLOR *pColorTable = m_pip.GetColorTable();

        // Fill in the depthmap data
    DWORD* lpBits = ( DWORD* )RGBRect.pBits;
    USHORT* pDepthMapCur = ( USHORT* )DepthRect.pBits;
    
    for( UINT y = 0; y < 240; ++ y )
    {
        for( UINT x = 0; x < 320; ++ x )
        {
            // To colorize the depth values, normalize the depth value against a maximum depth
            // value to be colorized and lookup into the color table
            const static FLOAT fMaxDepth = 3500.0f;                           // use 3.5 meters as max depth to colorize
            const static UINT uNormalize = (UINT) ceil( fMaxDepth / 511.0 );
            UINT uIndex = min( (USHORT)( pDepthMapCur[ x ] >> 3 ) / uNormalize, 511 );
            lpBits[x] = pColorTable[ uIndex ];
        }
        lpBits += RGBRect.Pitch / sizeof( DWORD );
        pDepthMapCur += DepthRect.Pitch / sizeof( USHORT );
    }

    GetDepthMap320x240()->UnlockRect( 0 );
    m_p320x240DepthVis->UnlockRect( 0 );

}

//--------------------------------------------------------------------------------------
// Read the joint data from the camera stream by polling for the latest data.
//--------------------------------------------------------------------------------------
BOOL CameraManager::CheckForNewSkeletonAndDepthMaps( FLOAT fElapsedTime, DWORD dwFilterFlags )
{    

    PIXBeginNamedEvent( 0, "WaitFor m_hFrameEndEvent" );
    if( WAIT_OBJECT_0 != WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        PIXEndNamedEvent();
        return FALSE;
    }
    PIXEndNamedEvent( );
    
    // Get data from the next camera depth frame
    HRESULT hr320x240 = NuiImageStreamGetNextFrame( m_hDepth320x240, 0, &m_pDepthFrame320x240 );
    HRESULT hr80x60 = NuiImageStreamGetNextFrame( m_hDepth80x60, 0, &m_pDepthFrame80x60 );
    // get skeleton data
    HRESULT hrSkeleton = NuiSkeletonGetNextFrame( 0, &m_SkeletonFrame );

    if( SUCCEEDED( hr320x240 ) )
    {
        Copy320x240Texture( );
    }
    else 
    {
        return FALSE;
    }

    if( SUCCEEDED( hr80x60 ) )
    {
        Copy8060Texture();
    }
    else
    {
        if ( m_pDepthFrame320x240 != NULL ) 
        {
            NuiImageStreamReleaseFrame( m_hDepth320x240, m_pDepthFrame320x240 );
            m_pDepthFrame320x240 = NULL;
        }
        return FALSE;
    }

    BOOL fTracked = FALSE;
    if ( SUCCEEDED( hrSkeleton ) )
    {

        for (UINT uCurrentSkeleton = 0; uCurrentSkeleton < NUI_SKELETON_COUNT; uCurrentSkeleton++)
        {
            if ( m_SkeletonFrame.SkeletonData[uCurrentSkeleton].eTrackingState == NUI_SKELETON_TRACKED )
            {
                fTracked = TRUE;
                break;
            }
        }
        
        if ( fTracked )
        {
            m_pip.SetSkeletons( &m_SkeletonFrame );
        }

    }
    if ( !fTracked )
    {
        if ( m_pDepthFrame320x240 != NULL ) 
        {
            NuiImageStreamReleaseFrame( m_hDepth320x240, m_pDepthFrame320x240 );
            m_pDepthFrame320x240 = NULL;
        }
        if ( m_pDepthFrame80x60 != NULL ) 
        {
            NuiImageStreamReleaseFrame( m_hDepth80x60, m_pDepthFrame80x60 );
            m_pDepthFrame80x60 = NULL;
        }
        return FALSE;
    }

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Determines if a player is facing the camera or not
//--------------------------------------------------------------------------------------
BOOL CameraManager::FacingCamera() const
{
    UINT uCurrentSkeleton = 0;

    // find the first tracked skeleton
    for (uCurrentSkeleton = 0; uCurrentSkeleton < NUI_SKELETON_COUNT; uCurrentSkeleton++)
    {
        if ( m_SkeletonFrame.SkeletonData[uCurrentSkeleton].eTrackingState == NUI_SKELETON_TRACKED )
        {
            break;
        }
    }
    
    // return FALSE if no skeletons are tracked
    if ( uCurrentSkeleton >= NUI_SKELETON_COUNT )
    {
        return FALSE;
    }

    CONST NUI_SKELETON_DATA*  pSkeletonData = &m_SkeletonFrame.SkeletonData[ uCurrentSkeleton ];

        // Create 4 body direction vectors from shoulders and hips
    XMVECTOR vNeckPos           = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_CENTER ];
    XMVECTOR vLeftShoulderPos   = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_LEFT ];
    XMVECTOR vRightShoulderPos  = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ];
    XMVECTOR vHipCenter         = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_CENTER ];
    XMVECTOR vLeftHipPos        = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_LEFT ];
    XMVECTOR vRightHipPos       = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_RIGHT ];

    XMVECTOR vLeftShoulderNormal    = XMVector3Normalize( XMVector3Cross( XMVector3Normalize( vLeftShoulderPos - vNeckPos ), XMVector3Normalize( vHipCenter - vNeckPos ) ) );
    XMVECTOR vRightShoulderNormal   = XMVector3Normalize( XMVector3Cross( XMVector3Normalize( vHipCenter - vNeckPos ), XMVector3Normalize( vRightShoulderPos - vNeckPos ) ) );
    XMVECTOR vLeftHipNormal         = XMVector3Normalize( XMVector3Cross( XMVector3Normalize( vNeckPos - vHipCenter ), XMVector3Normalize( vLeftHipPos - vHipCenter ) ) );
    XMVECTOR vRightHipNormal        = XMVector3Normalize( XMVector3Cross( XMVector3Normalize( vRightHipPos - vHipCenter ), XMVector3Normalize( vNeckPos - vHipCenter ) ) );

    // Get the tracking states for shoulders and hips
    NUI_SKELETON_POSITION_TRACKING_STATE leftShoulderState  = pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_LEFT ];
    NUI_SKELETON_POSITION_TRACKING_STATE rightShoulderState = pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ];
    NUI_SKELETON_POSITION_TRACKING_STATE leftHipState       = pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_LEFT ];
    NUI_SKELETON_POSITION_TRACKING_STATE rightHipState      = pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_RIGHT ];

    // Calculate weights from the tracking states, so that when we combine the left and right,
    // we weight angles from tracked positions higher than inferred positions
    FLOAT fWeights[ 4 ] = { leftShoulderState   == NUI_SKELETON_POSITION_TRACKED ? 1.0f : 0.25f,
                            rightShoulderState  == NUI_SKELETON_POSITION_TRACKED ? 1.0f : 0.25f,
                            leftHipState        == NUI_SKELETON_POSITION_TRACKED ? 1.0f : 0.25f,
                            rightHipState       == NUI_SKELETON_POSITION_TRACKED ? 1.0f : 0.25f };

    FLOAT fTotalShoulders   = fWeights[ 0 ] + fWeights[ 1 ];
    FLOAT fTotalHips        = fWeights[ 2 ] + fWeights[ 3 ];

    // Calculate shoulder and hip rotations
    FLOAT fLeftShoulderAngle    = XMConvertToDegrees( atan2( vLeftShoulderNormal.x, vLeftShoulderNormal.z ) );
    FLOAT fRightShoulderAngle   = XMConvertToDegrees( atan2( vRightShoulderNormal.x, vRightShoulderNormal.z ) );
    FLOAT fLeftHipAngle         = XMConvertToDegrees( atan2( vLeftHipNormal.x, vLeftHipNormal.z ) );
    FLOAT fRightHipAngle        = XMConvertToDegrees( atan2( vRightHipNormal.x, vRightHipNormal.z ) );
    FLOAT fAvgShoulderAngle     = ( ( fLeftShoulderAngle * fWeights[ 0 ] ) + ( fRightShoulderAngle * fWeights[ 1 ] ) ) / fTotalShoulders;
    FLOAT fAvgHipAngle          = ( ( fLeftHipAngle * fWeights[ 2 ] ) + ( fRightHipAngle * fWeights[ 3 ] ) ) / fTotalHips;
    FLOAT fAvgAngle             = ( fLeftShoulderAngle + fRightShoulderAngle + fLeftHipAngle + fRightHipAngle + fAvgShoulderAngle + fAvgHipAngle ) / 6.0f;

    // if body rotation is within [-45..45] degrees to camera, then we're facing the camera
    if ( fabs( fAvgAngle ) < 45.0f )
    {
        return TRUE;
    }
    
    return FALSE;
}