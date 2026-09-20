//--------------------------------------------------------------------------------------
// SkeletonTracking.h
//
// Declares functions used for simple skeleton tracking
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <nuiapi.h>

HRESULT InitializeSkeletonTracking( D3DDevice* pd3dDevice );

VOID UpdateSkeletonTracking( D3DDevice* pd3dDevice );

VOID VisualizeSkeletonTracking( D3DDevice* pd3dDevice );

VOID SetSmoothingState( const BOOL bState );
BOOL GetSmoothingState();

VOID SetTiltCorrectionState( const BOOL bState );
BOOL GetTiltCorrectionState();

NUI_SKELETON_DATA* GetClosestSkeletonData();
LARGE_INTEGER GetCurrentTimeStamp();
XMVECTOR GetCurrentNormalToGravity();