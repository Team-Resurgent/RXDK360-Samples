//-------------------------------------------------------------------------------------
// CameraManager.h
//  
// Manages the state and data associated with the natural input device.
//  
// Microsoft XNA Developer Connection
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef _CAMERA_MANAGER_H_
#define _CAMERA_MANAGER_H_

#include <xtl.h>
#include <xnamath.h>
#include <NuiApi.h>
#include <ATGNUIVisualization.h>
//-------------------------------------------------------------------------------------
// Constants
//-------------------------------------------------------------------------------------

class CameraManager
{
public:
    CameraManager();
    virtual ~CameraManager();

    HRESULT InitializeCamera( D3DDevice* pd3dDevice );
    VOID ShutdownCamera();
    VOID DisplayPIP();
    virtual BOOL Update( DWORD dwFilteringFlags);

protected:

    VOID ApplyTiltCorrection( NUI_SKELETON_FRAME* pFrame );

    NUI_SKELETON_FRAME m_SkeletonFrame;

    XMFLOAT4 m_JointVelocities[ NUI_SKELETON_POSITION_COUNT ];

    DWORD m_dwCurrentFrameIndex;

    // Visualize the depth and color buffers
    CONST NUI_IMAGE_FRAME*      m_pImageFrame;
	CONST NUI_IMAGE_FRAME*      m_pDepthFrame;
	ATG::NuiVisualization       m_pip;
	HANDLE                      m_hImage;
	HANDLE                      m_hDepth;

    HANDLE                      m_hFrameEndEvent;
    
public:
    inline const NUI_SKELETON_FRAME* GetSkeleton() const { return &m_SkeletonFrame ; }

};

#endif