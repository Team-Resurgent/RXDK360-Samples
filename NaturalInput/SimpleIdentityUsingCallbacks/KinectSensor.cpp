//--------------------------------------------------------------------------------------
// KinectSensor.cpp
//
// Microsoft Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <AtgNuiCommon.h>
#include "KinectSensor.h"


KinectSensor::KinectSensor()
:m_hFrameEndEvent( NULL ),
 m_hImage( NULL ),
 m_hDepth320x240( NULL ),
 m_hDepth80x60( NULL )
{
}


KinectSensor::~KinectSensor()
{
}


//--------------------------------------------------------------------------------------
// Name: KinectSensor::Initialize()
// Desc: Setup the sensor array for RGB and depth streaming
//--------------------------------------------------------------------------------------
HRESULT KinectSensor::Initialize()
{
    // Create an event to signaled when frame processing ends
    m_hFrameEndEvent = CreateEvent( NULL,
                                    FALSE,  // auto-reset
                                    FALSE,  // create unsignaled
                                    "NuiFrameEndEvent" );

    if ( ! m_hFrameEndEvent )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent" );
        return E_FAIL;
    }

    // Initializes the Natural Input system on the default thread
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
        ATG::NuiPrintError( hr, "NuiImageStreamOpen color image" );
        return E_FAIL;
    }

    // Open the 320 x 240 depth stream. It is required by NuiHandles which is used to manage the menu cursor.
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, NUI_IMAGE_RESOLUTION_320x240, 0, 1, NULL, &m_hDepth320x240 );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen depth image 320x240" );
        return E_FAIL;
    }

    // Open the 80 x 60 depth stream. It is required by NuiHandles which is used to manage the menu cursor.
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX_80x60, NUI_IMAGE_RESOLUTION_80x60, 0, 1, NULL, &m_hDepth80x60 );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen depth image 80x60" );
        return E_FAIL;
    }

    // Enable skeletal tracking
    hr = NuiSkeletonTrackingEnable( NULL, 0 );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
        return E_FAIL;
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: KinectSensor::AcquireKinectFrame()
// Desc: Retrieves the images streams and the skeletons
//--------------------------------------------------------------------------------------
HRESULT KinectSensor::AcquireKinectFrame( KinectFrame* pKinectFrame )
{
    assert( pKinectFrame != NULL );

    HRESULT hr = E_FAIL;

    // Wait for frame processing to end
    if( WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) == WAIT_OBJECT_0 )
    {
        // Get data from the next camera depth frame
        pKinectFrame->hr320x240Retrieved = NuiImageStreamGetNextFrame( m_hDepth320x240, 0, &pKinectFrame->pDepthFrame320x240 );

        // Get data from the next camera depth frame (80x60 resolution)
        pKinectFrame->hr80x60Retrieved = NuiImageStreamGetNextFrame( m_hDepth80x60, 0, &pKinectFrame->pDepthFrame80x60 );

        // Get data from the next image frame
        pKinectFrame->hrImageRetrieved = NuiImageStreamGetNextFrame( m_hImage, 0, &pKinectFrame->pImageFrame );

        // Finally, get the skeletal frame
	    pKinectFrame->hrSkeletonRetrieved = NuiSkeletonGetNextFrame( 0, &pKinectFrame->SkeletonFrame );
    
        hr = S_OK;
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: KinectSensor::ReleaseKinectFrame()
// Desc: Releases any buffers that were locked by KinectCamera_AcquireKinectFrame()
//--------------------------------------------------------------------------------------
HRESULT KinectSensor::ReleaseKinectFrame( KinectFrame* pKinectFrame )
{
    if ( SUCCEEDED ( pKinectFrame->hrImageRetrieved ) )
    {
        pKinectFrame->hrImageRetrieved = NuiImageStreamReleaseFrame( m_hImage, pKinectFrame->pImageFrame );
    }

    if ( SUCCEEDED ( pKinectFrame->hr80x60Retrieved ) )
    {
        pKinectFrame->hr80x60Retrieved = NuiImageStreamReleaseFrame( m_hDepth80x60, pKinectFrame->pDepthFrame80x60 );
    }

    if ( SUCCEEDED ( pKinectFrame->hr320x240Retrieved ) )
    {
        pKinectFrame->hr320x240Retrieved = NuiImageStreamReleaseFrame( m_hDepth320x240, pKinectFrame->pDepthFrame320x240 );
    }

    return S_OK;
}