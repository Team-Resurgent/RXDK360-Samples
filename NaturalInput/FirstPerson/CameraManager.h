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
#include <ATGNuiHandrefinement.h>

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
    virtual BOOL Update( FLOAT fElapsedTime, DWORD dwFilteringFlags);

protected:

    VOID ApplyTiltCorrection( NUI_SKELETON_FRAME* pFrame );

    NUI_SKELETON_FRAME m_SkeletonFrame;

    DWORD m_dwCurrentFrameIndex;

    // Visualize the depth and color buffers
	CONST NUI_IMAGE_FRAME*      m_pDepthFrame320x240;
    CONST NUI_IMAGE_FRAME*      m_pDepthFrame80x60;
    CONST NUI_IMAGE_FRAME*      m_pImageFrame;
    ATG::NuiVisualization       m_pip;
	HANDLE                      m_hDepth320x240;
    HANDLE                      m_hDepth80x60;
    HANDLE                      m_hImage;
    HANDLE                      m_hFrameEndEvent;
    ATG::RefinementData         m_RefinmentData;



public:
    inline const NUI_SKELETON_FRAME* GetSkeletonFrame() const { return &m_SkeletonFrame ; }
    inline INT GetActiveSkeletonIndex ()
    {
        for ( INT iIndex = 0; iIndex < NUI_SKELETON_COUNT; ++iIndex )
        {
            if ( m_SkeletonFrame.SkeletonData[iIndex].eTrackingState == NUI_SKELETON_TRACKED ) 
            {
                return iIndex;
            } 
        }
        return -1;
    }
    inline const NUI_SKELETON_DATA* GetTrackedSkeleton()
    {
        for ( INT iIndex = 0; iIndex < NUI_SKELETON_COUNT; ++iIndex )
        {
            if ( m_SkeletonFrame.SkeletonData[iIndex].eTrackingState == NUI_SKELETON_TRACKED ) 
            {
                return &m_SkeletonFrame.SkeletonData[iIndex];
            } 
        }
        return NULL;
    };
};

#endif