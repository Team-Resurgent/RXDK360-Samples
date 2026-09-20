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
#include <NuiHandles.h>


//--------------------------------------------------------------------------------------
// Constructor that initializes the natural input device.
//--------------------------------------------------------------------------------------
CameraManager::CameraManager() : 
	m_hDepth320x240( NULL ),
    m_hDepth80x60( NULL )
{
    m_pDepthFrame80x60 = NULL;
    m_pDepthFrame320x240 = NULL;

    ZeroMemory( &m_skeletonFrame, sizeof( m_skeletonFrame ) );
    ZeroMemory( &m_handPositions, sizeof( m_handPositions ) );
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

    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON | 
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

    // need 80x60 for arms only
    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX_80x60, NUI_IMAGE_RESOLUTION_80x60, 0, 1, NULL, &m_hDepth80x60 );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return hr;
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

    pd3dDevice->CreateTexture( 320, 240, 1, 0, ATG::GetAs16SRGBFormat( D3DFMT_LIN_X8R8G8B8 ), 0, &m_p320x240DepthVis, NULL );

    // nui handles
    hr = NuiHandlesArmsInit( &m_nuiCursor );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiArms" );
        return hr;
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
// Draw the Depth Map and Skeleton.
//--------------------------------------------------------------------------------------
VOID CameraManager::DisplayPIP()
{
    UINT uWidth, uHeight;
    ATG::GetVideoSettings( &uWidth, &uHeight );
    
    m_pip.BeginRender();
    m_pip.RenderCustomDepthStream( (FLOAT)pipDrawX, (FLOAT)pipDrawY, (FLOAT)pipDrawWidth, (FLOAT)pipDrawHeight, m_p320x240DepthVis );
    m_pip.RenderSkeletons( (FLOAT)pipDrawX, (FLOAT)pipDrawY, (FLOAT)pipDrawWidth, (FLOAT)pipDrawHeight );
    m_pip.EndRender();
}


//--------------------------------------------------------------------------------------
// Read the joint data from the camera stream by polling for the latest data.
//--------------------------------------------------------------------------------------
BOOL CameraManager::CheckForNewSkeletonAndDepthMaps( FLOAT fElapsedTime, BOOL bPause )
{    
    PIXBeginNamedEvent( 0, "CameraManager update" );

    PIXBeginNamedEvent( 0, "WaitFor m_hFrameEndEvent" );
    if( WAIT_OBJECT_0 != WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        PIXEndNamedEvent();
        return FALSE;
    }
    PIXEndNamedEvent();

    // if paused then there should be no change to the NUI state
    if( bPause )
        return TRUE;
    
    // get skeleton data
    const HRESULT hrSkeleton = NuiSkeletonGetNextFrame( 0, &m_skeletonFrame );

    // Get data from the next camera depth frame
    const HRESULT hr320x240 = NuiImageStreamGetNextFrame( m_hDepth320x240, 0, &m_pDepthFrame320x240 );
    if( FAILED( hr320x240 ) )
        return FALSE;

    const HRESULT hr80x60 = NuiImageStreamGetNextFrame( m_hDepth80x60, 0, &m_pDepthFrame80x60 );
    if( FAILED( hr80x60 ) )
    {
        NuiImageStreamReleaseFrame( m_hDepth320x240, m_pDepthFrame320x240 );
        return FALSE;
    }

    // colourise 320x240 and copy it out for external consumption
    {
        PIXBeginNamedEvent( 0, "Copy depth" );

        D3DLOCKED_RECT DepthRect;
        D3DLOCKED_RECT RGBRect;

        m_pDepthFrame320x240->pFrameTexture->LockRect(0, &DepthRect, NULL, 0 );
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
                UINT uIndex = min( (USHORT)( pDepthMapCur[ x ] >> 3 ) >> 3, 511 );      // just div by 8, 4096 max range
                lpBits[x] = pColorTable[ uIndex ];
            }
            lpBits += RGBRect.Pitch / sizeof( DWORD );
            pDepthMapCur += DepthRect.Pitch / sizeof( USHORT );
        }

        for( UINT i=0; i < 240; ++i )
            XMemCpy( &m_depth320x240[ i * 320 ], (BYTE*)DepthRect.pBits + DepthRect.Pitch * i, 320 * 2 );

        m_pDepthFrame320x240->pFrameTexture->UnlockRect( 0 );
        m_p320x240DepthVis->UnlockRect( 0 );

        PIXEndNamedEvent();
    }

    BOOL fTracked = FALSE;
    UINT uCurrentSkeleton = 0;
    if ( SUCCEEDED( hrSkeleton ) )
    {
        for (uCurrentSkeleton = 0; uCurrentSkeleton < NUI_SKELETON_COUNT; uCurrentSkeleton++)
        {
            if ( m_skeletonFrame.SkeletonData[uCurrentSkeleton].eTrackingState == NUI_SKELETON_TRACKED )
            {
                fTracked = TRUE;
                break;
            }
        }
        
        if ( fTracked )
        {
            m_pip.SetSkeletons( &m_skeletonFrame );
        }
    }
    
    if( fTracked )
    {
        PIXBeginNamedEvent( 0, "Arms" );
        NuiHandlesArmsUpdate( &m_nuiCursor, uCurrentSkeleton, &m_skeletonFrame, m_pDepthFrame320x240, NULL, m_pDepthFrame80x60, NULL );
        PIXEndNamedEvent();
    }

    // release stuff
    NuiImageStreamReleaseFrame( m_hDepth320x240, m_pDepthFrame320x240 );
    NuiImageStreamReleaseFrame( m_hDepth80x60, m_pDepthFrame80x60 );

    // update nui arms positions
    if( fTracked )
    {
        // update cursor using NUI
        XMFLOAT3 vNuiCursorRight;
        XMFLOAT3 vNuiCursorLeft;
        XMStoreFloat3( &vNuiCursorRight, NuiHandlesArmGetScreenSpaceLocation( &m_nuiCursor, NUI_HANDLES_ARMS_HANDEDNESS_RIGHT_ARM ) );
        XMStoreFloat3( &vNuiCursorLeft, NuiHandlesArmGetScreenSpaceLocation( &m_nuiCursor, NUI_HANDLES_ARMS_HANDEDNESS_LEFT_ARM ) );

        // detect active hand
        m_handPositions.m_bUseLeftHand = (vNuiCursorLeft.x >= -1 && vNuiCursorLeft.x <= 1 &&
                                          vNuiCursorLeft.y >= -1 && vNuiCursorLeft.y <= 1 );
        m_handPositions.m_bUseHands = m_handPositions.m_bUseLeftHand;
        if( !m_handPositions.m_bUseLeftHand )
        {
            m_handPositions.m_bUseHands = ( vNuiCursorRight.x >= -1 && vNuiCursorRight.x <= 1 &&
                                            vNuiCursorRight.y >= -1 && vNuiCursorRight.y <= 1 );
        }

        m_handPositions.m_leftHand.x = vNuiCursorLeft.x * 1280.f / 2.f + 1280.f / 2;
        m_handPositions.m_leftHand.y = 720.f / 2 - vNuiCursorLeft.y * 720.f/ 2.f;
        m_handPositions.m_leftHand.z = vNuiCursorLeft.z;

        m_handPositions.m_rightHand.x = vNuiCursorRight.x * 1280.f / 2 + 1280.f / 2;
        m_handPositions.m_rightHand.y = 720.f / 2 - vNuiCursorRight.y * 720.f / 2;
        m_handPositions.m_rightHand.z = vNuiCursorRight.z;
    }

    PIXEndNamedEvent();

    return fTracked;
}
