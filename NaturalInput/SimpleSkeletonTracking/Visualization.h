//--------------------------------------------------------------------------------------
// Visualization.h
//
// Declares functions used to visualize simple skeleton tracking
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <nuiapi.h>


HRESULT InitializeVisualization( D3DDevice* pd3dDevice,
                                 DWORD dwColorStreamWidth, DWORD dwColorStreamHeight,
                                 DWORD dwDepthStreamWidth, DWORD dwDepthStreamHeight );

VOID VisualizeStreams( D3DDevice* pd3dDevice );

VOID UpdateDepthTexture( D3DDevice* pd3dDevice, const NUI_IMAGE_FRAME* pDepthMap );

VOID UpdateColorTexture( D3DDevice* pd3dDevice, const NUI_IMAGE_FRAME* pColorMap );

VOID VisualizeSkeleton( const NUI_SKELETON_FRAME * const pSkeletonFrame );

