//-------------------------------------------------------------------------------------
// CameraManager.h
//  
// Manages the state and data associated with the natural input device.
//  
// Advanced Technology Group (ATG)
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef _CAMERA_MANAGER_H_
#define _CAMERA_MANAGER_H_

#include <xtl.h>
#include <xnamath.h>
#include <NuiApi.h>
#include <ATGNUIVisualization.h>
#include <NuiHandles.h>


class CameraManager
{
public:
    struct HandPositions
    {
        BOOL        m_bUseHands;
        BOOL        m_bUseLeftHand;
        XMFLOAT3    m_leftHand;
        XMFLOAT3    m_rightHand;
    };

    CameraManager();
    ~CameraManager();

    HRESULT InitializeCamera( D3DDevice* pd3dDevice );
    VOID ShutdownCamera();
    VOID DisplayPIP();
    BOOL CheckForNewSkeletonAndDepthMaps( FLOAT fElapsedTime, BOOL bPause );

    // pip
    static const UINT pipDrawWidth = 640;
    static const UINT pipDrawHeight = 480;
    static const UINT pipDrawX = 320;
    static const UINT pipDrawY = 150;

private:

    NUI_SKELETON_FRAME m_skeletonFrame;

    // Visualize the depth and color buffers
	CONST NUI_IMAGE_FRAME*      m_pDepthFrame320x240;
	CONST NUI_IMAGE_FRAME*      m_pDepthFrame80x60;
	HANDLE                      m_hDepth320x240;
	HANDLE                      m_hDepth80x60;
    HANDLE                      m_hFrameEndEvent;
    LPDIRECT3DTEXTURE9          m_p320x240DepthVis;
	ATG::NuiVisualization       m_pip;
    HandPositions               m_handPositions;
    NUI_HANDLES_ARMS            m_nuiCursor;

    VOID Copy8060Texture();
    VOID Copy320x240Texture();

    D3DDevice* m_pd3dDevice;

    USHORT  m_depth320x240[ 320 * 240 ];
public:

    __forceinline const NUI_SKELETON_FRAME* GetSkeletonFrame() const
    { 
        return &m_skeletonFrame;
    }
    
    __forceinline
     const NUI_SKELETON_DATA* GetTrackedSkeleton() const
    {
        for ( INT iIndex = 0; iIndex < NUI_SKELETON_COUNT; ++iIndex )
        {
            if ( m_skeletonFrame.SkeletonData[iIndex].eTrackingState == NUI_SKELETON_TRACKED ) 
            {
                return &m_skeletonFrame.SkeletonData[iIndex];
            } 
        }
        return NULL;
    };

    __forceinline
    INT GetTrackedSkeletonIndex() const
    {
        for ( INT iIndex = 0; iIndex < NUI_SKELETON_COUNT; ++iIndex )
        {
            if ( m_skeletonFrame.SkeletonData[iIndex].eTrackingState == NUI_SKELETON_TRACKED ) 
            {
                return iIndex;
            } 
        }
        return -1;
    };

    __forceinline
    const USHORT*   GetDepth320x240() const
    {
        return m_depth320x240;
    }

    __forceinline
    const HandPositions&    GetHandsPositions() const
    {
        return m_handPositions;
    }
};

#endif