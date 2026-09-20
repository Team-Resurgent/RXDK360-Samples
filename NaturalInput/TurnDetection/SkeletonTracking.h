//--------------------------------------------------------------------------------------
// SkeletonTracking.h
//
// Declares functions used for simple skeleton tracking
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

HRESULT InitializeSkeletonTracking( D3DDevice* pd3dDevice );

BOOL UpdateSkeletonTracking();

VOID VisualizeSkeletonTracking();

NUI_SKELETON_FRAME* GetSkeletonFrame();

INT GetSelectedSkeleton();