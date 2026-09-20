//----------------------------------------------------------------------------------------------------------------------
// Visualization.h
// 
// Visualizations for skeletons, depth map and audio data for the sample.
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (c) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#ifndef VISUALIZATION_H_GUARD
#define VISUALIZATION_H_GUARD

HRESULT InitializeVisualization( D3DDevice* pd3dDevice,
                                 DWORD dwDepthStreamWidth, DWORD dwDepthStreamHeight );

VOID VisualizeStreams( D3DDevice* pd3dDevice );

VOID UpdateDepthTexture( D3DDevice* pd3dDevice, const NUI_IMAGE_FRAME* pDepthMap, DWORD dwActivePlayer );

VOID VisualizeSkeleton( const NUI_SKELETON_FRAME* const pSkeletonFrame );

struct NUI_TALKER_POSITION;
VOID VisualizeTalkerPosition( D3DDevice* pd3dDevice, FLOAT fBeamDirection, FLOAT fConfidence );

VOID VisualizeAudioData( const SHORT* pAudioBuffer, FLOAT fX, FLOAT fY, FLOAT fWidth, FLOAT fHeight );
VOID VisualizeAudioBand( const FLOAT* pBandBuffer, FLOAT fX, FLOAT fY, FLOAT fWidth, FLOAT fHeight );
VOID VisualizeTopView( FLOAT fBeamAngle, FLOAT fX, FLOAT fY, FLOAT fWidth, FLOAT fHeight, BOOL bTracked );

#endif //VISUALIZATION_H_GUARD
