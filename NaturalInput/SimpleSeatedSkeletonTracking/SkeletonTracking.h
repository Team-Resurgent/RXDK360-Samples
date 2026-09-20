//--------------------------------------------------------------------------------------
// SkeletonTracking.h
//
// Declares functions used for simple skeleton tracking
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


#pragma once

HRESULT InitializeSkeletonTracking( D3DDevice* pd3dDevice, BOOL bUseSeatedST );

VOID UpdateSkeletonTracking( D3DDevice* pd3dDevice );

VOID VisualizeSkeletonTracking( D3DDevice* pd3dDevice, BOOL* pbTracking );

VOID SetSmoothingState( BOOL state );
BOOL GetSmoothingState();

VOID EnableSeatedSkeletonTracking( BOOL bEnableSeatedST );