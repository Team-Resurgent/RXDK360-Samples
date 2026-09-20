//--------------------------------------------------------------------------------------
// KinectSensor.h
//
// Microsoft Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#ifndef _KINECT_SENSOR_H_
#define _KINECT_SENSOR_H_


#include <NuiApi.h>


struct KinectFrame
{
    HRESULT                hrSkeletonRetrieved;
    NUI_SKELETON_FRAME     SkeletonFrame;

    HRESULT                hrImageRetrieved;
    CONST NUI_IMAGE_FRAME* pImageFrame;

    HRESULT                hr320x240Retrieved;
	CONST NUI_IMAGE_FRAME* pDepthFrame320x240;

    HRESULT                hr80x60Retrieved;
	CONST NUI_IMAGE_FRAME* pDepthFrame80x60;
};

class KinectSensor
{
public:
    KinectSensor();

    HRESULT Initialize( DWORD dwFlags,
                        DWORD dwHardwareThreadSkeleton = NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );

    HRESULT AcquireKinectFrame( KinectFrame* pKinectFrame );
    HRESULT ReleaseKinectFrame( KinectFrame* pKinectFrame );

private:
    KinectSensor( const KinectSensor& rhs );
    KinectSensor operator =( const KinectSensor& rhs );

    HANDLE m_hFrameEndEvent;
    HANDLE m_hImage;
    HANDLE m_hDepth320x240;
    HANDLE m_hDepth80x60;
};

#endif // _KINECT_SENSOR_H_